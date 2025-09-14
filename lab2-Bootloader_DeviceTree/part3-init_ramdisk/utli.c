#include "../include/utli.h"


int strcmp(char* str1, char* str2){
    char* p1 = str1;
    char* p2 = str2;

    while (*p1 == *p2){
        if (*p1 == '\0')
            return 1;
        p1++;
        p2++;
    }
    return 0;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num) {
    const unsigned char *p1 = (const unsigned char *)ptr1;  // 將指標轉型為 unsigned char* 來逐字節比較
    const unsigned char *p2 = (const unsigned char *)ptr2;

    // 逐字節比較，直到達到 num 次或者發現不相等的字節
    for (size_t i = 0; i < num; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];  // 返回兩個字節的差異
        }
    }

    return 0;  // 如果所有字節都相等，返回 0
}

int strncmp(const char *str1, const char *str2, size_t n) {
    size_t i = 0;

    // 比較前 n 個字符
    while (i < n) {
        // 若兩個字符不同，則返回差異
        if (str1[i] != str2[i]) {
            return (unsigned char)str1[i] - (unsigned char)str2[i];
        }
        
        // 若達到字串結尾，則返回 0，表示相等
        if (str1[i] == '\0') {
            return 0;
        }

        i++;
    }

    // 如果前 n 個字符都相等，返回 0
    return 0;
}

//str to int, 支援八進、10進、16進
int strtol(const char *str, char **endptr, int base) {
    int result = 0;
    int sign = 1;

    while (*str == ' ' || *str == '\t') {
        str++;
    }

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    if (base == 0) {
        if (*str == '0') {
            str++;
            if (*str == 'x' || *str == 'X') {
                base = 16;
                str++;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    }

    while ((*str >= '0' && *str <= '9') || 
           (*str >= 'a' && *str <= 'f') || 
           (*str >= 'A' && *str <= 'F')) {
        int digit = 0;
        if (*str >= '0' && *str <= '9') {
            digit = *str - '0';
        } else if (*str >= 'a' && *str <= 'f') {
            digit = *str - 'a' + 10;
        } else if (*str >= 'A' && *str <= 'F') {
            digit = *str - 'A' + 10;
        }

        if (digit >= base) {
            break;
        }

        result = result * base + digit;
        str++;
    }

    if (endptr != NULL) {
        *endptr = (char *)str;
    }

    return result * sign;
}


int atoi(char* val_str, int len){

    int val = 0;

    for (int i = 0; i < len && val_str[i] != '\0'; i++){
        int tmp_val = 0;
        if ('0' <= val_str[i] && val_str[i] <= '9'){
            tmp_val = val_str[i] - '0';
        }else if ('A' <= val_str[i] && val_str[i] <= 'F'){
            tmp_val = val_str[i] - 'A' + 10;
        }else if ('a' <= val_str[i] && val_str[i] <= 'f'){
            tmp_val = val_str[i] - 'a' + 10;
        }
        
        val = (val << 4) + tmp_val;
    }    
    
    return val;
}