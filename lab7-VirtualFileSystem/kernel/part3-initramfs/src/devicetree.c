#include "devicetree.h" 


// uint64_t* DEVTREE_ADDRESS  = 0;
// uint64_t *dtb_start_addr;
uintptr_t dtb_start_addr = 0;
uintptr_t dtb_end_addr = 0;




void join_path(char* dest, const char* parent, const char* child) {
    // 如果 parent 是空字串，代表目前是 root 下的直接節點
    if (parent[0] == '\0') {
        dest[0] = '/';
        int i = 0;
        while (child[i] != '\0') {
            dest[i + 1] = child[i];
            i++;
        }
        dest[i + 1] = '\0';
    } else {
        int i = 0;
        while (parent[i] != '\0') {
            dest[i] = parent[i];
            i++;
        }
        dest[i++] = '/';
        int j = 0;
        while (child[j] != '\0') {
            dest[i + j] = child[j];
            j++;
        }
        dest[i + j] = '\0';
    }
}



//fdt:  Flattened Device Tree
//遍歷 Device Tree，並對每個屬性呼叫 callback
void fdt_traverse(void (*callback)(char *, char *, struct fdt_property *, char*) , uint64_t* dtb_ptr) {
    //1. get dtb address
    //助教 hint : You may use x0 register to store and retrieve dtb address
    //通常 Bootloader（如 U-Boot 或 Raspberry Pi Firmware）會將 DTB 地址存入 x0，這是 Linux kernel 的標準約定
    uart_write_str("\r\n[dtb] Parsing DTB...");
    uart_write_str("\r\n    [dtb] kernel received dtb_ptr = ");
    uart_write_hex((uint64_t)dtb_ptr);
    struct fdt_header *fdt = (struct fdt_header *) dtb_ptr;  // Raspberry Pi 預設的 DTB 載入地址在 0x1000000


    //2. check dtb 的開頭是 4B MAGIC
    if (fdt32_to_cpu(fdt->magic) != FDT_HEADER_MAGIC) {
        uart_write_str("\r\n    [dtb] ERROR: Invalid DTB");
        uart_write_str("\r\n          fdt32_to_cpu(fdt->magic=");
        uart_write_hex(fdt32_to_cpu(fdt->magic));
        return;
    }

    //3. 找到 totalsize 並計算 dtb_end_addr，因為 lab4 part4 要從 buddy system reserve 他
    uint32_t total_size = fdt32_to_cpu(fdt->totalsize);
    dtb_end_addr = (uintptr_t)dtb_ptr + total_size;
    uart_write_str("\r\n    [dtb] Total DTB size = ");
    uart_write_hex(total_size);
    uart_write_str("\r\n    [dtb] DTB ends at = ");
    uart_write_hex(dtb_end_addr);


    uint32_t struct_offset = fdt32_to_cpu(fdt->off_dt_struct);
    uint32_t strings_offset = fdt32_to_cpu(fdt->off_dt_strings);

    uint8_t *struct_cur_ptr = (uint8_t *) fdt + struct_offset; //因為要逐 byte 解析，所以要用 char 來定義
    char *string_base = (char *) fdt + strings_offset;
    char* current_node_name = NULL;

    // 以下註解掉的是我想從 dtb 拿到 memory base & size 時失敗的寫法
    // char path_stack[10][128];  // 最多10層node
    // int path_top = -1;
    // char current_node_name[128] = "";
    // char current_path[128] = "";

    while (1) {
        uint32_t token = fdt32_to_cpu(*(uint32_t *) struct_cur_ptr);
        struct_cur_ptr += 4;//這個 4B 是 token 的大小

        //遇到一個新的 node，就讀取 node name 然後移動 ptr
        if (token == FDT_BEGIN_NODE) {
            current_node_name = (char *) struct_cur_ptr;
            struct_cur_ptr += strlen(current_node_name) + 1;
            struct_cur_ptr = (uint8_t *) (((uintptr_t) struct_cur_ptr + 3) & ~3);  // 4B 對齊

            // current_node_name[0] = '\0';
            // int i = 0;
            // while (struct_cur_ptr[i] != '\0') {
            //     current_node_name[i] = struct_cur_ptr[i];
            //     i++;
            // }
            // current_node_name[i] = '\0';
            // struct_cur_ptr += i + 1;
            // struct_cur_ptr = (uint8_t *)(((uintptr_t)struct_cur_ptr + 3) & ~3);  // align

            // path_top++;
            // join_path(path_stack[path_top], (path_top == 0 ? "" : path_stack[path_top - 1]), current_node_name);
            // for (int j = 0; j < 128; j++) current_path[j] = path_stack[path_top][j];  // copy to current_path
        }
        //遇到新的 property
        else if (token == FDT_PROP) {
            struct fdt_property *prop = (struct fdt_property *) struct_cur_ptr;
            uint32_t len = fdt32_to_cpu(prop->len); 
            //要特別注意 root 的 node name 為空，會用 4B 的 0
            char *name = string_base + fdt32_to_cpu(prop->nameoff);
            char *value = (char *) struct_cur_ptr + 8;

            //每找到一個 property 就呼叫 callback function
            // callback(name, value, prop, current_path);
            callback(name, value, prop, current_node_name);

            struct_cur_ptr += 8 + len;
            struct_cur_ptr = (uint8_t *) (((uintptr_t) struct_cur_ptr + 3) & ~3);  // 4-byte 對齊，原因 part3 寫過了
        } 
        
        //end of structure block 結束
        else if (token == FDT_END) {
            uart_write_str("\r\n    [dtb] End of dtb structure");
            break;
        }else if (token == FDT_NOP){
                //struct_cur_ptr 不需要 +4 ，因為我再 if else 外面就加過了
        }else if (token == FDT_END_NODE){
            // path_top--;
        }else {
            //正確解析的話應該不會印到這邊
            uart_write_str("\r\n    [dtb] ERROR: Token not matched");
        }
    }

}

