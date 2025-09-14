#include "thread_functions.h"
extern void preempt_callback(void* unused);
extern thread_t* get_current();




///////////////////////// kernel thread function //////////////////////////////////
// kernel thread 才可以直接呼叫 uart_write_X 系列，如果是 user thread 就要透過 system call 才行

// 測試 complete 情況，也就是 kernel thread 是否可以自願放棄 CPU，不過這邊我暫時不想管ㄏㄏ
void foo_sync(){
    for(int i = 0; i < 10; ++i) {
        uart_write_str("\r\n[foo] Thread id: ");
        uart_write_int(get_current()->tid);
        uart_write_str(", #");
        uart_write_int(i);
        sleep(10);       //說 linux 是 5-10ms 換一次 thread，這邊放太小就會卡死
    }
    thread_exit();
}
// 測試 preempt 情況，也就是 kernel thread 是否會被 timer interrup
void foo_sync_preempt(){
    for (int i = 0; i < 10; ++i) {
        uart_write_str("\r\n[foo] Thread id: ");
        uart_write_int(get_current()->tid);
        uart_write_str(", #");
        uart_write_int(i);
    }
    thread_exit();
}
















//////////////////////////////////////// user program helper ////////////////////////////////////////
//幫助 user function 寫入各種資料型別到指定 buf 上面
char* write_str_to_buf(char *buf_base, char *cur_ptr, const char *str) {
    while (*str) {
        *cur_ptr++ = *str++;
    }
    return cur_ptr;
}

char* write_int_to_buf(char *buf_base, char *cur_ptr, int num) {
    char tmp[16];
    int i = 0;
    if (num == 0) {
        tmp[i++] = '0';
    } else {
        int is_negative = 0;
        if (num < 0) {
            is_negative = 1;
            num = -num;
        }
        while (num > 0) {
            tmp[i++] = (num % 10) + '0';
            num /= 10;
        }
        if (is_negative) {
            tmp[i++] = '-';
        }
    }
    // 反向寫入
    while (i > 0) {
        *cur_ptr++ = tmp[--i];
    }
    return cur_ptr;
}

char* write_hex_to_buf(char *buf_base, char *cur_ptr, unsigned int num) {
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        *cur_ptr++ = hex_chars[(num >> i) & 0xF];
    }
    return cur_ptr;
}
char* write_addr_to_buf(char *buf_base, char *cur_ptr, uintptr_t addr) {
    const char hex_chars[] = "0123456789ABCDEF";
    // 針對 64-bit 地址 (16 hex digits)
    for (int i = (sizeof(uintptr_t) * 8 - 4); i >= 0; i -= 4) {
        *cur_ptr++ = hex_chars[(addr >> i) & 0xF];
    }
    return cur_ptr;
}














//////////////////////////////////////// user thread function //////////////////////////////////
void foo_sync_preempt_user() {
    char buf[128];
    for (int i = 0; i < 3000; ++i) {
        int pid = sys_getpid();
        char *p = buf;
        p = write_str_to_buf(buf, p, "\r\n[foo] Thread id: ");
        p = write_int_to_buf(buf, p, pid);
        p = write_str_to_buf(buf, p, ", #");
        p = write_int_to_buf(buf, p, i);
        *p = '\0';
        sys_uart_write(buf, p - buf);
    }
    sys_exit();
}

// Get the command from user prompt
void readcmd_user(char* str) {
    str[0] = 0;
    int str_ind = 0;

    while (1) {
        char input_ch;
        //使用 system call 讀 1 個字元，loop 直到真的讀到
        while (sys_uart_read(&input_ch, 1) != 1);

        if (input_ch == '\n') {
            break;
        }

        // 處理 backspace
        if (input_ch == 127 || input_ch == '\b') {
            if (str_ind > 0) {
                str_ind--;
                sys_uart_write("\b \b", 3);  // 用 sys_uart_write 取代 uart_write_str
            }
        } else if (input_ch >= 32 && input_ch <= 126) {
            str[str_ind] = input_ch;
            str_ind++;
            char buf[2] = {input_ch, 0};
            sys_uart_write(buf, 1);  // 用 system call echo 輸入字元
        }
    }
    str[str_ind] = 0;
}

