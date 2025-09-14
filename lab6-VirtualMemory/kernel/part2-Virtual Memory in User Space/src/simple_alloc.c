#include "simple_alloc.h"


static uintptr_t startup_alloc_base = 0;
static uintptr_t startup_alloc_end  = 0;

extern uintptr_t dtb_start_addr;
extern uintptr_t dtb_end_addr;
extern uint64_t initrd_start_addr;
extern uint64_t initrd_end_addr;

uintptr_t heap_start_addr = (uintptr_t)&__heap_start;

//buddy variables
int TOTAL_PAGES;
int MAX_ORDER;
int MAX_NODES;
uintptr_t BASE_ADDR;
state_t* buddy_tree;
int* page_order_map;

//dynamic variables
chunk_pool_t *pools;
chunk_page_t *chunk_pages;
int *page_belong_pool;
unsigned char (*chunk_bitmaps)[BITMAP_BYTES];


void startup_alloc_init(uintptr_t mem_base, size_t mem_size) {
    uart_write_str("\r\n[startup mem] startup memory allocating...");
    startup_alloc_base = PA_TO_VA_KERNEL(heap_start_addr);
    uart_write_str("\r\n    startup_alloc_base = ");
    uart_write_hex_raw(startup_alloc_base);
    startup_alloc_end  = PA_TO_VA_KERNEL(mem_base) + mem_size;
    uart_write_str("\r\n    startup_alloc_end = ");
    uart_write_hex_raw(startup_alloc_end);

    //1. alloc buddy system 資料結構
    size_t total_pages = mem_size / PAGE_SIZE;
    int max_order = 0;
    while ((1 << max_order) < total_pages) max_order++;
    BASE_ADDR = PA_TO_VA_KERNEL(mem_base); // mem_base 會是 0x0，但是要轉為 kernel virtual address，所以要加上 offset
    MAX_ORDER = max_order;                  //18
    TOTAL_PAGES = total_pages;              //245760
    MAX_NODES = (1 << (MAX_ORDER + 1)) - 1; //524287
    buddy_tree = (state_t*) startup_alloc(sizeof(state_t) * MAX_NODES, 8);
    page_order_map = (int*) startup_alloc(sizeof(int) * TOTAL_PAGES, 8);


    //2. dynamic allocator 資料結構 
    pools = (chunk_pool_t *)startup_alloc(sizeof(chunk_pool_t) * POOL_COUNT, 8);
    chunk_pages = (chunk_page_t *)startup_alloc(sizeof(chunk_page_t) * MAX_PAGE_COUNT, 8);
    page_belong_pool = (int *)startup_alloc(sizeof(int) * MAX_PAGE_COUNT, 8);
    chunk_bitmaps = (unsigned char (*)[BITMAP_BYTES])startup_alloc(sizeof(unsigned char) * MAX_PAGE_COUNT * BITMAP_BYTES, 8);
    
    //3. buddy init
    buddy_init();

    //4. reserve
    reserve_memory(PA_TO_VA_KERNEL(0x3C000000), PA_TO_VA_KERNEL(0x40000000-1));    //invalid memory
    reserve_memory(PA_TO_VA_KERNEL(0x0000), PA_TO_VA_KERNEL(0x1000-1));            //spin tables for multicore boot
    reserve_memory(PA_TO_VA_KERNEL(0x1000), PA_TO_VA_KERNEL(0x2000-1));            //boot 階段的 kernel PGD
    reserve_memory(PA_TO_VA_KERNEL(0x2000), PA_TO_VA_KERNEL(0x3000-1));            //boot 階段的 kernel PUD
    reserve_memory(PA_TO_VA_KERNEL(0x3000), PA_TO_VA_KERNEL(0x4000-1));            //變成 3-level 後的 kernel PGD
    reserve_memory(PA_TO_VA_KERNEL(0x4000), PA_TO_VA_KERNEL(0x5000-1));            //變成 3-level 後的 kernel PUD
    reserve_memory(PA_TO_VA_KERNEL(0x5000), PA_TO_VA_KERNEL(0x6000-1));            //變成 3-level 後的 kernel PMD1
    reserve_memory(PA_TO_VA_KERNEL(0x6000), PA_TO_VA_KERNEL(0x7000-1));            //變成 3-level 後的 kernel PMD2
    reserve_memory(PA_TO_VA_KERNEL(0x59000), PA_TO_VA_KERNEL(get_startup_current_ptr() + (1024*1024) -1));
        //0x59000 ~ heap_start_addr                                                 -> kernel code and bootloader code
        //heap_start_addr ~ get_startup_current_ptr()                               -> buddy system & dynamic allocator data structure
        //get_startup_current_ptr() ~ get_startup_current_ptr() + (1024*1024) -1    -> kernel heap, 1MB
    reserve_memory(dtb_start_addr, dtb_end_addr);               //dtb，這個再 main 已經轉為 VA 了
    reserve_memory(initrd_start_addr, initrd_end_addr);         //initramfs，這個在 cpio 裡面沒有轉過，還是 PA
    
     
    uart_write_str("\r\n[startup mem] heap_start_addr           = ");
    uart_write_hex(heap_start_addr);
    uart_write_str("\r\n[startup mem] get_startup_current_ptr() = ");
    uart_write_hex(get_startup_current_ptr());
    uart_write_str("\r\n[startup mem] kernel 1MB heap start     = ");
    uart_write_hex(get_startup_current_ptr() + (1024*1024) -1);
    
    //5. chunk init
    chunk_init();

    //6. log
    // uart_write_str("\r\n------------------ after chunk init -----------------------");
    // dump_allocated_nodes();
    // dump_free_blocks(0);
    // dump_free_blocks(1);
    // dump_free_blocks(2);
    // dump_free_blocks(3);
}

void* startup_alloc(size_t size, size_t align) {
    if (size == 0){
        uart_write_str("\r\n[startup mem] ERROR: Invalid Size(need positive integer)");
        return NULL;
    }

    //比如 page 需要 4KB 對齊、cacheline 需要 16B 對齊
    uintptr_t aligned = (startup_alloc_base + align - 1) & ~(align - 1);


    if (aligned + size > startup_alloc_end){
        uart_write_str("\r\n[startup mem] ERROR: Out of memory!");
        return NULL;
    }
    
    startup_alloc_base = aligned + size;

    // uart_write_str("\r\n    Allocating from ");
    // uart_write_hex(aligned);
    // uart_write_str(" to ");
    // uart_write_hex(startup_alloc_base);


    return (void*)aligned;
}

uintptr_t get_startup_current_ptr() {
    return startup_alloc_base;
}
