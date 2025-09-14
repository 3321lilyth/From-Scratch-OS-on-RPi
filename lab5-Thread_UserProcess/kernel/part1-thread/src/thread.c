#include "thread.h"

static thread_t* ready_queue[MAX_PRIORITY] = {0};
// run queue：每個 priority 對應一條 linked list
// priority 0 為最高，MAX_PRIORITY-1 為 idle thread

static thread_t* current_thread = 0;
static int next_tid = 1;

extern void switch_to(thread_t* prev, thread_t* next);
extern thread_t* get_current();


// === Thread context API ===
void set_current_thread(thread_t* t) {
    current_thread = t;
    asm volatile ("msr tpidr_el1, %0" :: "r"(t));
}



/////////////////////////////////////// thread operation (給user呼叫的) ///////////////////////////////////
thread_t* thread_create(void (*func)(), int priority) {
    uart_write_str("\r\n[thread] thread create, tid=");
    uart_write_int(next_tid);
    uart_write_str(", priority=");
    uart_write_int(priority);

    // 1. alloc 這個 thread 的 stack 跟 thread_t
    thread_t* t = (thread_t*)kmalloc(sizeof(thread_t));
    if (!t) return 0;
    void* stack = kmalloc(THREAD_STACK_SIZE);
    if (!stack) return 0;

    // 2. init thread_t
    memset(t, 0, sizeof(thread_t));
    t->sp = (uint64_t)stack + THREAD_STACK_SIZE;
    t->lr = (uint64_t)func;
    t->stack_base = stack;
    t->priority = priority;
    t->tid = next_tid++;
    t->status = THREAD_READY;
    t->next = 0;

    // 3. 加入 priority 對應的 queue
    thread_t** q = &ready_queue[priority];  //q:  pointer-to-pointer(**)，也就是 thread_t* 這個 pointer 的位置
    while (*q) q = &((*q)->next);           //*q 就是 q 指向的 pointer，代表目前的 node (thread_t*)
                                            //&((*q)->next) 表示下一個 pointer 的位置
    *q = t;

    return t;
}

void thread_exit() {
    thread_t* t = get_current();
    t->status = THREAD_ZOMBIE;
    uart_write_str("\r\n[thread] thread ");
    uart_write_int(t->tid);
    uart_write_str(" finished.");
    schedule();  // 永不返回
}

//本來 delay() 是呼叫 nop，但這樣還是會占用 CPU，所以實際上應該去 sleep 並主動讓出 CPU 才對
void thread_delay_sec(int sec) {
    thread_t* t = get_current();

    // 1. 設為 sleep 狀態
    t->status = THREAD_SLEEP;

    // 2. 塞入 timer queue
    // 套用 lab3 做的 timer queue，代表幾秒後會把這個 task 叫醒
    add_timer(wakeup_callback, "wakeup_callback", (void*)t, sec);

    // 3. 主動讓出 CPU
    schedule();
}














////////////////////////////////////// RR + Priority-aware Scheduler /////////////////////////////
void schedule() {
    thread_t* prev = get_current();

    for (int prio = 0; prio < MAX_PRIORITY; prio++) {
        thread_t** q = &ready_queue[prio];

        // 找第一個 READY 狀態的 task，也就是 next
        while (*q && (*q)->status != THREAD_READY) {
            q = &((*q)->next);  // skip zombie and sleep thread
        }

        if (*q) {
            thread_t* next = *q;    

            // dequeue
            *q = next->next;        // next 的前一個指向 next 的後一個
                                    //直接把原本 "prev->next" 從 "next" 改為 "next->next 這個 thread_t*"
            next->next = 0;

            // 把 prev task 重新加回去該 priority list 的尾端， requeue to tail (RR)
            thread_t** tail = &ready_queue[prio];
            while (*tail) tail = &((*tail)->next);
            *tail = next;


            //有可能根本不用換
            if (next != prev) {
                // uart_write_str("\r\n[thread] scheduling, form tid ");
                // uart_write_int(prev->tid);
                // uart_write_str(" to tid ");
                // uart_write_int(next->tid);
                // uart_write_str(", sp = ");
                // uart_write_hex(next->sp);
                // uart_write_str(", lr = ");
                // uart_write_hex(next->lr);

                set_current_thread(next);
                switch_to(prev, next);
            }
            return;
        }
    }
}

void wakeup_callback(void* t_ptr) {
    thread_t* t = (thread_t*)t_ptr;
    t->status = THREAD_READY;
}















////////////////////////////////// task fucntion ////////////////////////////////////
void idle() {
    uart_write_str("\r\n[thread] into idle task");
    while (1) {
        kill_zombies();
        schedule();
    }
}


// === 回收 zombie ===
void kill_zombies() {
    for (int prio = 0; prio < MAX_PRIORITY; prio++) {
        thread_t** cur = &ready_queue[prio];
        while (*cur) {
            if ((*cur)->status == THREAD_ZOMBIE) {
                thread_t* dead = *cur;
                uart_write_str("\r\n    killing zombie thread ");
                uart_write_int(dead->tid);
                *cur = dead->next;
                kfree((uintptr_t)dead->stack_base);
                kfree((uintptr_t)dead);
            } else {
                cur = &((*cur)->next);
            }
        }
    }
}


void foo(){
    for(int i = 0; i < 10; ++i) {
        uart_write_str("\r\n[foo] Thread id: ");
        uart_write_int(get_current()->tid);
        uart_write_str(", #");
        uart_write_int(i);
        thread_delay_sec(1);
    }
    thread_exit();
}