void user_shell(){
    //只是拿來測試一下 user thread 能不能用 system call
    // read cmd 測試 uart read/write
    // mailbox
	while(1){
		char cmd[256];
        //先不要管 async 好了
        // if (uart_interrupt_enabled == 0){
		//     uart_write_str("\r\nLily@Rpi3B+ (sync mode)> ");
		//     readcmd(cmd);
        // }else{
        //     uart_write_str_async("\r\nLily@Rpi3B+ (async mode)> ");
        //     readcmd_async(cmd);
        // }

        readcmd_user(cmd);

        // basic command
		if (strcmp(cmd, "help")){
            char buf[256];
            char *p = buf;
            p = write_str_to_buf(buf, p, "\r\n    -------------------  basic cmd  ----------------------------------");
            p = write_str_to_buf(buf, p, "\r\n    help           : print this help menu");
            p = write_str_to_buf(buf, p, "\r\n    hello          : print Hello World!");
            p = write_str_to_buf(buf, p, "\r\n    mailbox        : print mailbox info");
            *p = '\0';
            sys_uart_write(buf, p - buf);

		}else if (strcmp(cmd, "hello")){
            char buf[32];
            char *p = buf;
            p = write_str_to_buf(buf, p, "\r\nHello World!");
            *p = '\0';
            sys_uart_write(buf, p - buf);
            
		}else if (strcmp(cmd, "mailbox")){
            unsigned int __attribute__((aligned(16))) mailbox[36];
            mailbox[0] = 8 * 4;
            mailbox[1] = REQUEST_CODE;
            mailbox[2] = GET_BOARD_REVISION;
            mailbox[3] = 4;
            mailbox[4] = TAG_REQUEST_CODE;
            mailbox[5] = 0;
            mailbox[6] = GET_ARM_MEMORY;
            mailbox[7] = 8;
            mailbox[8] = TAG_REQUEST_CODE;
            mailbox[9] = 0;
            mailbox[10] = 0;
            mailbox[11] = END_TAG;  			// End tag
            
            char buf[256];
            char *p = buf;
            if (sys_mbox_call(8, mailbox)){
                p = write_str_to_buf(buf, p, "\r\nBoard revision: 0x");
                p = write_hex_to_buf(buf, p, mailbox[5]);// it should be 0xa020d3 for rpi3 b+
                p = write_str_to_buf(buf, p, "\r\nARM memory base address : 0x");
                p = write_hex_to_buf(buf, p, mailbox[9]);
                p = write_str_to_buf(buf, p, "\r\nARM memory size : ");
                p = write_hex_to_buf(buf, p, mailbox[10]);
        
            }else{
                p = write_str_to_buf(buf, p, "\r\nMailbox call failed.");
            }
            *p = '\0';
            sys_uart_write(buf, p - buf);
		}


        
        else{
			uart_write_str("\r\nWrong command, please type help");
		}
	}

}



