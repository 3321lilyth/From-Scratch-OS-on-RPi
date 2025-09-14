#ifndef _MMU_H
#define _MMU_H

#include "mini_uart.h"
#include "vm_config.h"
#include "memory_test.h"
#include "buddy.h"      //for page size define
#include "type.h"      //for page size define


//attribute
#define PMD_ATTR_DEVICE ((MAIR_IDX_DEVICE_nGnRnE << 2) | PD_ACCESS | PD_BLOCK | PD_AF)  //kernel space 的 PMD entry 的 attribute
#define PMD_ATTR_NORMAL ((MAIR_IDX_NORMAL_NOCACHE << 2) | PD_ACCESS | PD_BLOCK | PD_AF)


//address allocate (這是物理記憶體)
//注意 ttbr 裡面寫的一定要是 PA，這樣才能確保 MMU access 到 page table
// page table 裡面也應該要寫 PA
#define KERNEL_PGD_ADDR 0x3000 //本來是設定承跟 boot 階段一樣的，後來發現有錯(詳見 notion PART1 step5)，所以還是改成布一樣的，但就是浪費了兩個 page 的空間
#define KERNEL_PUD_ADDR (KERNEL_PGD_ADDR + 0x1000)
#define KERNEL_PMD_ADDR (KERNEL_PGD_ADDR + 0x2000)
#define KERNEL_PMD2_ADDR (KERNEL_PGD_ADDR + 0x3000)



// User Page Table related
#define PTE_VALID  (1L << 0)        // 此 entry 是有效的
#define PTE_TABLE  (1L << 1)        // 此 entry 是中繼層 table (非 leaf)
#define PTE_PAGE   (1L << 1)        // 此 entry 是 leaf。bit 位置跟上面一樣，根據所在的 level 不同有不同意義 
#define PTE_USER   (1L << 6)        // User 空間允許存取
#define PTE_RW     (0L << 7)        // 0 for read-write, 1 for read-only.
#define PTE_RO     (1L << 7)        // 0 for read-write, 1 for read-only.
#define PTE_AF     (1L << 10)       // Access Flag，若為 0，第一次 access 該 page 就會 page fault（軟體可用於 demand paging）
#define PTE_NORMAL ((1L << 2) * MAIR_IDX_NORMAL_NOCACHE)  // Attr index 1 → normal memory
#define PTE_DEVICE ((1L << 2) * MAIR_IDX_DEVICE_nGnRnE)  // Attr index 0 → device memory 


#define VA_BITS    48
#define MAX_PHYS_ADDR 0x40000000

// utli function
    // AArch64 4-level translation 使用：
    // PGD：位元 [47:39]
    // PUD：位元 [38:30]
    // PMD：位元 [29:21]
    // PTE：位元 [20:12]
    //0x1FF 等於 0b111111111，剛好 mask 掉 9 bits → 512 entries。
#define PGD_IDX(va) (((va) >> 39) & 0x1FF)
#define PUD_IDX(va) (((va) >> 30) & 0x1FF)
#define PMD_IDX(va) (((va) >> 21) & 0x1FF)
#define PTE_IDX(va) (((va) >> 12) & 0x1FF)


//我設定的 user space VA 範圍和他們對應的記憶體類型
#define VA_CODE_END         0x00100000UL
#define VA_FB_START         0x00100000UL
#define VA_FB_END           0x00400000UL
#define VA_HANDLER_STACK_START 0x000000000040b000UL
#define VA_HANDLER_STACK_END   0x000000000040f000UL
#define VA_USER_STACK_START    0x0000ffffffffb000UL
#define VA_USER_STACK_END      0x0000fffffffff000UL

void kernel_vm_init();
uint64_t *walk(pagetable_t pagetable, uint64_t va, int alloc);
int mappages(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, int attr);
void switch_user_address_space(uint64_t pgd_pa);
void restore_kernel_address_space() ;
void free_pagetable(pagetable_t pagetable, int level, uint64_t base_va);
pagetable_t copy_user_pagetable(pagetable_t old_pgd, int level, uint64_t base_va);
void trace_va_mapping(pagetable_t pgd, uint64_t va);
#endif