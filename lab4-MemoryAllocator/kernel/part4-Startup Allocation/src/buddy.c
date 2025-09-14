#include "buddy.h"




// PART1~PART3 靜態分配用
// static state_t buddy_tree[MAX_NODES];
// int page_order_map[TOTAL_PAGES];        //紀錄 alloc 出去的 block 的 order 是多少，-1 表示尚未被使用過
                                        // 只會在 free 跟 alloc 用，不影響 log

// ---------------- Utility ----------------
//根據 addr 和 order 計算 page index 範圍（start page index ~ end page index）。
static void print_range(uintptr_t addr, int order) {
    size_t start = (addr - BASE_ADDR) / PAGE_SIZE;
    size_t end = start + (1 << order) - 1;
    uart_write_str(", Range of pages: [");
    uart_write_int(start);
    uart_write_str(", ");
    uart_write_int(end);
    uart_write_str("]");
}

//full binary tree (heap-like tree) 的標準 index 計算方式
int left_child(int index) { return 2 * index + 1; }
int right_child(int index) { return 2 * index + 2; }
int parent(int index) { return (index - 1) / 2; }


//(node index, order) -> 實體記憶體位址 (注意是 node index 不是 page index)
// example : root 的 index = 0, order = MAX_ORDER = 18, relative_index = 0 - ((1 << 0) - 1) = 0, addr = BASE_ADDR + 0 = 0
static uintptr_t node_index_to_addr(int node_index, int order) {
    int first_index = (1 << (MAX_ORDER - order)) - 1;       //這一個 level 、這一個 order 的第一個 index
    // number of blocks per this order: num_blocks = 1 << (MAX_ORDER - order)
    int relative_index = node_index - first_index;          // 這是這個 level 的第幾個 page
    return BASE_ADDR + ((uintptr_t)relative_index << (order + 12));  // 12 = log2(PAGE_SIZE)
}

//  (node index, order)  ->  page index (0 ~ 245759)
static int get_page_index(int node_index, int order) {
    uintptr_t addr = node_index_to_addr(node_index, order);
    return (addr - BASE_ADDR) / PAGE_SIZE;
}


// 實體位址反推回 tree index，因為是 tree 的 list 而不是之前的一對一 link list，所以比較複雜
// 除了 addr 也要指定 target order，，不然比如整棵樹每個 level 最左邊的 node 都是 0x0000 開頭，誰知道你要印到哪一個?
int addr_to_node_index(uintptr_t addr,  int target_order) {
    int index = 0;
    int order = MAX_ORDER;
    while (order >= target_order) {
        uintptr_t block_addr = node_index_to_addr(index, order);
        if (order == target_order && block_addr == addr) {
            return index;
        }

        uintptr_t mid = block_addr + ((PAGE_SIZE << order) / 2);
        if (addr < mid)
            index = left_child(index);
        else
            index = right_child(index);
        order--;
    }
    return -1;
}


int node_index_to_order(int index) {
    for (int order = 0; order <= MAX_ORDER; order++) {
        int level_start = (1 << order) - 1;
        int level_end = (1 << (order + 1)) - 2;
        if (index >= level_start && index <= level_end)
            return MAX_ORDER - order;
    }
    return -1;
}





























////////////////////////////////////// dump function //////////////////////////////////
//印出所有 allocated 的 node index, page index 以及它的 order
void dump_allocated_nodes() {
    uart_write_str("\r\n=== Allocated Nodes ===");
    for (int i = 0; i < MAX_NODES; i++) {
        if (buddy_tree[i] == USED) {
            int order = node_index_to_order(i);
            size_t start_page = get_page_index(i, order);
            size_t end_page = start_page + (1 << order) - 1;

            uart_write_str("\r\n    Node ");
            uart_write_int(i);
            uart_write_str(": Page Index [");
            uart_write_int(start_page);
            uart_write_str(", ");
            uart_write_int(end_page);
            uart_write_str("], Order ");
            uart_write_int(order);
        }
    }
}


