#include "../include/mini_uart.h"
#include "../include/mailbox.h"
#include "../include/reboot.h" 
#include "../include/cpio.h" 
#include "../include/utli.h"


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


void shell(){
	while(1){
		uart_write_str("\r\nLily@Rpi3B+ > ");
		char cmd[256];
		readcmd(cmd);

		if (strcmp(cmd, "help")){
			uart_write_str("\r\nhelp       : print this help menu");
			uart_write_str("\r\nhello      : print Hello World!");
			uart_write_str("\r\nmailbox    : print mailbox info");
			uart_write_str("\r\nreboot     : reboot the device");        
			uart_write_str("\r\nls         : list all file");        
			uart_write_str("\r\ncat        : cat all file");        
		}else if (strcmp(cmd, "hello")){
			uart_write_str("\r\nHello World!");
		}else if (strcmp(cmd, "mailbox")){
		    get_board_info();
		}else if (strcmp(cmd, "reboot")){
		    uart_write_str("\r\nRebooting...");
		    reset(200);
            return;
		}else if (memcmp(cmd, "cat", 3) == 0){
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
        }else{
			uart_write_str("\r\nWrong command, please type help");
		}
	}
}

int main(){

    uart_init();
    uart_write_str("\r\nLogin Shell");
	shell();

    return 0;
}