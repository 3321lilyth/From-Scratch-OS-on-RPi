#ifndef _BOOTLOADER_H
#define _BOOTLOADER_H

#include "./mini_uart.h"
#include "./type.h"

#define BOOT_MAGIC        	0x544F4F42  // "BOOT" in hex (0x54='T', 0x4F='O', 0x4F='O', 0x42='B')
#define BOOTLOADER_ADDR  	0x60000
#define KERNEL_LOAD_ADDR  	0x80000


// 定義 Boot Header 結構
typedef struct {
    uint32_t magic;
    uint32_t size;
    uint32_t checksum;
} __attribute__((packed)) BootHeader;
// __attribute__((packed)) 是 GCC（GNU Compiler Collection）和 Clang 編譯器提供的 屬性（attribute）
//用來告訴編譯器 不要對結構體（struct）或聯合體（union）進行記憶體對齊（alignment padding）。
//不要讓它對齊我們在讀取 header 的時候才會對


uint32_t get_kernel_size();
void load_kernel();

#endif