//印出某個節點的狀態（給定 address 或 node index）
void dump_node_info_by_index(int node_index) {
    if (node_index < 0 || node_index >= MAX_NODES) return;

    int order = node_index_to_order(node_index);
    uintptr_t addr = node_index_to_addr(node_index, order);
    size_t page_index = (addr - BASE_ADDR) / PAGE_SIZE;

    uart_write_str("\r\n    Found block for addr ");
    uart_write_hex(addr);
    uart_write_str(" => node index ");
    uart_write_int(node_index);
    uart_write_str(", order ");
    uart_write_int(order);
    uart_write_str(", page index ");
    uart_write_int(page_index);
    uart_write_str(", state ");
    switch (buddy_tree[node_index]) {
        case UNUSABLE: uart_write_str("UNUSABLE"); break;
        case RESERVED: uart_write_str("RESERVED"); break;
        case USED:     uart_write_str("USED");     break;
        case FREE:     uart_write_str("FREE");     break;
        case SPLIT:    uart_write_str("SPLIT");    break;
        default:       uart_write_str("UNKNOWN");  break;
    }
}


// 每個 order 可能都有同一個起始 arrd 的 node，比如整棵樹所有 height 最左邊的所有 node 都一定是 0x0 開頭
// 這個 function 印出從 root 開始往下找，第一個 "狀態不是 split" 的該 addr 的 node 
void dump_node_info_by_addr(uintptr_t addr, int target_order) {
    //target_order=-1 代表不指定或不知道 node order，就從 root 往下找第一個 "不是 split" 的人印出來
    if (target_order == -1){
        int index = 0;
        int order = MAX_ORDER;

        while (order >= 0) {
            uintptr_t block_start = node_index_to_addr(index, order);
            uintptr_t block_end = block_start + (PAGE_SIZE << order) - 1;

            if (addr < block_start || addr > block_end) {
                uart_write_str("\r\n[ERROR] Address is not within node range.");
                return;
            }

            if (buddy_tree[index] != SPLIT) {
                // 印出這個 node 的資訊
                uart_write_str("\r\n    Found block for addr ");
                uart_write_hex(addr);
                uart_write_str(" => node index ");
                uart_write_int(index);
                uart_write_str(", order ");
                uart_write_int(order);
                uart_write_str(", page index ");
                uart_write_int(get_page_index(index, order));
                uart_write_str(", state ");
                switch (buddy_tree[index]) {
                    case UNUSABLE: uart_write_str("UNUSABLE"); break;
                    case RESERVED: uart_write_str("RESERVED"); break;
                    case USED:     uart_write_str("USED");     break;
                    case FREE:     uart_write_str("FREE");     break;
                    case SPLIT:    uart_write_str("SPLIT");    break;
                    default:       uart_write_str("UNKNOWN");  break;
                }

                return;
            }

            // 否則繼續往下找
            uintptr_t mid = block_start + ((PAGE_SIZE << order) / 2);
            if (addr < mid) {
                index = left_child(index);
            } else {
                index = right_child(index);
            }
            order--;
        }

        uart_write_str("\r\n[ERROR] Could not find any matching node for addr ");
        uart_write_hex(addr);
    }else{
        int index = addr_to_node_index(addr, target_order);
        if (index >= 0) {
            dump_node_info_by_index(index);
        } else {
            uart_write_str("\r\n[ERROR] Address not matched to any node at order ");
            uart_write_int(target_order);
        }
    }
}


