#ifndef THREAD_H
#define THREAD_H
#include "mini_uart.h"
#include "utli.h"
#include "memory_test.h"                // for kmalloc 跟 kfree API
#include "type.h"
#include "timer.h"

#define THREAD_STACK_SIZE 8192
#define STACK_ALIGN 16
#define MAX_PRIORITY 32
#define THREAD_READY 0
#define THREAD_ZOMBIE 1
#define THREAD_SLEEP 2
#define THREAD_WAITING 3


typedef struct thread {
    // callee-saved registers
    uint64_t x19, x20;
    uint64_t x21, x22;
    uint64_t x23, x24;
    uint64_t x25, x26;
    uint64_t x27, x28;
    uint64_t fp, lr;
    uint64_t sp;

    // metadata
    unsigned int tid;
    int status;                     //thread_t offset = 108
    int priority;

    void* stack_base;              // stack base
    struct thread* next;            // linked list node
} thread_t;


void ready_queue_dump();

//thread operation, global function
thread_t* thread_create(void (*func)(), int priority);
void thread_exit();
void sleep(int milliseconds) ;
void remove_from_ready_queue(thread_t* t);

//local function
void schedule(int init);
void wakeup_callback(void* t_ptr);
void kill_zombies();
void set_current_thread(thread_t* t);


// task function
void idle();
// void foo();

#endif
