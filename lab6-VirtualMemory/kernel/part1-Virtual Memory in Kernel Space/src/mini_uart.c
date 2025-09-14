#include "mini_uart.h"

wait_queue_t uart_rx_lock;
wait_queue_t uart_tx_lock;

//Ring Buffer
char read_buf[BUFFER_MAX_SIZE];
char write_buf[BUFFER_MAX_SIZE];
int read_buf_start, read_buf_end;
int write_buf_start, write_buf_end;


static void enable_interrupt() { asm volatile("msr DAIFClr, 0xf"); }   // 0b0010，enable IRQ
static void disable_interrupt() { asm volatile("msr DAIFSet, 0xf"); }  // 0xf = 0b1111，代表設置 D, A, I, F，禁用所有中斷
// static void delay() {unsigned int r = 150; while(r--) { asm volatile("nop"); }}
void set_gpio_uart(){
	register unsigned int r;

	// 設置 GPIO14 (TXD) 和 GPIO15 (RXD) 為 ALT5（Mini UART 功能）
	r = *GPFSEL1;
	r &= ~((7 << 12) | (7 << 15)); // 清除 GPIO14, GPIO15
	r |= ((2 << 12) | (2 << 15));  // 設置為 ALT5
	*GPFSEL1 = r;

	// 禁用 GPIO pull-up/down
	*GPPUD = 0;
	for (r = 300; r > 0; r--) { asm volatile("nop"); }
	*GPPUDCLK0 = (1 << 14) | (1 << 15);
	for (r = 300; r > 0; r--) { asm volatile("nop"); }
	*GPPUDCLK0 = 0;
}

void enable_uart_interrupt(){
    // 啟用 Mini UART IRQ (bit 29)
    *ENABLE_IRQS_1 |= (1 << IRQ_AUX);   //mini uart 本身可以發送 interrupt
    *AUX_MU_IER |= 0x1;        // 0x0 代表全部 disable
                            // 0x01 是 RX interrupt，代表 kernel 要從 uart 收資料 
                            // 0x02 是 TX interrupt，代表 kernel 要寫資料到 uart
                            // 設定為 3 就是同時啟動

    asm volatile("msr daifclr, #2");  // kernel(EL2) enable IRQ
    unsigned int r = 1000000; while(r--) { asm volatile("nop"); }
}

void disable_uart_interrupt(){
    *AUX_MU_IER &= ~0x1;
    *AUX_MU_IER &= ~0x2;
    *DISABLE_IRQS_1 |= (1 << IRQ_AUX);  //mini uart 本身不可以發送 interrupt
    unsigned int r = 1000000; while(r--) { asm volatile("nop"); }
}

void uart_init(){
	set_gpio_uart();
    read_buf_start = read_buf_end = 0;
    write_buf_start = write_buf_end = 0;
    for (int i = 0; i < BUFFER_MAX_SIZE; i++) {
        read_buf[i] = write_buf[i] = '0';
    }

	//每個欄位的意義是參考 spec 2-1 p.8 的表格，然後值是跟著助教(https://nycu-caslab.github.io/OSC2025/labs/hardware/uart.html)設定的
    *AUX_ENABLE |= 1;           // Enable mini UART
    *AUX_MU_CNTL = 0;       // Disable TX, RX during configuration
    *AUX_MU_IER = 0;            //disable interrupt
    *AUX_MU_LCR = 3;        // Set the data size to 8 bit
    *AUX_MU_MCR = 0;        // Don't need auto flow control
    *AUX_MU_BAUD = 270;     // Set baud rate to 115200, After booting, the system clock is 250 MHz.
    *AUX_MU_IIR = 0xc6;        // enable FIFO, 是一種緩衝機制，比如 read 的時候會先寫到 FIFO，CPU再去從 FIFO撈數據，可以提高數據傳輸效率、防止數據丟失
                                // 1100 0110: Clear both receive and transmit FIFOs.	
	

	unsigned int r = 1000000; while(r--) { asm volatile("nop"); }
    
    // *ENABLE_IRQS_1 |= (1 << IRQ_AUX);
    // *AUX_MU_IER |= 3;        // 0x0 代表全部 disable
    //                         // 0x01 是 RX interrupt，代表 kernel 要從 uart 收資料 
    //                         // 0x02 是 TX interrupt，代表 kernel 要寫資料到 uart
    //                         // 設定為 3 就是同時啟動
	// r = 1000000; while(r--) { asm volatile("nop"); }
    // asm volatile("msr daifclr, #2");  // 允許 IRQ
	// r = 1000000; while(r--) { asm volatile("nop"); }

	*AUX_MU_CNTL = 3;
	r = 1000000; while(r--) { asm volatile("nop"); }

    wait_queue_init(&uart_rx_lock, 1);
    wait_queue_init(&uart_tx_lock, 1);
    uart_write_str("\r\n------- 0.  uart_init finished ----------");
}


