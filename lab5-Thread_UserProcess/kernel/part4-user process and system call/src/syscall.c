#include "syscall.h"

extern thread_t* get_current();
//傳入的是 save_all_preempt 結束後的 sp，要自己用 sp offset 推算 trap frame 位置
void syscall_handler(uint64_t* sp) {
    thread_t *current = get_current();
    current->user_sp = sp[34];      //更新一下 thread_t 裡面的 user_sp
    unsigned long syscall_num = sp[4 * 2];  // offset (16*4) 的地方是 x8（system call number） 

    if (syscall_num != SYS_UART_READ && syscall_num !=SYS_UART_WRITE){
        uart_write_str_raw("\r\n[syscall] Syscall num = ");
        uart_write_int_raw(syscall_num);
        switch (syscall_num) {
        case SYS_GETPID:
            uart_write_str_raw(" (SYS_GETPID)");
                break;
            case SYS_UART_READ:
                uart_write_str_raw(" (SYS_UART_READ)");
                break;
            case SYS_UART_WRITE:
                uart_write_str_raw(" (SYS_UART_WRITE)");
                break;
            case SYS_EXEC:
                uart_write_str_raw(" (SYS_EXEC)");
                break;
            case SYS_FORK:
                uart_write_str_raw(" (SYS_FORK)");
                break;
            case SYS_EXIT:
                uart_write_str_raw(" (SYS_EXIT)");
                break;
            case SYS_MBOX_CALL:
                uart_write_str_raw(" (SYS_MBOX_CALL)");
                break;
            case SYS_KILL:
                uart_write_str_raw(" (SYS_KILL)");
                break;
            case SYS_SIGNAL:
                uart_write_str_raw(" (SYS_SIGNAL)");
                break;
            case SYS_SIGNAL_KILL:
                uart_write_str_raw(" (SYS_SIGNAL_KILL)");
                break;
            default:
                uart_write_str_raw(" (UNKNOWN)");
                break;
        }
        uart_write_str_raw(", caller pid = ");
        uart_write_int_raw(current->tid);

    }


    switch (syscall_num) {
        case SYS_GETPID:  //0, 板子OK
            sp[0] = current->tid;  // 把 return value 放在 x0 (sp + 0)
            break;

        case SYS_UART_READ: {
            char *buf = (char *)sp[0];
            int size = sp[1];
            for (int i = 0; i < size; i++) {
                buf[i] = uart_read();
            }
            sp[0] = size;            // save return value to trap frame
            break;
        }

        case SYS_UART_WRITE: {  //2，板子OK
            const char *buf = (const char *)sp[0];
            int size = sp[1];
            // uart_write_str_raw("\r\n[syscall] write addr: ");
            // uart_write_hex_raw((uint64_t)buf);
            // uart_write_str_raw(", size: ");
            // uart_write_int_raw(size);

            for (int i = 0; i < size; i++) {
                uart_write_char(buf[i]);
                // uart_write_char_raw(buf[i]);
            }
            sp[0] = size;
            break;
        }

        case SYS_EXEC: { //3 板子OK
            //  exec 不會產生新的 thread，而是讓目前 thread 變身成另一個程式，替換掉 code 跟 stack
            char *name_or_func  = (char *)sp[0];
            uint64_t is_func = sp[1];  // 加一個 flag 判斷 sp[0] 是 filename 還是 function 
            if (is_func == 0){
                sp[0] = cpio_exec_user_program(name_or_func);
            }else{
                sp[0] = thread_exec_user_function((void (*)())name_or_func);  // 傳 function pointer
            }
            break;
        }


        case SYS_FORK: //4 板子OK
            //建立一個完全一樣的 child process（thread 結構、stack、register 都要複製）
            //讓 parent 和 child 一起執行，parent 中 ret 是 child 的 tid，在 child 中是 0
            sp[0] = thread_fork();
            break;

        case SYS_EXIT: //5 板子OK
            thread_exit();
            break;

        case SYS_MBOX_CALL: { //6
            check_framebuffer();
            unsigned char ch = sp[0];
            unsigned int *user_mbox = (unsigned int *)sp[1];

            // 分配 kernel buffer (假設 mailbox 大小固定 36 words)
            unsigned int kbuf[36];
            // 複製 user-space buffer 到 kernel buffer
            for (int i = 0; i < 36; i++) {
                kbuf[i] = user_mbox[i];
            }
            // 呼叫真正的 mailbox_call
            int result = mailbox_call(ch, kbuf);

            // 再把結果複製回 user-space buffer
            for (int i = 0; i < 36; i++) {
                user_mbox[i] = kbuf[i];
            }

            sp[0] = result;
            break;
        }

        case SYS_KILL: //7
            int target_pid = sp[0];  // x0 傳進來的 pid
            int ret = thread_kill(target_pid);  // 我們實作一個新的 thread_kill()
            sp[0] = ret;  // 回傳 0 表示成功，-1 表示找不到
            break;
        case SYS_SIGNAL: //8
            break;
        case SYS_SIGNAL_KILL: //9
            break;


        default:
            uart_write_str_raw("\r\n[syscall] Unknown syscall number: ");
            uart_write_int_raw(syscall_num);
            break;
    }
}



