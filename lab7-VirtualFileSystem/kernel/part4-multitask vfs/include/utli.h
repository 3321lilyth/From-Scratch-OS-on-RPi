#ifndef _UTLI_H
#define _UTLI_H

#include "type.h"

#define size_of(type) ((char*)(&((type *)1)[1]) - (char *)(&((type *)1)[0]))
    //(type*)1 是一個假裝指向 address 1 的指標（不會真的去 dereference）
    //&((type*)1)[1] = 指向「下一個元素」
    //&((type*)1)[0] = 原本的位置，相減就得到 type 的大小（bytes）


//type related
uint32_t fdt32_to_cpu(uint32_t x);
int atoi(char* val_str, int len);

//string related
size_t strlen(const char *str);
int strcmp(char* str1, char* str2);
int strncmp(const char *str1, const char *str2, size_t n);
int strtol(const char *str, char **endptr, int base);
char* strncpy(char* dest, const char* src, size_t n);
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);

//memory related
int memcmp(const void *ptr1, const void *ptr2, size_t num);
void simple_memcpy(void* dest, const void* src, unsigned long size);
void *memset(void *s, int c, size_t n);

//以下是 lab3 需要的
int get_el();
void delay_busy_wait(int r);


#endif