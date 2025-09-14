#include "mini_uart.h"
#include "mailbox.h"
#include "utli.h"
#include "reboot.h" 
#include "devicetree.h"
#include "cpio.h" 
#include "mem_alloc.h" 
#include "exception.h" 
#include "timer.h" 


// Get the command from user prompt
void readcmd(char* str){
    char input_ch;
    str[0] = 0;
    int str_ind = 0;

	while (1) {
        input_ch = uart_read();  // 從 UART 讀取字符

        if (input_ch == '\n') {  // 回車鍵處理
            // uart_write_char('\n');  // 輸出換行
            break;  // 結束輸入
        }

        if (input_ch == 127 || input_ch == '\b') {  // 處理刪除鍵 (backspace)
            if (str_ind > 0) {  // 如果還有字符可刪除
                str_ind--;
                uart_write_char('\b');  // 輸出回退字符
                uart_write_char(' ');  // 顯示刪除的字符（空格）
                uart_write_char('\b');  // 再回退一次，顯示刪除效果
            }
        } else if (input_ch >= 32 && input_ch <= 126) {  // 可顯示的 ASCII 字符
            str[str_ind++] = input_ch;
            uart_write_char(input_ch);  // 輸出字符
        }
    }
    str[str_ind] = 0;
}


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
    uart_write_str("\r\n    -------------------  basic cmd  -------------------------------");        
    uart_write_str("\r\n    help           : print this help menu");
    uart_write_str("\r\n    hello          : print Hello World!");
    uart_write_str("\r\n    mailbox        : print mailbox info");
    uart_write_str("\r\n    reboot         : reboot the device");        
    uart_write_str("\r\n    timer_on       : enable timer, and interrupt every 2 second");        
    uart_write_str("\r\n    -------------------  mem related -------------------------------");        
    uart_write_str("\r\n    malloc <int>   : allocate size_t memory space (in decimal)");        
    uart_write_str("\r\n    free           : deallocate latest malloc");        
    uart_write_str("\r\n    heap_info      : show current heap pointer and remaining heap size");        
    uart_write_str("\r\n    ------------------- file related -------------------------------");        
    uart_write_str("\r\n    ls             : list all file");        
    uart_write_str("\r\n    cat <filename> : cat one file");        
    uart_write_str("\r\n    exec <program> : execute user program");        
}

void shell(){
	while(1){
		uart_write_str("\r\nLily@Rpi3B+ > ");
		char cmd[256];
		readcmd(cmd);


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
		}else if (strcmp(cmd, "timer_on")){ //lab3
		    uart_write_str("\r\nenable core timer...");
            core_timer_enable();
            // 印出計時器的當前狀態
            // unsigned long cntp_ctl, cntp_tval, cntfrq, irq_ctrl;
            
            // asm volatile("mrs %0, cntp_ctl_el0" : "=r"(cntp_ctl));  // 讀取 timer 控制
            // asm volatile("mrs %0, cntp_tval_el0" : "=r"(cntp_tval)); // 讀取倒數時間
            // asm volatile("mrs %0, cntfrq_el0" : "=r"(cntfrq));       // 讀取計時器頻率
            // irq_ctrl = *(volatile unsigned int*) CORE0_TIMER_IRQ_CTRL; // 讀取 IRQ 狀態
            
            // uart_write_str("\r\n[Core Timer] cntp_ctl_el0 = ");
            // uart_write_hex(cntp_ctl);
            
            // uart_write_str("\r\n[Core Timer] cntp_tval_el0 = ");
            // uart_write_int(cntp_tval);
            
            // uart_write_str("\r\n[Core Timer] cntfrq_el0 = ");
            // uart_write_int(cntfrq);

            // uart_write_str("\r\n[Core Timer] CORE0_TIMER_IRQ_CTRL = ");
            // uart_write_hex(irq_ctrl);
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