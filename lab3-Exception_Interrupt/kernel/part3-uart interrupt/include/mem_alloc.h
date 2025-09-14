#ifndef _MEM_ALLOC_H
#define _MEM_ALLOC_H

#include "type.h"
#include "utli.h"
#include "mini_uart.h"

extern volatile char __heap_start; //讓 heap 從 kernel 的最後面接續著往 high mem 長

void* malloc(size_t);
void free();
void heap_info();

#endif