int sys_getpid() {
    int ret;
    asm volatile(
        "mov x8, %1\n"
        "svc 0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "I"(SYS_GETPID)
        : "x8", "x0"
    );
    return ret;
}

size_t sys_uart_read(char buf[], size_t size) {
    size_t ret;
    asm volatile(
        "mov x0, %1\n"
        "mov x1, %2\n"
        "mov x8, %3\n"
        "svc 0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "r"(buf), "r"(size), "I"(SYS_UART_READ)
        : "x0", "x1", "x8"
    );
    return ret;
}

size_t sys_uart_write(const char buf[], size_t size) {
    size_t ret;
    asm volatile(
        "mov x0, %1\n"  //buffer addr
        "mov x1, %2\n"  //buffer size
        "mov x8, %3\n"  //SYS_UART_WRITE 
        "svc 0\n"
        "mov %0, x0\n"  //ret
        : "=r"(ret)
        : "r"(buf), "r"(size), "I"(SYS_UART_WRITE)
        : "x0", "x1", "x8"
    );
    return ret;
}

int sys_exec(const char* name_or_func, int is_func) {
    //sys_exec("user_program", 0); // initramfs 載入
    //sys_exec((const char*)foo_user_func, 1); // 執行 function
    uart_write_str_raw("\r\n[syscall] sys_exec warpper ");

    int ret;
    asm volatile(
        "mov x0, %1\n"
        "mov x1, %2\n"
        "mov x8, %3\n"
        "svc 0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "r"(name_or_func), "r"(is_func), "I"(SYS_EXEC)
        : "x0", "x1", "x8"
    );
    return ret;
}

int sys_fork() {
    int ret;
    asm volatile(
        "mov x8, %1\n"
        "svc 0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "I"(SYS_FORK)
        : "x8", "x0"
    );
    return ret;
}

void sys_exit() {
    asm volatile(
        "mov x8, %0\n"
        "svc 0\n"
        :
        : "I"(SYS_EXIT)
        : "x8"
    );
}

int sys_mbox_call(unsigned char ch, unsigned int *mbox) {
    uart_write_str_raw("\r\n[syscall] sys_mbox_call warpper ");
    int ret;
    asm volatile(
        "mov x0, %1\n"
        "mov x1, %2\n"
        "mov x8, %3\n"
        "svc 0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "r"(ch), "r"(mbox), "I"(SYS_MBOX_CALL)
        : "x0", "x1", "x8"
    );
    return ret;
}

void sys_kill(int pid) {
    asm volatile(
        "mov x0, %0\n"
        "mov x8, %1\n"
        "svc 0\n"
        :
        : "r"(pid), "I"(SYS_KILL)
        : "x0", "x8"
    );
}
