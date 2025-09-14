#include "dynamic_alloc.h"

//PART1~PART3 靜態宣告
// static chunk_pool_t pools[POOL_COUNT];
// static chunk_page_t chunk_pages[MAX_PAGE_COUNT];
// static int page_belong_pool[MAX_PAGE_COUNT];        // 紀錄每個 page 是屬於哪個 pool
// static unsigned char chunk_bitmaps[MAX_PAGE_COUNT][BITMAP_BYTES];


/// @brief /////////////////////// local function ///////////////////////
void chunk_init() {
    uart_write_str("\r\n[chunk] initial... ");

    // 分配動態記憶體
    // pools = (chunk_pool_t *)startup_alloc(sizeof(chunk_pool_t) * POOL_COUNT, 8);
    // chunk_pages = (chunk_page_t *)startup_alloc(sizeof(chunk_page_t) * MAX_PAGE_COUNT, 8);
    // page_belong_pool = (int *)startup_alloc(sizeof(int) * MAX_PAGE_COUNT, 8);
    // chunk_bitmaps = (unsigned char (*)[BITMAP_BYTES])startup_alloc(sizeof(unsigned char) * MAX_PAGE_COUNT * BITMAP_BYTES, 8);
    
    
    // reserve_memory(heap_start_addr, get_startup_current_ptr()); //reserve buddy system & dynamic allocator data structure
    // reserve_memory(get_startup_current_ptr(), get_startup_current_ptr() + (1024*1024) -1 ); //reserve kernel heap, 1MB

    for (int i = 0; i < MAX_PAGE_COUNT; i++) {
        chunk_pages[i].base = 0;
    }


    // 預設每個 pool 配一頁
    for (int i = 0; i < POOL_COUNT; i++) {  // i 同時是 pool index 和 chunk_page index
        pools[i].slot_size = pool_sizes[i];
        pools[i].slots_per_page = pool_slots[i];
        pools[i].pages = 0;

        if (chunk_pages[i].base == 0) {
            void *page_addr = buddy_alloc(4096);
            if (!page_addr){
                uart_write_str("\r\n[ERROR] chunk init: buddy_alloc failed");
                break;
            }


            chunk_page_t *pg = &chunk_pages[i];
            pg->base = page_addr;
            pg->bitmap = chunk_bitmaps[i];
            pg->free_count = pool_slots[i];
            pg->next = 0;
            pg->is_initial = 1;
            for (int b = 0; b < BITMAP_BYTES; b++) pg->bitmap[b] = 0;

            pools[i].pages = pg;
            page_belong_pool[i] = i;
        }
    }
}

static int find_pool_index(unsigned int size) {
    for (int i = 0; i < POOL_COUNT; i++) {
        if (size <= pool_sizes[i]) return i;
    }
    return -1;
}








////////////////////////////////// global function ///////////////////////////////////////
void *chunk_alloc(unsigned int size) {
    //1. 找出 size 對應的 pool index
    int pool_id = find_pool_index(size);
    if (pool_id == -1) {
        uart_write_str("\r\n[ERROR] chunk alloc: invalid size, please use buddy system");
        return 0;
    }
    chunk_pool_t *pool = &pools[pool_id];
    chunk_page_t *pg = pool->pages;

    //2.  在該 pool 中現有的 pages 中找一個 空的 slot，然後分配給使用者
    while (pg) {
        // 快速跳過滿的 page
        if (pg->free_count == 0) {
            pg = pg->next;
            continue;
        }

        for (int i = 0; i < pool->slots_per_page; i++) {
            if (!(pg->bitmap[i / 8] & (1 << (i % 8)))) {    
                //pg->bitmap[i / 8]: 找到第幾個 byte
                //(1 << (i % 8)): 產生一個對應位置的 mask bit
                //如果結果是 0 ， 表示這個 slot 是 空的，可以直接分配給使用者
                pg->bitmap[i / 8] |= (1 << (i % 8));
                pg->free_count--;
                uart_write_str("\r\n[chunk] allocated, addr=");
                uart_write_hex((uintptr_t)pg->base + i * pool->slot_size);
                uart_write_str(", size=");
                uart_write_int(pool->slot_size);
                return (void *)((uintptr_t)pg->base + i * pool->slot_size);
            }
        }
        pg = pg->next;
    }

    
    //3.所有 page 都沒有空 slot (且未超過最大 page 數) 就去跟 buddy 要一個 page
    for (int i = 0; i < MAX_PAGE_COUNT; i++) {
        //挑一個空的 chunk_pages 欄位來用
        if (chunk_pages[i].base == 0) {
            void *page_addr = buddy_alloc(4096);
            if (!page_addr){
                uart_write_str("\r\n[ERROR] chunk alloc: buddy_alloc failed");
                return 0;
            }
            
            chunk_pages[i].base = page_addr;
            chunk_pages[i].bitmap = chunk_bitmaps[i];
            chunk_pages[i].free_count = pool->slots_per_page;
            chunk_pages[i].is_initial = 0;
            for (int b = 0; b < BITMAP_BYTES; b++) chunk_pages[i].bitmap[b] = 0;
            
            chunk_pages[i].next = pool->pages;
            pool->pages = &chunk_pages[i];
            page_belong_pool[i] = pool_id;
            
            chunk_pages[i].bitmap[0] |= 1;
            chunk_pages[i].free_count--;
            uart_write_str("\r\n[chunk] allocated, addr=");
            uart_write_hex((uintptr_t)chunk_pages[i].base);
            uart_write_str(", size=");
            uart_write_int(pool->slot_size);
            return (void *)((uintptr_t)chunk_pages[i].base);
        }
    }
    
    uart_write_str("\r\n[ERROR] chunk alloc: dynamic allocator full (MAX_PAGE_COUNT)");
    return 0;
}

