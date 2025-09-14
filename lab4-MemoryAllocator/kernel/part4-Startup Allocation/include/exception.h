#ifndef _EXCEPTION_H
#define _EXCEPTION_H

#include "utli.h"
#include "mini_uart.h"
#include "peripherals/irq.h"
#include "timer.h"          //for timer interrupt handler
#include "mini_uart.h"      //for mini uart interrupt handler

#define MAX_TASKS 64
#define OTHER_PRIORITY 3
#define UART_PRIORITY 2
#define TIMER_PRIORITY 1

typedef struct task {
    void (*callback)(void*);  // callback function
    void* arg;
    int priority;            // 越小越高 (0 > 1 > 2)
    struct task* next;
} task_t;


//handler
void default_handler(unsigned long type, unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long far);
void lower_sync_handler(unsigned long type, unsigned long esr, unsigned long elr, unsigned long spsr);
void handle_el1_irq();

//task related
void task_enqueue(void (*callback)(void *), void *arg, int priority);
void task_execute_loop();
void task_execute_one();
#endif