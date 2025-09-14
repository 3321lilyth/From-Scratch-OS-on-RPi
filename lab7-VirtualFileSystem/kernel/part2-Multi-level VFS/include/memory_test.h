#ifndef _MEMORY_TEST_H
#define _MEMORY_TEST_H

#include "buddy.h"
#include "dynamic_alloc.h"


void buddy_test1 ();
void buddy_test2 ();
void buddy_test3 ();
void buddy_test4 ();

void chunk_test1();
void chunk_test2();
void chunk_test3();


void kfree(uintptr_t addr);
void *kmalloc(size_t size);
void mix_test1();
void mix_test2();
#endif