////////////////////////////////////   handler ///////////////////////////////////////
void c_write_handler() {
    //如果 kernel 有資料要寫
    char c;
    disable_interrupt();
    while (write_buf_start != write_buf_end) {
        c = write_buf[write_buf_start];

        //如果你只印 \n 而不印 \r，螢幕會出現輸出「向右偏移 n 格」的情況
        //所以這邊看到 \n 就要先傳一個 \r 過去才能真的傳 \n
        //因為 \n 是換行，不會回到行首，所以要加上 \r
        if (c == '\n') {
            while (!((*AUX_MU_LSR) & 0x20));
            *AUX_MU_IO = '\r';
        }

        while (!((*AUX_MU_LSR) & 0x20));
        *AUX_MU_IO = c;
        while (!(*AUX_MU_LSR & 0x20));  //等一下確保AUX_MU_IO硬體部分有正確被寫入
        write_buf_start = (write_buf_start + 1) % BUFFER_MAX_SIZE;

        // 每搬一個字元，就 signal 一下（讓卡在 wait 的 thread 知道 buffer 有空了）
        // signal(&uart_tx_lock);
    }

    
    // 如果已經傳完 buffer 中的字元，就關閉 TX IRQ
    if (write_buf_start == write_buf_end) {
        *AUX_MU_IER &= ~0x02;  // disable TX IRQ
    }else {
        *AUX_MU_IER |= 0x2;
    }
    enable_interrupt();

}

void c_read_handler(){
    disable_interrupt();
    while (*AUX_MU_LSR & 0x01) {
        char c = (char)(*AUX_MU_IO);
        if (c == '\r') c = '\n';
    
        read_buf[read_buf_end] = c;
        read_buf_end = (read_buf_end + 1) %BUFFER_MAX_SIZE;

        // 每搬一個字元，signal 一下（讓卡在 wait 的 thread 知道有新資料）
        // signal(&uart_rx_lock);
    }
    *AUX_MU_IER |= 0x1;     // task 完成後重新開啟 RX IRQ
    enable_interrupt();
}

















/////////////////////////////////////////////////// async version function //////////////////////////////////////////////////////////////
// char uart_read_async() {
//     wait(&uart_rx_lock);
//     disable_interrupt();
//     if (read_buf_start == read_buf_end) {
//         asm volatile("msr daifclr, #2");  // enable IRQ
//         return -1;
//     }
//     char c = read_buf[read_buf_start];
//     read_buf_start = (read_buf_start + 1) % BUFFER_MAX_SIZE;
//     enable_interrupt();
//     signal(&uart_rx_lock);
//     return c;
// }

