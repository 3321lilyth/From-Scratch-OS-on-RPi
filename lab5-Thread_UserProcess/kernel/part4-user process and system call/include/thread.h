#ifndef THREAD_H
#define THREAD_H
#include "mini_uart.h"
#include "utli.h"
#include "memory_test.h"                // for kmalloc 跟 kfree API
#include "type.h"
#include "timer.h"
#include "exception.h"

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


// calee saved registers
// typedef struct thread_context{
//     unsigned long long x19;
//     unsigned long long x20;
//     unsigned long long x21;
//     unsigned long long x22;
//     unsigned long long x23;
//     unsigned long long x24;
//     unsigned long long x25;
//     unsigned long long x26;
//     unsigned long long x27;
//     unsigned long long x28;
//     unsigned long long fp;   //x29, pointed to the bottom of the stack, which is the value of the stack pointer just before the function was called(should be immutable).
//     unsigned long long lr;   //x30, but it's refered as PC in some implementation
//     unsigned long long sp;
// }thread_context_t;


// struct trapframe {
//     uint64_t regs[31];    // x0 - x30
//     uint64_t sp_el0;      // user stack
//     uint64_t elr_el1;     // user pc
//     uint64_t spsr_el1;    // pstate
// };

typedef struct thread {
    uint64_t kernel_sp;               // offset 0
    unsigned int tid;          // offset 8
    int status;                // offset 12
    int priority;              // offset 16
    void* kernel_stack_base;          // offset 24

    void* user_stack_base;     // offset 32
    uint64_t user_sp;          // offset 40，其實好像不用存ㄟ，如果你後面會再 C code 用到她，記得去更新 save_all 裡面，除了 push 進 kernel stack 以外也要另外存到這裡
    int is_user_mode;          // offset 48
    struct thread* next;       // offset 56
} thread_t;

void ready_queue_dump();


//thread operation, global function
thread_t* thread_create(void (*func)(), int priority, int is_user_mode);
int thread_fork();
int thread_exec_user_function(void (*func)());
void thread_exit();
int thread_kill(int target_tid);

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
// void foo();

#endif