// 打印從 root 到目標 node 的整條 path（包含 state
// 除了 addr 以外也要指定 target_order，不然比如整棵樹每個 level 最左邊的 node 都是 0x0000 開頭，誰知道你要印到哪一個?
void dump_tree_path_to_addr(uintptr_t addr, int target_order) {
    int index = 0;
    int order = MAX_ORDER;

    uart_write_str("\r\nPath to address ");
    uart_write_hex(addr);
    uart_write_str(" at target order ");
    uart_write_int(target_order);
    uart_write_str(":");

    while (order >= 0) {
        uintptr_t node_addr = node_index_to_addr(index, order);
        uart_write_str("\r\n  Node ");
        uart_write_int(index);
        uart_write_str(", Addr: ");
        uart_write_hex(node_addr);
        uart_write_str(", Order ");
        uart_write_int(order);
        uart_write_str(", State: ");
        switch (buddy_tree[index]) {
            case UNUSABLE: uart_write_str("UNUSABLE"); break;
            case RESERVED: uart_write_str("RESERVED"); break;
            case USED:     uart_write_str("USED");     break;
            case FREE:     uart_write_str("FREE");     break;
            case SPLIT:    uart_write_str("SPLIT");    break;
        }


        if (order == target_order) break;
        // if (node_addr == addr) break;

        uintptr_t mid = node_addr + ((PAGE_SIZE << order) / 2);
        if (addr < mid) {
            index = left_child(index);
        } else {
            index = right_child(index);
        }
        order--;
    }
}


//印出指定 order 上的 free block 數量、所有 page index，助教說要的
void dump_free_blocks(int order) {
    if (order > MAX_ORDER){
        uart_write_str("\r\n[ERROR] exceed MAX_ORDER");
        return;
    }

    uart_write_str("\r\n=== Free Blocks at Order ");
    uart_write_int(order);
    uart_write_str(" ===");

    int count = 0;
    int start_index = (1 << (MAX_ORDER - order)) - 1;
    int end_index = start_index + (1 << (MAX_ORDER - order));

    uart_write_str("\r\n    Start Page Index List: [");
    for (int i = start_index; i < end_index && i < MAX_NODES; i++) {
        if (buddy_tree[i] == FREE) {
            if (count > 0) uart_write_str(", ");
            uart_write_int(get_page_index(i, order));
            count++;
        }
        if (count !=0 && count %10 == 0){
            uart_write_str("\r\n    ");
        }
    }
    uart_write_str("]");

    uart_write_str("\r\n    Total Free Blocks: ");
    uart_write_int(count);
}





















// ---------------- Helper ----------------
//判斷傳入 size 是否為 2 的次方
int is_power_of_two(size_t x) {
    return x && !(x & (x - 1));
}

void reserve_recursive(int index, int order, uintptr_t start, uintptr_t end) {
    uintptr_t block_start = node_index_to_addr(index, order);
    uintptr_t block_end = block_start + (PAGE_SIZE << order) - 1;

    // 這個 block 跟我要標記 reserved 的記憶體區塊沒有重疊，直接跳過
    if (end < block_start || start > block_end) return;

    // 如果這個 block 完全被覆蓋，直接標記為 UNUSABLE。
    if (start <= block_start && end >= block_end) {
        buddy_tree[index] = RESERVED;
        uart_write_str("\r\n    [x] Reserve address [0x");
        uart_write_hex(start);
        uart_write_str(", 0x");
        uart_write_hex(end);
        uart_write_str("] at order ");
        uart_write_int(order);
        print_range(block_start, order);
        return;
    }

    if (order == 0) {
        buddy_tree[index] = RESERVED;
        uart_write_str("\r\n    [x] Reserve address [0x");
        uart_write_hex(start);
        uart_write_str(", 0x");
        uart_write_hex(end);
        uart_write_str("] at order ");
        uart_write_int(order);
        print_range(block_start, order);
        return;
    }

    // Mark parent as SPLIT，然後去 reserve 左右 child
    if (buddy_tree[index] == FREE) {
        buddy_tree[index] = SPLIT;
        int left = left_child(index);
        int right = right_child(index);
        if (left < MAX_NODES && buddy_tree[left] == UNUSABLE) buddy_tree[left] = FREE;
        if (right < MAX_NODES && buddy_tree[right] == UNUSABLE) buddy_tree[right] = FREE;
    }
    reserve_recursive(index * 2 + 1, order - 1, start, end);
    reserve_recursive(index * 2 + 2, order - 1, start, end);
}

