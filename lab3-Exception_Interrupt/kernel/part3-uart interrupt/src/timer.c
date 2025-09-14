#include "timer.h"

void core_timer_enable() { 
    //Rpi3B+ 的 Interrupt Controller CORE0_TIMER_IRQ_CTRL address
    unsigned long irq_ctrl_addr = (unsigned long)CORE0_TIMER_IRQ_CTRL;
    asm volatile(
        //cntp_ctl_el0, physical timer control register,
        //bit0=1 代表 enable timer; bit1=1 代表 enable timer interrupt，所以要設定 0x11
        "mov    x0, 1\n"             
        "msr    cntp_ctl_el0, x0\n"

        //Counter Frequency, 計時器的頻率
        "mrs    x0, cntfrq_el0\n"     //cntfrq_el0, Counter Frequency, 計時器的頻率，也就是每秒的 tick 數
        "msr    cntp_tval_el0, x0\n"  //倒數1秒，因為是設定為 cntfrq_el0 個 ticks

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

    uart_write_str("\r\n[Core Timer] Seconds after booting: ");
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