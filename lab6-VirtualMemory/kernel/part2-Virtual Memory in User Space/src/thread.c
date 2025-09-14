#include "thread.h"

thread_t* ready_queue[MAX_PRIORITY] = {0};   // run queue：每個 priority 對應一條 linked list
                                                    // priority 0 為最高，MAX_PRIORITY-1 為 idle thread
static unsigned int next_tid = 1;
volatile unsigned int cxtsw_timeout_ms = 200; // 預設值為 200ms

thread_t *cur_thread;

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


/// @brief ////////////// 維護資料結構用的 helper function ////////////////////////
//note: 一定要用 disable interrupt 把這個 function 包起來!!!
//我之前直接在這裡面 disable + enable，但是其實呼叫 remove_from_ready_queue 這個 function 的 caller 
// 也必須要是 disable interrupt 的狀態，卻因為執行這個 function 而 enable interrupt 了!!
void remove_from_ready_queue(thread_t* t) {
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

extern wait_queue_t uart_rx_lock;
extern wait_queue_t uart_tx_lock;
thread_t* find_in_list(thread_t* head, int tid) {
    thread_t* t = head;
    while (t) {
        if (t->tid == tid) {
            return t;
        }
        t = t->next;
    }
    return NULL;
}
thread_t* find_thread_by_tid(int tid) {
    // 1️. 搜尋 ready queue
    for (int prio = 0; prio < MAX_PRIORITY; prio++) {
        thread_t* t = find_in_list(ready_queue[prio], tid);
        if (t) {
            return t;
        }
    }

    // 2️. 搜尋 uart_rx_lock 的 wait queue
    thread_t* t = find_in_list(uart_rx_lock.head, tid);
    if (t) {
        return t;
    }

    // 3️. 搜尋 uart_tx_lock 的 wait queue
    t = find_in_list(uart_tx_lock.head, tid);
    if (t) {
        return t;
    }

    // 4️. 沒找到
    return NULL;
}





/// @brief ////////////// debug 用 helper function ////////////////////////
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
    if (!t || !t->kernel_stack_base) panic_null_thread();
    if (t->kernel_sp < (uint64_t)t->kernel_stack_base ||
        t->kernel_sp >= (uint64_t)t->kernel_stack_base + THREAD_STACK_SIZE ||
        t->kernel_sp % STACK_ALIGN != 0) {
        uart_write_str_raw("\r\n[PANIC] tid ");
        uart_write_uint_raw(t->tid);
        uart_write_str_raw(" have invalid sp: ");
        uart_write_hex_raw(t->kernel_sp);
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

            // 防止 next kernel_sp 指向無效記憶體（根據你的記憶體範圍）
            if ((uintptr_t)cur->kernel_sp < PA_TO_VA_KERNEL(0x1000) 
                || ((uintptr_t)cur->kernel_sp >= PA_TO_VA_KERNEL(0x59000)           && (uintptr_t)cur->kernel_sp < PA_TO_VA_KERNEL(get_startup_current_ptr())) 
                || ((uintptr_t)cur->kernel_sp >= PA_TO_VA_KERNEL(dtb_start_addr)    && (uintptr_t)cur->kernel_sp < PA_TO_VA_KERNEL(dtb_end_addr)) 
                || ((uintptr_t)cur->kernel_sp >= PA_TO_VA_KERNEL(initrd_start_addr) && (uintptr_t)cur->kernel_sp < PA_TO_VA_KERNEL(initrd_end_addr)) 
                || (uintptr_t)cur->kernel_sp > PA_TO_VA_KERNEL(0x3C000000)) {
                uart_write_str_raw("\r\n[PANIC] validate_ready_queue: invalid pointer ");
                uart_write_hex_raw((uintptr_t)cur);
                uart_write_str_raw(" at priority ");
                uart_write_int_raw(prio);
                while (1) {}
            }

            // 防止 cur 本身資料壞掉（例如 sp 非法）
            if (cur->kernel_stack_base == 0 || cur->tid == 0 || cur->priority >= MAX_PRIORITY) {
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




























/////////////////////////////////////// basic thread operation (給user呼叫的) ///////////////////////////////////
thread_t* thread_create(void (*func)(), int priority, int is_user_mode, char* filename) {
    uart_write_str_raw("\r\n  -------------------- thread create --------------------");
    // 1. alloc 這個 thread 的 stack 跟 thread_t
    thread_t* t = (thread_t*)kmalloc(sizeof(thread_t));
    void* kernel_stack = kmalloc(THREAD_STACK_SIZE);
    if (!t || !kernel_stack) return 0;
    memset(t, 0, sizeof(thread_t));
    memset(kernel_stack, 0, THREAD_STACK_SIZE);

    // 2. 初始化 thread_t
    disable_interrupt();
    t->tid = next_tid++;
    enable_interrupt();
    t->priority = priority;
    t->status = THREAD_READY;
    t->next = 0;
    t->is_user_mode = is_user_mode;


    // 3. 初始化 kernel stack 裡面的 CPU register 和重要的 system register，第一次被執行的時候才會從正確的地方執行
    uint64_t* kernel_sp = (uint64_t*)((uint64_t)kernel_stack + THREAD_STACK_SIZE  -16); //sp 要對其 16，但是直接 stack + THREAD_STACK_SIZE 的話會用到下一個 page 的空間，錯誤!
    kernel_sp -= 36; //18*2，這邊要記得跟著你 save_all 裡面存了多少東西而改變
    memset(kernel_sp, 0, 34 * 8);
    kernel_sp[30] = is_user_mode ? 0 : (uint64_t)func;          // 因為 func 是 kernel space VA，user mode 不應該可以用到
    kernel_sp[31] = 0;                                          // is_preempt
    kernel_sp[32] = is_user_mode ? 0x340 : 0x345;               // spsr_el1: EL0h vs EL1h
    kernel_sp[33] = is_user_mode ? 0x0 : (uint64_t)func;    // elr_el1 = 要跳到的function。
    //kernel_sp[34] 要在  user_pagetable_setup 才會得到 t->user_sp 在來設定
    kernel_sp[35] = is_user_mode ? 1 : 0;                       // return_via_eret

    t->kernel_sp = (uint64_t)kernel_sp;
    t->kernel_stack_base = kernel_stack;

    // 4. 初始化 user stack
    if (is_user_mode == 1) {
        disable_interrupt();
        extern int cpio_exec_user_program_to_thread(thread_t* t, char* filename);
        if (cpio_exec_user_program_to_thread(t, filename) < 0) {
            uart_write_str_raw("\r\n[uthread_create] Failed to exec user image");
            return 0;
        }
        enable_interrupt();
        kernel_sp[34] = t->user_sp;                                 // sp_el0
        
    }else{
        kernel_sp[34] = 0;                                          // sp_el0
        t->user_sp =  0;
        t->user_stack_base = 0;
        t->user_pagetable = 0;
    }

    uart_write_str_raw("\r\n[thread] thread create, tid=");
    uart_write_int_raw(next_tid-1);
    uart_write_str_raw(", priority=");
    uart_write_int_raw(priority);
    uart_write_str_raw(", func=");
    uart_write_hex_raw((uint64_t)func);
    uart_write_str_raw(", sp=");
    uart_write_hex_raw((uint64_t)kernel_sp);
    uart_write_str_raw(", stack base=");
    uart_write_hex_raw((uint64_t)kernel_stack);
    if (is_user_mode) {
        uart_write_str_raw(", user stack base=");
        uart_write_hex_raw((uint64_t)t->user_stack_base);
        uart_write_str_raw(", user sp=");
        uart_write_hex_raw((uint64_t)t->user_sp);
        trace_va_mapping(t->user_pagetable, 0x0);
        trace_va_mapping(t->user_pagetable, 0xffffffffc000);
        trace_va_mapping(t->user_pagetable, 0xffffffffd000);
        trace_va_mapping(t->user_pagetable, 0xffffffffe000);
        trace_va_mapping(t->user_pagetable, 0xfffffffff000);
        trace_va_mapping(t->user_pagetable, VA_USER_STACK_END);
        trace_va_mapping((pagetable_t)KERNEL_PGD_ADDR, 0xFFFF00003C100000);
        trace_va_mapping((pagetable_t)KERNEL_PGD_ADDR, 0xFFFF000000080000);
        trace_va_mapping((pagetable_t)KERNEL_PGD_ADDR, 0xFFFF000040000040);
    }


    // 3. 加入 priority 對應的 queue
    disable_interrupt();
    thread_t** q = &ready_queue[priority];  //q:  pointer-to-pointer(**)，也就是 thread_t* 這個 pointer 的位置
    while (*q) q = &((*q)->next);           //*q 就是 q 指向的 pointer，代表目前的 node (thread_t*)
                                            //&((*q)->next) 表示下一個 pointer 的位置
    *q = t;
    enable_interrupt();

    return t;
}


void uart_write_hex_byte_raw(uint8_t val) {
    const char hex[] = "0123456789ABCDEF";
    uart_write_char_raw(hex[(val >> 4) & 0xF]);
    uart_write_char_raw(hex[val & 0xF]);
}

//return child pid if success
int thread_fork(){
    disable_interrupt();

    //1. allocate
    thread_t *parent = get_current();
    thread_t* child = (thread_t*)kmalloc(sizeof(thread_t));
    void* kernel_stack = kmalloc(THREAD_STACK_SIZE);
    if (!child || !kernel_stack) return -1;
    memset(child, 0, sizeof(thread_t));
    memset(kernel_stack, 0, THREAD_STACK_SIZE);

    //2.  初始化 thread_t 結構
    child->tid = next_tid++;
    child->status = THREAD_READY;
    child->priority = parent->priority;
    child->kernel_stack_base = kernel_stack;
    child->is_user_mode = parent->is_user_mode;
    child->next = 0;
    
    //3. 複製 page table (先不考慮 COW)
    pagetable_t parent_pgd  = (pagetable_t)parent->user_pagetable;
    pagetable_t new_pgd = copy_user_pagetable(parent_pgd, 0, 0);
    if (!new_pgd) {
        uart_write_str_raw("\r\n[thread_fork] ERROR: failed to copy user page table");
        return -1;
    }
    child->user_pagetable = new_pgd;

    //4. user stack 設定(複製在 user_pagetable_setup 裡面做了)
    //卡，因為用 VA 所以其實可以直接把 parent 的拿過去給 child 用
    uint64_t stack_top = VA_USER_STACK_END;
    uint64_t sp_offset = parent->user_sp - (stack_top - 4 * PAGE_SIZE);
    child->user_stack_base = (void *)(stack_top - 4 * PAGE_SIZE);
    child->user_sp = (uint64_t)child->user_stack_base + sp_offset;
    
    //6. 複製 signal handler
    simple_memcpy(&(child->signals), &(parent->signals), sizeof(signal_info_t));
    child->saved_user_elr = 0;
    child->saved_user_spsr = 0;
    child->saved_x30 = 0;
    child->saved_user_sp = 0;
    child->signal_stack_base = 0;
    child->handling_signal = 0;
 
    // uart_write_str_raw("\r\n[DEBUG] Parent user stack (partial): ");
    // uint8_t* p_user = (uint8_t*)parent->user_sp - 320;  // 印出 sp 附近的 32 bytes
    // for (int i = 0; i < 320; i++) {
    //     if (i % 16 == 0) {
    //         uart_write_str_raw("\r\n");
    //         uart_write_hex_raw((uint64_t)(p_user + i));
    //         uart_write_str_raw(": ");
    //     }
    //     uart_write_hex_byte_raw(p_user[i]);
    //     uart_write_char_raw(' ');
    // }

    // uart_write_str_raw("\r\n[DEBUG] Child user stack (partial): ");
    // uint8_t* c_user = (uint8_t*)child->user_sp - 320;
    // for (int i = 0; i < 320; i++) {
    //     if (i % 16 == 0) {
    //         uart_write_str_raw("\r\n");
    //         uart_write_hex_raw((uint64_t)(c_user + i));
    //         uart_write_str_raw(": ");
    //     }
    //     uart_write_hex_byte_raw(c_user[i]);
    //     uart_write_char_raw(' ');
    // }

    //4. 複製 kernel stack（只複製 trap frame，因為 kernel stack 的 絕大多數 內容只是暫存而已，不是 process 實際資料。
    uint64_t* child_kernel_sp = (uint64_t*)((uint64_t)kernel_stack + THREAD_STACK_SIZE - 16);
    child_kernel_sp -= 36;  // 跟你的 save_all 一樣，先扣掉 36 之後，把 parent sp ~parent sp + 36*8 的部分，複製到 child sp ~ child sp + 36*8 
    simple_memcpy(child->kernel_stack_base, parent->kernel_stack_base, THREAD_STACK_SIZE -16);
    child->kernel_sp = (uint64_t)child_kernel_sp;

    //5. 設定 system register ，lr 跟 elr 就維持跟 parent 相同就好
    child_kernel_sp[34] = child->user_sp;   // sp_el0
    child_kernel_sp[0] = 0;                 // 把 x0 改成 0，因為 child return 0

    //6. 加入 ready queue
    thread_t** q = &ready_queue[child->priority];
    while (*q) q = &((*q)->next);
    *q = child;

    uart_write_str_raw("\r\n[thread] forked, parent tid=");
    uart_write_int_raw(parent->tid);
    uart_write_str_raw(", child tid=");
    uart_write_int_raw(child->tid);
    uart_write_str_raw(", new_pgd = ");
    uart_write_hex_raw((uintptr_t)new_pgd);
    trace_va_mapping(child->user_pagetable, 0x0);
    trace_va_mapping(child->user_pagetable, VA_USER_STACK_END);

    enable_interrupt();
    return child->tid;
}


void thread_exit() {
    disable_interrupt();
    thread_t* t = get_current();
    remove_from_ready_queue(t);
    t->status = THREAD_ZOMBIE;
    t->next = 0;
    enable_interrupt();
    schedule_complete(0);  // 永不返回
}

int thread_kill(int target_tid) {
    disable_interrupt();

    // 不允許殺掉自己，因為 thread_exit 應該自己呼叫
    thread_t* current = get_current();
    if (current->tid == target_tid) {
        uart_write_str_raw("\r\n[thread_kill] ERROR: cannot kill self, tid=");
        uart_write_int_raw(target_tid);
        enable_interrupt();
        return -1;
    }

    // 找到 target
    thread_t* target = NULL;
    for (int prio = 0; prio < MAX_PRIORITY; prio++) {
        thread_t* cur = ready_queue[prio];
        while (cur) {
            if (cur->tid == target_tid) {
                target = cur;
                break;
            }
            cur = cur->next;
        }
        if (target) break;
    }

    if (!target) {
        uart_write_str_raw("\r\n[thread_kill] ERROR: thread not found, tid=");
        uart_write_int_raw(target_tid);
        enable_interrupt();
        return -1;
    }

    // 標記 zombie 並拔掉 queue
    remove_from_ready_queue(target);
    target->status = THREAD_ZOMBIE;
    target->next = 0;

    uart_write_str_raw("\r\n[thread_kill] SUCCESS: killed tid=");
    uart_write_int_raw(target_tid);

    enable_interrupt();
    return 0;
}


//本來 delay() 是呼叫 nop，但這樣還是會占用 CPU，所以實際上應該去 sleep 並主動讓出 CPU 才對
void sleep(int milliseconds) {
    disable_interrupt();
    thread_t* t = get_current();
    remove_from_ready_queue(t);

    // 1. 設為 sleep 狀態
    t->status = THREAD_SLEEP;
    t->next = 0;

    // 2. 塞入 timer queue
    // 套用 lab3 做的 timer queue，代表幾秒後會把這個 task 叫醒
    add_timer(wakeup_callback, "wakeup_callback", (void*)t, milliseconds);
    enable_interrupt();
    // 3. 主動讓出 CPU
    schedule_complete(0);
}


int thread_send_signal(int target_tid, int signum) {
    disable_interrupt();
    thread_t* target = find_thread_by_tid(target_tid);
    if (!target) {
        enable_interrupt();
        return -1;
    }
    target->signals.pending[signum] = 1;
    enable_interrupt();
    return 0;
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
    disable_interrupt();
    validate_ready_queue();
    uart_write_str_raw("\r\n[DEBUG] into schedule_complete");
    ready_queue_dump();
    
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

    // 切換 user space page table
    uint64_t pgd_pa = VA_TO_PA_KERNEL(next->user_pagetable);
    switch_user_address_space(pgd_pa);

    // 在切換到 user mode 之前，檢查 signal
    for (int i = 0; i < SIGNAL_MAX; i++) {
        if (next->signals.pending[i]) {
            next->signals.pending[i] = 0;  // 清掉 pending

            if (i == SIGKILL && !next->signals.handlers[i]) {
                // 預設 handler: 殺掉
                remove_from_ready_queue(next);
                next->status = THREAD_ZOMBIE;
                next->next = 0;
            } else if (next->signals.handlers[i]) {

                // 保存原本的 context
                uint64_t *tf = (uint64_t *)next->kernel_sp;     //雖然 kernel 裡面會有 function call，但是這些 function call  呼叫後的 kernel_sp
                                                                //不會被我保存到 thread_t->kernel_sp 上，所以這邊的確是 tf 起點沒錯!
                next->saved_user_elr = tf[33];  // elr_el1
                next->saved_user_spsr = tf[32]; // spsr_el1
                next->saved_x30 = tf[30];
                next->saved_user_sp = tf[34];
                next->handling_signal = 1;      // 設 flag

                 // 分配 handler 專用 stack
                if (next->signal_stack_base == 0) {
                    uint64_t stack_top = VA_HANDLER_STACK_END;
                    next->signal_stack_base = stack_top - 4 * PAGE_SIZE;
                    for (int i = 0; i < 4; i++) {
                        pagetable_t handler_stack_page = (pagetable_t)kmalloc(PAGE_SIZE);
                        memset(handler_stack_page, 0, PAGE_SIZE);
                        void* pa = (void *)VA_TO_PA_KERNEL(handler_stack_page); //這邊拿到的是記憶體空間的 base，所以真正的 stack top 應該要再加上 PAGE_SIZE 才對
                        mappages(next->user_pagetable, stack_top - i * PAGE_SIZE, PAGE_SIZE, (uint64_t)pa + PAGE_SIZE, PTE_NORMAL | PTE_USER);
                    }
                    tf[34] = stack_top;      // sp_el0
                }


                // 替換 trap frame
                tf[33] = (uint64_t)(next->signals.handlers[i]);             // elr_el1 -> 指到 handler


                // 設定 lr 讓 handler 跑完自動 call sigreturn
                // 你應該在 user space 提供一個 __asm__("svc #X") 的 sigreturn
                extern void sys_sigreturn();
                tf[30] = (uint64_t)sys_sigreturn;  // 你定義一個 global export 給 user space
            }
            break;  // 一次處理一個 signal
        }
    }

    // cxtsw 到 next 或者執行 next 的 signal handler
    if (prev->tid != next->tid) {
        // uart_write_str_raw("\r\n[thread] schedule_complete, into if(next != prev) part");
        validate_thread_stack(next);
        if (!init) validate_thread_stack(prev);
        validate_ready_queue();

        
        // uart_write_str_raw("\r\n[thread] schedule_complete, form tid ");
        // uart_write_int_raw(prev ? prev->tid : 0);
        // uart_write_str_raw(" to tid "); 
        // uart_write_int_raw(next->tid);
        // uart_write_str_raw(", next->kernel_sp = ");
        // uart_write_hex_raw(next->kernel_sp);
        // uart_write_str_raw(", next->user_pagetable= ");
        // uart_write_hex_raw((uintptr_t)next->user_pagetable);
        // uart_write_str_raw(", pgd_pa = ");
        // uart_write_hex_raw(pgd_pa);

        
        if (init) {
            add_timer(preempt_callback, "preempt_callback", 0, cxtsw_timeout_ms);
            cxtsw_complete_first(next);
        } else {
            add_timer(preempt_callback, "preempt_callback", 0, cxtsw_timeout_ms);
            cxtsw_complete(prev, next);
        }
    }else{
        // 就算是同一個人做也要記得 add_timer
        add_timer(preempt_callback, "preempt_callback", 0, cxtsw_timeout_ms);
    }
    enable_interrupt();
}


void schedule_preempt() {
    disable_interrupt();
    validate_ready_queue();
    
    thread_t* prev = get_current();
    if (prev->status == THREAD_READY) {
        move_to_ready_queue_tail(prev);
    }

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

    // 切換 user space page table
    uint64_t pgd_pa = VA_TO_PA_KERNEL(next->user_pagetable);
    switch_user_address_space(pgd_pa);

    // 在切換到 user mode 之前，檢查 signal
    for (int i = 0; i < SIGNAL_MAX; i++) {
        if (next->signals.pending[i]) {
            next->signals.pending[i] = 0;  // 清掉 pending

            if (i == SIGKILL && !next->signals.handlers[i]) {
                // 預設 handler: 殺掉
                remove_from_ready_queue(next);
                next->status = THREAD_ZOMBIE;
                next->next = 0;
            } else if (next->signals.handlers[i]) {
                // 保存原本的 context
                uint64_t *tf = (uint64_t *)next->kernel_sp;     //雖然 kernel 裡面會有 function call，但是這些 function call  呼叫後的 kernel_sp
                                                                //不會被我保存到 thread_t->kernel_sp 上，所以這邊的確是 tf 起點沒錯!
                next->saved_user_elr = tf[33];  // elr_el1
                next->saved_user_spsr = tf[32]; // spsr_el1
                next->saved_x30 = tf[30];
                next->saved_user_sp = tf[34];
                next->handling_signal = 1;      // 設 flag

                // 分配 handler 專用 stack
                if (next->signal_stack_base == 0) {
                    uint64_t stack_top = VA_HANDLER_STACK_END;
                    next->signal_stack_base = stack_top - 4 * PAGE_SIZE;
                    for (int i = 0; i < 4; i++) {
                        pagetable_t handler_stack_page = (pagetable_t)kmalloc(PAGE_SIZE);
                        memset(handler_stack_page, 0, PAGE_SIZE);
                        void* pa = (void *)VA_TO_PA_KERNEL(handler_stack_page); //這邊拿到的是記憶體空間的 base，所以真正的 stack top 應該要再加上 PAGE_SIZE 才對
                        mappages(next->user_pagetable, stack_top - i * PAGE_SIZE, PAGE_SIZE, (uint64_t)pa + PAGE_SIZE, PTE_NORMAL | PTE_USER);
                    }
                    tf[34] = stack_top;      // sp_el0
                    
                }


                tf[33] = (uint64_t)(next->signals.handlers[i]);                 // elr_el1 -> 指到 handler，這會是 user space VA


                // 設定 lr 讓 handler 跑完自動 call sigreturn
                // 你應該在 user space 提供一個 __asm__("svc #X") 的 sigreturn
                extern void sys_sigreturn();
                tf[30] = (uint64_t)sys_sigreturn;  // 你定義一個 global export 給 user space
            }
            break;  // 一次處理一個 signal
        }
    }

    // cxtsw 到 next 或者執行 next 的 signal handler
    if (prev->tid != next->tid) {
        executing_task = 0;
        current_priority = 999;

        // uart_write_str_raw("\r\n[thread] schedule_preempt, form tid ");
        // uart_write_int_raw(prev ? prev->tid : 0);
        // uart_write_str_raw(" to tid ");
        // uart_write_int_raw(next->tid);
        // uart_write_str_raw(", sp = ");
        // uart_write_hex_raw(next->kernel_sp);
        // uart_write_str_raw(", next->user_pagetable= ");
        // uart_write_hex_raw((uintptr_t)next->user_pagetable);
        // uart_write_str_raw(", pgd_pa = ");
        // uart_write_hex_raw(pgd_pa);
        

        add_timer(preempt_callback, "preempt_callback", 0, cxtsw_timeout_ms);
        switch_to(prev, next);
        load_all_preempt();         //never return
    }else{
        // 就算是同一個人做也要記得 add_timer
        add_timer(preempt_callback, "preempt_callback", 0, cxtsw_timeout_ms);
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
    uart_write_str_raw("\r\n[thread] into idle task");
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

                thread_t* dead = cur;
                if (prev) {
                    prev->next = cur->next;
                } else {
                    ready_queue[prio] = cur->next;
                }
                cur = cur->next;

                dead->next = 0;
                // === 回收 user stack，因為是 user space VA 所以要先 walk 找到 PA 後再轉到 kernel space VA===
                // uint64_t stack_top = (uint64_t)dead->user_stack_base + 4 * PAGE_SIZE;
                // for (int i = 0; i < 4; i++) {
                //     uint64_t va = stack_top - (i + 1) * PAGE_SIZE;
                //     uint64_t* pte = walk(dead->user_pagetable, va, 0);
                //     if (pte && (*pte & PTE_VALID)) {
                //         uint64_t pa = *pte & ~(PAGE_SIZE - 1);
                //         //pa 會是這個 page 的 high memory side，但是我們 buddy system 統一用 low memory 那邊來分配，所以這邊要 free 的時候，記得要扣掉 PAGE_SIZE
                //         // 比如 kmalloc stack 時得到 0x3000，那因為 stack 是往下長所以我們會把 stack top 的 PA 設定為 0x3000+PAGE_SIZE
                //         // 現在釋放的時候就要把 PAGE_SIZE 扣回來才能釋放
                //         kfree(PA_TO_VA_KERNEL(pa-PAGE_SIZE));
                //     }
                // }
                
                // 寫法2
                // extern void free_va_range(pagetable_t, uint64_t, int);
                // if (dead->user_stack_base)
                //     free_va_range(dead->user_pagetable, (uint64_t)dead->user_stack_base, 4);

                // 寫法3
                free_pagetable(dead->user_pagetable, 0, 0); // 直接釋放整個 pagetable，這樣就會自動回收所有 user space 的 stack
                dead->user_stack_base = 0;

                // === 回收其他資源 ===
                kfree((uintptr_t)dead->kernel_stack_base);
                kfree((uintptr_t)dead);
            } else {
                prev = cur;
                cur = cur->next;
            }
        }
        enable_interrupt();
    }
}

