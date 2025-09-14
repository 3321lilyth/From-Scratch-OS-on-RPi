#ifndef WAIT_H
#define WAIT_H
#include "mini_uart.h"
#include "utli.h"
#include "type.h"
#include "thread.h"

struct thread;  // forward declaration，不然 thread.h 跟 wait.h 的編譯順序會有問題

typedef struct wait_queue {
    int val;                //可以有幾個人同時在用資源，uart 的話明顯就是 1 而已
    struct thread* head;
} wait_queue_t;

int is_in_wait_queue(struct thread* t, wait_queue_t* q);
void wait_queue_init(wait_queue_t* q, int val) ;
void wait(wait_queue_t* q);
void signal(wait_queue_t* q);
void signal_all(wait_queue_t* q);

#endif
