#ifndef _TIMER_H
#define _TIMER_H

#include "mini_uart.h"
#include "type.h"
#include "peripherals/irq.h"
#include "utli.h"
#include "exception.h"


#define MAX_TIMER_EVENTS 64
#define MAX_MSG_LEN 64

typedef struct timer_event {
    unsigned long long expire_time;
    void (*callback)(void*);
    void *arg;
    char* type;
    struct timer_event *next;
} timer_event_t;


//helper function
unsigned long get_current_tick();
unsigned long get_cntfrq();
void simple_strncpy(char *dest, const char *src, int max_len);
void core_timer_enable();
void parse_settimeout(char *cmd);

//add timer function, callback function 請寫在需要用到的.c檔案而不是這裡
void add_timer(void (*callback)(void*), char* type, void* arg, unsigned int milliseconds);


//hanlder function
void two_second_core_timer_handler();
void core_timer_handler();
#endif