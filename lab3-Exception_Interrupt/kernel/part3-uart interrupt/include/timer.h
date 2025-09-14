#ifndef _TIMER_H
#define _TIMER_H

#include "mini_uart.h"
#include "peripherals/irq.h"

void core_timer_enable();
void two_second_core_timer_handler();
#endif