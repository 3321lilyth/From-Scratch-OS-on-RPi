#include "include/devicetree.h" 


uint64_t* DEVTREE_ADDRESS  = 0;


//fdt:  Flattened Device Tree
//遍歷 Device Tree，並對每個屬性呼叫 callback
void fdt_traverse(void (*callback)(char *, char *, struct fdt_property *) , uint64_t* dtb_ptr) {
    //get dtb address
    //助教 hint : You may use x0 register to store and retrieve dtb address
    //通常 Bootloader（如 U-Boot 或 Raspberry Pi Firmware）會將 DTB 地址存入 x0，這是 Linux kernel 的標準約定
    uart_write_str("\r\n------- 1. Parsing DTB   --------- ");
    uart_write_str("\r\n    kernel received dtb_ptr = ");
    uart_write_hex((uint64_t)dtb_ptr);
    struct fdt_header *fdt = (struct fdt_header *) dtb_ptr;  // Raspberry Pi 預設的 DTB 載入地址在 0x1000000

    

    //dtb 的開頭是 4B MAGIC
    if (fdt32_to_cpu(fdt->magic) != FDT_HEADER_MAGIC) {
        uart_write_str("\r\n    Invalid DTB");
        uart_write_str("\r\n    fdt32_to_cpu(fdt->magic=");
        uart_write_hex(fdt32_to_cpu(fdt->magic));
        return;
    }

    uint32_t struct_offset = fdt32_to_cpu(fdt->off_dt_struct);
    uint32_t strings_offset = fdt32_to_cpu(fdt->off_dt_strings);

    uint8_t *struct_cur_ptr = (uint8_t *) fdt + struct_offset; //因為要逐 byte 解析，所以要用 char 來定義
    char *string_base = (char *) fdt + strings_offset;


    while (1) {
        uint32_t token = fdt32_to_cpu(*(uint32_t *) struct_cur_ptr);
        struct_cur_ptr += 4;//這個 4B 是 token 的大小

        //遇到一個新的 node，就讀取 node name 然後移動 ptr
        if (token == FDT_BEGIN_NODE) {
            char *name = (char *) struct_cur_ptr;
            struct_cur_ptr += strlen(name) + 1;
            struct_cur_ptr = (uint8_t *) (((uintptr_t) struct_cur_ptr + 3) & ~3);  // 4B 對齊
        }

        
        //遇到新的 property
        else if (token == FDT_PROP) {
            struct fdt_property *prop = (struct fdt_property *) struct_cur_ptr;
            uint32_t len = fdt32_to_cpu(prop->len); 
            //要特別注意 root 的 node name 為空，會用 4B 的 0
            char *name = string_base + fdt32_to_cpu(prop->nameoff);
            char *value = (char *) struct_cur_ptr + 8;

            //每找到一個 property 就呼叫 callback function
            callback(name, value, prop);

            struct_cur_ptr += 8 + len;
            struct_cur_ptr = (uint8_t *) (((uintptr_t) struct_cur_ptr + 3) & ~3);  // 4-byte 對齊，原因 part3 寫過了
        } 
        
        //end of structure block 結束
        else if (token == FDT_END) {
            uart_write_str("\r\n    End of structure");
            break;
        }else if (token == FDT_NOP || token == FDT_END_NODE){
                //struct_cur_ptr 不需要 +4 ，因為我再 if else 外面就加過了
        }else {
            //正確解析的話應該不會印到這邊
            uart_write_str("\r\n    Token not matched");
        }
    }

}