#ifndef _UTLI_H
#define _UTLI_H

#include "type.h"


uint32_t fdt32_to_cpu(uint32_t x);

size_t strlen(const char *str);
int strcmp(char* str1, char* str2);
int strncmp(const char *str1, const char *str2, size_t n);
int strtol(const char *str, char **endptr, int base);
int atoi(char* val_str, int len);
int memcmp(const void *ptr1, const void *ptr2, size_t num);
void *memset(void *s, int c, size_t n);

//以下是 lab3 需要的
int get_el();


#endif