#ifndef _CPIO_H
#define _CPIO_H

#include "mini_uart.h" 
#include "utli.h" 
#include "type.h"
#include "devicetree.h"
#include "exception.h"
#include "thread.h"
#include "file_system/vfs.h"

#define CPIO_END "TRAILER!!!"
#define CPIO_MAGIC "070701"
    //"070701" //find . | cpio -o --format=newc > initramfs.cpio
#define FIELD_SIZE 8
#define MAGIC_SIZE 6

#define CURRENT_PLATFORM_CPIO_BASE DEVTREE_CPIO_BASE
    // 你可以根據平台選擇使用 CPIO_BASE_RPI 或 CPIO_BASE_QEMU 或 DEVTREE_CPIO_BASE


//這兩個是 for part3 寫 initial ram disk 的時候的定義
//這邊要注意我原本用 uint8_t 的話是不能順利被轉為 cpio_newc_header* 的，因為他不會幫你 4B 對齊，所以一定要用 32
#define CPIO_BASE_RPI (uint64_t*)(0x20000000)
#define CPIO_BASE_QEMU (uint64_t*)(0x8000000)


//cpio 架構: header1 -> filename1 -> file1 
    //-> header2 -> filename2 -> file2
    //-> ... 而且全部都要對齊4B
// 110 B
struct cpio_newc_header {
    char c_magic[6];        // The string 070701 for new ASCII
    char c_ino[8];          // inode number (unused in this format)
    char c_mode[8];         // file type and permissions (octal)
    char c_uid[8];          // user ID (owner)
    char c_gid[8];          // group ID (owner)
    char c_nlink[8];        // number of links (not used in this format)
    char c_mtime[8];        // last modification time (octal)
    char c_filesize[8];     // file size (octal, must be 0 for directories or FIFOs)
    char c_devmajor[8];     // major device number (unused in this format)
    char c_devminor[8];     // minor device number (unused in this format)
    char c_rdevmajor[8];    // major device number for special files (unused)
    char c_rdevminor[8];    // minor device number for special files (unused)
    char c_namesize[8];     // name size including terminating NUL byte (octal) **!!!
    char c_check[8];        // checksum for the header (unused in "newc" format)
                            // 0 for "new" portable format; for CRC format the sum of all the bytes in the file
};

extern uint32_t initrd_start_addr;
extern uint32_t initrd_end_addr;


void cpio_ls();
int cpio_cat(char *filename);
int cpio_exec_user_program(char *filename);
int cpio_exec_user_program_lab7(const char *filename) ;
void initramfs_callback(char* prop_name, char* value, struct fdt_property* prop, char* node_name); 
// void memory_callback(char* prop_name, char* value, struct fdt_property* prop, char* node_name); 
#endif