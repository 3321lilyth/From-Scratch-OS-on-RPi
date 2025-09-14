#include "wait.h"

extern thread_t* get_current();
extern thread_t* ready_queue[MAX_PRIORITY];
static void enable_interrupt() { asm volatile("msr DAIFClr, 0xf"); }   // 0b0010，enable IRQ
static void disable_interrupt() { asm volatile("msr DAIFSet, 0xf"); } 

void wait_queue_init(wait_queue_t* q, int val) {
    q->val = val;
    q->head = 0;
}

int is_in_wait_queue(thread_t* t, wait_queue_t* q) {
    enable_interrupt();
    thread_t* cur = q->head;
    while (cur) {
        if (cur == t) {
            return 1;   // 找到了
        }
        cur = cur->next;
    }
    disable_interrupt();
    return 0;           // 沒找到
}

// 等待資源：若 val > 0 則可取得並繼續，否則 sleep
void wait(wait_queue_t* q) {
    disable_interrupt();
    q->val--;                           //lock
    if (q->val < 0) {                   //lock fail
        thread_t* t = get_current();
        if (!t || t->status != THREAD_READY) {
            uart_write_str_raw("\r\n[PANIC] wait() something wrong!");
            while (1) {}
        }

        remove_from_ready_queue(t);
        t->status = THREAD_WAITING;
        t->next = 0;

        if (!q->head) {
            q->head = t;
        } else {
            thread_t* cur = q->head;
            while (cur) {
                if (!cur->next) break;   //安全：一定先檢查 cur 再訪問 cur->next
                cur = cur->next;
            }
            cur->next = t;
        }
        schedule(0);
    } else {                            //lock success, keep working
        enable_interrupt();
    }
}

// 釋放資源：若有 thread 被 block，就喚醒一個
void signal(wait_queue_t* q) {
    disable_interrupt();
    q->val++;
    if (q->val <= 0) {
        if (!q->head) {
            enable_interrupt();
            return;
        }
        thread_t* t = q->head;
        q->head = t->next;
        t->status = THREAD_READY;
        t->next = 0;
        
        if (!t || t->status != THREAD_SLEEP) {
            uart_write_str_raw("\r\n[PANIC] signal() woke up non-sleeping thread!");
            while (1) {}
        }

        thread_t** rq = &ready_queue[t->priority];
        while (*rq) rq = &((*rq)->next);
        *rq = t;
    }
    enable_interrupt();
}

// 喚醒 wait queue 中所有 thread，並把 val 加總（非 semaphore 標準行為）
void signal_all(wait_queue_t* q) {
    disable_interrupt();
    while (q->head) {
        q->val++;
        signal(q);
    }
    enable_interrupt();
}