// uasge: main 裡面就以下這兩條就好
// thread_create(idle, MAX_PRIORITY - 1, 0);
// thread_create(fork_test, 0, 1);
static void delay(unsigned int r) {while(r--) { asm volatile("nop"); }}
void fork_test(){
    char buf[256];      //256B
    char *p = buf;      //8B
    long long cur_sp1;  //8B
    volatile int cnt = 1;    //4B
    volatile int ret = 0;    //4B


    asm volatile("mov %0, sp" : "=r"(cur_sp1));
    p = write_str_to_buf(buf, p, "\r\nFork Test, pid ");
    p = write_int_to_buf(buf, p, sys_getpid());
    sys_uart_write(buf, p - buf);
    asm volatile("" : : : "memory"); // 防止 GCC 最佳化


    if ((ret = sys_fork()) == 0) { // child
        long long cur_sp;
        asm volatile("mov %0, sp" : "=r"(cur_sp));
        char *p = buf;
        p = write_str_to_buf(buf, p, "\r\nfirst child pid: ");
        p = write_int_to_buf(buf, p, sys_getpid());
        p = write_str_to_buf(buf, p, ", cnt: ");
        p = write_int_to_buf(buf, p, cnt);
        p = write_str_to_buf(buf, p, ", cnt ptr: ");
        p = write_addr_to_buf(buf, p, (uintptr_t)&cnt);
        p = write_str_to_buf(buf, p, ", sp: ");
        p = write_addr_to_buf(buf, p, (uintptr_t)cur_sp);
        *p = '\0';
        sys_uart_write(buf, p - buf);
        // printf("first child pid: %d, cnt: %d, ptr: %x, sp : %x\n", get_pid(), cnt, &cnt, cur_sp);

        ++cnt;

        asm volatile("" : : : "memory"); // 防止 GCC 最佳化
        if ((ret = sys_fork()) != 0){
            asm volatile("mov %0, sp" : "=r"(cur_sp));
            // asm volatile("mrs %0, CurrentEL" : "=r"(currentEL)); //這樣寫會觸發 exception 是正常的，因為 EL0 本來就不能隨便存取 system register
            //在 EL0（user mode）跑時，執行 mrs 會 觸發 synchronous exception（通常 invalid instruction fault）。-> 真的要印出的話請提供system call
            p = buf;
            p = write_str_to_buf(buf, p, "\r\nfirst child pid: ");
            p = write_int_to_buf(buf, p, sys_getpid());
            p = write_str_to_buf(buf, p, ", cnt: ");
            p = write_int_to_buf(buf, p, cnt);
            p = write_str_to_buf(buf, p, ", cnt ptr: ");
            p = write_addr_to_buf(buf, p, (uintptr_t)&cnt);
            p = write_str_to_buf(buf, p, ", sp: ");
            p = write_addr_to_buf(buf, p, (uintptr_t)cur_sp);
            // p = write_str_to_buf(buf, p, ", current EL: ");
            // p = write_int_to_buf(buf, p, currentEL);
            *p = '\0';
            sys_uart_write(buf, p - buf);
            //printf("first child pid: %d, cnt: %d, ptr: %x, sp : %x\n", get_pid(), cnt, &cnt, cur_sp);
        }else{
            while (cnt < 5) {
                asm volatile("mov %0, sp" : "=r"(cur_sp));
                // asm volatile("mrs %0, CurrentEL" : "=r"(currentEL));
                p = buf;
                p = write_str_to_buf(buf, p, "\r\nsecond child pid: ");
                p = write_int_to_buf(buf, p, sys_getpid());
                p = write_str_to_buf(buf, p, ", cnt: ");
                p = write_int_to_buf(buf, p, cnt);
                p = write_str_to_buf(buf, p, ", cnt ptr: ");
                p = write_addr_to_buf(buf, p, (uintptr_t)&cnt);
                p = write_str_to_buf(buf, p, ", sp: ");
                p = write_addr_to_buf(buf, p, (uintptr_t)cur_sp);
                // p = write_str_to_buf(buf, p, ", current EL: ");
                // p = write_int_to_buf(buf, p, currentEL);
                *p = '\0';
                sys_uart_write(buf, p - buf);
                // printf("second child pid: %d, cnt: %d, ptr: %x, sp : %x\n", get_pid(), cnt, &cnt, cur_sp);
                delay(1000000); //不搞 complete 只搞 preempt 應該也是可以吧?
                ++cnt;
            }
        }
        sys_exit();
    }
    else {
        p = buf;
        p = write_str_to_buf(buf, p, "\r\nparent here, pid ");
        p = write_int_to_buf(buf, p, sys_getpid());
        p = write_str_to_buf(buf, p, ", child: ");
        p = write_int_to_buf(buf, p, ret);
        p = write_str_to_buf(buf, p, ", cnt: ");
        p = write_int_to_buf(buf, p, cnt);
        p = write_str_to_buf(buf, p, ", cnt ptr: ");
        p = write_addr_to_buf(buf, p, (uintptr_t)&cnt);
        *p = '\0';
        sys_uart_write(buf, p - buf);
        // printf("parent here, pid %d, child %d\n", get_pid(), ret);
        sys_exit();
    }
}

// 測試看看跳去執行內建 function 
void exec_test1(){
    char buf[256];      //256B
    char *p = buf;      //8B
    p = write_str_to_buf(buf, p, "\r\ninto exec_test1(), go to fork_test(), current tid = ");
    p = write_int_to_buf(buf, p, sys_getpid());
    sys_uart_write(buf, p - buf);
    
    sys_exec((const char*)fork_test, 1);

    p = buf;      //8B
    p = write_str_to_buf(buf, p, "\r\n[ERROR] after sys_exec() ???????");
    sys_uart_write(buf, p - buf);
    sys_exit();
}

// 測試看看跳去執行 cpio 複製進來的檔案
void exec_test2(){
    char buf[256];      //256B
    char *p = buf;      //8B
    p = write_str_to_buf(buf, p, "\r\ninto exec_test2(), go to cpio file, current tid = ");
    p = write_int_to_buf(buf, p, sys_getpid());
    sys_uart_write(buf, p - buf);
    
    sys_exec("home/lab5/exec_test.img", 0);

    p = buf;      //8B
    p = write_str_to_buf(buf, p, "\r\n[ERROR] after sys_exec() ???????");
    sys_uart_write(buf, p - buf);
    sys_exit();
}

