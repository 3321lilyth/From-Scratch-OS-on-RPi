#include "mini_uart.h"
#include "mailbox.h"
#include "utli.h"
#include "reboot.h" 
#include "devicetree.h"
#include "cpio.h" 
#include "mem_alloc.h" 
#include "exception.h" 
#include "timer.h" 

int uart_interrupt_enabled = 0; // only change "help command (write)" and "enter cmd (read)"

size_t parse_malloc_size(const char *cmd) {
    char *endptr;
    
    // 找到 "malloc " 之後的數字部分
    while (*cmd && *cmd != ' ') {
        cmd++;  // 跳過 "malloc"
    }
    while (*cmd == ' ') {
        cmd++;  // 跳過空格
    }

    // 使用 strtol ，from string to int，並允許不同進位
    size_t size = (size_t)strtol(cmd, &endptr, 0);

    // 確保 size 是有效的正整數
    if (size == 0 || *endptr != '\0') {
        return 0;  // 解析失敗，回傳 0
    }

    return size;
}


void print_help(){
    if (uart_interrupt_enabled == 0){
        uart_write_str("\r\n    -------------------  basic cmd  ----------------------------------");        
        uart_write_str("\r\n    help           : print this help menu");
        uart_write_str("\r\n    hello          : print Hello World!");
        uart_write_str("\r\n    mailbox        : print mailbox info");
        uart_write_str("\r\n    reboot         : reboot the device");        
        uart_write_str("\r\n    -------------------  mem related ---------------------------------");        
        uart_write_str("\r\n    malloc <int>   : allocate size_t memory space (in decimal)");        
        uart_write_str("\r\n    free           : deallocate latest malloc");        
        uart_write_str("\r\n    heap_info      : show current heap pointer and remaining heap size");        
        uart_write_str("\r\n    ------------------- file related ---------------------------------");        
        uart_write_str("\r\n    ls             : list all file");        
        uart_write_str("\r\n    cat <filename> : cat one file");        
        uart_write_str("\r\n    exec <program> : execute user program");        
        uart_write_str("\r\n    ------------------- interrupt related ----------------------------");        
        uart_write_str("\r\n    timer_on       : enable timer, and interrupt every 2 second");
        uart_write_str("\r\n    big_string     : uart write 1000 'A' char, to test ability");
        uart_write_str("\r\n    async_on       : change to async mode(for read cmd and write 'help' cmd)");
        uart_write_str("\r\n    async_off      : change to sync mode");
        uart_write_str("\r\n    setTimeout <msg> <sec> : print msg after sec seconds");
    }else{
        uart_write_str_async("\r\n    -------------------  basic cmd  ----------------------------------");        
        uart_write_str_async("\r\n    help           : print this help menu");
        uart_write_str_async("\r\n    hello          : print Hello World!");
        uart_write_str_async("\r\n    mailbox        : print mailbox info");
        uart_write_str_async("\r\n    reboot         : reboot the device");        
        uart_write_str_async("\r\n    -------------------  mem related ---------------------------------");        
        uart_write_str_async("\r\n    malloc <int>   : allocate size_t memory space (in decimal)");        
        uart_write_str_async("\r\n    free           : deallocate latest malloc");        
        uart_write_str_async("\r\n    heap_info      : show current heap pointer and remaining heap size");        
        uart_write_str_async("\r\n    ------------------- file related ---------------------------------");        
        uart_write_str_async("\r\n    ls             : list all file");        
        uart_write_str_async("\r\n    cat <filename> : cat one file");        
        uart_write_str_async("\r\n    exec <program> : execute user program");        
        uart_write_str_async("\r\n    ------------------- interrupt related ----------------------------");        
        uart_write_str_async("\r\n    timer_on       : enable timer, and interrupt every 2 second");
        uart_write_str_async("\r\n    big_string     : uart write 1000 'A' char, to test ability");
        uart_write_str_async("\r\n    async_on       : change to async mode(for read cmd and write 'help' cmd)");
        uart_write_str_async("\r\n    async_off      : change to sync mode");
        uart_write_str_async("\r\n    setTimeout <msg> <sec> : print msg after sec seconds");

    }
}



void shell(){
	while(1){
		char cmd[256];
        if (uart_interrupt_enabled == 0){
		    uart_write_str("\r\nLily@Rpi3B+ (sync mode)> ");
		    readcmd(cmd);
        }else{
            uart_write_str_async("\r\nLily@Rpi3B+ (async mode)> ");
            readcmd_async(cmd);
        }


        // basic command
		if (strcmp(cmd, "help")){
            print_help();
		}else if (strcmp(cmd, "hello")){
			uart_write_str("\r\nHello World!");
		}else if (strcmp(cmd, "mailbox")){
		    get_board_info();
		}else if (strcmp(cmd, "reboot")){
		    uart_write_str("\r\nRebooting...");
		    reset(200);
            return;
		}
        
        //initramfs related
        else if (memcmp(cmd, "cat", 3) == 0){
            char filename[256];
            int i = 4;  // "cat " 的長度
            int j = 0;
            int ret_value = 1;
            while (cmd[i] != 0 && cmd[i] != ' ') {
                filename[j++] = cmd[i++];
            }
            filename[j] = '\0';
            ret_value = cpio_cat(filename);

            //沒有找到指定檔案
            if (ret_value == 0){
			    uart_write_str("\r\nNo file found");
            }
        }else if (strcmp(cmd, "ls")){
            cpio_ls();
        }else if (memcmp(cmd, "exec", 4) == 0){
            char program_name[256];
            int i = 5;  // "exec " 的長度
            int j = 0;
            int ret_value = 1;
            while (cmd[i] != 0 && cmd[i] != ' ') {
                program_name[j++] = cmd[i++];
            }
            program_name[j] = '\0';
            ret_value = cpio_exec_user_program(program_name);

            //沒有找到指定檔案
            if (ret_value == 0){
			    uart_write_str("\r\nNo Program found");
            }
        }
        
        //simple memory allocator related
        else if (memcmp(cmd, "malloc", 6) == 0){
            size_t size = parse_malloc_size(cmd);
            malloc(size);
        }else if (strcmp(cmd, "free")){
           free();
        }else if (strcmp(cmd, "heap_info")){
            heap_info();
        }

        // interrupt related
        else if (strcmp(cmd, "timer_on")){ //lab3
		    uart_write_str("\r\nenable core timer...");
            core_timer_enable();
        }else if (strcmp(cmd, "async_on")){
            if (uart_interrupt_enabled == 0){
                uart_write_str("\r\nenable uart interrupt ...");
                enable_uart_interrupt();
                uart_interrupt_enabled = 1;
            }else{
                uart_write_str("\r\nalready in async mode");
            }
        }else if (strcmp(cmd, "async_off")){
            if (uart_interrupt_enabled == 1){
                uart_write_str("\r\ndisable uart interrupt ...");
                disable_uart_interrupt();
                uart_interrupt_enabled = 0;
            }else{
                uart_write_str("\r\nalready in sync mode");   
            }
        }else if (memcmp(cmd, "setTimeout", 10) == 0){
            // 格式: setTimeout <message> <seconds>
            parse_settimeout(cmd);
        }

        
        else{
			uart_write_str("\r\nWrong command, please type help");
		}
	}
}

int main(){

    uart_init();
    
    uint64_t *dtb_address; // lab2 part6
    asm volatile("mov %0, x21" :  "=r"(dtb_address));
    fdt_traverse(initramfs_callback, dtb_address);

    uart_write_str("\r\n------- 2. Login Shell   ---------");
	shell();

    return 0;
}