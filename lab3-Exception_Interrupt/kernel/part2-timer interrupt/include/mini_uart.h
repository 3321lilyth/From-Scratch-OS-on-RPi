#ifndef _MINI_UART_H
#define _MINI_UART_H

#include "peripherals/gpio.h"
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

// void set_gpio_alt(int, int);
// void gpio_disable_pull(int);
void set_gpio_uart();
void uart_init();

char uart_read();
void uart_write_char(char);
void uart_write_str(const char *);

void int2str_dec(int, char *);
void uart_write_int(int);
void int2str_hex(int, char *);
void uart_write_hex(int);


#endif 