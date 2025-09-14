#ifndef _TYPE_H
#define _TYPE_H 

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long size_t;
typedef unsigned long long uint64_t;
typedef unsigned long int uintptr_t;

typedef uint64_t *pagetable_t;  // 指向 PGD 的指標
#define NULL ((void*)0)


#endif