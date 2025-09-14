#define CORE0_TIMER_IRQ_CTRL 0x40000040

.global core_timer_enable_s
core_timer_enable_s:
    //cntp_ctl_el0, physical timer control register
    mov x0, 3               //bit0=1 代表 enable timer; bit1=1 代表 enable timer interrupt，所以要設定 0x11
    msr cntp_ctl_el0, x0 

    //Counter Frequency, 計時器的頻率
    mrs x0, cntfrq_el0      //cntfrq_el0, Counter Frequency, 計時器的頻率，也就是每秒的 tick 數       
    msr cntp_tval_el0, x0   //倒數1秒，因為是設定為 cntfrq_el0 個 ticks
    
    
    //unmask timer interrupt   
    mov x0, 2
    ldr x1, =CORE0_TIMER_IRQ_CTRL
    str w0, [x1]            //把 w0 的值存倒 X1 指向的記憶體位置
                            //因為 CORE0_TIMER_IRQ_CTRL 只有 32 bit 所以要用 w0 而不是 x0
    
    //設定 spsr_el1 來 enable IRQ
    mov x0, 0x345           //DAIF:1101 (only enable IRQ), M[3:0]:0101 (EL1, SP_EL1)
    msr spsr_el1, x0        // eret 之後才生效，所以要先直接設定  daifclr
    msr daifclr, #2

    //設定 elr_el1 來 eret 回去 core_timer_enable C function 
    msr elr_el1, lr         //因為我在 C 那邊 bl core_timer_enable_s，所以 lr 就可以回去!!
    eret


.global core_timer_handler_s
core_timer_handler_s:
    //設定下一次在兩秒以後
    mrs x0, cntfrq_el0
    add x0, x0, x0
    msr cntp_tval_el0, x0

    //設定 spsr_el1 來 enable IRQ
    mov x0, 0x345           //DAIF:1101 (only enable IRQ), M[3:0]:0101 (EL1, SP_EL1)
    msr spsr_el1, x0

    //設定 elr_el1 來 eret 回去 two_second_core_timer_handler C function 
    msr elr_el1, lr         //因為我在 C 那邊 bl core_timer_handler_s lr 就可以回去!!
    //eret