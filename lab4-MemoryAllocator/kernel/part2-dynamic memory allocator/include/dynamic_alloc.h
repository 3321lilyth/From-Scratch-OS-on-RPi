#ifndef _DYNAMIC_ALLOC_H
#define _DYNAMIC_ALLOC_H


#include "buddy.h"
#include "type.h"

#define POOL_COUNT 5
static const unsigned int pool_sizes[POOL_COUNT] = {16, 32, 48, 96, 1024};
static const unsigned int pool_slots[POOL_COUNT] = {
    4096 / 16,   // 256
    4096 / 32,   // 128
    4096 / 48,   // 85
    4096 / 96,    // 42
    4096 / 1024  // 4
};
#define INITIAL_PAGE_COUNT 1                        // 每個 pool 預設先要一頁
#define MAX_PAGE_COUNT 3*POOL_COUNT                 // 總共限制最多 3 頁
#define MAX_SLOTS_PER_POOL (4096 / 16)              // 最大可能 slot 數
#define BITMAP_BYTES ((MAX_SLOTS_PER_POOL + 7) / 8) //需要 32B bitmap 才可以四種 size 都裝得下

typedef struct chunk_page {
    struct chunk_page *next;
    void *base;                                     // base address of this page
    unsigned char *bitmap;                          // bitmap of used slots
    unsigned int free_count;                        // remaining slots
    int is_initial;                                 // 是否為第一頁
} chunk_page_t;

typedef struct chunk_pool {
    unsigned int slot_size;
    unsigned int slots_per_page;
    chunk_page_t *pages;                            // linked list of pages
} chunk_pool_t;

//global function
void chunk_init();
void *chunk_alloc(unsigned int size);
void chunk_free(void *ptr);
void dump_chunk_pools();

// test case function
void chunk_test1();
void chunk_test2();
void chunk_test3();
#endif