char uart_read_async() {
    char c;

    while (1) {
        // wait(&uart_rx_lock);        //防止其他 thread 存去
        disable_interrupt();        //防止 interrupt handler 存取

        if (read_buf_start != read_buf_end) {
            c = read_buf[read_buf_start];
            read_buf_start = (read_buf_start + 1) % BUFFER_MAX_SIZE;
            enable_interrupt();
            // signal(&uart_rx_lock);
            return c;
        }

        // 沒有資料，釋放 lock + sleep 等待 handler signal
        enable_interrupt();
        // signal(&uart_rx_lock);
        // wait(&uart_rx_lock);          //「睡覺」，而不是直接 loop
    }
}


// void uart_write_char_async(char c) {
//     wait(&uart_tx_lock);
//     disable_interrupt();
//     int next = (write_buf_end + 1) % BUFFER_MAX_SIZE;
    
//     // kernel write buffer full
//     while (next == write_buf_start){}
//     write_buf[write_buf_end] = c;
//     write_buf_end = next;
    

//     enable_interrupt();
//     signal(&uart_tx_lock);

//     // 啟用 TX IRQ (確保 TX 會發送資料)
//     // 因為上面 handler 會用 *AUX_MU_IER &= ~0x02; 把 TX 關掉，有資料的時候要重新打開
//     if (!(*AUX_MU_IER & 0x2)) {
//         *AUX_MU_IER |= 0x2;
//     }
// }

void uart_write_char_async(char c) {
    while (1) {
        // wait(&uart_tx_lock);
        disable_interrupt();

        int next = (write_buf_end + 1) % BUFFER_MAX_SIZE;
        if (next != write_buf_start) {
            write_buf[write_buf_end] = c;
            write_buf_end = next;
            enable_interrupt();
            // signal(&uart_tx_lock);

            // 確保 TX IRQ 開啟
            if (!(*AUX_MU_IER & 0x2)) {
                *AUX_MU_IER |= 0x2;
            }
            return;
        }

        // buffer 滿，釋放 lock + sleep 等待 handler 搬資料後喚醒
        enable_interrupt();
        // signal(&uart_tx_lock);
        // wait(&uart_tx_lock);          //「睡覺」，而不是直接 loop
    }
}


void readcmd_async(char* str){
    char input_ch;
    str[0] = 0;
    int str_ind = 0;

	while (1) {
        input_ch = uart_read_async();
        //-1 在 char 裡面會變成 255
        if ((int)input_ch == 255) continue;
        if (input_ch > 127 || input_ch < 0) continue;

        if (input_ch == '\n') {
            str[str_ind] = '\0';
            break;
        }
        if ((int)input_ch == 127 || input_ch == '\b') {  // 處理刪除鍵 (backspace)
            if (str_ind > 0) {  
                str_ind--;
                uart_write_char_async('\b');
                uart_write_char_async(' ');
                uart_write_char_async('\b');
            }
        } else if ((int)input_ch >= 32 && (int)input_ch <= 126 && str_ind < BUFFER_MAX_SIZE) {  // 可顯示的 ASCII 字符
            str[str_ind] = input_ch;
            str_ind += 1;
            uart_write_char_async(input_ch);  
        }

    }
}


// void uart_write_str_async(const char *str){
//     wait(&uart_tx_lock);
//     disable_interrupt();
//     for(int i = 0; str[i] != '\0'; i++){
//         if(str[i] == '\n'){
//             int next = (write_buf_end + 2) % BUFFER_MAX_SIZE;
//             if (next == write_buf_start) {
//                 // buffer full
//                 enable_interrupt();
//                 continue;
//             }
//             write_buf[write_buf_end] = '\r';
//             write_buf[write_buf_end+1] = str[i];
//             write_buf_end = next;
//             // write_buf[write_buf_end++] = '\r';
//             // write_buf_end %= BUFFER_MAX_SIZE;
//         }else{
//             int next = (write_buf_end + 1) % BUFFER_MAX_SIZE;
//             if (next == write_buf_start) {
//                 // buffer full
//                 enable_interrupt();
//                 continue;
//             }
//             write_buf[write_buf_end] = str[i];
//             write_buf_end = next;
//             // write_buf[write_buf_end++] = str[i];
//             // write_buf_end %= BUFFER_MAX_SIZE;
//         }
//     }

