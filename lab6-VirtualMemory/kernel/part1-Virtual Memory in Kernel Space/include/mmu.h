#ifndef _MMU_H
#define _MMU_H

#include "mini_uart.h"
#include "vm_config.h"


//attribute
#define PMD_ATTR_DEVICE ((MAIR_IDX_DEVICE_nGnRnE << 2) | PD_ACCESS | PD_BLOCK)  //kernel space 的 PMD entry 的 attribute
#define PMD_ATTR_NORMAL ((MAIR_IDX_NORMAL_NOCACHE << 2) | PD_ACCESS | PD_BLOCK)


//address allocate (這是物理記憶體)
//注意 ttbr 裡面寫的一定要是 PA，這樣才能確保 MMU access 到 page table
// page table 裡面也應該要寫 PA
#define KERNEL_PGD_ADDR 0x3000 //本來是設定承跟 boot 階段一樣的，後來發現有錯(詳見 notion PART1 step5)，所以還是改成布一樣的，但就是浪費了兩個 page 的空間
#define KERNEL_PUD_ADDR (KERNEL_PGD_ADDR + 0x1000)
#define KERNEL_PMD_ADDR (KERNEL_PGD_ADDR + 0x2000)
#define KERNEL_PMD2_ADDR (KERNEL_PGD_ADDR + 0x3000)





void kernel_vm_init();
#endif