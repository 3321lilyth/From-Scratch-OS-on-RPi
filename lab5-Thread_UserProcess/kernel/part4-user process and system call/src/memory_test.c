#include "memory_test.h"


void kfree(uintptr_t addr) {
    // uintptr_t addr = (uintptr_t)ptr;
    chunk_page_t *pages = get_chunk_pages();
    int page_count = get_chunk_page_count();

    for (int i = 0; i < page_count; ++i) {
        if (pages[i].base == 0) continue;  // 未使用的空 slot 在 dynamic system 中會被初始化為 0
        uintptr_t base = (uintptr_t)pages[i].base;
        uintptr_t end = base + PAGE_SIZE;
        if (addr >= base && addr < end) {
            chunk_free((void *)addr);
            return;
        }
    }

    // 如果不在任何 chunk_page 內，那就是 buddy 分配的
    buddy_free((void *)addr);
}

void *kmalloc(size_t size) {
    void *addr = 0;
    if (size > 1024) {
        addr = buddy_alloc(size);
        if (addr) {
            uart_write_str("\r\n[main] Buddy Allocated at: ");
            uart_write_hex((uintptr_t)addr);
        } else {
            uart_write_str("\r\n[main] Buddy Allocation Failed");
        }
    } else {
        addr = chunk_alloc(size);
        if (addr) {
            uart_write_str("\r\n[main] Dynamic Allocated at: ");
            uart_write_hex((uintptr_t)addr);
        } else {
            uart_write_str("\r\n[main] Dynamic Allocation Failed");
        }
    }
    return addr;
}

////////////////////////////////// buddy system test case function ///////////////////////////////////////
//助教PPT 裡面的案例，沒啥用，因為這邊有 reserved memory page 0，所以要改用 test2 喔
void buddy_test1 (){
    uart_write_str("\r\n------------------ before test start-----------------------");
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_free_blocks(3);


    uart_write_str("\r\n--------------------------- alloc a  -------------------------------");
    void *a = buddy_alloc(1*PAGE_SIZE); // 0x1000~0x2000
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_free_blocks(3);
    // dump_tree_path_to_addr((uintptr_t)a, 0);

    uart_write_str("\r\n---------------------------  alloc b -------------------------------");
    void *b = buddy_alloc(2*PAGE_SIZE); // 0x2000~0x4000
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_free_blocks(3);
    // dump_tree_path_to_addr((uintptr_t)b, 0);

    uart_write_str("\r\n---------------------------  alloc c -------------------------------");
    void *c = buddy_alloc(8*PAGE_SIZE); // 0x8000~0x16000
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_free_blocks(3);
    // dump_tree_path_to_addr((uintptr_t)c, 0);



    uart_write_str("\r\n---------------------------  free b -------------------------------");
    buddy_free(b);
    dump_allocated_nodes();
    // dump_tree_path_to_addr((uintptr_t)c, 0);
    
    uart_write_str("\r\n---------------------------  free a -------------------------------");
    buddy_free(a);
    
    uart_write_str("\r\n---------------------------  free c -------------------------------");
    buddy_free(c);
}

// 放大到整個記憶體空間 + reserve memory 之下我寫的案例，測試 merge。可以看 notion 筆記
//https://www.notion.so/lab4-memory-allocator-1c8c95fbbc3a805daf20d5aae1484c7d?pvs=4#1d2c95fbbc3a80dc9604fdaeb6ab30aa
void buddy_test2(){
    uart_write_str("\r\n------------------ before test start-----------------------");
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);

    uart_write_str("\r\n------------------ alloc 1page -> 2page -----------------------");
    void *a = buddy_alloc(1*PAGE_SIZE); //0x1000~0x2000
    void *b = buddy_alloc(2*PAGE_SIZE); //0x2000~0x4000
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);


    uart_write_str("\r\n------------------ alloc 1page -> 1page -> 2page-----------------------");
    void *c = buddy_alloc(1*PAGE_SIZE); //0x4000~0x5000
    void *d = buddy_alloc(1*PAGE_SIZE); //0x5000~0x6000
    void *e = buddy_alloc(2*PAGE_SIZE); //0x6000~0x8000
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    // dump_tree_path_to_addr((uintptr_t)a, 0);

    uart_write_str("\r\n---------- free 0x5000->0x6000->0x4000 to test merge ------------------");
    buddy_free(d);
    buddy_free(e);
    buddy_free(c);

    dump_allocated_nodes();

    uart_write_str("\r\n------------------ free other -----------------------");
    buddy_free(b);
    buddy_free(a);

    dump_allocated_nodes();

}

