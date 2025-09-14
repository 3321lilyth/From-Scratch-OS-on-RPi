#include "mmu.h"


// 0x00000000 ~ 0x3eff_ffff 是 normal memory
// 0x3f000000 ~ 0x3fff_ffff 是 device memory
#define DEVICE_MEM_START 0x3C000000
#define DEVICE_MEM_END   0x80000000
#define PA_TO_PMD_ATTR(pa) \
    (((pa) >= DEVICE_MEM_START && (pa) < DEVICE_MEM_END) ? \
    PMD_ATTR_DEVICE : PMD_ATTR_NORMAL)    //用 PA 判斷這段記憶體是不是屬於 MMIO
    


// reference count system
typedef struct {
    int refcount;
} page_meta_t;

#define MAX_PAGES 245760
#define PHYS_BASE 0x0

static page_meta_t page_table[MAX_PAGES];

static int pa_to_index(uint64_t pa) {
    return (pa - PHYS_BASE) / PAGE_SIZE;
}

void incr_refcount(uint64_t pa, uint64_t va) {
    if (va >= VA_USER_STACK_START && va < VA_USER_STACK_END){
        pa -= PAGE_SIZE;  // user stack: adjust for downwarellsed growth
    }else if (va >= 0x40b000 && va < 0x40f000) {
        pa -= PAGE_SIZE;  // user signal stack: adjust for downward growth
    }
    int idx = pa_to_index(pa);
    page_table[idx].refcount++;
}
    
void decr_refcount_and_free(uint64_t pa, uint64_t va) {
    if (va >= VA_USER_STACK_START && va < VA_USER_STACK_END){
        pa -= PAGE_SIZE;  // user stack: adjust for downward growth
    }else if (va >= 0x40b000 && va < 0x40f000) {
        pa -= PAGE_SIZE;  // user signal stack: adjust for downward growth
    }
    int idx = pa_to_index(pa);
    if (--page_table[idx].refcount == 0) {
        kfree(PA_TO_VA_KERNEL(pa));
    }
}




// 做 Finer Granularity Paging，從 2-level 變成 3-level
// 總共 map 的 virtual memory spave 還是 2GB，跟 boot 一樣
// 所以會需要 PGD table 的1個entry + PUD table的2個entry 和兩個完整的 PMD table
void kernel_vm_init() {
    uart_write_str_raw("\r\n[mmu] Finer granularity to 3 level paging...");
    uart_write_str_raw("\r\n[mmu] PMD_ATTR_NORMAL = ");
    uart_write_hex_raw(PMD_ATTR_NORMAL);
    uart_write_str_raw("\r\n[mmu] PMD_ATTR_DEVICE = ");
    uart_write_hex_raw(PMD_ATTR_DEVICE);
    uart_write_str_raw("\r\n[mmu] PA_TO_PMD_ATTR(0x3C100000) = ");
    uart_write_hex_raw(PA_TO_PMD_ATTR(0x3C100000));
    unsigned long *pgd = (unsigned long *)KERNEL_PGD_ADDR;
    unsigned long *pud = (unsigned long *)KERNEL_PUD_ADDR;
    unsigned long *pmd1 = (unsigned long *)KERNEL_PMD_ADDR;
    unsigned long *pmd2 = (unsigned long *)KERNEL_PMD2_ADDR;

    // 建立 PGD → PUD
    pgd[0] = (unsigned long)pud | PD_TABLE; 

    // 建立 PUD[0] → PMD1, PUD[1] → PMD2
    pud[0] = (unsigned long)pmd1 | PD_TABLE;
    pud[1] = (unsigned long)pmd2 | PD_TABLE;

    // 建立 PMD1： map VA 0xFFFF_0000_0000_0000 ~ 0xFFFF_0000_3FFF_FFFF
                // to PA 0x00000000 ~ 0x3fffffff（第一個 1GB）
    for (int i = 0; i < 512; i++) {
        unsigned long pa = i * 0x200000UL;          //pa 是每一個 2MB block 的起始位址的 PA
        unsigned long attr = PA_TO_PMD_ATTR(pa);    //用 PA 判斷這段記憶體是不是屬於 MMIO
        pmd1[i] = pa | attr;
    }

    // 建立 PMD2：map VA 0xFFFF_0000_4000_0000 ~ 0xFFFF_0000_7FFF_FFFF
                // to PA 0x40000000 ~ 0x7fffffff（第二個 1GB）
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
    asm volatile ("mrs %0, tcr_el1\n" : "=r"(tcr));
    tcr |= (64 << 0);  // T0SZ = 64 => ttbr0 disabled
    asm volatile ("msr tcr_el1, %0\n" : : "r"(tcr));
    asm volatile("dsb ish \n");
    asm volatile("isb \n");
}



