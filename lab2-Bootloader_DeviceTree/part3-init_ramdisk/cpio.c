#include "../include/cpio.h"
#include "../include/utli.h" 
#include "../include/type.h" 

void print_64(uint64_t *cpio_base, int len) { 
    // 使用 uint8_t* 來 access 單一 char
    uint8_t *byte_ptr = (uint8_t *)cpio_base;
    for (int i = 0; i < len; i++) {
        uart_write_char(byte_ptr[i]);
    }
}
void cpio_ls() {
    uint64_t* cpio_base = CPIO_BASE_RPI; // 你可以根據平台選擇使用 CPIO_BASE_RPI 或 CPIO_BASE_QEMU
    struct cpio_newc_header* header_ptr = (struct cpio_newc_header*)cpio_base;

    while (1) {
        unsigned int r = 50; while(r--) { asm volatile("nop"); }
        // 1. 檢查魔術數是否符合
        if (memcmp(header_ptr->c_magic, CPIO_MAGIC, MAGIC_SIZE) != 0) {
            uart_write_str("\r\nError: Invalid cpio magic");
            uart_write_str("\r\nheader->c_magic =");
            for (int i = 0; i < 6; i++) {
                uart_write_char(header_ptr->c_magic[i]);
            }
            uart_write_str("\r\nCPIO_BASE_QEMU =");
            print_64(CPIO_BASE_QEMU, 6);
            uart_write_str("\r\nCPIO_BASE_RPI =");
            print_64(CPIO_BASE_RPI, 6);
            break;
        }

        // 2. 讀取文件名稱大小
        int filename_size = atoi(header_ptr->c_namesize, 8);
        // uart_write_str("\r\nname_size = ");
        // uart_write_int(filename_size);

        //3.  提取文件名
        char *filename = (char *)header_ptr + sizeof(struct cpio_newc_header);
        if (memcmp(filename, "TRAILER!!!", filename_size) == 0) {
            break;
        }else{
            uart_write_str("\r\n");
            uart_write_str(filename);
        }

        // 4. 計算檔案大小，並捕齊 4 的倍數，header 的移動才會對
        uint32_t file_size = atoi(header_ptr->c_filesize,8);
        // uart_write_str("\r\n file_size1 = ");
        // uart_write_int(file_size);
        file_size =(file_size + 3) & ~3;
            //因為 filename_size 本身不包含 NUL，所以需要 +3，反正後面 &3 時會把多出來的清掉
            //&~3 也就是 and 11111111111111111111111111111100，清除最低的兩位原來對齊

        // 計算 (header sizeS+檔名) 大小，並捕齊 4 的倍數，header 的移動才會對
        uint32_t entry_size = sizeof(struct cpio_newc_header) + filename_size;
        // uart_write_str("\r\n entry_size1 = ");
        // uart_write_int(entry_size);
        entry_size = (entry_size + 3) & ~3;
        
        // 5. 更新 header 指向下一個文件
        header_ptr = (struct cpio_newc_header*)((uint8_t*)header_ptr + file_size + entry_size);
    }
}


int cpio_cat(char *filename) {
    uint64_t* cpio_base = CPIO_BASE_RPI; // 你可以根據平台選擇使用 CPIO_BASE_RPI 或 CPIO_BASE_QEMU
    struct cpio_newc_header* header_ptr = (struct cpio_newc_header*)cpio_base;

    while (1) {
        unsigned int r = 50; while(r--) { asm volatile("nop"); }
        // 1. 檢查魔術數是否符合
        if (memcmp(header_ptr->c_magic, CPIO_MAGIC, MAGIC_SIZE) != 0) {
            uart_write_str("\r\nError: Invalid cpio magic");
            uart_write_str("\r\nheader->c_magic =");
            for (int i = 0; i < 6; i++) {
                uart_write_char(header_ptr->c_magic[i]);
            }
            uart_write_str("\r\nCPIO_BASE_QEMU =");
            print_64(CPIO_BASE_QEMU, 6);
            uart_write_str("\r\nCPIO_BASE_RPI =");
            print_64(CPIO_BASE_RPI, 6);
            break;
        }

        // 2. 讀取文件名稱大小
        int filename_size = atoi(header_ptr->c_namesize, 8);
        int file_size = atoi(header_ptr->c_filesize, 8);
        uint32_t entry_size = sizeof(struct cpio_newc_header) + filename_size;
        entry_size = (entry_size + 3) & ~3;


        //3.  提取文件名
        char *current_filename = (char *)header_ptr + sizeof(struct cpio_newc_header);
        if (memcmp(current_filename, "TRAILER!!!", filename_size) == 0) {
            break;
        }else if (memcmp(current_filename, filename, filename_size) == 0){
            // uint32_t entry_size = (sizeof(struct cpio_newc_header) + filename_size + 3) & ~3;
            uint8_t *file_data = (uint8_t *)header_ptr + entry_size;
            uart_write_str("\r\n***** Contents of file: ");
            uart_write_str(filename);
            uart_write_str("*****\r\n");

            // 顯示檔案內容
            for (int i = 0; i < file_size; i++) {
                uart_write_char(file_data[i]);
            }
            return 1;
        }

        // 計算下個 header 的位置
        file_size =(file_size + 3) & ~3;

        // 更新 header 指向下一個文件
        header_ptr = (struct cpio_newc_header*)((uint8_t*)header_ptr + file_size + entry_size);
    }

    //沒找到檔案就 return 0
    return 0;
}