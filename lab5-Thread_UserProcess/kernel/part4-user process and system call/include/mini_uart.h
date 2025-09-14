#ifndef _MINI_UART_H
#define _MINI_UART_H

#include "peripherals/gpio.h"
#include "peripherals/irq.h"
#include "type.h"
#include "wait.h"
//spec: https://github.com/ec-enggshivam/CPPWork/blob/master/Raspberry%20Pi/BCM2837-ARM-Peripherals.pdf

//下面的值是參考 spec 2-1 p.8 的表格抄來的
// Auxiliary Interrupt status
#define AUX_IRQ         ((volatile unsigned int*)(MMIO_BASE+0x00215000))
#define AUX_ENABLE      ((volatile unsigned int*)(MMIO_BASE+0x00215004))
#define AUX_MU_IO       ((volatile unsigned int*)(MMIO_BASE+0x00215040))
#define AUX_MU_IER      ((volatile unsigned int*)(MMIO_BASE+0x00215044))
#define AUX_MU_IIR      ((volatile unsigned int*)(MMIO_BASE+0x00215048))
#define AUX_MU_LCR      ((volatile unsigned int*)(MMIO_BASE+0x0021504c))
#define AUX_MU_MCR      ((volatile unsigned int*)(MMIO_BASE+0x00215050))
#define AUX_MU_LSR      ((volatile unsigned int*)(MMIO_BASE+0x00215054))
#define AUX_MU_MSR      ((volatile unsigned int*)(MMIO_BASE+0x00215058))
#define AUX_MU_SCRATCH  ((volatile unsigned int*)(MMIO_BASE+0x0021505c))
#define AUX_MU_CNTL     ((volatile unsigned int*)(MMIO_BASE+0x00215060))
#define AUX_MU_STAT     ((volatile unsigned int*)(MMIO_BASE+0x00215064))
#define AUX_MU_BAUD     ((volatile unsigned int*)(MMIO_BASE+0x00215068))


#define BUFFER_MAX_SIZE 256u
void set_gpio_uart();
void uart_init();

///////////////////////handler
void c_read_handler();
void c_write_handler();

////////////////////////async version
char uart_read_async();
void uart_write_char_async(char c);
void readcmd_async(char* str);
void uart_write_str_async(const char *str);
void uart_write_int_async(int num);
void enable_uart_interrupt();
void disable_uart_interrupt();
void uart_write_flush();


/////////////////////////// sync version
//第一組，/遮一組無論如何都能寫的是保留給系統 log 還有 exception handler 用的
char uart_read_raw();
void uart_write_char_raw(char ch);
void uart_write_str_raw(const char *str);
void uart_write_uint_raw(unsigned int num);
void uart_write_int_raw(int num);
// void uart_write_hex_raw(int num);
void uart_write_hex_raw(uint64_t num);
//第二組，//這一組需要 lock 才能用的是給 thread 用的(比如 shell 跟 foo 這種 task)
char uart_read();
void uart_write_char(char);
void uart_write_str(const char *);
void readcmd(char* str);
void uart_write_int(int);

////////////////////// other function
void int2str_dec(int, char *);
// void int2str_hex(int, char *);
void int2str_hex(uint64_t, char *);
void uart_write_hex(int);


#endif 