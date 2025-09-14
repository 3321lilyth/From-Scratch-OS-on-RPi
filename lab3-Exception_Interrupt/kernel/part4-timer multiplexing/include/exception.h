#ifndef _EXCEPTION_H
#define _EXCEPTION_H

#include "utli.h"
#include "mini_uart.h"
#include "peripherals/irq.h"
#include "timer.h"          //for timer interrupt handler
#include "mini_uart.h"      //for mini uart interrupt handler


void disable_interrupt();
void enable_interrupt();

void default_handler(unsigned long type, unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long far);
void lower_sync_handler(unsigned long type, unsigned long esr, unsigned long elr, unsigned long spsr);
void handle_el1_irq();

#endif