void reserve_memory(uintptr_t start_addr, uintptr_t end_addr) {
    //傳入 0 代表從 root 開始找，而 root 是 MAX_ORDER
    reserve_recursive(0, MAX_ORDER, start_addr, end_addr);
}


// ---------------- Initialization ----------------

void buddy_init() {
    // 1. 計算 page 數與 order
    // size_t total_pages = mem_size / PAGE_SIZE;
    // int max_order = 0;
    // while ((1 << max_order) < total_pages) max_order++;
    // MAX_ORDER = max_order;
    // TOTAL_PAGES = total_pages;
    // MAX_NODES = (1 << (MAX_ORDER + 1)) - 1;

    //2. 使用 startup allocator 配置 buddy_tree 跟 page_order_map
    // buddy_tree = (state_t*) startup_alloc(sizeof(state_t) * MAX_NODES, 8);
    // page_order_map = (int*) startup_alloc(sizeof(int) * TOTAL_PAGES, 8);

    //3. 初始化
    for (int i = 0; i < MAX_NODES; i++) {
        buddy_tree[i] = UNUSABLE;
    }
    for (int i = 0; i <TOTAL_PAGES; i++){
        page_order_map[i] = -1;
    }
    buddy_tree[0] = FREE; // root
    uart_write_str("\r\n[buddy] Init buddy tree, root set to FREE");
}


// ---------------- Allocation ----------------

int alloc_recursive(int index, int order, int target_order) {
    // uart_write_str("\r\n    cur2 node = ");
    // uart_write_int(index);
    // uart_write_str(", order = ");
    // uart_write_int(order);
    // uart_write_str(", page ");
    // uart_write_int(get_page_index(index, order));
    // uart_write_str(", state = ");
    // switch (buddy_tree[index]) {
    //     case UNUSABLE: uart_write_str("UNUSABLE"); break;
    //     case RESERVED: uart_write_str("RESERVED"); break;
    //     case USED:     uart_write_str("USED");     break;
    //     case FREE:     uart_write_str("FREE");     break;
    //     case SPLIT:    uart_write_str("SPLIT");    break;
    // }

    // 不能分配或已使用，直接失敗
    if (buddy_tree[index] == RESERVED || buddy_tree[index] == UNUSABLE || buddy_tree[index] == USED) return -1;

    // order 不對就要 split(如果尚未 split) 並往左右 child 去找 
    if (order > target_order) {
        //尚未 split
        if (buddy_tree[index] == FREE) {
            buddy_tree[index] = SPLIT;
            int left = left_child(index);
            int right = right_child(index);

            // 加上是否為合法 node index 的判斷
            if (left < MAX_NODES && buddy_tree[left] == UNUSABLE){
                buddy_tree[left] = FREE;
                size_t start_page = get_page_index(left, order - 1);
                size_t end_page = start_page + (1 << (order - 1)) - 1;

                uart_write_str("\r\n    release redundant block at order ");
                uart_write_int(order - 1);
                uart_write_str(", Range of pages: [");
                uart_write_int(start_page);
                uart_write_str(", ");
                uart_write_int(end_page);
                uart_write_str("]");
            }
            
            if (right < MAX_NODES && buddy_tree[right] == UNUSABLE){
                buddy_tree[right] = FREE;
                size_t start_page = get_page_index(right, order - 1);
                size_t end_page = start_page + (1 << (order - 1)) - 1;

                uart_write_str("\r\n    release redundant block at order ");
                uart_write_int(order - 1);
                uart_write_str(", Range of pages: [");
                uart_write_int(start_page);
                uart_write_str(", ");
                uart_write_int(end_page);
                uart_write_str("]");
            }
            
        }

        int left = alloc_recursive(index * 2 + 1, order - 1, target_order);
        if (left >= 0) return left;

        int right = alloc_recursive(index * 2 + 2, order - 1, target_order);
        if (right >= 0) return right;

        return -1;
    }

    // order 正確且可用就分配
    if (order == target_order && buddy_tree[index] == FREE) {
        buddy_tree[index] = USED;
        page_order_map[get_page_index(index, order)] = order;

        uintptr_t addr = node_index_to_addr(index, order);
        uart_write_str("\r\n[buddy] Allocated block at ");
        uart_write_hex(addr);
        print_range(addr, order);
        return index;
    }
    return -1;
}

