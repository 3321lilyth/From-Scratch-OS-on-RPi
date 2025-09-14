#include "timer.h"

extern void schedule();
// 預先分配的事件 pool (因為現在還沒有 malloc)
static int timer_event_used[MAX_TIMER_EVENTS];
static timer_event_t timer_event_pool[MAX_TIMER_EVENTS];
char timer_msg_pool[MAX_TIMER_EVENTS][MAX_MSG_LEN];
static timer_event_t *timer_queue = 0;              // first event in the queue


/////////////////////////////////helper function/////////////////////////////////
static void enable_interrupt() { asm volatile("msr DAIFClr, 0xf"); }   // 0b0010，enable IRQ
static void disable_interrupt() { asm volatile("msr DAIFSet, 0xf"); } 
unsigned long get_current_tick() {
    unsigned long cntpct;
    asm volatile("mrs %0, cntpct_el0" : "=r"(cntpct));
    return cntpct;
}
unsigned long get_cntfrq() {
    unsigned long frq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(frq));
    return frq;
}
void simple_strncpy(char *dest, const char *src, int max_len) {
    int i = 0;
    while (i < max_len - 1 && src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';  // null terminate
}

void core_timer_enable() { 
    //Rpi3B+ 的 Interrupt Controller CORE0_TIMER_IRQ_CTRL address
    unsigned long irq_ctrl_addr = (unsigned long)CORE0_TIMER_IRQ_CTRL;
    asm volatile(
        //  adv part 要拿掉，不然 cntp_tval_el0<=0 會不斷進入 core_timer_handler 造成無窮迴圈
        // 因為我的 core_timer_handler() 並沒有重新設定 cntp_tval_el0，只有 two_second_core_timer_handler() 有
        // 詳細情況我有記錄在 notion lab3 part4 實作問題1
        // "mrs    x0, cntfrq_el0\n"     //cntfrq_el0, Counter Frequency, 計時器的頻率，也就是每秒的 tick 數
        // "msr    cntp_tval_el0, x0\n"  //倒數1秒，因為是設定為 cntfrq_el0 個 ticks
        
        //cntp_ctl_el0, physical timer control register,
        //bit0=1 代表 enable timer; bit1=1 代表 enable timer interrupt，所以要設定 0x11
        "mov    x0, 1\n"             
        "msr    cntp_ctl_el0, x0\n"

        //unmask timer interrupt   
        "mov    x0, 2\n"
        "str    w0, [%0]\n"             // %0 舊式第一個參數 irq_ctrl_addr。把 w0 的值存倒 irq_ctrl_addr 指向的記憶體位置
                                        //因為 CORE0_TIMER_IRQ_CTRL 只有 32 bit 所以要用 w0 而不是 x0
                                        // `%0` 參數對應到 `irq_ctrl_addr`
        
        //設定 spsr_el1 來 enable IRQ
        "mov    x0, 0x345\n"            //DAIF:1101 (only enable IRQ), M[3:0]:0101 (EL1, SP_EL1)
        "msr    spsr_el1, x0\n"         // eret 之後才生效，所以要先直接設定  daifclr
        // "msr    daifclr, #2\n"

        //設定 elr_el1 來 eret 回去 core_timer_enable C function
        "msr    elr_el1, lr\n"          //這邊的 lr 應該是 main.c 裡面呼叫 core_timer_enable() 的下一行，這是學習助教 el2->el1 的寫法
        "eret"

        :
        : "r"(irq_ctrl_addr)
        : "x0", "memory"
    );  
}





//////////////////////////////// add timer callback function ////////////////////////////////
// 將 second 轉換為 tick 並排序插入 queue
// 同時支援 lab3 要的 "幾秒後印出 log" 還有 lab5 要得 "幾秒後叫醒 task"
// type=1: timeout_callback_log (lab3), type = 2: thread wakeup_callback (lab5)
void add_timer(void (*callback)(void*), char* type, void* arg, unsigned int seconds) {
    // 1. 找空 slot
    int idx = -1;
    disable_interrupt();
    for (int i = 0; i < MAX_TIMER_EVENTS; i++) {
        if (!timer_event_used[i]) {
            timer_event_used[i] = 1;
            idx = i;
            break;
        }
    }
    enable_interrupt();

    if (idx == -1) {
        uart_write_str("\r\n[ERROR] No more timer slots");
        return;
    }

    // 3. 計算超時 tick
    unsigned long long cntfrq = get_cntfrq();
    unsigned long long now = get_current_tick();
    unsigned long long timeout_tick = now + (cntfrq * seconds);

    // 4. 填寫 timer event
    timer_event_t *new_event = &timer_event_pool[idx];
    new_event->expire_time = timeout_tick;
    new_event->callback = callback;
    new_event->next = 0;
    if (strcmp(type, "timeout_callback_log")){
        //deep copy 而不只 shadow copy，不然可能會出錯(比如又再次輸入指令導致後面的 string 覆蓋這個)
        simple_strncpy(timer_msg_pool[idx], (char *)arg, MAX_MSG_LEN);
        new_event->arg = timer_msg_pool[idx]; // 把訊息傳進 callback
    }else if (strcmp(type, "wakeup_callback")){
        new_event->arg = arg;
    }

    // 5. 插入排序
    disable_interrupt();
    if (!timer_queue || timeout_tick < timer_queue->expire_time) {
        //queue 為空
        new_event->next = timer_queue;
        timer_queue = new_event;

        //確保 timer 是開啟的
        unsigned int ctl;
        asm volatile("mrs %0, cntp_ctl_el0" : "=r"(ctl));
        ctl |= 1;
        asm volatile("msr cntp_ctl_el0, %0" :: "r"(ctl));
    } else if ( timeout_tick < timer_queue->expire_time){
        //new event 比 queue 裡面的 event 還要早
        new_event->next = timer_queue;
        timer_queue = new_event;
    }else {
        timer_event_t *curr = timer_queue;
        while (curr->next && curr->next->expire_time < timeout_tick) {
            curr = curr->next;
        }
        new_event->next = curr->next;
        curr->next = new_event;
    }
    enable_interrupt();
    
    // 6. 如果這是最早的 timer，更新 core timer
    if (timer_queue == new_event) {
        unsigned long long interval = timeout_tick - now;
        if (interval == 0) interval = 1;
        asm volatile("msr cntp_tval_el0, %0" :: "r"(interval));
    }
}









//////////////////////////////// handler for core timer interrupt ////////////////////////////////
//handler for basic part
void two_second_core_timer_handler() {
    //////////////// 1. 印出現在的系統值行秒數 ///////////////////
    // 讀取系統時鐘頻率 (ticks per second)
    unsigned long cntfrq;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (cntfrq));

    // 讀取當前的計時器計數值 (ticks since boot)
    unsigned long cntpct;
    asm volatile("mrs %0, cntpct_el0" : "=r" (cntpct));

    // 計算開機後的秒數 (ticks / frequency)
    unsigned long seconds_after_boot = cntpct / cntfrq;

    uart_write_str("\r\n[timer] Seconds after booting: ");
    uart_write_int(seconds_after_boot);


    /////////// 2. 設定下一次的 Timer interval 為 2 秒 (cntfrq * 2) ///////////
    asm volatile(
        "mrs    x0, cntfrq_el0\n"     //cntfrq_el0, Counter Frequency, 計時器的頻率，也就是每秒的 tick 數
        "add    x0, x0, x0\n"
        "msr    cntp_tval_el0, x0\n"  //倒數2秒，因為是設定為 2*cntfrq_el0 個 ticks

        //設定 spsr_el1 來 enable IRQ
        "mov    x0, 0x345\n"          //DAIF:1101 (only enable IRQ), M[3:0]:0101 (EL1, SP_EL1)
        "msr spsr_el1, x0\n"

        //注意這邊千萬不要亂設定 elr_el1 還有叫 eret，因為你還需要回到 vector table 去 load_all 之後才可以 eret
    );  
}

