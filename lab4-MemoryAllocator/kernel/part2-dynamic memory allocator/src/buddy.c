#include "buddy.h"

static page_metadata_t frame_array[TOTAL_PAGES];                // 每個 page 的 metadata
static buddy_block_t *free_list[MAX_ORDER + 1];     // 各種大小的 freelist。。因為 還有 2^0 這個 list 所以要 +1。會自動初始化為 NULL（全 0）
static buddy_block_t block_pool[TOTAL_PAGES];       // 每頁一個 block 結構




/// @brief /////////////////////// local function ///////////////////////
void dump_frame_array() {
    uart_write_str("\r\n=== Frame Array Status ===\r\n");
    for (int i = 0; i < TOTAL_PAGES; i++) {
        if (frame_array[i].start_of_block == 1){
            uart_write_str("[");
            uart_write_int(i);
            uart_write_str("] alloc=");
            uart_write_int(frame_array[i].allocated);
            uart_write_str(", start=");
            uart_write_int(frame_array[i].start_of_block);
            uart_write_str(", order=");
            uart_write_int(frame_array[i].order);
            uart_write_str("\r\n");
        }
    }
    uart_write_str("\r\n");
}

void dump_freelist() {
    uart_write_str("\r\n=== Free List Status ===\r\n");
    for (int order = 0; order <= MAX_ORDER; order++) {
        uart_write_str("Order ");
        uart_write_int(order);
        uart_write_str(": ");
        buddy_block_t *blk = free_list[order];
        while (blk) {
            uart_write_int(blk->index);
            uart_write_str(" -> ");
            blk = blk->next;
        }
        uart_write_str("NULL\r\n");
    }
    uart_write_str("\r\n");
}

void buddy_info(){
    dump_frame_array();
    dump_freelist();
}

void buddy_init() {
    uart_write_str("\r\n[buddy] init");
    uart_write_str("\r\n    [x]Reserving pages from 0x10000000 to 0x10040000");
    for (int i = 0; i < TOTAL_PAGES; i++) {
        frame_array[i].allocated = 0;
        frame_array[i].start_of_block = 0;
        frame_array[i].order = -1;
    }


    // Initial free block is whole memory
    buddy_block_t *blk = &block_pool[0];
    blk->next = 0;
    blk->index = 0;
    free_list[MAX_ORDER] = blk;
    frame_array[0].start_of_block = 1;
    frame_array[0].order = MAX_ORDER;

    uart_write_str("\r\n    [+]Adding to free list (order 6): index 0");
}

// 找到某個 block(block 可以用 index, order 表示) 的 buddy，也就是「和你一樣大、相鄰、可以合併的另一個區塊」
// Buddy pair 會相差 2^order，比如 block size = 8 (order 3)，那麼 buddy pair 就是：(0,8) (16, 24) (32, 40)
                                            // (0b0000, 0b1000) (0b10000, 0b11000) (0b100000, 0b110000)
// 可看出 pair 中，兩個 index 差別在第 order bit 上一個是 0 一個是 1 -> 一個在 index，另一個在 index ^ (1 << order)
// 因為 XOR 特性是 "兩邊只有一個1時結果才為1"，當右邊固定為 1 (1 << order)時，結果就會跟左邊數字的 0、1 相反 (0^1=1, 1^1=0)
// 例如 index = 4、order = 1，則 0b100 ^ 0x10 = 6 (0b110)
static unsigned int get_buddy_index(unsigned int index, int order) {
    return index ^ (1 << order);
}

//將 block 加入對應 order 的 freelist。
static void add_to_free_list(unsigned int index, int order) {
    buddy_block_t *blk = &block_pool[index];
    blk->index = index;
    blk->next = free_list[order];
    free_list[order] = blk;

    for (int i = 0; i < (1 << order); i++) {
        frame_array[index + i].allocated = 0;
        frame_array[index + i].start_of_block = 0;
        frame_array[index + i].order = -1;
    }
    frame_array[index].start_of_block = 1;
    frame_array[index].order = order;
    
    uart_write_str("\r\n    [+]Adding to free list (order ");
    uart_write_int(order);
    uart_write_str("): index ");
    uart_write_int(index);
}

//從 freelist 拿出第一個可用的 block，沒有的話就回傳 NULL
static buddy_block_t *remove_from_free_list(int order) {
    buddy_block_t *blk = free_list[order];
    if (blk) {
        free_list[order] = blk->next;
        uart_write_str("\r\n    [-]Removing from free list (order ");
        uart_write_int(order);
        uart_write_str("): index ");
        uart_write_int(blk->index);
    }
    return blk;
}