//測試不正常 size 大小
void buddy_test3(){
    uart_write_str("\r\n--------------------------- alloc 1*PAGE_SIZE/2  and 3*PAGE_SIZE -------------------------------");
    void *a = kmalloc(1*PAGE_SIZE/2);
    void *b = kmalloc(3*PAGE_SIZE);
    dump_allocated_nodes();

    uart_write_str("\r\n--------------------------- alloc 2,147,483,647 -------------------------------");
    void *c = kmalloc(2147483647);
    dump_allocated_nodes();
    kfree((uintptr_t)c);

    uart_write_str("\r\n--------------------------- free -------------------------------");
    kfree((uintptr_t)b);
    kfree((uintptr_t)a);
    dump_allocated_nodes();

    uart_write_str("\r\n--------------------------- alloc 64*PAGE_SIZE -------------------------------");
    void *d = kmalloc(64*PAGE_SIZE); // alloc 64 pages
    dump_allocated_nodes();
    
    uart_write_str("\r\n--------------------------- free -------------------------------");
    kfree((uintptr_t)d);
    dump_allocated_nodes();
}




void buddy_test4(){
    uart_write_str("\r\n------------------ before test start-----------------------");
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_free_blocks(3);


    uart_write_str("\r\n--------------------------- alloc a  -------------------------------");
    void *a = buddy_alloc(1*PAGE_SIZE); // 0x1000~0x2000
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_free_blocks(3);
    // dump_tree_path_to_addr((uintptr_t)a, 0);

    uart_write_str("\r\n---------------------------  free a -------------------------------");
    buddy_free(a);
    dump_allocated_nodes();
    

    uart_write_str("\r\n---------------------------  alloc b -------------------------------");
    void *b = buddy_alloc(1*PAGE_SIZE); // 0x2000~0x4000
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_free_blocks(3);
    // dump_tree_path_to_addr((uintptr_t)b, 0);

    uart_write_str("\r\n---------------------------  free a -------------------------------");
    buddy_free(a);
    dump_allocated_nodes();
    
    
    uart_write_str("\r\n---------------------------  free b -------------------------------");
    buddy_free(b);
    dump_allocated_nodes();
}














////////////////////////////////// dynamic allocator test case function ///////////////////////////////////////
//測試每一種 size 是否都可以正確 allocate
void chunk_test1(){
    uart_write_str("\r\n--------------[TEST 1]mixed size alloc (16/32/48/96), both two----------------------");
    void *m1 = chunk_alloc(16);
    void *m2 = chunk_alloc(32);
    void *m3 = chunk_alloc(48);
    void *m4 = chunk_alloc(96);
    void *m5 = chunk_alloc(16);
    void *m6 = chunk_alloc(32);
    void *m7 = chunk_alloc(48);
    void *m8 = chunk_alloc(96);
    uart_write_str("\r\nm1:");
    uart_write_hex((uintptr_t)m1);
    uart_write_str("  m2:");
    uart_write_hex((uintptr_t)m2);
    uart_write_str("  m3:");
    uart_write_hex((uintptr_t)m3);
    uart_write_str("  m4:");
    uart_write_hex((uintptr_t)m4);
    uart_write_str("\r\nm5:");
    uart_write_hex((uintptr_t)m5);
    uart_write_str("  m6:");
    uart_write_hex((uintptr_t)m6);
    uart_write_str("  m7:");
    uart_write_hex((uintptr_t)m7);
    uart_write_str("  m8:");
    uart_write_hex((uintptr_t)m8);
    chunk_free(m1);
    chunk_free(m2);
    chunk_free(m3);
    chunk_free(m4);
    chunk_free(m5);
    chunk_free(m6);
    chunk_free(m7);
    chunk_free(m8);
}