void *buddy_alloc(size_t size) {
    //1. 根據要 size in byte 轉換為能容納此大小的最小 order
    size_t pages_needed  = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    int order = 0;
    while ((1 << order) < pages_needed) order++;

    if (order > MAX_ORDER){
        uart_write_str("\r\n    [ERROR] buddy alloc: exceed MAX_ORDER :( ");
        return 0;
    }


    uart_write_str("\r\n[buddy] Alloating a block with order ");
    uart_write_int(order);


    //傳入 0 代表從 root 開始找，而 root 是 MAX_ORDER
    int index = alloc_recursive(0, MAX_ORDER, order);
    if (index < 0) {
        uart_write_str("\r\n    [ERROR] buddt alloc: no enough memory space");
        return 0;
    }
    return (void *)node_index_to_addr(index, order);
}

// ---------------- Free ----------------
//遞迴檢查 buddy 是否為 free 然後做 merge 
void update_parent_on_free(int index, int cur_order) {
    //往上檢查到 root (index=0) 為止
    while (index) {
        int parent = (index - 1) / 2;
        int sibling = (index % 2) ? index + 1 : index - 1;
        
        // sibling 還沒釋放就不能合併
        if (buddy_tree[sibling] != FREE) {
            uart_write_str("\r\n    Merging faild, target buddy = ");
            uart_write_int(get_page_index(sibling, cur_order));
            uart_write_str(", stop");
            break;
        }

        //左右都是 free 且 parent 是 split，那就合併
        if (buddy_tree[parent] == SPLIT) {
            buddy_tree[parent] = FREE;

            
            size_t start_page = get_page_index(parent, cur_order+1);
            size_t end_page = start_page + (1 << (cur_order +1)) - 1;
            uart_write_str("\r\n    [+] Free block at order ");
            uart_write_int(cur_order+1);
            uart_write_str(", Range of pages: [");
            uart_write_int(start_page);
            uart_write_str(", ");
            uart_write_int(end_page);
            uart_write_str("]");

            buddy_tree[sibling] = UNUSABLE; // collapsed
            buddy_tree[index] = UNUSABLE;   // collapsed
            index = parent;
        } else {
            break;
        }

        cur_order++;
    }
}


void buddy_free(void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;

    // 查表取得原本分配出去時的 order
    int page_index = (addr - BASE_ADDR) / PAGE_SIZE;
    int order = page_order_map[page_index];
    
    if (ptr == 0){
        uart_write_str("\r\n    [ERROR] buddy free failed: invalid pointer");
        return;
    }

    if (order < 0 || order > MAX_ORDER) {
        uart_write_str("\r\n    [ERROR] buddy free failed: invalid or unknown page order");
        return;
    }

    // 透過 address + order 找到對應的 tree node index
    int node_index = addr_to_node_index(addr, order);
    if (node_index < 0) {
        uart_write_str("\r\n    [ERROR] buddy free failed: address not found in tree");
        return;
    }
    
    uart_write_str("\r\n[buddy] Freeing addr ");
    uart_write_hex(addr);
    uart_write_str(" at order ");
    uart_write_int(order);

    // 標記為 FREE
    buddy_tree[node_index] = FREE;

    // 清除 page_order_map 記錄
    page_order_map[page_index] = -1;

    uart_write_str("\r\n    [+] Free block at order ");
    uart_write_int(order);
    print_range(addr, order);

    // 檢查是否可以合併
    update_parent_on_free(node_index, order);
}