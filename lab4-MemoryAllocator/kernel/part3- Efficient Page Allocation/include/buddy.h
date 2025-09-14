#ifndef _BUDDY_H
#define _BUDDY_H

#include "mini_uart.h"
#include "utli.h"
#include "type.h"


#define PAGE_SIZE       4096
#define TOTAL_PAGES     245760      // 因為 rpi 的 usable addr 為 0x00~0x3C000000， 0x3C000000B/4096B 就是這個數字
#define MAX_ORDER       18          //因為 2^18=262144 是 最接近 245760 的 2 的刺分了。當然多出來的 16384 個 page 要標記為 UNUSABLE 
#define BASE_ADDR       0x00000000

#define MAX_NODES       ((1 << (MAX_ORDER + 1)) - 1)    //total 2^{h+1}-1 個 node

typedef enum {
    UNUSABLE,       // 初始狀態
    RESERVED,       // reserved memory 以及不足 2^18 的那些 invalid pages 都要標記成這個
    USED,
    FREE,
    SPLIT           // 已拆分成更小的子節點。
} state_t;

// void buddy_info();
void buddy_init();
void buddy_free(void *addr);
void *buddy_alloc(size_t size);

//dump functions
void dump_allocated_nodes();
void dump_free_blocks(int order);
void dump_tree_path_to_addr(uintptr_t addr, int target_order) ;

#endif