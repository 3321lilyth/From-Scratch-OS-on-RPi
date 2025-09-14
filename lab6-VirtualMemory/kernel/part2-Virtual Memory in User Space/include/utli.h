#ifndef _UTLI_H
#define _UTLI_H

#include "type.h"

//virtual memory 相關
// 注意這邊都回傳整數，要當指標用要自己轉換
#define PA_TO_VA_USER(x)   ((x) | 0xffff000000000000)
#define PA_TO_VA_KERNEL(x) ((uintptr_t)(x) | 0xffff000000000000) //給 kernel 用的，把 PA 轉承 VA 的 function
#define VA_TO_PA_KERNEL(x) ((uintptr_t)(x) & ~0xffff000000000000UL) // 去掉高位 VA offset
    //VA = PA | 0xffff000000000000 這種設計，VA_TO_PA 就是把高位遮掉。
#define PAGE_UP(x) ((((x) + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE) //向上對齊 page size 的倍數


#define size_of(type) ((char*)(&((type *)1)[1]) - (char *)(&((type *)1)[0]))
    //(type*)1 是一個假裝指向 address 1 的指標（不會真的去 dereference）
    //&((type*)1)[1] = 指向「下一個元素」
    //&((type*)1)[0] = 原本的位置，相減就得到 type 的大小（bytes）

uint32_t fdt32_to_cpu(uint32_t x);

size_t strlen(const char *str);
int strcmp(char* str1, char* str2);
int strncmp(const char *str1, const char *str2, size_t n);
int strtol(const char *str, char **endptr, int base);
unsigned long long strtoull(const char *str, char **endptr, int base);
int atoi(char* val_str, int len);
int memcmp(const void *ptr1, const void *ptr2, size_t num);
void simple_memcpy(void* dest, const void* src, unsigned long size);
void *memset(void *s, int c, size_t n);

//以下是 lab3 需要的
int get_el();
void delay_busy_wait(int r);


#endif