//     if (!(*AUX_MU_IER & 0x2)) {
//         *AUX_MU_IER |= 0x2;         //AUX_MU_IER  也是 global var 吧某種程度上
//     }
//     enable_interrupt();
//     signal(&uart_tx_lock);
//     wait(&uart_tx_lock);          //「睡覺」，而不是直接 loop
// }
void uart_write_str_async(const char *str){
    for (int i = 0; str[i] != '\0'; i++) {
        uart_write_char_async(str[i]);
    }
}


void uart_write_flush() {
    while (write_buf_start != write_buf_end || !(*AUX_MU_LSR & 0x20));
}

















/////////////////////////////////////////////////// sync version function ////////////////////////
char uart_read(){
    // wait(&uart_rx_lock);
    /* Check bit 0 for data ready field */
    while (!(*AUX_MU_LSR & 0x01)){
        //為了避免編譯器對這部分程式碼進行優化。
        //因為編譯器可能會將一個空的 while 迴圈優化掉，從而導致程式無法正確等待硬體狀態改變。
        asm volatile("nop");
    }
    
	//卡，這是我原本寫法，不知道為甚麼不行，問一下
    // char* buf_ptr = (char*)AUX_MU_IO;
    // return *buf_ptr == '\r' ? '\n' : *buf_ptr;
    //檢查buf_ptr指向的內容是否是 \r ，如果是的話要返回 \n，否則就返回原本的字元就好
	char r = (char)(*AUX_MU_IO & 0xFF);		// 只取低 8-bit

    // signal(&uart_rx_lock);
    return (r == '\r') ? '\n' : r; 			// Convert carriage return to newline
}


void uart_write_char(char ch){
    // wait(&uart_tx_lock);
    /* Check bit 5 for Transmitter empty field */
    while (!(*AUX_MU_LSR & 0x20)){
        asm volatile("nop");
    }
	*AUX_MU_IO = (unsigned int)ch;
    while (!(*AUX_MU_LSR & 0x20));  //等一下確保AUX_MU_IO硬體部分有正確被寫入
    // signal(&uart_tx_lock);
}


// Get the command from user prompt
void readcmd(char* str){
    char input_ch;
    str[0] = 0;
    int str_ind = 0;

	while (1) {
        // 從 UART 讀取字符，注意這邊如果直接改成 uart_read_async 不行
        // 因為我的 uart_read_async 在read_buf_start == read_buf_end，函數會回傳 0
        //也就是說當程式執行到這邊原本會卡住等待使用者輸入，現在使用者還沒輸入就會回傳 0!!!
        input_ch = uart_read();

        if (input_ch == '\n') {  // 回車鍵處理
            break;
        }

        // 處理刪除鍵 (backspace)
        if (input_ch == 127 || input_ch == '\b') {
            if (str_ind > 0) {  // 如果還有字符可刪除
                str_ind--;
                uart_write_str("\b \b");
                // uart_write_char('\b');  // 輸出回退字符
                // uart_write_char(' ');  // 顯示刪除的字符（空格）
                // uart_write_char('\b');  // 再回退一次，顯示刪除效果
            }
        } else if (input_ch >= 32 && input_ch <= 126) {  // 可顯示的 ASCII 字符
            str[str_ind] = input_ch;
            str_ind+=1;
            char buf[2] = {input_ch, 0};  // \0 terminate it
            uart_write_str(buf);
            // uart_write_char(input_ch);
        }
    }
    str[str_ind] = 0;
}
void uart_write_str(const char *str){
    // wait(&uart_tx_lock);
    while (*str) {
        if (*str == '\n') uart_write_char('\r');  // 處理 CRLF
        uart_write_char(*str++);
    }
    // signal(&uart_tx_lock);
}



