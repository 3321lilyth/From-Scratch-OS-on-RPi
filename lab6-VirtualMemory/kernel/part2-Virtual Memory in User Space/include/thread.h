#ifndef THREAD_H
#define THREAD_H
#include "mini_uart.h"
#include "utli.h"
#include "memory_test.h"                // for kmalloc 跟 kfree API
#include "type.h"
#include "timer.h"
#include "exception.h"
#include "mmu.h"

#define THREAD_STACK_SIZE 16 * 1024
#define STACK_ALIGN 16
#define MAX_PRIORITY 32
// #define CXTSW_TIMEOUT 200                // 每 CXTSW_TIMEOUT ms preempt一次
                                        //這邊如果設定太短，uart 會變成超級奇怪，會每個人輪流印出一個 char
extern volatile unsigned int cxtsw_timeout_ms;  //助教 lab 規定要 "每 CORE_FREQ / 32 個 time tick 就換一次"，所以我改在 main 裡面呼叫 function 來初始化這個值

//thread status
#define THREAD_READY 0
#define THREAD_ZOMBIE 1
#define THREAD_SLEEP 2
#define THREAD_WAITING 3

//signal
#define SIGNAL_MAX 32
#define SIGKILL 9
#define SIGUSR1 10
#define SIGUSR2 11



typedef struct signal_info {
    void (*handlers[SIGNAL_MAX])();  // User registered handler
    int pending[SIGNAL_MAX];         // 是否 pending
} signal_info_t;

//這裡面全部都是 VA，但是要注意有些是 kernel space VA 有些是 user space VA
typedef struct thread {
    uint64_t kernel_sp;        // offset 0，這是 VA
    unsigned int tid;          // offset 8
    int status;                // offset 12
    int priority;              // offset 16
    void* kernel_stack_base;   // offset 24，這是 VA

    void* user_stack_base;     // offset 32，這是 user space VA，一定會是 0xffffffffb000
    uint64_t user_sp;          // offset 40，這是 user space VA。注意我只有在進入 sysetm call 時會更新這個變數
                               // 其實好像不用存ㄟ，如果你後面會再 C code 用到她，記得去更新 save_all 裡面，除了 push 進 kernel stack 以外也要另外存到這裡
    int is_user_mode;          // offset 48
    struct thread* next;       // offset 56
    signal_info_t signals;      // 只記錄「這個 signal 有沒有被送過來」，而不是「來幾次」。linux 應該也是這樣做的吧

    // hanlder related
    uint64_t saved_user_elr;    // 原本的 elr_el1
    uint64_t saved_user_spsr;   // 原本的 spsr_el1
    uint64_t saved_x30;         // 原本的 x30
    uint64_t saved_user_sp;     // 原本的 user_sp，因為我 user_sp 的維護只有在 system call handler 裡面偷改
                                //也就是說呼叫 sys_sigreturn  也會把錯誤的 sp_el0 存到 thread_t->user_sp 裡面，所以要手動保存並修復

    uint64_t signal_stack_base;      // handler 專用的 user stack
    int handling_signal;        // flag

    //virtual memory
    pagetable_t user_pagetable; //這是 kernel space VA喔，寫入 ttbr0_el1 之前要轉為 PA 才行!!
} thread_t;




void ready_queue_dump();


//thread operation, global function
thread_t* thread_create(void (*func)(), int priority, int is_user_mode, char* filename);
int thread_fork();                                      // fork a new, same process
void thread_exit();                                     // thread call this to kill itself
int thread_kill(int target_tid);                        // parent can kill child thread
int thread_send_signal(int target_tid, int signum);

//主動放棄 CPU 相關
void sleep(int milliseconds) ;
void schedule_complete(void* unused);
void remove_from_ready_queue(thread_t* t);
void wakeup_callback(void* t_ptr);

//被迫放棄 CPU 相關
void schedule_preempt();
void move_to_ready_queue_tail(thread_t* t);
void preempt_callback(void* unused);



//local function
void kill_zombies();


// task function
void idle();
#endif