// 走訪 4-level page table，找到 target VA 對應的 PTE entry 是哪一個
// 第一次 access 該 page 時要 kmalloc 一個塊區域給他，比 page address 放到上一層裡面
// 參數
    // pagetable: PGD base 地址（即第 0 層 page table），應該是 VA 就好
    // int alloc：代表 當中間某一層 page table 不存在時，是否需要動態建立它
uint64_t *walk(pagetable_t pagetable, uint64_t va, int alloc) {
    
    //pagetable 從 level0 開始往下找，值到 PT 為止，然後回傳 PTE
    for (int level = 0; level < 3; level++) {
        int idx = (level == 0) ? PGD_IDX(va) :
                  (level == 1) ? PUD_IDX(va) : PMD_IDX(va);

        uint64_t *entry = &pagetable[idx];

        if (*entry & PTE_VALID) {
            // 取得下一層 page table 的 VA（先從 entry 取得 PA）
            pagetable = (pagetable_t)PA_TO_VA_USER(*entry & ~(PAGE_SIZE - 1));
        } else {
            if (!alloc) return 0;

            // 分配新的 page table，得到的是 VA
            void *new_table_va = kmalloc(PAGE_SIZE);
            if (!new_table_va) return 0;
            memset(new_table_va, 0, PAGE_SIZE);
            
            // 清空 table
            for (int i = 0; i < 512; i++)
                ((uint64_t *)new_table_va)[i] = 0;

            // 把 VA 轉為 PA 寫入 page table entry
            uint64_t new_table_pa = VA_TO_PA_KERNEL(new_table_va);
            *entry = new_table_pa | PTE_VALID | PTE_TABLE;

            pagetable = (pagetable_t)new_table_va;
            uart_write_str_raw("\r\n [mmu] walk(), allocate a new page for level ");
            uart_write_int_raw(level+1);
            uart_write_str_raw(", parent entry idx = ");
            uart_write_int_raw(idx);
            uart_write_str_raw(", new_table_va = ");
            uart_write_hex_raw((uintptr_t)new_table_va);
        }
    }

    // 最後一層 PTE
    return &pagetable[PTE_IDX(va)];
}



