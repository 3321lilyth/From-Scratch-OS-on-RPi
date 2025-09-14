#include "../include/mailbox.h"
#include "../include/mini_uart.h"

//__attribute__((aligned(16))) 是 GCC 的編譯器屬性 (attribute)，表示這塊記憶體的起始位址 (address) 必須是16的倍數(對齊16)。
// GPU 才能正確地讀取資料
volatile unsigned int __attribute__((aligned(16))) mailbox[36];  // Mailbox buffer

int mailbox_call(unsigned char channel){
	//1. Combine the message address (upper 28 bits) with channel number (lower 4 bits)
    // unsigned int msg = ((unsigned long)mailbox_addr & ~0xf) | channel;
	unsigned int msg = ((unsigned int)((unsigned long)&mailbox) & ~0xF) | (channel & 0xF);
		//uint 轉 ulong 是因為 AArch64 是 64bit 位址，而 uint 只有 32 bit 放不下
		//0xF 的二進制是 0b1111，取反 (~0xF) 就變成 0xFFFFFFF0 (最低 4 位元清零)。
		//& ~0xF 的作用是 確保 mailbox 位址的最低 4 個 bit 是 0，確保該位址是 16-byte 對齊。
		//ch & 0xF 確保 ch 只佔用最低 4 個 bit（避免溢出影響 mailbox 位址）。
    
	
	//2. Check if Mailbox 0 status register’s full flag is set.
    while (*MAILBOX_STATUS & MAILBOX_FULL) {asm volatile("nop");};
    
    //3. If not full, then you can write to Mailbox 1 Read/Write register.
    *MAILBOX_WRITE = msg;

    while (1) {
        //4. Check if Mailbox 0 status register’s empty flag is set.
        while (*MAILBOX_STATUS & MAILBOX_EMPTY) {asm volatile("nop");};

        //5. If not, then you can read from Mailbox 0 Read/Write register.
        //6. Check if the value is the same as you wrote in step 1.
        if (msg == *MAILBOX_READ){ 	
			uart_write_str("\r\n");
			uart_write_str("check Mailbox address: ");
			uart_write_hex((unsigned int)((unsigned long)&mailbox));
            return 1;  // 檢查回應是否成功
		}
    }
	return 0;
}

void get_board_info(){
    // unsigned int mailbox[7];
    mailbox[0] = 8 * 4;                 // buffer size in bytes
    mailbox[1] = REQUEST_CODE;

	// 這些值要參考助教給的連結: https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
	// 取得 Board revision
    // tags begin
    mailbox[2] = GET_BOARD_REVISION;    // tag identifier
    mailbox[3] = 4;                     // maximum of request and response value buffer's length.
										// 因為網址寫說Get board revision的request length=0；response length=4
    mailbox[4] = TAG_REQUEST_CODE;
    mailbox[5] = 0;                     // value buffer (output)
	

	// 取得 ARM memory base address and size
	mailbox[6] = GET_ARM_MEMORY;  		// Tag: ARM memory
    mailbox[7] = 8;						//因為網址寫說Get ARM memory的request length=0；response length=8
										//那因為是8，所以下面才要給他 mailbox[9] + mailbox[10] 共8B的空間給他
    mailbox[8] = TAG_REQUEST_CODE;
    mailbox[9] = 0;  					// value buffer (output)
    mailbox[10] = 0; 					// value buffer (output)

    mailbox[11] = END_TAG;  			// End tag

    if (mailbox_call(8)){
		uart_write_str("\r\n");
		uart_write_str("Board revision: ");
		uart_write_hex(mailbox[5]);		// it should be 0xa020d3 for rpi3 b+
		uart_write_str("\r\n");
		uart_write_str("ARM memory base address : ");
		uart_write_hex(mailbox[9]);
		uart_write_str("\r\n");
		uart_write_str("ARM memory size : ");
		uart_write_hex(mailbox[10]);

	}else{
		uart_write_str("\r\nMailbox call failed.");
	}
}