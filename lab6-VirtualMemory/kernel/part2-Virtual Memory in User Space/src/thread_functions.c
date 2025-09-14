#include "thread_functions.h"
extern void preempt_callback(void* unused);
extern thread_t* get_current();




///////////////////////// kernel thread function //////////////////////////////////
// kernel thread 才可以直接呼叫 uart_write_X 系列，如果是 user thread 就要透過 system call 才行

// 測試 complete 情況，也就是 kernel thread 是否可以自願放棄 CPU，不過這邊我暫時不想管ㄏㄏ
void foo_sync(){
    for(int i = 0; i < 10; ++i) {
        uart_write_str("\r\n[foo] Thread id: ");
        uart_write_int(get_current()->tid);
        uart_write_str(", #");
        uart_write_int(i);
        sleep(10);       //說 linux 是 5-10ms 換一次 thread，這邊放太小就會卡死
    }
    thread_exit();
}
// 測試 preempt 情況，也就是 kernel thread 是否會被 timer interrup
void foo_sync_preempt(){
    for (int i = 0; i < 10; ++i) {
        uart_write_str("\r\n[foo] Thread id: ");
        uart_write_int(get_current()->tid);
        uart_write_str(", #");
        uart_write_int(i);
    }
    thread_exit();
}