// 對某段 VA 範圍(pa~pa+size)建立 page mapping（4KB granularity），對應到"連續的"物理記憶體空間(va~va+sizw)
// 如果在物理記憶體是不連續的，就要分開呼叫 mappages
int mappages(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, int attr) {
    uint64_t a = va & ~(PAGE_SIZE - 1);                     //現在這一個要 map 的 page 的虛擬位址
            //size 可能不是 page 對齊的，因此用 & ~(PAGE_SIZE-1) 向下對齊。
            // 比如我需要 access 0x1017，就要 enable 0x1000 這個 page
    uint64_t last = ((va + size - 1) & ~(PAGE_SIZE - 1));   //最後一個 page

    while (1) {
        // 先拿到這個 VA 對應到的 level3 page table entry
        // uart_write_str_raw("\r\n [mmu] mappages(), map va ");
        // uart_write_hex_raw(a);
        // uart_write_str_raw(" to pa ");
        // uart_write_hex_raw(pa);

        uint64_t *pte = walk(pagetable, a, 1);
        if (!pte) return -1;

        // 如果 pte 已經是有效的，代表之前 free 有做錯，就先 warning 然後直接覆蓋
        if (*pte & PTE_VALID) {
            uart_write_str_raw("\r\n[WARNING] mappages(): overwriting valid PTE at va=");
            uart_write_hex_raw(a);
        }

        //把 PA 存到這個 PTE 裡面，(pa & ~(PAGE_SIZE - 1)) 是因為要先把原本的 sttr 都清掉
        *pte = (pa & ~(PAGE_SIZE - 1)) | PTE_VALID | PTE_PAGE | PTE_AF | attr;
        incr_refcount(pa, a);
        // uart_write_str_raw(", PTE = ");
        // uart_write_hex_raw(*pte);

        if (a == last) break;
        a += PAGE_SIZE;
        pa += PAGE_SIZE;
    }
    return 0;
}



///////////////////////////////////// helper for sysem call ///////////////////////////////////
// 切換到指定使用者 PGD
void switch_user_address_space(uint64_t pgd_pa) {
    // uart_write_str_raw("\r\n[mmu] switch_user_address_space, pgd_pa = ");
    // uart_write_hex_raw(pgd_pa);
    asm volatile("dsb ish");
    asm volatile("msr ttbr0_el1, %0" : : "r"(pgd_pa));
    asm volatile("tlbi vmalle1is");
    asm volatile("dsb ish");
    asm volatile("isb");
}

void restore_kernel_address_space() {
    // uart_write_str_raw("\r\n[mmu] restore_kernel_address_space");
    // uint64_t kernel_pgd = KERNEL_PGD_ADDR;
    // asm volatile("dsb ish");
    // asm volatile("msr ttbr0_el1, %0" : : "r"(kernel_pgd));
    // asm volatile("tlbi vmalle1is");
    // asm volatile("dsb ish");
    // asm volatile("isb");
}

// 創建一個新的 user thread 的時候會呼叫這邊，img_data 是指 image 在 cpio 裡面得到的位置
void user_pagetable_setup(thread_t *t, void* img_base, size_t img_size){
// void user_pagetable_setup(thread_t *t, void* img_data, size_t img_size) {
    pagetable_t pgd_va = (pagetable_t)kmalloc(PAGE_SIZE);
    memset(pgd_va, 0, PAGE_SIZE);
    t->user_pagetable = pgd_va;
    

    uart_write_str_raw("\r\n--------------- user_pagetable_setup, mapping code section --------------------");
    uint64_t user_text_va = 0x0;
    size_t aligned_size = PAGE_UP(img_size);
    for (size_t offset = 0; offset < aligned_size; offset += PAGE_SIZE) {
        uint64_t pa = VA_TO_PA_KERNEL((uint64_t)img_base + offset);
        mappages(pgd_va, user_text_va + offset, PAGE_SIZE, pa, PTE_NORMAL | PTE_USER);
    }

    //這是一個一個 page 分配的版本，好像怪怪
    // size_t aligned_size = PAGE_UP(img_size);
    // for (size_t offset = 0; offset < aligned_size; offset += PAGE_SIZE) {
    //     void* page = kmalloc(PAGE_SIZE);
    //     memset(page, 0, PAGE_SIZE);

    //     // memcpy from image source
    //     size_t copy_size = (offset + PAGE_SIZE <= img_size) ? PAGE_SIZE : (img_size - offset);
    //     simple_memcpy(page, (uint8_t *)img_data + offset, copy_size);

    //     uint64_t pa = VA_TO_PA_KERNEL((uint64_t)page);
    //     mappages(pgd_va, offset, PAGE_SIZE, pa, PTE_NORMAL | PTE_USER);
    // }
    

    uart_write_str_raw("\r\n--------------- user_pagetable_setup, mapping stack section --------------------");
    uint64_t stack_top = VA_USER_STACK_END;
    t->user_sp = stack_top;
    t->user_stack_base = (void *)(stack_top - 4 * PAGE_SIZE);
    for (int i = 0; i < 4; i++) {
        pagetable_t stack_page = (pagetable_t)kmalloc(PAGE_SIZE);
        memset(stack_page, 0, PAGE_SIZE);
        void* pa = (void *)VA_TO_PA_KERNEL(stack_page); //這邊拿到的是記憶體空間的 base，所以真正的 stack top 應該要再加上 PAGE_SIZE 才對
        mappages(pgd_va, stack_top - i * PAGE_SIZE, PAGE_SIZE, (uint64_t)pa + PAGE_SIZE, PTE_NORMAL | PTE_USER);
    }
}