//需要多找 buddy 要 page 的情況 
void chunk_test2(){
    uart_write_str("\r\n------------------- [TEST 2]allocate 300 x 16 bytes (cross page)-----------------------");
    void *b[300];
    for (int i = 0; i < 300; i++) b[i] = chunk_alloc(16);
    uart_write_str("\r\n1th ptr addr:");
    uart_write_hex((uintptr_t)b[0]);
    uart_write_str(", 256th ptr addr:");
    uart_write_hex((uintptr_t)b[255]);
    uart_write_str("\r\n257th ptr addr:");
    uart_write_hex((uintptr_t)b[256]);
    uart_write_str(", 300th ptr addr:");
    uart_write_hex((uintptr_t)b[299]);
    uart_write_str("\r\nafter allocating 300 x 16 bytes:");
    dump_chunk_pools();
    for (int i = 0; i < 300; i++) chunk_free(b[i]);
}

//reuse after free
void chunk_test3() {
    uart_write_str("\r\n-------------------- [TEST 3]reuse after free-----------------------");

    void *r1 = chunk_alloc(16);
    uart_write_str("\r\n  ptr1:");
    uart_write_hex((uintptr_t)r1);
    void *r2 = chunk_alloc(16);
    uart_write_str("\r\n  ptr2:");
    uart_write_hex((uintptr_t)r2);

    chunk_free(r1);
    
    void *r3 = chunk_alloc(16);
    uart_write_str("\r\n  ptr3(reused):");
    uart_write_hex((uintptr_t)r3);
    chunk_free(r2);
    chunk_free(r3);

}




////////////////////////////////// mix test case function ///////////////////////////////////////
//交叉用 dynamic 跟 buddy，看 kfree 能不能正確分辨出 ptr 要傳給誰
void mix_test1(){
    uart_write_str("\r\n------------------ before test start-----------------------");
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();

    uart_write_str("\r\n------------------ alloc dynamic system -----------------------");
    void *a = kmalloc(16);
    void *b = kmalloc(48);
    void *c = kmalloc(1024);
    void *d = kmalloc(1024);
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();

    uart_write_str("\r\n------------------ alloc buddy system -----------------------");
    void *e = kmalloc(4000);
    void *f = kmalloc(4000);
    void *g = kmalloc(8000);
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();

    uart_write_str("\r\n------------------ free dynamic system -----------------------");
    kfree((uintptr_t)a);
    kfree((uintptr_t)b);
    kfree((uintptr_t)c);
    kfree((uintptr_t)d);
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();

    uart_write_str("\r\n------------------ free buddy system -----------------------");
    kfree((uintptr_t)f);
    kfree((uintptr_t)g);
    kfree((uintptr_t)e);
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();
}


//讓 dynamic 需要跟 buddy 多要 page 來切，看 kfree 能不能知道
void mix_test2(){
    uart_write_str("\r\n------------------1. before test start-----------------------");
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();

    uart_write_str("\r\n-------------------2.  allocate 300 x 16 bytes (cross page) a[300] --------------");
    void *a[300];
    for (int i = 0; i < 300; i++) a[i] = kmalloc(16);
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();

    uart_write_str("\r\n------------------ 3. alloc buddy system -----------------------");
    void *b = kmalloc(4000);
    void *c = kmalloc(4000);
    void *d = kmalloc(8000);
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();

    uart_write_str("\r\n------------------- 4. allocate 300 x 16 bytes (cross page) e[300] ------------------");
    void *e[300];
    for (int i = 0; i < 300; i++) e[i] = kmalloc(16);
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();


    uart_write_str("\r\n------------------ 5. free buddy system -----------------------");
    kfree((uintptr_t)b);
    kfree((uintptr_t)c);
    kfree((uintptr_t)d);
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();

    uart_write_str("\r\n------------------6. free dyanmic system a[300]-----------------------");
    for (int i = 0; i < 300; i++) kfree((uintptr_t)a[i]);
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();

    uart_write_str("\r\n------------------7. free dyanmic system e[300]-----------------------");
    for (int i = 0; i < 300; i++) kfree((uintptr_t)e[i]);
    dump_allocated_nodes();
    dump_free_blocks(0);
    dump_free_blocks(1);
    dump_free_blocks(2);
    dump_chunk_pools();
}