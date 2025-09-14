#include "cpio.h" 


//這個是 for part6 把 initial ram disk 改用 devicetree 讀取，而不是寫死位址 (CPIO_BASE_RPI 或 CPIO_BASE_QEMU)
//會由 initramfs_callback() function 賦值
uint64_t* DEVTREE_CPIO_BASE = 0;
uint64_t initrd_start_addr = 0;
uint64_t initrd_end_addr = 0;


void print_64(uint64_t *cpio_base, int len) { 
    // 使用 uint8_t* 來 access 單一 char
    uint8_t *byte_ptr = (uint8_t *)cpio_base;
    for (int i = 0; i < len; i++) {
        uart_write_char_async(byte_ptr[i]);
    }
}
void cpio_ls() {
    uint64_t* cpio_base = CURRENT_PLATFORM_CPIO_BASE; 


    struct cpio_newc_header* header_ptr = (struct cpio_newc_header*)cpio_base;

    while (1) {
        unsigned int r = 50; while(r--) { asm volatile("nop"); }
        // 1. 檢查魔術數是否符合
        if (memcmp(header_ptr->c_magic, CPIO_MAGIC, MAGIC_SIZE) != 0) {
            uart_write_str_raw("\r\nError: Invalid cpio magic");
            uart_write_str_raw("\r\nheader->c_magic =");
            for (int i = 0; i < 6; i++) {
                uart_write_char_async(header_ptr->c_magic[i]);
            }
            break;
        }

        // 2. 讀取文件名稱大小
        int filename_size = atoi(header_ptr->c_namesize, 8);
        // uart_write_str_raw("\r\nname_size = ");
        // uart_write_int_raw(filename_size);

        //3.  提取文件名
        char *filename = (char *)header_ptr + sizeof(struct cpio_newc_header);
        if (memcmp(filename, "TRAILER!!!", filename_size) == 0) {
            break;
        }else{
            uart_write_str_raw("\r\n");
            uart_write_str_raw(filename);
        }

        // 4. 計算檔案大小，並捕齊 4 的倍數，header 的移動才會對
        uint32_t file_size = atoi(header_ptr->c_filesize,8);
        // uart_write_str_raw("\r\n file_size1 = ");
        // uart_write_int_raw(file_size);
        file_size =(file_size + 3) & ~3;
            //因為 filename_size 本身不包含 NUL，所以需要 +3，反正後面 &3 時會把多出來的清掉
            //&~3 也就是 and 11111111111111111111111111111100，清除最低的兩位原來對齊

        // 計算 (header sizeS+檔名) 大小，並捕齊 4 的倍數，header 的移動才會對
        uint32_t entry_size = sizeof(struct cpio_newc_header) + filename_size;
        // uart_write_str_raw("\r\n entry_size1 = ");
        // uart_write_int_raw(entry_size);
        entry_size = (entry_size + 3) & ~3;
        
        // 5. 更新 header 指向下一個文件
        header_ptr = (struct cpio_newc_header*)((uint8_t*)header_ptr + file_size + entry_size);
    }
}