//從指定 level 的 page table 開始往下遞迴釋放所有 child node 直到 leaf
void free_pagetable(pagetable_t pagetable, int level, uint64_t base_va) {
    uart_write_str_raw("\r\ncurrent level = ");
    uart_write_int_raw(level);
    uart_write_str_raw(", page table = ");
    uart_write_hex_raw((uintptr_t)pagetable);

    if (level == 3) {
        for (int i = 0; i < 512; i++) {
            uint64_t pte = pagetable[i];
            if ((pte & PTE_VALID) && (pte & PTE_PAGE)) {
                uint64_t pa = pte & ~(PAGE_SIZE - 1);
                // uint64_t va = ((uint64_t)i << 12);  // 重建 VA 位置
                uint64_t va = base_va + ((uint64_t)i << 12);

                // === Framebuffer (跳過釋放) ===
                if (va >= VA_FB_START && va < VA_FB_END) {
                    continue;
                }

                // === 正常處理 stack 或 code/data ===
                decr_refcount_and_free(pa, va);
                uart_write_str_raw("\r\n    freeing level3 PT at pa=");
                uart_write_hex_raw(pa);
                pagetable[i] = 0;
            }
        }
    } else {
        for (int i = 0; i < 512; i++) {
            uint64_t entry = pagetable[i];
            if ((entry & PTE_VALID) && (entry & PTE_TABLE)) {
                uint64_t pa = entry & ~(PAGE_SIZE - 1);
                pagetable_t next_level = (pagetable_t)PA_TO_VA_KERNEL(pa);
                uint64_t child_base_va = base_va + ((uint64_t)i << ((3 - level - 1) * 9 + 12));
                free_pagetable(next_level, level + 1, child_base_va);
                
                decr_refcount_and_free(pa, 0);  // internal page 無需指定 VA
                uart_write_str_raw("\r\n    freeing level");
                uart_write_int_raw(level);
                uart_write_str_raw(" PT at pa=");
                uart_write_hex_raw(pa);
                pagetable[i] = 0;
            }
        }
    }
}

void free_va_range(pagetable_t pgd, uint64_t base, int pages) {
    for (int i = 0; i < pages; i++) {
        uint64_t va = base + (i+1) * PAGE_SIZE;
        uint64_t *pte = walk(pgd, va, 0);
        if (pte && (*pte & PTE_VALID)) {
            uint64_t pa = *pte & ~(PAGE_SIZE - 1);
            decr_refcount_and_free(pa, va);
            *pte = 0;
        }
    }
}

//fork 會呼叫到這邊。傳入一個現存 PGD，就會自動分配另外一個 PGD，並且遞迴複製裡面的內容(deep copy)，最後回傳新 PGD 的地址
// pagetable_t copy_user_pagetable(pagetable_t old_page, int level) {
//     if (level >= 4) return 0;  // 超過 PTE level 就不合法

//     // ----------- Leaf level: PTE -----------
//     if (level == 3) {
        