//handler for adv part
void core_timer_handler() {
    uart_write_str("\r\n[timer] into core_timer_handler");
    unsigned long long curr_tick = get_current_tick();

    disable_interrupt();
    while (timer_queue && timer_queue->expire_time <= curr_tick) {
        // 觸發 callback
        timer_queue->callback(timer_queue->arg);

        // 回收 event 結構
        for (int i = 0; i < MAX_TIMER_EVENTS; ++i) {
            if (&timer_event_pool[i] == timer_queue) {
                timer_event_used[i] = 0;
                break;
            }
        }

        // 移除 queue 第一個元素
        timer_queue = timer_queue->next;
    }
    enable_interrupt();

    // 如果 queue 還有下一個 event，設定新的 one-shot timer
    if (timer_queue) {
        unsigned long long now = get_current_tick();
        
        //為了避免設定 core timer interval 為 0，所以至少設定為 1
        //如果你設定為 0，代表「馬上觸發中斷」，這會造成 core_timer_handler() 被立即重新呼叫，陷入無限中斷迴圈。
        //雖然前面的迴圈理論上已經處理掉這種 case，但是有可能會有邊緣 case
        unsigned long long interval = timer_queue->expire_time > now ? (timer_queue->expire_time - now) : 1;
        asm volatile("msr cntp_tval_el0, %0\n" :: "r"(interval));
    } else{
        // 如果沒有下一個 event，就停用 timer
        // qemu 中，enable timer 之後應該會先進到這邊一次
        asm volatile("mov    x0, 0\n");
        asm volatile("msr    cntp_ctl_el0, x0\n");    
    }
    //task 完成後重新 unmask timer
    *(unsigned int*)CORE0_TIMER_IRQ_CTRL = 2;   
}

