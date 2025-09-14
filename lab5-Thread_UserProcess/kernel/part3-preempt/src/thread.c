#include "thread.h"

thread_t* ready_queue[MAX_PRIORITY] = {0};   // run queue：每個 priority 對應一條 linked list
                                                    // priority 0 為最高，MAX_PRIORITY-1 為 idle thread
static unsigned int next_tid = 1;



extern int executing_task;
extern int current_priority;

/// helper function
extern void cxtsw_complete_first(thread_t* next);
extern void switch_to(thread_t* prev, thread_t* next);
extern void cxtsw_complete(thread_t* prev, thread_t* next);
extern void load_all_preempt();

extern thread_t* get_current();
static void enable_interrupt() { asm volatile("msr DAIFClr, 0xf"); }   // 0b0010，enable IRQ
static void disable_interrupt() { asm volatile("msr DAIFSet, 0xf"); } 


void remove_from_ready_queue(thread_t* t) {
    disable_interrupt();
    thread_t* cur = ready_queue[t->priority];
    thread_t* prev = 0;

    while (cur) {
        if (cur == t) {
            if (prev) {
                prev->next = cur->next;
            } else {
                // cur 是頭節點
                ready_queue[t->priority] = cur->next;
            }
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    t->next = 0;
    enable_interrupt();
}


void move_to_ready_queue_tail(thread_t* t) {
    if (!t) return;
    disable_interrupt();
    // 1. 先從 ready queue 把自己拔掉
    thread_t** p = &ready_queue[t->priority];
    while (*p) {
        if (*p == t) {
            *p = t->next;
            break;
        }
        p = &((*p)->next);
    }

    t->next = 0; // 保險起見清掉

    // 2. 再加到 ready queue 的尾巴
    thread_t** q = &ready_queue[t->priority];
    while (*q) {
        q = &((*q)->next);
    }
    *q = t;
    enable_interrupt();
}



void ready_queue_dump() {
    uart_write_str_raw("\r\n    ===== Ready Queue Dump =====");

    for (int prio = 0; prio < MAX_PRIORITY; prio++) {

        thread_t* cur = ready_queue[prio];
        //empty queue
        if (!cur) continue;
        uart_write_str_raw("\r\n    Priority ");
        uart_write_int_raw(prio);
        uart_write_str_raw(": ");

        while (cur) {
            uart_write_str_raw("tid=");
            uart_write_int_raw(cur->tid);
            uart_write_str_raw(" -> ");
            cur = cur->next;
        }
    }

    uart_write_str_raw("\r\n    =============================");
}
void panic_invalid_sp() {
    uart_write_str_raw("\r\n[PANIC] invalid stack pointer");
    ready_queue_dump();
    while (1) {}
}

void panic_null_thread() {
    uart_write_str_raw("\r\n[PANIC] null thread given to switch");
    ready_queue_dump();
    while (1) {}
}

void validate_thread_stack(thread_t* t) {
    if (!t || !t->stack_base) panic_null_thread();
    if (t->sp < (uint64_t)t->stack_base ||
        t->sp >= (uint64_t)t->stack_base + THREAD_STACK_SIZE ||
        t->sp % STACK_ALIGN != 0) {
        uart_write_str_raw("\r\n[PANIC] tid ");
        uart_write_uint_raw(t->tid);
        uart_write_str_raw(" have invalid sp: ");
        uart_write_hex_raw(t->sp);
        panic_invalid_sp();
        ready_queue_dump();
    }
}



extern uintptr_t get_startup_current_ptr();
extern uintptr_t dtb_start_addr;
extern uintptr_t dtb_end_addr;
extern uint32_t initrd_start_addr;
extern uint32_t initrd_end_addr;
void validate_ready_queue() {
    for (int prio = 0; prio < MAX_PRIORITY; prio++) {
        thread_t* cur = ready_queue[prio];
        int count = 0;
        while (cur) {
            // 防止無窮迴圈：next 指向自己
            if (cur == cur->next) {
                uart_write_str_raw("\r\n[PANIC] validate_ready_queue: self-loop detected at priority ");
                uart_write_int_raw(prio);
                uart_write_str_raw(", tid ");
                uart_write_int_raw(cur->tid);
                while (1) {}
            }

            // 防止 next 指向無效記憶體（根據你的記憶體範圍）
            if ((uintptr_t)cur < 0x1000 
                || ((uintptr_t)cur >= 0x59000 && (uintptr_t)cur < get_startup_current_ptr()) 
                || ((uintptr_t)cur >= dtb_start_addr && (uintptr_t)cur < dtb_end_addr) 
                || ((uintptr_t)cur >= initrd_start_addr && (uintptr_t)cur < initrd_end_addr) 
                || (uintptr_t)cur > 0x3C000000) {
                uart_write_str_raw("\r\n[PANIC] validate_ready_queue: invalid pointer ");
                uart_write_hex_raw((uintptr_t)cur);
                uart_write_str_raw(" at priority ");
                uart_write_int_raw(prio);
                while (1) {}
            }

            // 防止 cur 本身資料壞掉（例如 sp 非法）
            if (cur->stack_base == 0 || cur->tid == 0 || cur->priority >= MAX_PRIORITY) {
                uart_write_str_raw("\r\n[PANIC] validate_ready_queue: broken thread_t structure at priority ");
                uart_write_int_raw(prio);
                while (1) {}
            }

            cur = cur->next;
            count++;

            // 防止異常過長的 linked list（通常超過 MAX_THREAD 數量就有問題）
            if (count > 1000) {
                uart_write_str_raw("\r\n[PANIC] validate_ready_queue: too many nodes at priority ");
                uart_write_int_raw(prio);
                while (1) {}
            }
        }
    }
}































/////////////////////////////////////// thread operation (給user呼叫的) ///////////////////////////////////
thread_t* thread_create(void (*func)(), int priority) {

    uart_write_str_raw("\r\n  -------------------- thread create --------------------");
    // 1. alloc 這個 thread 的 stack 跟 thread_t
    thread_t* t = (thread_t*)kmalloc(sizeof(thread_t));
    if (!t) return 0;
    void* stack = kmalloc(THREAD_STACK_SIZE);
    if (!stack) return 0;

    // 2. init thread_t
    memset(t, 0, sizeof(thread_t));
    disable_interrupt();
    t->tid = next_tid++;
    enable_interrupt();

    uint64_t* sp = (uint64_t*)((uint64_t)stack + THREAD_STACK_SIZE  -16); //sp 要對其 16，但是直接 stack + THREAD_STACK_SIZE 的話會用到下一個 page 的空間，錯誤!
    sp -= 34;
    memset(sp, 0, 34 * 8);
    sp[30] = (uint64_t)func;
    sp[31] = 0;
    sp[32] = 0x345;                 // spsr_el1 (SP_EL1, enable interrupt)
    sp[33] = (uint64_t)func;   // elr_el1 = 要跳到的function，初始情況lr=elr
    t->sp = (uint64_t)sp;

    uart_write_str_raw("\r\n[thread] thread create, tid=");
    uart_write_int_raw(next_tid);
    uart_write_str_raw(", priority=");
    uart_write_int_raw(priority);
    uart_write_str_raw(", func=");
    uart_write_hex_raw((uint64_t)func);
    uart_write_str_raw(", sp=");
    uart_write_hex_raw((uint64_t)sp);
    uart_write_str_raw(", stack base=");
    uart_write_hex_raw((uint64_t)stack);
    uart_write_str_raw(", sp[31]=");
    uart_write_hex_raw((uint64_t)sp[31]);

    // t->sp = (uint64_t)stack + THREAD_STACK_SIZE -16;        //sp 要對其 16，但是直接 stack + THREAD_STACK_SIZE 的話會用到下一個 page 的空間，錯誤!
    // t->lr = (uint64_t)func;
    t->stack_base = stack;
    t->priority = priority;
    t->status = THREAD_READY;
    t->next = 0;

    //3. init CPU ergister on kernel stack

    // 3. 加入 priority 對應的 queue
    disable_interrupt();
    thread_t** q = &ready_queue[priority];  //q:  pointer-to-pointer(**)，也就是 thread_t* 這個 pointer 的位置
    while (*q) q = &((*q)->next);           //*q 就是 q 指向的 pointer，代表目前的 node (thread_t*)
                                            //&((*q)->next) 表示下一個 pointer 的位置
    *q = t;
    enable_interrupt();

    return t;
}

void thread_exit() {
    thread_t* t = get_current();
    disable_interrupt();
    remove_from_ready_queue(t);
    t->status = THREAD_ZOMBIE;
    t->next = 0;
    uart_write_str_raw("\r\n[thread] thread ");
    uart_write_int_raw(t->tid);
    uart_write_str_raw(" finished.");
    enable_interrupt();
    schedule_complete(0);  // 永不返回
}

//本來 delay() 是呼叫 nop，但這樣還是會占用 CPU，所以實際上應該去 sleep 並主動讓出 CPU 才對
void sleep(int milliseconds) {
    thread_t* t = get_current();
    remove_from_ready_queue(t);

    // 1. 設為 sleep 狀態
    t->status = THREAD_SLEEP;
    t->next = 0;

    // 2. 塞入 timer queue
    // 套用 lab3 做的 timer queue，代表幾秒後會把這個 task 叫醒
    add_timer(wakeup_callback, "wakeup_callback", (void*)t, milliseconds);

    // 3. 主動讓出 CPU
    schedule_complete(0);
}


















////////////////////////////////////// RR + Priority-aware Scheduler /////////////////////////////
//選出下一個 thread
thread_t* choose_next() {
    for (int prio = 0; prio < MAX_PRIORITY; prio++) {
        thread_t* cur = ready_queue[prio];
        while (cur) {
            if (cur->status == THREAD_READY)
                return cur;
            cur = cur->next;
        }
    }
    return 0; // fallback
}

void schedule_complete(void* unused) {
    // uart_write_str_raw("\r\n[thread] into schedule_complete ");
    disable_interrupt();
    validate_ready_queue();
    
    int init = (int)(uintptr_t)unused; 
    thread_t* prev = get_current();
    thread_t* next = choose_next();

    if (!next) {
        // 如果找不到 READY thread
        if (prev && prev->priority == MAX_PRIORITY - 1) {
            uart_write_str_raw("\r\n[thread] schedule_complete, no ready thread, keep idle ");
            // 正在跑 idle，自然繼續跑 idle
            next = prev;
        } else {
            // 不是 idle，卻找不到任何 task，出問題了
            uart_write_str_raw("\r\n[PANIC] No READY task and not in idle!, init = ");
            uart_write_int_raw(init);
            ready_queue_dump();
            while (1) {}
        }
    }

    // uart_write_str_raw("\r\n[thread] schedule_complete, form tid ");
    // uart_write_int_raw(prev ? prev->tid : 0);
    // uart_write_str_raw(" to tid ");
    // uart_write_int_raw(next->tid);
    // uart_write_str_raw(", sp = ");
    // uart_write_hex_raw(next->sp);
    // uart_write_str_raw(", t->stack_base= ");
    // uart_write_hex_raw((uint64_t)next->stack_base);
    // uart_write_str_raw(", prev->stack_base= ");
    // uart_write_hex_raw((uint64_t)prev->stack_base);

    if (prev->tid != next->tid) {
        // uart_write_str_raw("\r\n[thread] schedule_complete, into if(next != prev) part");
        validate_thread_stack(next);
        if (!init) validate_thread_stack(prev);
        validate_ready_queue();

        // uart_write_str_raw("\r\n[thread] schedule_complete, form tid ");
        // uart_write_int_raw(prev ? prev->tid : 0);
        // uart_write_str_raw(" to tid ");
        // uart_write_int_raw(next->tid);
        // uart_write_str_raw(", sp = ");
        // uart_write_hex_raw(next->sp);
        // uart_write_str_raw(", t->stack_base= ");
        // uart_write_hex_raw((uint64_t)next->stack_base);
        // uart_write_str_raw(", prev->stack_base= ");
        // uart_write_hex_raw((uint64_t)prev->stack_base);

        if (init) {
            add_timer(preempt_callback, "preempt_callback", 0, CXTSW_TIMEOUT);
            cxtsw_complete_first(next);
        } else {
            add_timer(preempt_callback, "preempt_callback", 0, CXTSW_TIMEOUT);
            cxtsw_complete(prev, next);
        }
    }else{
        // 就算是同一個人做也要記得 add_timer
        add_timer(preempt_callback, "preempt_callback", 0, CXTSW_TIMEOUT);
    }
    // uart_write_str_raw("\r\n !!!schedule_complete below!!!");
    enable_interrupt();
}


void schedule_preempt() {
    disable_interrupt();
    validate_ready_queue();
    
    thread_t* prev = get_current();
    move_to_ready_queue_tail(prev);
    thread_t* next = choose_next();

    if (!next) {
        // 如果找不到 READY thread
        if (prev && prev->priority == MAX_PRIORITY - 1) {
            // 正在跑 idle，自然繼續跑 idle
            next = prev;
        } else {
            // 不是 idle，卻找不到任何 task，出問題了
            uart_write_str_raw("\r\n[PANIC] No READY task and not in idle!");
            ready_queue_dump();
            while (1) {}
        }
    }
    
    validate_thread_stack(next);
    validate_ready_queue();

    // uart_write_str_raw("\r\n[thread] schedule_preempt, form tid ");
    // uart_write_int_raw(prev ? prev->tid : 0);
    // uart_write_str_raw(" to tid ");
    // uart_write_int_raw(next->tid);
    // uart_write_str_raw(", sp = ");
    // uart_write_hex_raw(next->sp);
    
    
    if (prev->tid != next->tid) {
        // uart_write_str_raw("\r\n[thread] schedule_preempt, form tid ");
        // uart_write_int_raw(prev ? prev->tid : 0);
        // uart_write_str_raw(" to tid ");
        // uart_write_int_raw(next->tid);
        // uart_write_str_raw(", sp = ");
        // uart_write_hex_raw(next->sp);
        
        executing_task = 0;
        current_priority = 999;
        add_timer(preempt_callback, "preempt_callback", 0, CXTSW_TIMEOUT);
        switch_to(prev, next);
        load_all_preempt();         //never return
    }else{
        // 就算是同一個人做也要記得 add_timer
        add_timer(preempt_callback, "preempt_callback", 0, CXTSW_TIMEOUT);
    }
    enable_interrupt();
}


void wakeup_callback(void* t_ptr) {
    thread_t* t = (thread_t*)t_ptr;

    disable_interrupt();
    t->status = THREAD_READY;
    t->next = 0;   // 確保 wakeup 時 next pointer reset！
    thread_t* cur = ready_queue[t->priority];
    if (!cur) {
        ready_queue[t->priority] = t;
    } else {
        while (cur->next) {
            cur = cur->next;
        }
        cur->next = t;
    }
    enable_interrupt();
}

void preempt_callback(void* unused) {
    (void)unused;  // 避免 compiler warning
    task_enqueue((void (*)(void *))schedule_preempt, (void *)0, TIMER_PRIORITY);
}






////////////////////////////////// task fucntion ////////////////////////////////////
void idle() {
    uart_write_str("\r\n[thread] into idle task");
    while (1) {
        kill_zombies();
        schedule_complete(0);
    }
}

extern wait_queue_t uart_rx_lock;
extern wait_queue_t uart_tx_lock;
extern int is_in_wait_queue(thread_t* t, wait_queue_t* q);
// === 回收 zombie ===
void kill_zombies() {
    for (int prio = 0; prio < MAX_PRIORITY; prio++) {
        disable_interrupt();
        thread_t* cur = ready_queue[prio];
        thread_t* prev = 0;

        while (cur) {
            if (cur->status == THREAD_ZOMBIE) {
                // zombie 但不能在任一 wait queue 中
                if (is_in_wait_queue(cur, &uart_rx_lock) || is_in_wait_queue(cur, &uart_tx_lock)) {
                    prev = cur;
                    cur = cur->next;
                    continue;  // 暫時不能砍
                }

                // uart_write_str("\r\n    killing zombie thread ");
                // uart_write_int(cur->tid);

                thread_t* dead = cur;
                if (prev) {
                    prev->next = cur->next;
                } else {
                    ready_queue[prio] = cur->next;
                }
                cur = cur->next;

                dead->next = 0;
                kfree((uintptr_t)dead->stack_base);
                kfree((uintptr_t)dead);
            } else {
                prev = cur;
                cur = cur->next;
            }
        }
        enable_interrupt();
    }
}