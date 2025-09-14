#include "../include/bootloader.h"

uint32_t get_kernel_size(){
	uint32_t size;
	for (int i = 4; i >= 0; i--) {
        size |= (uart_read() << (i * 8));
    }

    uart_write_str("\r\n	get kernel size: ");
    uart_write_hex(size);

    return size;
}

uint8_t uart_read_binary(){

    while (!(*AUX_MU_LSR & 0x01)){
        asm volatile("nop");
    }

    uint8_t* buf_ptr = (uint8_t*)AUX_MU_IO;

    return *buf_ptr;
}

void load_kernel(){
	
	BootHeader header;
    uint32_t calculated_checksum = 0;
	
	//1. 讀取 12 bytes 的 header
	for (size_t i = 0; i < sizeof(BootHeader); i++) {
		((uint8_t *)&header)[i] = uart_read();
		if ((i+1)%1024 == 0){
            uart_write_str("\r\n");
            uart_write_hex(i);
        }
    }
	
	//2. 驗證 Magic Number
    if (header.magic != BOOT_MAGIC) {
		uart_write_str("\r\nInvalid Boot Header!");
        return;
    }
	
	//3. 讀取 kernel binary 到 0x80000
	uart_write_str("\r\nLoading Kernel...");
	uint8_t *kernel_addr = (uint8_t *)KERNEL_LOAD_ADDR;		//load 到 0x80000
    for (uint32_t i = 0; i < header.size; i++) {
        kernel_addr[i] = uart_read_binary();				//用 uart_read_binary() 而不是原本的 uart_read() 
															//是因為那邊是希望印出來好看一點，這邊卻要計算 checksum
        calculated_checksum += kernel_addr[i];  			//計算 checksum
    }
	uart_write_str("\r\nKernel has been loaded");

	//4. 驗證 checksum
    if (calculated_checksum != header.checksum) {
        uart_write_str("\r\nChecksum Mismatch!");
        return;
    }

	//5. 跳轉到 kernel 開始執行
	uart_write_str("\r\nStart Booting...");	
	//作法1 - 設定函數指標指向 0x80000，並執行它，讓 CPU 跳轉到該記憶體執行 kernel。
	// void (*kernel_entry)() = (void (*)())KERNEL_LOAD_ADDR;
	// kernel_entry(); //相當於 PC = 0x80000
	//kernel_entry 是一個函數指標，指向一個 不帶參數且回傳 void 的函數。
	//函數指標不是真正的function，所以不能被呼叫，而是指向其他function的位址
	// (void (*)()) 把數值地址(0x80000)轉型為函數指標

	//做法2
    // asm volatile(
    //     "mov x30, 0x80000;"   	//x30 是 Link Register (LR)，通常用來存放返回地址。
    //     "ret;"					//讓 CPU 跳轉到 x30 指定的地址執行，相當於 PC = x30 = 0x80000。
    // );

	//做法3
	unsigned int r = 150; while(r--) { asm volatile("nop"); }
    asm volatile(
        "mov x0, 0x80000;"
        "br x0"
    );
	

}
