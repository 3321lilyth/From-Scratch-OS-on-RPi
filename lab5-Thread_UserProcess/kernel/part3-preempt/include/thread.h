#ifndef THREAD_H
#define THREAD_H
#include "mini_uart.h"
#include "utli.h"
#include "memory_test.h"                // for kmalloc 跟 kfree API
#include "type.h"
#include "timer.h"
#include "exception.h"

#define THREAD_STACK_SIZE 8192
#define STACK_ALIGN 16
#define MAX_PRIORITY 32
#define CXTSW_TIMEOUT 200                // 每 CXTSW_TIMEOUT ms preempt一次
                                        //這邊如果設定太短，uart 會變成超級奇怪，會每個人輪流印出一個 char


//thread status
#define THREAD_READY 0
#define THREAD_ZOMBIE 1
#define THREAD_SLEEP 2
#define THREAD_WAITING 3



typedef struct thread {
    uint64_t sp;         // offset 0
    unsigned int tid;    // offset 8
    int status;          // offset 12
    int priority;        // offset 16
    void* stack_base;    // stack base
    struct thread* next; // linked list node
} thread_t;

void ready_queue_dump();


//thread operation, global function
thread_t* thread_create(void (*func)(), int priority);
void thread_exit();

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
