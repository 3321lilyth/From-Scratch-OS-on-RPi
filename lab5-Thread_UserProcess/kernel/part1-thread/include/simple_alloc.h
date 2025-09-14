#ifndef _MEM_ALLOC_H
#define _MEM_ALLOC_H

#include "type.h"
#include "utli.h"
#include "mini_uart.h"
#include "buddy.h"
#include "dynamic_alloc.h"

extern volatile char __heap_start;
extern uintptr_t heap_start_addr;

//buddy variables
extern int TOTAL_PAGES;
extern int MAX_ORDER;
extern int MAX_NODES;
extern uintptr_t BASE_ADDR;
extern state_t* buddy_tree;
extern int* page_order_map;

//dynamic variables
extern chunk_pool_t *pools;
extern chunk_page_t *chunk_pages;
extern int *page_belong_pool;
extern unsigned char (*chunk_bitmaps)[BITMAP_BYTES];


void* startup_alloc(size_t size, size_t align);
void startup_alloc_init(uintptr_t base, size_t size);
uintptr_t get_startup_current_ptr();



#endif