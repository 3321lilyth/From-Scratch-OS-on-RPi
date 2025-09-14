#include "dynamic_alloc.h"

static chunk_pool_t pools[POOL_COUNT];
static chunk_page_t chunk_pages[MAX_PAGE_COUNT];
static int page_belong_pool[MAX_PAGE_COUNT];        // 紀錄每個 page 是屬於哪個 pool
static unsigned char chunk_bitmaps[MAX_PAGE_COUNT][BITMAP_BYTES];


/// @brief /////////////////////// local function ///////////////////////
void chunk_init() {
    uart_write_str("\r\n[chunk] init");
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








////////////////////////////////// test case function ///////////////////////////////////////
//測試每一種 size 是否都可以正確 allocate
void chunk_test1(){
    uart_write_str("\r\n--------------[TEST 1]mixed size alloc (16/32/48/96), both two----------------------");
    void *m1 = chunk_alloc(16);
    void *m2 = chunk_alloc(32);
    void *m3 = chunk_alloc(48);
    void *m4 = chunk_alloc(96);
    void *m5 = chunk_alloc(16);
    void *m6 = chunk_alloc(32);
    void *m7 = chunk_alloc(48);
    void *m8 = chunk_alloc(96);
    uart_write_str("\r\nm1:");
    uart_write_hex((uintptr_t)m1);
    uart_write_str("  m2:");
    uart_write_hex((uintptr_t)m2);
    uart_write_str("  m3:");
    uart_write_hex((uintptr_t)m3);
    uart_write_str("  m4:");
    uart_write_hex((uintptr_t)m4);
    uart_write_str("\r\nm5:");
    uart_write_hex((uintptr_t)m5);
    uart_write_str("  m6:");
    uart_write_hex((uintptr_t)m6);
    uart_write_str("  m7:");
    uart_write_hex((uintptr_t)m7);
    uart_write_str("  m8:");
    uart_write_hex((uintptr_t)m8);
    chunk_free(m1);
    chunk_free(m2);
    chunk_free(m3);
    chunk_free(m4);
    chunk_free(m5);
    chunk_free(m6);
    chunk_free(m7);
    chunk_free(m8);
}

//需要多找 buddy 要 page 的情況 
void chunk_test2(){
    uart_write_str("\r\n------------------- [TEST 2]allocate 300 x 16 bytes (cross page)-----------------------");
    void *b[300];
    for (int i = 0; i < 300; i++) b[i] = chunk_alloc(16);
    uart_write_str("\r\n1th ptr addr:");
    uart_write_hex((uintptr_t)b[0]);
    uart_write_str(", 256th ptr addr:");
    uart_write_hex((uintptr_t)b[255]);
    uart_write_str("\r\n257th ptr addr:");
    uart_write_hex((uintptr_t)b[256]);
    uart_write_str(", 300th ptr addr:");
    uart_write_hex((uintptr_t)b[299]);
    uart_write_str("\r\nafter allocating 300 x 16 bytes:");
    dump_chunk_pools();
    for (int i = 0; i < 300; i++) chunk_free(b[i]);
}

//reuse after free
void chunk_test3() {
    uart_write_str("\r\n-------------------- [TEST 3]reuse after free-----------------------");

    void *r1 = chunk_alloc(16);
    uart_write_str("\r\n  ptr1:");
    uart_write_hex((uintptr_t)r1);
    void *r2 = chunk_alloc(16);
    uart_write_str("\r\n  ptr2:");
    uart_write_hex((uintptr_t)r2);

    chunk_free(r1);
    
    void *r3 = chunk_alloc(16);
    uart_write_str("\r\n  ptr3(reused):");
    uart_write_hex((uintptr_t)r3);
    chunk_free(r2);
    chunk_free(r3);

}

