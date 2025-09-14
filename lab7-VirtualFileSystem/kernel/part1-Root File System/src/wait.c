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
        if (!t) {
            uart_write_str_raw("\r\n[PANIC] wait(), invalid therad_t");
            while (1) {}
        }else if ( t->status != THREAD_READY){
            uart_write_str_raw("\r\n[PANIC] wait() invalid therad status, t->status  = ");
            uart_write_int_raw(t->status);
            uart_write_str_raw(", tid = ");
            uart_write_int_raw(t->tid);
            while (1) {}
        }

        remove_from_ready_queue(t);
        t->status = THREAD_WAITING;
        t->next = 0;
        // uart_write_str_raw("\r\n[WAIT] wait(), caller tid=");
        // uart_write_int_raw(t->tid);
        // uart_write_str_raw(", t->status=");
        // uart_write_int_raw(t->status);
        // ready_queue_dump();
        

        //insert into waiting queue
        if (!q->head) {
            q->head = t;
        } else {
            thread_t* cur = q->head;
            while (cur) {
                if (!cur->next) break;
                cur = cur->next;
            }
            cur->next = t;
        }
        schedule_complete((void*)0);
    } else {                            //lock success, keep working
        enable_interrupt();
    }
}


// 釋放資源：若有 thread 被 block，就喚醒一個
void signal(wait_queue_t* q) {
    disable_interrupt();
    // uart_write_str_raw("\r\n[wait] signal() start");

    q->val++;
    if (q->val <= 0) {
        if (!q->head) {
            enable_interrupt();
            return;
        }

        thread_t* t = q->head;
        if (t){
            if (t->status != THREAD_WAITING) {
                uart_write_str_raw("\r\n[PANIC] signal() woke up non-waiting thread tid=");
                uart_write_int_raw(t->tid);
                uart_write_str_raw(", t->status=");
                uart_write_int_raw(t->status);
                while (1) {}
            }
            
            q->head = t->next;
            t->status = THREAD_READY;
            t->next = 0;


            // insert into ready_queue
            thread_t* cur = ready_queue[t->priority];
            if (!cur) {
                ready_queue[t->priority] = t;
            } else {
                while (cur->next) {
                    cur = cur->next;
                }
                cur->next = t;
            }
        }
    }

    // uart_write_str_raw("\r\n[wait] signal() end");
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