/////////////////////////////////////////////////// other function ////////////////////////
//dec 格式數字
void int2str_dec(int num, char *str){
    int length = 0, temp = num;
    int is_negative = 0;

    //1. 處理負數的情況
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }

    //2. 計算數字的長度
    do {
        length++;
        num /= 10;
    } while (num != 0);
    if (is_negative) { // 如果是負數，需要多一位來存儲負號
        length++;
    }
    str[length] = '\0';


    //3. 填充數字到str內
    num = temp;
    if (is_negative) {
        str[0] = '-';
        num = -num;
    }
    for (int i = length - 1; i >= is_negative; i--) {
        str[i] = (num % 10) + '0';      // int to char (ASCII)
        num /= 10;
    }
}
void uint2str_dec(unsigned int num, char* str) {
    int length = 0;
    unsigned int temp = num;

    // 計算長度
    do {
        length++;
        num /= 10;
    } while (num != 0);

    str[length] = '\0';

    // 填入字元
    num = temp;
    for (int i = length - 1; i >= 0; i--) {
        str[i] = (num % 10) + '0';
        num /= 10;
    }
}

void uart_write_int(int num){
    char str[12];
    int2str_dec(num, str);
    uart_write_str(str);
}
void uart_write_int_async(int num){
    char str[12];
    int2str_dec(num, str);
    uart_write_str_async(str);
}



//hex 格式數字
void int2str_hex(uint64_t num, char *str) {
    int index = 0;
    str[index++] = '0';
    str[index++] = 'x';

    for (int i = 15; i >= 0; i--) {
        int digit = (num >> (i * 4)) & 0xF;
        str[index++] = (digit < 10) ? (digit + '0') : (digit - 10 + 'A');

        // 每四個 hex 數字之後插入一個底線（但最後一組不加）
        if (i % 4 == 0 && i != 0) {
            str[index++] = '_';
        }
    }

    str[index] = '\0';
}
void uart_write_hex(uint64_t num) {
    char str[24];
    int2str_hex(num, str);
    uart_write_str(str);
}
























/// @brief //////////// 給 handler 用的，無條件可以印出的
char uart_read_raw(){
    /* Check bit 0 for data ready field */
    while (!(*AUX_MU_LSR & 0x01)){
        //為了避免編譯器對這部分程式碼進行優化。
        //因為編譯器可能會將一個空的 while 迴圈優化掉，從而導致程式無法正確等待硬體狀態改變。
        asm volatile("nop");
    }
    
	//卡，這是我原本寫法，不知道為甚麼不行，問一下
    // char* buf_ptr = (char*)AUX_MU_IO;
    // return *buf_ptr == '\r' ? '\n' : *buf_ptr;
    //檢查buf_ptr指向的內容是否是 \r ，如果是的話要返回 \n，否則就返回原本的字元就好
	char r = (char)(*AUX_MU_IO & 0xFF);		// 只取低 8-bit
    return (r == '\r') ? '\n' : r; 			// Convert carriage return to newline
}

void uart_write_char_raw(char ch){
    /* Check bit 5 for Transmitter empty field */
    while (!(*AUX_MU_LSR & 0x20)){
        asm volatile("nop");
    }
	*AUX_MU_IO = (unsigned int)ch;
}

void uart_write_str_raw(const char *str){
    while (*str) {
        if (*str == '\n') uart_write_char_raw('\r');  // 處理 CRLF
        uart_write_char_raw(*str++);
    }
}

void uart_write_int_raw(int num){
    char str[12];
    int2str_dec(num, str);
    uart_write_str_raw(str);
}
void uart_write_uint_raw(unsigned int num) {
    char str[12];
    uint2str_dec(num, str);
    uart_write_str_raw(str);
}
void uart_write_hex_raw(uint64_t num) {
    char str[24];
    int2str_hex(num, str);
    uart_write_str_raw(str);
}