void chunk_free(void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t base = addr & ~(PAGE_SIZE - 1);
        //PAGE_SIZE = 4096 = 0x1000
        //~(PAGE_SIZE - 1) = ~0xFFF = 0xFFFFF000
        //比如 addr = 0x10001234，則 base address 為 addr & 0xFFFFF000 = 0x10001000

        
        for (int i = 0; i < MAX_PAGE_COUNT; i++) {
            if ((uintptr_t)chunk_pages[i].base == base) {
                chunk_page_t *pg = &chunk_pages[i];
                chunk_pool_t *pool = &pools[page_belong_pool[i]];
                int offset = addr - base;
                int slot_idx = offset / pool->slot_size;                // 求出使用者歸還的是哪一個 slotㄅ
                pg->bitmap[slot_idx / 8] &= ~(1 << (slot_idx % 8));     // 將 bitmap 中對應的 bit 清掉
                pg->free_count++;
                
                uart_write_str("\r\n[chunk] freeing, addr=");
                uart_write_hex(addr);
                uart_write_str(", size=");
                uart_write_int(pool->slot_size);

            // 從 pool list 中移除
            if (pg->free_count == pool->slots_per_page && !pg->is_initial) {
                chunk_page_t **prev = &pool->pages;
                while (*prev && *prev != pg) prev = &(*prev)->next;
                if (*prev) *prev = pg->next;

                buddy_free(pg->base);
                pg->base = 0;                                   // mark this slot reusable
            }
            return;
        }
    }
}

// dump 出四個 pool 所有的 page 的資訊
void dump_chunk_pools() {
    uart_write_str("\r\n[chunk] Chunk Pools:");

    for (int i = 0; i < POOL_COUNT; i++) {
        uart_write_str("\r\n    pool");
        uart_write_int(i);
        uart_write_str(" (size: ");
        uart_write_int(pool_sizes[i]);
        uart_write_str("B): ");

        chunk_page_t *pg = pools[i].pages;
        while (pg) {
            uart_write_hex((uintptr_t)pg->base);
            // uintptr_t addr = (uintptr_t)pg->base;
            // char hex[sizeof(uintptr_t) * 2 + 1];
            // for (int j = (sizeof(uintptr_t) * 2) - 1; j >= 0; j--) {
            //     hex[j] = "0123456789ABCDEF"[addr & 0xF];
            //     addr >>= 4;
            // }
            // hex[sizeof(uintptr_t) * 2] = '\0';
            // uart_write_str(hex);
            
            if (pg->is_initial)
                uart_write_str(" (init)");
                
            uart_write_str(" -> ");
            pg = pg->next;
        }
        uart_write_str("NULL");
    }
    uart_write_str("\r\n");
}

chunk_page_t *get_chunk_pages(void) {
    return chunk_pages;
}

int get_chunk_page_count(void) {
    return MAX_PAGE_COUNT;
}