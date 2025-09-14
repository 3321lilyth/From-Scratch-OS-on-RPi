#ifndef IRQ_H
#define IRQ_H

#include "gpio.h"

//------------this file: IRQ Numbers from BCM2837 Manual Pending IRQ Registers
#define IRQ_TIMER_MATCH_1  1
#define IRQ_TIMER_MATCH_3  3
#define IRQ_USB            9
#define IRQ_AUX            29   //lab3 會用到!! 就是 mini uart IQR
#define IRQ_I2C_SPI_SLV    43
#define IRQ_PWA0           45
#define IRQ_PWA1           46
#define IRQ_SMI            48
#define IRQ_GPIO_0         49
#define IRQ_GPIO_1         50
#define IRQ_GPIO_2         51
#define IRQ_GPIO_3         52
#define IRQ_I2C            53
#define IRQ_SPI            54
#define IRQ_PCM            55
#define IRQ_UART           57  // PL011 UART IRQ

// IRQ Registers (Pending IRQ)
#define IRQ_PENDING_1 	 ((volatile unsigned int*)(MMIO_BASE + 0x0000b204))     // IRQ 0-31
#define IRQ_PENDING_2 	 ((volatile unsigned int*)(MMIO_BASE + 0x0000b208))     // IRQ 32-63
#define ENABLE_IRQS_1 		 ((volatile unsigned int*)(MMIO_BASE + 0x0000b210)) // IRQ 0-31
#define ENABLE_IRQS_2 		 ((volatile unsigned int*)(MMIO_BASE + 0x0000b214)) // IRQ 0-31
#define DISABLE_IRQS_1 	 ((volatile unsigned int*)(MMIO_BASE + 0x0000b21c))
// #define ARM_IRQ_REG_BASE ((volatile unsigned int*)(MMIO_BASE + 0x0000b000))

//-------------------------------- PART2 - IRQ Source for Core 0
#define CORE0_INTERRUPT_SOURCE      ((volatile unsigned int *)(0x40000060))
#define CORE0_TIMER_IRQ_CTRL ((volatile unsigned int *)(0x40000040))
#define CORE_TIMER_IRQ_BIT          (1 << 1)   // Core Timer IRQ (Bit 1 in CORE0_INTERRUPT_SOURCE)
#define CORE0_GPU_IRQ_BIT          (1 << 8)     //GPU IRQ (Bit 8 in CORE0_INTERRUPT_SOURCE)
                                                //所有周邊IRQ，也就是 pending1 + pending2 對 core 來說都屬於 GPU IRQ



#endif
