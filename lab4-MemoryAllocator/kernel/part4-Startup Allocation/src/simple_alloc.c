#include "simple_alloc.h"


static uintptr_t startup_alloc_base = 0;
static uintptr_t startup_alloc_end  = 0;

extern uintptr_t dtb_start_addr;
extern uintptr_t dtb_end_addr;
extern uint32_t initrd_start_addr;
extern uint32_t initrd_end_addr;

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
    startup_alloc_base = heap_start_addr;
    startup_alloc_end  = mem_base + mem_size;

    //1. alloc buddy system 資料結構
    size_t total_pages = mem_size / PAGE_SIZE;
    int max_order = 0;
    while ((1 << max_order) < total_pages) max_order++;
    BASE_ADDR = mem_base;                   // 0x00
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
    reserve_memory(0x3C000000, 0x40000000-1);                   //invalid memory
    reserve_memory(0x0000, 0x1000-1);                           //spin tables for multicore boot
    reserve_memory(0x59000, get_startup_current_ptr() + (1024*1024) -1 );
        //0x59000 ~ heap_start_addr                                                 -> kernel code and bootloader code
        //heap_start_addr ~ get_startup_current_ptr()                               -> buddy system & dynamic allocator data structure
        //get_startup_current_ptr() ~ get_startup_current_ptr() + (1024*1024) -1    -> kernel heap, 1MB
    reserve_memory(dtb_start_addr, dtb_end_addr);               //dtb
    reserve_memory(initrd_start_addr, initrd_end_addr);         //initramfs
    
     
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

    uart_write_str("\r\n    Allocating from ");
    uart_write_hex(aligned);
    uart_write_str(" to ");
    uart_write_hex(startup_alloc_base);


    return (void*)aligned;
}

uintptr_t get_startup_current_ptr() {
    return startup_alloc_base;
}