//         void *new_leaf = kmalloc(PAGE_SIZE);
//         if (!new_leaf) return 0;
//         memset(new_leaf, 0, PAGE_SIZE);

//         uart_write_str_raw("\r\n [mmu] copy_user_pagetable(), allocate a new page for level 3 ");
//         uart_write_str_raw(", old_page = ");
//         uart_write_hex_raw((uintptr_t)old_page);
//         uart_write_str_raw(", new_leaf = ");
//         uart_write_hex_raw((uintptr_t)new_leaf);
        
        
//         //整頁複製，這是 linux 作法，child 的 code section 直接指向跟 parent 一樣的地方，refcount++
//         // simple_memcpy(new_leaf, old_page, PAGE_SIZE);

//         //我目前的偷懶作法
//         for (int i = 0; i < 512; i++) {
//             uint64_t pte = old_page[i];
//             if ((pte & PTE_VALID) && (pte & PTE_PAGE)) {
//                 uint64_t old_pa = pte & ~(PAGE_SIZE - 1);

//                 // MODIFIED: 分配新 page，並從原本 page 複製內容
//                 void* new_page = kmalloc(PAGE_SIZE);
//                 if (!new_page) continue;
//                 memset(new_page, 0, PAGE_SIZE);
//                 simple_memcpy(new_page, (const void*)PA_TO_VA_USER(old_pa), PAGE_SIZE);

//                 uint64_t new_page_pa = VA_TO_PA_KERNEL(new_page);

//                 // MODIFIED: 設定新的 PTE
//                 ((uint64_t*)new_leaf)[i] = new_page_pa | (pte & 0xFFF);  // 保留原本的 flags
//             }
//         }

//         return (pagetable_t)new_leaf;

//     }

//     // ----------- Middle levels: PGD / PUD / PMD -----------
//     pagetable_t new_table = (pagetable_t)kmalloc(PAGE_SIZE);
//     if (!new_table) return 0;
//     memset(new_table, 0, PAGE_SIZE);
//     uart_write_str_raw("\r\n [mmu] copy_user_pagetable(), allocate a new page for level ");
//     uart_write_int_raw(level);
//     uart_write_str_raw(", old_page = ");
//     uart_write_hex_raw((uintptr_t)old_page);
//     uart_write_str_raw(", new_table = ");
//     uart_write_hex_raw((uintptr_t)new_table);

//     for (int i = 0; i < 512; i++) {
//         uint64_t entry = old_page[i];
//         // ----------- 1. 中繼層 table entry -----------
//         if ((entry & PTE_VALID) && (entry & PTE_TABLE)) {
//             uint64_t pa = entry & ~(PAGE_SIZE - 1);
//             if (pa >= MAX_PHYS_ADDR || pa % PAGE_SIZE != 0) continue;

//             pagetable_t old_next = (pagetable_t)PA_TO_VA_USER(pa);
//             pagetable_t new_next = copy_user_pagetable(old_next, level + 1);
//             if (!new_next) return 0;
//             new_table[i] = VA_TO_PA_KERNEL(new_next) | PTE_VALID | PTE_TABLE;
//         }

//         // ----------- 2. 處理 block entry (e.g., PTE_BLOCK @ level 2) -----------
//         else if (level == 2 && (entry & PTE_VALID) && (entry & PD_BLOCK)) {
//             uart_write_str_raw("\r\n[mmu] copy_user_pagetable(), found PD_BLOCK");

//             uint64_t old_pa = entry & ~(PAGE_SIZE - 1);

//             // 2MB block size (512 * 4KB)
//             size_t block_size = 0x200000;
//             void* new_block = kmalloc(block_size);
//             if (!new_block) continue;

//             simple_memcpy(new_block, (const void*)PA_TO_VA_USER(old_pa), block_size);
//             uint64_t new_block_pa = VA_TO_PA_KERNEL(new_block);

