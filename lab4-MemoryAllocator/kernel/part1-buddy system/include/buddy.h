#ifndef _BUDDY_H
#define _BUDDY_H

#include "mini_uart.h"
#include "utli.h"
#include "type.h"
#define PAGE_SIZE       4096
#define MAX_ORDER       6
#define TOTAL_PAGES     (1 << MAX_ORDER)
#define BASE_ADDR       0x10000000

// #define VAL_ALLOCATED   -1
// #define VAL_BUDDY       -2 //表示此 frame 不是連續記憶體空間的開頭 fram，不可以單獨使用

// link list node
typedef struct buddy_block {
    struct buddy_block *next;
    unsigned int index;     // start index
} buddy_block_t;

//每個 page 的 metadata
typedef struct page_metadata {
    // int state;              // -1: allocated, -2: buddy, >=0: free
    int allocated;          // 0 or 1, 1 代表 page allocated
    int start_of_block;     // 0 or 1, 1 代表這個 page 是一個 block 的開頭
    int order;              // 如果 start_of_block=1，不管是 free 或是 allocated，紀錄這個 block 的 order 是多少。
                            // 雖然說 free 的話應該用不到，會直接放在 freelist 正確的位置拉
} page_metadata_t;


void buddy_info();
void buddy_init();
void buddy_free(void *addr);
void *buddy_alloc(size_t size);

void test3();
void test2();
void test1();
#endif