////////////////////////////////// global function ///////////////////////////////////////
void *buddy_alloc(size_t size_in_bytes) {
    //找出最少需要 2^order 個 page 才能滿足這個需求
    size_t pages_needed = (size_in_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    int order = 0;
    while ((1 << order) < pages_needed && order <= MAX_ORDER) order++;

    uart_write_str("\r\n[buddy] try to allocating order=");
    uart_write_int(order);

    // 從小的 order 先開始找，沒空間了再去拆大一點的 order
    for (int curr = order; curr <= MAX_ORDER; curr++) {
        if (free_list[curr]) {
            buddy_block_t *blk = remove_from_free_list(curr);
            size_t index = blk->index;
            
            // Split into smaller blocks
            while (curr > order) {
                curr--;
                size_t buddy_index = index + (1 << curr);
                    //因為大 block 從 index 開始，你要把他切一半，那另一半的起始 index 就一定會是 index + 一半的 block size一半的 block size
                    //所以 curr order 先 -1，然後取2^curr 就是這個 block 的大小，然後加上 index 就是另一半的起始 index
                uart_write_str("\r\n    Release redundant block to free list: index ");
                uart_write_int(buddy_index);
                add_to_free_list(buddy_index, curr);
            }

            for (int i = 0; i < (1 << order); i++) {
                frame_array[index + i].allocated = 1;
                frame_array[index + i].start_of_block = 0;
                frame_array[index + i].order = -1;
            }
            frame_array[index].start_of_block = 1;
            frame_array[index].order = order;
            uart_write_str("\r\n    allocated, start from page index=");
            uart_write_int(index);
            uart_write_str(", addr=");
            uart_write_hex((uintptr_t)BASE_ADDR + index * PAGE_SIZE);
            return (void *)((uintptr_t)BASE_ADDR + index * PAGE_SIZE);
        }
    }

    uart_write_str("\r\n    failed: no available block");
    return 0;
}


// ---------- Free ----------
//使用者釋放一塊記憶體，傳入的 addr 是實際的記憶體地址。
void buddy_free(void *addr) {
    size_t index = ((uintptr_t )addr - BASE_ADDR) / PAGE_SIZE;      // address 轉成 page index
    int order = frame_array[index].order;                   // 這個 block 當初分配的 order 是多少   
    
    uart_write_str("\r\n[buddy] freeing block start from index ");
    uart_write_int(index);

    for (int i = 0; i < (1 << order); i++) {
        frame_array[index + i].allocated = 0;
        frame_array[index + i].start_of_block = 0;
        frame_array[index + i].order = -1;
    }
    
    // while 找一下最大可以合併到哪裡
    while (order < MAX_ORDER) {
        size_t buddy = get_buddy_index(index, order);                 //找出這個 block 在當前 order 下的 buddy
        if (buddy >= TOTAL_PAGES) break;

        page_metadata_t *buddy_meta = &frame_array[buddy];
        if (buddy_meta->allocated || !buddy_meta->start_of_block || buddy_meta->order != order){
            uart_write_str("\r\n    Merging faild, target buddy = ");
            uart_write_int(buddy);
            break;
        }

        // Merge log
        uart_write_str("\r\n    Merging index ");
        uart_write_int(index);
        uart_write_str(" with buddy ");
        uart_write_int(buddy);
        uart_write_str(" at order ");
        uart_write_int(order);

        // Remove buddy from free list
        //這裡不用 remove_from_free_list() 是因為我們已知 index，而不是單純要拿取第一個
        buddy_block_t **prev = &free_list[order];
        while (*prev && (*prev)->index != buddy) {
            prev = &(*prev)->next;
        }
        if (*prev) *prev = (*prev)->next;

        index = (index < buddy) ? index : buddy;            //合併後的 block index 應該選擇小的作為起始 index
        order++;
    }

    //無法再合併時，把這塊 block（大小為 2^order）加回對應的 freelist
    add_to_free_list(index, order); 
}

//助教PPT 裡面的案例
void buddy_test1 (){
    // uart_write_str("\r\n------------------------  init ----------------------------------");
    // buddy_init();
    // dump_frame_array();
    // dump_freelist();

    uart_write_str("\r\n--------------------------- alloc a  -------------------------------");
    void *a = buddy_alloc(1*PAGE_SIZE); // alloc 1 pages
    dump_frame_array();
    dump_freelist();

    uart_write_str("\r\n---------------------------  alloc b -------------------------------");
    void *b = buddy_alloc(2*PAGE_SIZE); // alloc 2 pages
    dump_frame_array();
    dump_freelist();

    uart_write_str("\r\n---------------------------  alloc c -------------------------------");
    void *c = buddy_alloc(8*PAGE_SIZE); // alloc 8 pages
    dump_frame_array();
    dump_freelist();



    uart_write_str("\r\n---------------------------  free b -------------------------------");
    buddy_free(b);
    dump_frame_array();
    dump_freelist();
    
    uart_write_str("\r\n---------------------------  free a -------------------------------");
    buddy_free(a);
    dump_frame_array();
    dump_freelist();
    
    uart_write_str("\r\n---------------------------  free c -------------------------------");
    buddy_free(c);
    dump_frame_array();
    dump_freelist();
}


void buddy_test2(){
    // buddy_init();

    void *a = buddy_alloc(4*PAGE_SIZE); // alloc 4 pages
    void *b = buddy_alloc(4*PAGE_SIZE); // alloc 4 pages
    void *c = buddy_alloc(8*PAGE_SIZE); // alloc 8 pages
    dump_frame_array();
    dump_freelist();

    buddy_free(b);
    buddy_free(a);
    buddy_free(c);

    dump_frame_array();
    dump_freelist();

}

//測試不正常 size 大小
void buddy_test3(){
    // buddy_init();
    
    uart_write_str("\r\n--------------------------- alloc 1*PAGE_SIZE/2  and 3*PAGE_SIZE -------------------------------");
    void *a = buddy_alloc(1*PAGE_SIZE/2);
    void *b = buddy_alloc(3*PAGE_SIZE);
    dump_frame_array();
    dump_freelist();

    uart_write_str("\r\n--------------------------- free -------------------------------");
    buddy_free(b);
    buddy_free(a);
    dump_frame_array();
    dump_freelist();
    
    uart_write_str("\r\n--------------------------- alloc 64*PAGE_SIZE -------------------------------");
    void *c = buddy_alloc(64*PAGE_SIZE); // alloc 64 pages
    dump_frame_array();
    dump_freelist();
    
    uart_write_str("\r\n--------------------------- free -------------------------------");
    buddy_free(c);
    dump_frame_array();
    dump_freelist();
}
// for local test
// int main() {
//     test2();

//     return 0;
// }