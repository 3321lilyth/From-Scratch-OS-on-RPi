// #include "reboot.h"
#include "include/mini_uart.h"
#include "include/mailbox.h"

int strcmp(char* str1, char* str2){
    char* p1 = str1;
    char* p2 = str2;

    while (*p1 == *p2){
        if (*p1 == '\0')
            return 1;
        p1++;
        p2++;
    }
    return 0;
}


// Get the command from user prompt
void readcmd(char* str){
    char input_ch;
    str[0] = 0;
    int str_ind = 0;

	while (1) {
        input_ch = uart_read();  					// 從 UART 讀取字符

        if (input_ch == '\n') {  					// 回車鍵處理
            // uart_write_char('\n'); 	 			// 輸出換行
            break;  								// 結束輸入
        }

        if (input_ch == 127 || input_ch == '\b') {  		// 處理刪除鍵 (backspace)
            if (str_ind > 0) {  							// 如果還有字符可刪除
                str_ind--;
                uart_write_char('\b');  					// 輸出回退字符
                uart_write_char(' ');  						// 顯示刪除的字符（空格）
                uart_write_char('\b');  					// 再回退一次，顯示刪除效果
            }
        } else if (input_ch >= 32 && input_ch <= 126) {  	// 可顯示的 ASCII 字符
            str[str_ind++] = input_ch;
            uart_write_char(input_ch);  					// 輸出字符
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
		}else if (strcmp(cmd, "hello")){
			uart_write_str("\r\nHello World!");
		}else if (strcmp(cmd, "mailbox")){
		    get_board_info();
		// }else if (strcmp(cmd, "reboot")){
		//     uart_write_str("\nRebooting...");
		//     reset(200);
		// }else{
		//     uart_write_str("\nCommand Not Found");
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