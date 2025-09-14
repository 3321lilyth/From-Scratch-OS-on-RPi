#include "mmu.h"


// 0x00000000 ~ 0x3eff_ffff 是 normal memory
// 0x3f000000 ~ 0x3fff_ffff 是 device memory
#define DEVICE_MEM_START 0x3f000000
#define DEVICE_MEM_END   0x80000000
#define PA_TO_PMD_ATTR(pa) \
    (((pa) >= DEVICE_MEM_START && (pa) < DEVICE_MEM_END) ? \
    PMD_ATTR_DEVICE : PMD_ATTR_NORMAL)    //用 PA 判斷這段記憶體是不是屬於 MMIO
    


// 做 Finer Granularity Paging，從 2-level 變成 3-level
// 總共 map 的 virtual memory spave 還是 2GB，跟 boot 一樣
// 所以會需要 PGD table 的1個entry + PUD table的2個entry 和兩個完整的 PMD table
void kernel_vm_init() {
    uart_write_str_raw("\r\n[mmu] Finer granularity to 3 level paging...");
    unsigned long *pgd = (unsigned long *)KERNEL_PGD_ADDR;
    unsigned long *pud = (unsigned long *)KERNEL_PUD_ADDR;
    unsigned long *pmd1 = (unsigned long *)KERNEL_PMD_ADDR;
    unsigned long *pmd2 = (unsigned long *)KERNEL_PMD2_ADDR;

    // 建立 PGD → PUD
    pgd[0] = (unsigned long)pud | PD_TABLE; 

    // 建立 PUD[0] → PMD1, PUD[1] → PMD2
    pud[0] = (unsigned long)pmd1 | PD_TABLE;
    pud[1] = (unsigned long)pmd2 | PD_TABLE;

    // 建立 PMD1： map VA 0xFFFF_0000_4000_0000 ~ 0xFFFF_0000_7FFF_FFFF
                // to PA 0x00000000 ~ 0x3fffffff（第一個 1GB）
    for (int i = 0; i < 512; i++) {
        unsigned long pa = i * 0x200000UL;          //pa 是每一個 2MB block 的起始位址的 PA
        unsigned long attr = PA_TO_PMD_ATTR(pa);    //用 PA 判斷這段記憶體是不是屬於 MMIO
        pmd1[i] = pa | attr;
    }

    // 建立 PMD2：map 0x40000000 ~ 0x7fffffff（第二個 1GB）
    for (int i = 0; i < 512; i++) {
        unsigned long pa = 0x40000000UL + i * 0x200000UL;
        unsigned long attr = PMD_ATTR_DEVICE;
        pmd2[i] = pa | attr;
    }

    // 載入 PGD 到 TTBR1（kernel space 專用）
    // 注意 ttbr 裡面寫的一定要是 PA，這樣才能確保 MMU access 到 page table
    asm volatile("msr ttbr1_el1, %0" : : "r"(pgd)); 
    // Disable TTBR0_EL1 by setting TCR_EL1.T0SZ = 64 (means no VA range for ttbr0)
    uint64_t tcr;
    asm volatile ("mrs %0, tcr_el1" : "=r"(tcr));
    tcr |= (64 << 0);  // T0SZ = 64 => ttbr0 disabled
    asm volatile ("msr tcr_el1, %0" : : "r"(tcr));
    asm volatile("dsb ish; isb");
    uart_write_str_raw("\r\n[mmu] after Finer granularity");
}