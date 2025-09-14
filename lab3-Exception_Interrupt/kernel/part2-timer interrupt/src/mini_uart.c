#include "peripherals/gpio.h"
#include "peripherals/irq.h"
#include "type.h"
#include "mini_uart.h"
#define BUFFER_MAX_SIZE 256u




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



void uart_init(){
	set_gpio_uart();

	//每個欄位的意義是參考 spec 2-1 p.8 的表格，然後值是跟著助教(https://nycu-caslab.github.io/OSC2025/labs/hardware/uart.html)設定的
    *AUX_ENABLE |= 1;           // Enable mini UART
    *AUX_MU_CNTL = 0;       // Disable TX, RX during configuration
    *AUX_MU_IER = 0;        // 0x0 代表全部 disable
                            // 0x01 是 RX interrupt，代表 kernel 要從 uart 收資料 
                            // 0x02 是 TX interrupt，代表 kernel 要寫資料到 uart
                            // 設定為 3 就是同時啟動
    *AUX_MU_LCR = 3;        // Set the data size to 8 bit
    *AUX_MU_MCR = 0;        // Don't need auto flow control
    *AUX_MU_BAUD = 270;     // Set baud rate to 115200, After booting, the system clock is 250 MHz.
    *AUX_MU_IIR = 6;        // enable FIFO, 是一種緩衝機制，比如 read 的時候會先寫到 FIFO，CPU再去從 FIFO撈數據，可以提高數據傳輸效率、防止數據丟失	
	*AUX_MU_CNTL = 3;

	unsigned int r = 1000000; while(r--) { asm volatile("nop"); }
    uart_write_str("\r\n------- 0. after uart_init() ----------");
}

char uart_read(){
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

void uart_write_char(char ch){
    /* Check bit 5 for Transmitter empty field */
    while (!(*AUX_MU_LSR & 0x20)){
        asm volatile("nop");
    }

    // char* buf_ptr = (char*)AUX_MU_IO;
    // *buf_ptr = ch;
	*AUX_MU_IO = (unsigned int)ch;
}

void uart_write_str(const char *str){
    while (*str) {
        if (*str == '\n') uart_write_char('\r');  // 處理 CRLF
        uart_write_char(*str++);
    }
}


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
void uart_write_int(int num){
    char str[12];
    int2str_dec(num, str);
    uart_write_str(str);
}



//hex 格式數字
void int2str_hex(int num, char *str) {
	//C 語言中，負數在 unsigned int 轉換時會變為對應的二補數，因此可以直接處理。
    unsigned int temp = (unsigned int)num;
    int index = 0;

    // 輸出 "0x" 前綴
    str[index++] = '0';
    str[index++] = 'x';

    // 轉換數字為 8 位十六進制，補 0
    for (int i = 7; i >= 0; i--) {
        int digit = (temp >> (i * 4)) & 0xF;  // 取出對應的 4-bit (16進位嘛所以要 %16)
        str[index++] = (digit < 10) ? (digit + '0') : (digit - 10 + 'A');
    }

    // 結尾
    str[index] = '\0';
}

void uart_write_hex(int num) {
    char str[12];
    int2str_hex(num, str);
    uart_write_str(str);
}
