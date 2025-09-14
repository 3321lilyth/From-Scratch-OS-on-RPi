#include "simple_alloc.h"

#define HEAP_START  (uint64_t)&__heap_start            // 設定 Heap 開始位置
#define HEAP_SIZE   0x100000                    // 設定 Heap 大小 (1MB), 十進位是 1048576
#define HEAP_LIMIT (uint64_t)&__heap_start + HEAP_SIZE

//static 全域變數代表變數只能在這個 .c 檔內可用
//其他 .c 檔案不能存取他
static uint8_t* heap_cur_ptr = (uint8_t*)&__heap_start;    //一開始 ptr 指向
static uint8_t* last_alloc_ptr = NULL;             // 紀錄最後一次分配的記憶體起始位置


void* malloc(size_t size) {

    //必須輸入合法 size
    if (size == 0){
        uart_write_str("\r\nInvalid Size(need positive integer)");
        return NULL;
    }

    // 確保 8-byte 對齊
    if (size % 8 != 0) {
        size += (8 - (size % 8));
    }

    // 檢查是否還有足夠空間
    //這邊轉成 uint8_t* 而不是 uint64_t* 的原因是，uint64_t* 這個 pointer 的加法規則
    //+1 代表移動 8B，而不是我們想要的 1B，這樣 size 會被解釋成 size * 8，導致計算錯誤
    if ((heap_cur_ptr + size) > (uint8_t*)HEAP_LIMIT) {
        uart_write_str("\r\n[ERROR] Out of memory!\r\n");
        return NULL;
    }

    uart_write_str("\nAllocating from ");
    uart_write_hex((uintptr_t)heap_cur_ptr);
    uart_write_str(" to ");
    uart_write_hex((uintptr_t)heap_cur_ptr+size);
    uart_write_str("\r\n");
    
    // 記錄最後一次分配的記憶體起始位址
    last_alloc_ptr = heap_cur_ptr;

    // 分配記憶體
    void *allocated = (void *)heap_cur_ptr;
    heap_cur_ptr += size;  // 移動指標

    return allocated;
}


//釋放最後一次分配的記憶體
void free() {
    if (last_alloc_ptr == NULL) {
        uart_write_str("\r\n[ERROR] No memory to free! (only latest malloc can free))\r\n");
        return;
    }

    uart_write_str("\nFreeing memory at ");
    uart_write_hex((uintptr_t)last_alloc_ptr);

    heap_cur_ptr = last_alloc_ptr;  // 回退到最後一次分配的記憶體
    last_alloc_ptr = NULL;          // 避免多次 free

    uart_write_str("\r\nMemory freed successfully.\r\n");
}


void heap_info() {
    uart_write_str("\r\n[Heap Info]");
    uart_write_str("\r\n    Current Heap Pointer: ");
    uart_write_hex((uintptr_t)heap_cur_ptr);

    uint64_t remaining_size = HEAP_LIMIT - (uintptr_t)heap_cur_ptr;
    uart_write_str("\r\n    Remaining Heap Size: ");
    uart_write_int(remaining_size);
    uart_write_str(" bytes\r\n");
}