int cpio_cat(char *filename) {
    uint64_t* cpio_base = CURRENT_PLATFORM_CPIO_BASE;
    struct cpio_newc_header* header_ptr = (struct cpio_newc_header*)cpio_base;

    while (1) {
        unsigned int r = 50; while(r--) { asm volatile("nop"); }
        // 1. 檢查魔術數是否符合
        if (memcmp(header_ptr->c_magic, CPIO_MAGIC, MAGIC_SIZE) != 0) {
            uart_write_str_raw("\r\nError: Invalid cpio magic");
            uart_write_str_raw("\r\nheader->c_magic =");
            for (int i = 0; i < 6; i++) {
                uart_write_char_async(header_ptr->c_magic[i]);
            }
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
            uart_write_str_raw("\r\n***** Contents of file: ");
            uart_write_str_raw(filename);
            uart_write_str_raw(" *****\r\n");

            // 顯示檔案內容
            for (int i = 0; i < file_size; i++) {
                uart_write_char_async(file_data[i]);
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


//for 直接 create 一個 user thread 用的，而不是透過 fork 一個 kernel thread 後在呼叫
int cpio_exec_user_program_to_thread(thread_t* t, char* filename) {
    uint64_t* cpio_base = CURRENT_PLATFORM_CPIO_BASE;
    struct cpio_newc_header* header_ptr = (struct cpio_newc_header*)cpio_base;

    while (1) {
        if (memcmp(header_ptr->c_magic, CPIO_MAGIC, MAGIC_SIZE) != 0)
            return -1;

        int filename_size = atoi(header_ptr->c_namesize, 8);
        int file_size = atoi(header_ptr->c_filesize, 8);
        uint32_t entry_size = (sizeof(struct cpio_newc_header) + filename_size + 3) & ~3;

        char* current_filename = (char *)header_ptr + sizeof(struct cpio_newc_header);
        if (memcmp(current_filename, "TRAILER!!!", filename_size) == 0)
            break;
        else if (memcmp(current_filename, filename, filename_size) == 0) {
            //把 code image 複製出來
            void* image = kmalloc(PAGE_UP(file_size));
            memset(image, 0, PAGE_UP(file_size));
            simple_memcpy(image, (uint8_t *)header_ptr + entry_size, file_size);
            
            //然後把複製的地點和大小送去設定 page table，包括分配 user stack
            extern void user_pagetable_setup(thread_t *t, void* img_base, size_t img_size);
            user_pagetable_setup(t, image, file_size);

            // 這是一個一個 page 分配的版本
            // extern void user_pagetable_setup(thread_t *t, void* img_data, size_t img_size);
            // user_pagetable_setup(t, (uint8_t *)header_ptr + entry_size, file_size);
            return 0;
        }

        file_size = (file_size + 3) & ~3;
        header_ptr = (struct cpio_newc_header*)((uint8_t*)header_ptr + file_size + entry_size);
    }
    return -1;
}



//不要再直接 eret 跳到 user mode，而是 建立一個 user process（用 thread + trapframe）。
extern thread_t* get_current();
int cpio_exec_user_program(char *filename) {
    uint64_t* cpio_base = CURRENT_PLATFORM_CPIO_BASE;
    struct cpio_newc_header* header_ptr = (struct cpio_newc_header*)cpio_base;

    while (1) {
        unsigned int r = 50; while(r--) { asm volatile("nop"); }
        
        if (memcmp(header_ptr->c_magic, CPIO_MAGIC, MAGIC_SIZE) != 0) {
            uart_write_str_raw("\r\nError: Invalid cpio magic");
            return -1;
        }

        int filename_size = atoi(header_ptr->c_namesize, 8);
        int file_size = atoi(header_ptr->c_filesize, 8);        //user_program.S 應該是 799(dec, 用 uart_write_int_raw印出來), 31F(hex，用uart_write_hex_raw印的話)
        uint32_t entry_size = sizeof(struct cpio_newc_header) + filename_size;
        entry_size = (entry_size + 3) & ~3;

        char *current_filename = (char *)header_ptr + sizeof(struct cpio_newc_header);
        if (memcmp(current_filename, "TRAILER!!!", filename_size) == 0) {
            break;
        } else if (memcmp(current_filename, filename, filename_size) == 0) {
            thread_t *current = get_current();
            uint64_t *kernel_sp = (uint64_t *)current->kernel_sp;

            // 1. 釋放原本的 user page table
            if (current->user_pagetable) {
                free_pagetable(current->user_pagetable, 0, 0); //傳入 PGD page table, 0 表示 level=0 (PGD)
            }
            uart_write_str_raw("\r\n[debug] ======== after free page table ============");
            uart_write_str_raw("\r\n    current->user_pagetable = ");
            uart_write_hex_raw((uintptr_t)current->user_pagetable);
            
            // 這是一個一個 page 分配的版本
            // extern void user_pagetable_setup(thread_t *t, void* img_data, size_t img_size);
            // user_pagetable_setup(current, (uint8_t *)header_ptr + entry_size, file_size);
            
            
            // 3. 映射新的 text 段到 VA=0x0
            void *image = kmalloc(PAGE_UP(file_size));
            memset(image, 0, PAGE_UP(file_size));
            simple_memcpy(image, (uint8_t *)header_ptr + entry_size, file_size);
            
            //分配 user stack 並設定 page table
            extern void user_pagetable_setup(thread_t *t, void* img_base, size_t img_size);
            user_pagetable_setup(current, image, file_size);
            uart_write_str_raw("\r\n[debug] ======== after user_pagetable_setup ============");
            uart_write_str_raw("\r\n    current->user_pagetable = ");
            uart_write_hex_raw((uintptr_t)current->user_pagetable);


            //3. 重設 trap frame（重用原本的 stack）
            kernel_sp[30] = 0x0;                //因為有了 MMU 後 code base 就是 0x0
            kernel_sp[32] = 0x340;
            kernel_sp[33] = 0x0;                // elr_el1: 程式入口
            kernel_sp[34] = current->user_sp;   // sp_el0，current->user_sp 是在 user_pagetable_setup 裡面設定的
            kernel_sp[35] = 1;                  // return_via_eret

            //4. Reset signal handlers and pending signals
            for (int i = 0; i < SIGNAL_MAX; i++) {
                current->signals.handlers[i] = 0;
                current->signals.pending[i] = 0;
            }
            current->signal_stack_base = 0;
            current->handling_signal = 0;
            
            uart_write_str_raw("\r\nLoaded user program: ");
            uart_write_str_raw(current_filename);
            uart_write_str_raw(", file_size(in dec)=  ");
            uart_write_int_raw(file_size);
            uart_write_str_raw(", tid = ");
            uart_write_int_raw(current->tid);
            uart_write_str_raw(",  user_sp = ");
            uart_write_hex_raw((uintptr_t)current->user_sp);
            trace_va_mapping(current->user_pagetable, 0x0);
            trace_va_mapping(current->user_pagetable, 0xffffffffc000);
            trace_va_mapping(current->user_pagetable, 0xffffffffd000);
            trace_va_mapping(current->user_pagetable, 0xffffffffe000);
            trace_va_mapping(current->user_pagetable, 0xfffffffff000);


            return 0;
        }

        // 計算下個 header 的位置
        file_size = (file_size + 3) & ~3;
        header_ptr = (struct cpio_newc_header*)((uint8_t*)header_ptr + file_size + entry_size);
    }

    uart_write_str_raw("\r\nUser program not found.");
    return -1;  // 找不到檔案
}


//lab2 part6 code
//Use the API to get the address of initramfs instead of hardcoding it. 
//The initramfs address is located in the /chosen/linux,initrd-start node.
    //BTW結尾在 /chosen/linux,initrd-end
//callback 是 fdt_traverse 遍歷到符合條件的 Device Tree 節點時，會執行的函數，所以這邊定義 initramfs_callback 讓他找到  initramfs 時執行
void initramfs_callback(char* prop_name, char* value, struct fdt_property* prop, char* node_name) {

    //沒有檢查 node name，因為一個 property 名稱通常在不同的 node 之間不會重複
    //"linux,initrd-start" 和 "linux,initrd-end" 這兩個 property 通常只會出現在 /chosen 節點，而不會出現在其他地方。
    if (memcmp(node_name, "chosen", 7) == 0){
        if (memcmp(prop_name, "linux,initrd-start", 18) == 0) {
            initrd_start_addr = PA_TO_VA_KERNEL(fdt32_to_cpu(*(uint32_t*)value));
                //bcm2710-rpi-3-b-plus.dtb 裡面的 /chosen/linux,initrd-end 就在 /chosen/linux,initrd-start 的隔壁
                //阿兩個都是 32bit address，所以直接 pointer+4
            
            //因為 dtb 格式規範，即使是 AArch64，dtb 裡面的數值也還是 32bit 所以才用uint32_t
            //但是要在 AArch64 中把 initrd_start 當作 pointer 使用就要用 64bit (uintptr_t)
            //跟 0xffff000000000000UL 做 or 是要把拿到的 PA 轉為 VA，這樣之後用 DEVTREE_CPIO_BASE 去 access 才會正確的 VA->PA
            DEVTREE_CPIO_BASE = (uint64_t*)(uintptr_t)initrd_start_addr; 
                //uintptr_t 才 64bit save，不能直接轉 uint32_t
            uart_write_str_raw("\r\n    [dtb] Found initrd_start = ");
            uart_write_hex_raw(initrd_start_addr);
        }else if (memcmp(prop_name, "linux,initrd-end", 16) == 0){
            initrd_end_addr = PA_TO_VA_KERNEL(fdt32_to_cpu(*(uint32_t*)value)); 
            uart_write_str_raw("\r\n    [dtb] Found initrd_end = ");
            uart_write_hex_raw(initrd_end_addr);
            //Found initramfs, initrd_end = :0x08000200
            //Found initramfs, initrd_start = :0x08000000
        }
    }
    
    return;
}


// 卡，問一下助教這樣為甚麼不行，助教說可以，但是 memory 後面應該要有 @
// int found_memory_node = 0;
// void memory_callback(char* prop_name, char* value, struct fdt_property* prop, char* current_path) {
//     if (strcmp(prop_name, "device_type") == 0 && strcmp(value, "memory") == 0) {
//         if (strcmp(current_path, "/memory") == 0) {
//             found_memory_node = 1;
//         }
//     }
//     // if (strcmp(node_name, "memory") == 0 && strcmp(prop_name, "initial-mapped-area") == 0  && fdt32_to_cpu(prop->len) == 16) {
//     if (found_memory_node && strcmp(current_path, "/memory") == 0 && strcmp(prop_name, "reg") == 0 
//         && fdt32_to_cpu(prop->len) == 16) {             // length is 16 bytes (2 * u32)
//         // uint32_t base = fdt32_to_cpu(((uint32_t*)value)[0]);
//         // uint32_t size = fdt32_to_cpu(((uint32_t*)value)[3]);
//         uint64_t base = ((uint64_t)fdt32_to_cpu(((uint32_t*)value)[0]) << 32)
//                   | fdt32_to_cpu(((uint32_t*)value)[1]);
//         uint64_t size = ((uint64_t)fdt32_to_cpu(((uint32_t*)value)[2]) << 32)
//                   | fdt32_to_cpu(((uint32_t*)value)[3]);
//         mem_base = (uintptr_t)base;
//         mem_size = (uintptr_t)size;
//         uart_write_str_raw("\r\n    [dtb] Found physical memory range: ");
//         uart_write_hex_raw(mem_base);
//         uart_write_str_raw(" ~ ");
//         uart_write_hex_raw(mem_base + mem_size);
//         found_memory_node = 0;
//     }
// }