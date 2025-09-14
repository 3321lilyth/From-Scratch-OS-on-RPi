#include "../include/mini_uart.h"
#include "../include/bootloader.h"

int main(){

	uart_init();
	load_kernel();
    return 0;
}