// 測試 sys_kill 這個 system call，看 parent 能不能殺掉 child
void kill_test(){
    char buf[256];      //256B
    char *p = buf;      //8B
    p = write_str_to_buf(buf, p, "\r\ninto kill_test(), parent tid = ");
    p = write_int_to_buf(buf, p, sys_getpid());
    sys_uart_write(buf, p - buf);

    int child_pid = sys_fork();
    if (child_pid == 0) {
        while (1) {
            p = buf;      //8B
            p = write_str_to_buf(buf, p, "\r\nChild running..., child tid = ");
            p = write_int_to_buf(buf, p, sys_getpid());
            sys_uart_write(buf, p - buf);
            delay(1000000);
        }
    } else {
        delay(5000000);  // parent 等一會，在 qemu 我是用 50000000，在板子上可以除以實用 5000000 就好
        sys_kill(child_pid);  // 殺掉 child
        p = buf;      //8B
        p = write_str_to_buf(buf, p, "\r\nChild killed!");
        sys_exit();
    }
}


// 測試 sys_signal_kill 
void signal_kill_test() {
    char buf[256];      // 256B
    char *p = buf;      // 8B
    p = write_str_to_buf(buf, p, "\r\ninto signal_kill_test(), parent tid = ");
    p = write_int_to_buf(buf, p, sys_getpid());
    sys_uart_write(buf, p - buf);

    int child_pid = sys_fork();
    if (child_pid == 0) {
        while (1) {
            p = buf;      // 8B
            p = write_str_to_buf(buf, p, "\r\nChild running..., child tid = ");
            p = write_int_to_buf(buf, p, sys_getpid());
            sys_uart_write(buf, p - buf);
            delay(1000000);
        }
    } else {
        delay(5000000);  // parent 等一會
        sys_signal_kill(child_pid, SIGKILL);  // 用 signal_kill 殺掉 child
        p = buf;      // 8B
        p = write_str_to_buf(buf, p, "\r\nChild killed by signal!");
        sys_uart_write(buf, p - buf);
        sys_exit();
    }
}



// 測試 SYS_SIGNAL 和 SYS_SIGRETURN
// 預期結果 : 
// 1. parent 印出 into signal_handler_test()... 
// 2. child 不斷印 Child running...
// 3. parent 印 Parent sent SIGUSR1!
// 4. child 收到 signal 後跳去執行 handler, 印出 [SIGNAL] Hello from handler! tid = ...
// 5. handler 結束後child 應該會 繼續印 Child running...，代表 handler return 後正常回原本執行。是一個無窮迴圈沒有錯喔!!!
void my_handler() {
    char buf[256];
    char *p = buf;
    p = write_str_to_buf(buf, p, "\r\nHello from handler! tid = ");
    p = write_int_to_buf(buf, p, sys_getpid());
    sys_uart_write(buf, p - buf);
}
void signal_handler_test() {
    char buf[256];
    char *p = buf;
    p = write_str_to_buf(buf, p, "\r\ninto signal_handler_test(), parent tid = ");
    p = write_int_to_buf(buf, p, sys_getpid());
    sys_uart_write(buf, p - buf);

    int child_pid = sys_fork();
    if (child_pid == 0) {
        // child: 註冊 handler
        sys_signal(SIGUSR1, my_handler);

        while (1) {
            p = buf;
            p = write_str_to_buf(buf, p, "\r\nChild running..., tid = ");
            p = write_int_to_buf(buf, p, sys_getpid());
            sys_uart_write(buf, p - buf);
            delay(1000000);  // sleep 一下
        }
    } else {
        delay(5000000);  // 等 child 開始跑
        sys_signal_kill(child_pid, SIGUSR1);  // 傳 signal 給 child
        p = buf;
        p = write_str_to_buf(buf, p, "\r\nParent sent SIGUSR1!");
        sys_uart_write(buf, p - buf);

        delay(5000000);  // 再等一段時間看 child 是否正常回來
        sys_exit();
    }
}


// 助教給的測試程式碼
void video_player(){
    char buf[256];      //256B
    char *p = buf;      //8B
    p = write_str_to_buf(buf, p, "\r\ninto video_player(), go to cpio file, current tid = ");
    p = write_int_to_buf(buf, p, sys_getpid());
    sys_uart_write(buf, p - buf);
    
    sys_exec("syscall.img", 0);

    p = buf;      //8B
    p = write_str_to_buf(buf, p, "\r\n[ERROR] after sys_exec() ???????");
    sys_uart_write(buf, p - buf);
    sys_exit();
}



