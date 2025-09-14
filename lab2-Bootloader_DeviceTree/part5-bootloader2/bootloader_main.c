#include "../include/mini_uart.h"
#include "../include/bootloader.h"
#include "../include/utli.h"

uint8_t uart_read_binary(){
    while (!(*AUX_MU_LSR & 0x01)){
        asm volatile("nop");
    }
    return (uint8_t)(*AUX_MU_IO & 0xFF);// 只取低 8-bit
}


void load_kernel(){
	BootHeader header;
    uint32_t calculated_checksum = 0;
	uart_write_str("\r\n------- 1. into load_kernel() ----------");
	
	//1. 讀取 12 bytes 的 header
	for (size_t i = 0; i < sizeof(BootHeader); i++) {
		((uint8_t *)&header)[i] = uart_read_binary();
    }
	
	//2. 驗證 Magic Number
    if (header.magic != BOOT_MAGIC) {
		uart_write_str("\r\nInvalid Boot Header!");
        return;
    }
	
	// 3. 讀取 kernel binary 到 0x80000
	uart_write_str("\r\n------- 2. Loading Kernel ----------");

	uint8_t *kernel_addr = (uint8_t *)KERNEL_LOAD_ADDR;
    for (uint32_t i = 0; i < header.size; i++) {
        kernel_addr[i] = uart_read_binary();	//用 uart_read_binary() 而不是原本的 uart_read() 
												//是因為那邊是希望印出來好看一點，這邊卻要計算 checksum
        calculated_checksum = (calculated_checksum + kernel_addr[i]) & 0xFFFFFFFF;  //計算 checksum
    }
	uart_write_str("\r\n------- 3. Kernel has been loaded ----------");

	//4. 驗證 checksum
    if (calculated_checksum != header.checksum) {
        uart_write_str("\r\nChecksum Mismatch!, calculated_checksum= ");
		uart_write_hex(calculated_checksum);
        return;
    }

	//5. part6: 拿到 x0 裡面放的 dtb address，等等作為參數傳給 kernel 的 x0
	uint32_t* dtb_address  = 0;
	asm volatile("mov %0, x21" :  "=r"(dtb_address));
    uart_write_str("\r\n    bootloader get dtb addr = ");
    uart_write_hex((uintptr_t)dtb_address);


	//6. 跳轉到 kernel 開始執行
	uart_write_str("\r\n------- 4. Start Booting ----------");
	// 作法1 - 設定函數指標指向 0x80000，並執行它，讓 CPU 跳轉到該記憶體執行 kernel。
	// void (*kernel_entry)(uint64_t*) = (void (*)(uint64_t*))KERNEL_LOAD_ADDR;
	// kernel_entry(_dtb);														//相當於 PC = 0x80000，同時把 _dtb 作為參數傳遞過去
	// (void (*)(char*))把 0x80000 轉型為 function pointer ，指向一個 "char*為參數" 且 "回傳 void" 的函數。
	// 函數指標不是真正的function，所以不能被呼叫，而是指向其他function的位址
	

	//做法2
    // asm volatile(
	//     "mov x30, 0x80000;"   	//x30 是 Link Register (LR)，通常用來存放返回地址。
	//     "ret;"					//讓 CPU 跳轉到 x30 指定的地址執行，相當於 PC = x30 = 0x80000。
	// );
	
	//做法3
	unsigned int r = 500; while(r--) { asm volatile("nop"); }
	asm volatile(
        "mov x0, %0;"   // 把 `_dtb` 傳遞到 `x0`
        "mov x2, %1;"   // 設定 `x2` 為 Kernel 地址
        "br x2;"        // 跳轉到 `x2`（Kernel Entry）
        :
        : "r" (dtb_address), "r" (KERNEL_LOAD_ADDR)  // 傳入 `_dtb` 和 `KERNEL_LOAD_ADDR`
        : "x0", "x2"
    );
}

void bootloader_main(){

	load_kernel();
}