//             new_table[i] = new_block_pa | (entry & 0xFFF);  // 保留 flags
//         }
//         else if (level != 2 && (entry & PTE_VALID) && (entry & PD_BLOCK)){
//             uart_write_str_raw("\r\n[mmu] RRRRRRRRRRRRRRRRRRRRRRRRR");
//             while(1);
//         }
//     }

//     return new_table;
// }

pagetable_t copy_user_pagetable(pagetable_t old_page, int level, uint64_t base_va) {
    if (level >= 4) return 0;

    if (level == 3) {
        pagetable_t new_leaf = (pagetable_t)kmalloc(PAGE_SIZE);
        if (!new_leaf) return 0;
        memset(new_leaf, 0, PAGE_SIZE);

        for (int i = 0; i < 512; i++) {
            uint64_t pte = old_page[i];
            if ((pte & PTE_VALID) && (pte & PTE_PAGE)) {
                uint64_t pa = pte & ~(PAGE_SIZE - 1);
                // uint64_t va = ((uint64_t)i << 12);  // 虛擬位址相對 base
                uint64_t va = base_va + ((uint64_t)i << 12);

                // === Code/Data/BSS segment: VA < 0x100000 ===
                if (va < VA_CODE_END) {
                    incr_refcount(pa, va);
                    new_leaf[i] = pa | (pte & 0xFFF);
                }

                // === Framebuffer: 0x00100000 ~ 0x00400000 ===
                else if (va >= VA_FB_START && va < VA_FB_END) {
                    // 直接複製 page table entry，不 refcount
                    new_leaf[i] = pa | (pte & 0xFFF);
                }

                // === User handler Stack: VA 0x40_f000~0x40_b000 ===
                else if (va >= VA_HANDLER_STACK_START && va <= VA_HANDLER_STACK_END) {
                    continue;
                }

                // === User Stack: 0x0000_ffff_ffff_b000 ~ 0x0000_ffff_ffff_f000 ===
                else if (va >= VA_USER_STACK_START && va <= VA_USER_STACK_END) {
                    // 這邊要特別處理 user stack 的 downward growth`
                    void* new_page = kmalloc(PAGE_SIZE);
                    if (!new_page) continue;
                    memset(new_page, 0, PAGE_SIZE);
                    simple_memcpy(new_page, (const void*)PA_TO_VA_USER(pa), PAGE_SIZE);
                    uint64_t new_pa = VA_TO_PA_KERNEL(new_page+PAGE_SIZE);  // +PAGE_SIZE 是因為 user stack 是 downward growth，所以要把新 page 放在上面`
                    new_leaf[i] = new_pa | (pte & 0xFFF);
                }

                else if (va >= 0x0001000000000000UL) {
                    uart_write_str_raw("\r\n[mmu WARNING] copy_user_pagetable(): unexpected VA = ");
                    uart_write_hex_raw(va);
                    continue;
                    // while(1);  // 強制 debug trap
                }

                // === Other: default refcount clone ===
                else {
                    uart_write_str_raw("\r\n[mmu WARNING] copy_user_pagetable(), copying other VA = ");
                    uart_write_hex_raw(va);
                    incr_refcount(pa, va);
                    new_leaf[i] = pa | (pte & 0xFFF);
                }
            }
        }
        return new_leaf;
    }

    // ===== 中繼層遞迴處理 =====
    pagetable_t new_table = (pagetable_t)kmalloc(PAGE_SIZE);
    uart_write_str_raw("\r\n        [mmu] copy_user_pagetable(), allocate a new page for level ");
    uart_write_int_raw(level);
    if (!new_table) return 0;
    memset(new_table, 0, PAGE_SIZE);

    for (int i = 0; i < 512; i++) {
        uint64_t entry = old_page[i];
        if ((entry & PTE_VALID) && (entry & PTE_TABLE)) {
            uint64_t pa = entry & ~(PAGE_SIZE - 1);
            if (pa >= MAX_PHYS_ADDR || pa % PAGE_SIZE != 0) continue;
            
            uint64_t child_base = base_va + ((uint64_t)i << ((3 - level - 1) * 9 + 12));
            pagetable_t old_next = (pagetable_t)PA_TO_VA_USER(pa);
            pagetable_t new_next = copy_user_pagetable(old_next, level + 1, child_base);
            if (!new_next) return 0;

            new_table[i] = VA_TO_PA_KERNEL(new_next) | PTE_VALID | PTE_TABLE;
        }
    }

    return new_table;
}



