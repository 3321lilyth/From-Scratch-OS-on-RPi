#include "syscall.h"


extern thread_t* get_current();
extern void switch_user_address_space(uint64_t pgd_pa);
extern void restore_kernel_address_space();

//使用 statically allocated aligned global buffer，如果你在 system call handler 裡面定義 kbuf 的話
//那就會位在 user thread 的 kernel stack 上面，就不是 linear mapping，就不能用 VA_TO_PA_KERNEL 做轉換了
static volatile unsigned int __attribute__((aligned(16))) mailbox_kbuf[36];

void copy_from_user(volatile void* kernel_dst, const void* user_src, size_t size) {
    thread_t* current = get_current();
    uint64_t pgd_pa = VA_TO_PA_KERNEL(current->user_pagetable);
    
    // 1. 暫時切換到 user page table
    switch_user_address_space(pgd_pa);
    
    // 2. copy byte-by-byte（因為 page crossing 不易處理）
    const char* src = (const char*)user_src;
    char* dst = (char*)kernel_dst;
    for (size_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }

    // 3. 切回 kernel page table（由 kernel_vm_init 設定）
    restore_kernel_address_space();
}

void copy_to_user(void* user_dst, const volatile void* kernel_src, size_t size) {
    thread_t* current = get_current();
    uint64_t pgd_pa = VA_TO_PA_KERNEL(current->user_pagetable);
    
    switch_user_address_space(pgd_pa);
    
    char* dst = (char*)user_dst;
    const char* src = (const char*)kernel_src;
    for (size_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
    
    restore_kernel_address_space();
}


//傳入的是 save_all_preempt 結束後的 sp，要自己用 sp offset 推算 trap frame 位置
void syscall_handler(uint64_t* sp) {
    thread_t *current = get_current();
    current->user_sp = sp[34];      //更新一下 thread_t 裡面的 user_sp
    unsigned long syscall_num = sp[4 * 2];  // offset (16*4) 的地方是 x8（system call number） 

    // if (syscall_num !=SYS_UART_WRITE){
    if (syscall_num != SYS_UART_READ && syscall_num !=SYS_UART_WRITE){
        uart_write_str_raw("\r\n------------ [syscall] Syscall num = ");
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
            case SYS_SIGRETURN:
                uart_write_str_raw(" (SYS_SIGRETURN)");
                break;
            default:
                uart_write_str_raw(" (UNKNOWN)");
                break;
        }
        uart_write_str_raw(", caller pid = ");
        uart_write_int_raw(current->tid);
        uart_write_str_raw("    ");
        uart_write_str_raw("------------\r\n");

    }


    switch (syscall_num) {
        case SYS_GETPID:  //0, 板子OK
            sp[0] = current->tid;  // 把 return value 放在 x0 (sp + 0)
            break;

        case SYS_UART_READ: {
            char *ubuf  = (char *)sp[0];
            int size = sp[1];
            char kbuf[size];
            // uart_write_str_raw("\r\n    [read] ubuf: ");
            // uart_write_hex_raw((uint64_t)ubuf);
            // uart_write_str_raw(", kbuf: ");
            // uart_write_hex_raw((uint64_t)kbuf);
            // uart_write_str_raw(", size: ");
            // uart_write_int_raw(size);

            for (int i = 0; i < size; i++) {
                kbuf[i] = uart_read();  // read 進 kernel buffer
                // kbuf[i] = uart_read_async();  // read 進 kernel buffer
            }

            // for (int i = 0; i < size; i++) {
            //     uart_write_str_raw("\r\n    kbuf[");
            //     uart_write_int_raw(i);
            //     uart_write_str_raw("] = ");
            //     uart_write_hex_raw((uint8_t)kbuf[i]);
            // }
            copy_to_user(ubuf, kbuf, size);  // 寫回 user space
            sp[0] = size;            // save return value to trap frame
            break;
        }

        case SYS_UART_WRITE: {  //2，板子OK
            const char *buf = (const char *)sp[0];
            int size = sp[1];

            for (int i = 0; i < size; i++) {
                uart_write_char(buf[i]);
            }
            sp[0] = size;
            break;
        }

        case SYS_EXEC: { //3 板子OK
            //  exec 不會產生新的 thread，而是讓目前 thread 變身成另一個程式，替換掉 code 跟 stack
            char *filename  = (char *)sp[0];
            sp[0] = cpio_exec_user_program(filename);  // 傳 function pointer

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
            unsigned char ch = sp[0];
            unsigned int *user_mbox = (unsigned int *)sp[1];
            // check_framebuffer();  // 確保 framebuffer 已經初始化

            memset((void *)(volatile void *)mailbox_kbuf, 0, sizeof(mailbox_kbuf));
            copy_from_user(mailbox_kbuf, user_mbox, sizeof(mailbox_kbuf));
            
            int result = mailbox_call(ch,  (unsigned int *)VA_TO_PA_KERNEL(mailbox_kbuf));
        
            // 判斷是否包含 framebuffer allocate tag
            for (int i = 2; i < 36; i++) {
                if (mailbox_kbuf[i] == 0x00040001) {  // 找到 allocate framebuffer tag
                    // 從 mailbox 結果取得 framebuffer PA
                    uint32_t fb_pa = mailbox_kbuf[i + 3];  // 回傳會寫在 tag 的 value buffer
                    uint32_t fb_size = mailbox_kbuf[i + 4];
                    // uart_write_str_raw("\r\n[syscall] framebuffer PA = ");
                    // uart_write_hex_raw(fb_pa);
                    // uart_write_str_raw(", size = ");
                    // uart_write_int_raw(fb_size);
                    // 指定一個 VA，例如 0x100000 (user space)
                    uint64_t fb_va = 0x100000;
                    thread_t *current = get_current();
        
                    // 建立 user pagetable 的 mapping
                    for (size_t offset = 0; offset < fb_size; offset += PAGE_SIZE) {
                        mappages(current->user_pagetable,
                                    fb_va + offset,
                                    PAGE_SIZE,
                                    fb_pa + offset,
                                    PTE_NORMAL | PTE_USER | PTE_RW);
                    }

                    // 將 fb_va 回寫到 user buffer（讓 user 從那個 VA 開始畫）
                    mailbox_kbuf[i + 3] = fb_va;
        
                    break;  // 只處理第一個 framebuffer tag
                }
            }
        
            // 回寫整包結果
            copy_to_user(user_mbox, mailbox_kbuf, sizeof(mailbox_kbuf));
            sp[0] = result;
        
            break;
        }

        case SYS_KILL: {//7
            int target_pid = sp[0];  // x0 傳進來的 pid
            int ret = thread_kill(target_pid);  // 我們實作一個新的 thread_kill()
            sp[0] = ret;  // 回傳 0 表示成功，-1 表示找不到
            break;
        }

        case SYS_SIGNAL: {//8
            int signum = sp[0];
            void (*handler)() = (void (*)())sp[1];
            thread_t *t = get_current();
            if (signum < SIGNAL_MAX) {
                t->signals.handlers[signum] = handler;
                uart_write_str_raw("\r\n[syscall] Registered signal handler for signal ");
                uart_write_int_raw(signum);
            }
            break;
        }

        case SYS_SIGNAL_KILL: {//9 kill(int pid, int signum)
            int target_pid = sp[0];
            int signum = sp[1];
            uart_write_str_raw("\r\n[syscall] send signal ");
            uart_write_int_raw(signum);
            uart_write_str_raw(" to tid ");
            uart_write_int_raw(target_pid);
            int ret = thread_send_signal(target_pid, signum);  // code in thread.c
            sp[0] = ret;
            break;

        }
        case SYS_SIGRETURN: {  // 10
            thread_t *t = get_current();
            if (t->handling_signal) {
                uart_write_str_raw("\r\n[syscall] sigreturn: restoring context and freeing handler stack");
                uint64_t *tf = (uint64_t *)t->kernel_sp;
                tf[30] = t->saved_x30;
                tf[33] = t->saved_user_elr;
                tf[32] = t->saved_user_spsr;
                tf[34] = t->saved_user_sp;  // t->user_sp  不能用，因為我維護他的地方就只有在 system call handler 最上面而已
                                            // 也就是說呼叫 sys_sigreturn  也會把錯誤的 sp_el0 存到 thread_t->user_sp 裡面，所以要手動保存並修復
                t->user_sp = t->saved_user_sp; 
                // === 回收 handler stack，因為是 user space VA 所以要先 walk 找到 PA 後再轉到 kernel space VA ===
                // uint64_t stack_top = (uint64_t)t->signal_stack_base + 4 * PAGE_SIZE;
                // for (int i = 0; i < 4; i++) {
                //     uint64_t va = stack_top - i * PAGE_SIZE;
                //     uint64_t* pte = walk(t->user_pagetable, va, 0);
                //     if (pte && (*pte & PTE_VALID)) {
                //         uint64_t pa = *pte & ~(PAGE_SIZE - 1);
                //         //pa 會是這個 page 的 high memory side，但是我們 buddy system 統一用 low memory 那邊來分配，所以這邊要 free 的時候，記得要扣掉 PAGE_SIZE
                //         // 比如 kmalloc stack 時得到 0x3000，那因為 stack 是往下長所以我們會把 stack top 的 PA 設定為 0x3000+PAGE_SIZE
                //         // 現在釋放的時候就要把 PAGE_SIZE 扣回來才能釋放
                //         kfree(PA_TO_VA_KERNEL(pa-PAGE_SIZE));
                //     }
                // }

                // 釋放 signal stack (4 pages)
                if (t->signal_stack_base) {
                    uart_write_str_raw("\r\n[syscall] sigreturn: free signal stack at ");
                    uart_write_hex_raw(t->signal_stack_base);
                    extern void free_va_range(pagetable_t pgd, uint64_t base, int pages);
                    free_va_range(t->user_pagetable, t->signal_stack_base, 4);
                    t->signal_stack_base = 0;
                }
                t->signal_stack_base = 0;
            }
            break;
        }


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

void sys_signal(int signum, void (*handler)()) {
    asm volatile(
        "mov x0, %0\n"
        "mov x1, %1\n"
        "mov x8, %2\n"
        "svc 0\n"
        :
        : "r"(signum), "r"(handler), "I"(SYS_SIGNAL)
        : "x0", "x1", "x8"
    );
}


void sys_signal_kill(int pid, int signum) {
    asm volatile(
        "mov x0, %0\n"
        "mov x1, %1\n"
        "mov x8, %2\n"
        "svc 0\n"
        :
        : "r"(pid), "r"(signum), "I"(SYS_SIGNAL_KILL)
        : "x0", "x1", "x8"
    );
}


void sys_sigreturn() {
    asm volatile(
        "mov x8, 10\n"   // SYS_SIGRETURN
        "svc 0\n"
    );
}