uint64_t VA_TO_PA_USER(pagetable_t pgd, uint64_t va) {
    uint64_t *pte = walk(pgd, va, 0);
    if (!pte || !(*pte & PTE_VALID) || !(*pte & PTE_PAGE)) {
        uart_write_str_raw("\r\n[VA_TO_PA_USER] ERROR: invalid PTE for VA = ");
        uart_write_hex_raw(va);
        return 0;
    }

    uint64_t page_base = *pte & ~(PAGE_SIZE - 1);  // PTE 的低 12 bits 是 flags，要遮掉
    uint64_t offset = va & (PAGE_SIZE - 1);        // VA 在該 page 中的 offset

    return page_base + offset;
}


//////////////////////////////////// debug helper ////////////////////////////////////
void trace_va_mapping(pagetable_t pgd, uint64_t va) {
    int pgd_idx = PGD_IDX(va);
    int pud_idx = PUD_IDX(va);
    int pmd_idx = PMD_IDX(va);
    int pte_idx = PTE_IDX(va);

    uart_write_str_raw("\r\n[debug] VA mapping trace:");
    uart_write_str_raw("\r\n  VA = "); uart_write_hex_raw(va);

    uint64_t pgd_entry = pgd[pgd_idx];
    uart_write_str_raw("\r\n  pgd idx = "); uart_write_int_raw(pgd_idx);
    if (!(pgd_entry & PTE_VALID)) {
        uart_write_str_raw(", entry = invalid");
        return;
    }
    uart_write_str_raw(", entry = "); uart_write_hex_raw(pgd_entry);
    pagetable_t pud = (pagetable_t)PA_TO_VA_KERNEL(pgd_entry & ~(PAGE_SIZE - 1));

    uint64_t pud_entry = pud[pud_idx];
    uart_write_str_raw("\r\n  pud idx = "); uart_write_int_raw(pud_idx);
    if (!(pud_entry & PTE_VALID)) {
        uart_write_str_raw(", entry = invalid");
        return;
    }
    uart_write_str_raw(", entry = "); uart_write_hex_raw(pud_entry);
    pagetable_t pmd = (pagetable_t)PA_TO_VA_KERNEL(pud_entry & ~(PAGE_SIZE - 1));

    uint64_t pmd_entry = pmd[pmd_idx];
    uart_write_str_raw("\r\n  pmd idx = "); uart_write_int_raw(pmd_idx);
    if (!(pmd_entry & PTE_VALID)) {
        uart_write_str_raw(", entry = invalid");
        return;
    }
    uart_write_str_raw(", entry = "); uart_write_hex_raw(pmd_entry);
    if (!(pmd_entry & PTE_TABLE)) {
        uart_write_str_raw("\r\n  --> PMD is a block entry, stop trace here.");
        return;
    }
    
    pagetable_t pte = (pagetable_t)PA_TO_VA_KERNEL(pmd_entry & ~(PAGE_SIZE - 1));
    uint64_t pte_entry = pte[pte_idx];
    uart_write_str_raw("\r\n  pte idx = "); uart_write_int_raw(pte_idx);
    if (!(pte_entry & PTE_VALID)) {
        uart_write_str_raw(", entry = invalid");
        return;
    }
    uart_write_str_raw(", entry = "); uart_write_hex_raw(pte_entry);
}

