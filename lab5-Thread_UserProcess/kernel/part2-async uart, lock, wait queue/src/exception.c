#include "exception.h"



extern void schedule();
//static : 限制只有這個C檔案可以用，表示這些變數是 全域區域（.bss 或 .data），不是 stack allocation
static task_t task_pool[MAX_TASKS];
static int task_used[MAX_TASKS];
static task_t* task_queue = 0;
static int current_priority = 999;  // 越小越高，999 表示空閒中
static int executing_task = 0;

static void enable_interrupt() { asm volatile("msr DAIFClr, 0xf"); }   // 0b0010，enable IRQ
static void disable_interrupt() { asm volatile("msr DAIFSet, 0xf"); }  // 0xf = 0b1111，代表設置 D, A, I, F，禁用所有中斷
const char *entry_error_messages[] = {
	"SYNC_INVALID_EL1t",
	"IRQ_INVALID_EL1t",		
	"FIQ_INVALID_EL1t",		
	"ERROR_INVALID_EL1T",		

	"SYNC_INVALID_EL1h",		
	"IRQ_INVALID_EL1h",		
	"FIQ_INVALID_EL1h",		
	"ERROR_INVALID_EL1h",		

	"SYNC_INVALID_EL0_64",		
	"IRQ_INVALID_EL0_64",		
	"FIQ_INVALID_EL0_64",		
	"ERROR_INVALID_EL0_64",	

	"SYNC_INVALID_EL0_32",		
	"IRQ_INVALID_EL0_32",		
	"FIQ_INVALID_EL0_32",		
	"ERROR_INVALID_EL0_32"	
};



///////////////////////////// handler //////////////////////////////
void handle_el1_irq() {
    disable_interrupt();
    // 1. 讀取 `CORE0_INTERRUPT_SOURCE`
    unsigned int core_irq_source = *(volatile unsigned int*) CORE0_INTERRUPT_SOURCE;
        //至少 bit8 會被 set，也就是 0x00000100，Bit 8 代表 GPU 產生的 IRQ

    // 2. 判斷是否是 `Core Timer IRQ`
    if (core_irq_source & CORE_TIMER_IRQ_BIT) {
        uart_write_str_raw("\r\n[IRQ] Core Timer Interrupt detected.");
        // 屏蔽 timer interrupt，直到 core_timer_handler 處理完畢
        unsigned int *timer_ctrl = (unsigned int*) CORE0_TIMER_IRQ_CTRL;
        *timer_ctrl = 0;
        task_enqueue((void (*)(void *))core_timer_handler, 0, TIMER_PRIORITY);
        
        
        //所有周邊的 IRQ，也就是 pending1+pending2 ，對 CORE0_INTERRUPT_SOURCE 來說都算是 bit8 的 GPU IRQ
    }else if (core_irq_source & CORE0_GPU_IRQ_BIT){
        // 3. 讀取 `Pending IRQ Registers`
        unsigned int pending_1 = *(volatile unsigned int*) IRQ_PENDING_1;
        
        // 4. 判斷是否是 `Mini UART IRQ`
        if (pending_1 & (1 << IRQ_AUX)) {
            if (*AUX_MU_IIR & 0x4){
                *AUX_MU_IER &= ~0x1;        // mask RX interrupt
                task_enqueue((void (*)(void *))c_read_handler, 0, UART_PRIORITY);
            }
            if (*AUX_MU_IIR & 0x2){
                *AUX_MU_IER &= ~0x2;
                task_enqueue((void (*)(void *))c_write_handler, 0, UART_PRIORITY);
            } 
        }else{
            uart_write_str_raw("\r\n[UART ERROR] other IRQ:");
            uart_write_hex_raw(pending_1);
        }

    }else{
        uart_write_str_raw("\r\n[IRQ ERROR] other core IRQ:");
        uart_write_hex_raw(core_irq_source);
    }



    if (!executing_task){
        executing_task = 1;
        enable_interrupt();

        task_execute_loop();

        // 執行完畢後重設執行狀態
        disable_interrupt();
        current_priority = 999;
        executing_task = 0;
        enable_interrupt();
    }
    enable_interrupt();
    return;
}

void lower_sync_handler(unsigned long type, unsigned long esr, unsigned long elr, unsigned long spsr){
    disable_interrupt();


    uart_write_str_raw("\r\n-------- enter lower_sync_handler -------- ");
    uart_write_str_raw("\r\ntype = ");
    uart_write_str_raw(entry_error_messages[type]);
    // print out interruption type
    // switch(type) {
    //     case 0: uart_write_str_raw("Synchronous"); break;
    //     case 1: uart_write_str_raw("IRQ"); break;
    //     case 2: uart_write_str_raw("FIQ"); break;
    //     case 3: uart_write_str_raw("SError"); break;
    // }

    // decode exception type (some, not all. See ARM DDI0487B_b chapter D10.2.28)
    uart_write_str_raw("; exception type = ");
    switch(esr>>26) {
        case 0b000000: uart_write_str_raw("Unknown"); break;
        case 0b000001: uart_write_str_raw("Trapped WFI/WFE"); break;
        case 0b00000010: uart_write_str_raw("Trapped MCR or MRC access with (coproc==0b1111) (AArch32)"); break;
        case 0b001110: uart_write_str_raw("Illegal execution"); break;
        case 0b010101: uart_write_str_raw("System call"); break;
        case 0b100000: uart_write_str_raw("Instruction abort, lower EL"); break;
        case 0b100001: uart_write_str_raw("Instruction abort, same EL"); break;
        case 0b100010: uart_write_str_raw("Instruction alignment fault"); break;
        case 0b100100: uart_write_str_raw("Data abort, lower EL"); break;
        case 0b100101: uart_write_str_raw("Data abort, same EL"); break;
        case 0b100110: uart_write_str_raw("Stack alignment fault"); break;
        case 0b101100: uart_write_str_raw("Floating point"); break;
        default: uart_write_str_raw("Unknown"); break;
    }

    // decode data abort cause
    uart_write_str_raw("; cause = ");
    if(esr>>26==0b100100 || esr>>26==0b100101) {
        uart_write_str_raw(", ");
        switch((esr>>2)&0x3) {
            case 0: uart_write_str_raw("Address size fault"); break;
            case 1: uart_write_str_raw("Translation fault"); break;
            case 2: uart_write_str_raw("Access flag fault"); break;
            case 3: uart_write_str_raw("Permission fault"); break;
        }
        switch(esr&0x3) {
            case 0: uart_write_str_raw(" at level 0"); break;
            case 1: uart_write_str_raw(" at level 1"); break;
            case 2: uart_write_str_raw(" at level 2"); break;
            case 3: uart_write_str_raw(" at level 3"); break;
        }
    }

    // dump registers
    uart_write_str_raw("\r\nESR_EL1 ");
    uart_write_hex_raw(esr);
    uart_write_str_raw("\r\nELR_EL1 ");
    uart_write_hex_raw(elr);
    uart_write_str_raw("\r\nSPSR_EL1 ");
    uart_write_hex_raw(spsr);

    enable_interrupt();
}

//lab3: The design of system calls is left to the next lab. Now, your kernel 
//only needs to print the content of spsr_el1, elr_el1, and esr_el1 in the exception handler.
void default_handler(unsigned long type, unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long far){
    disable_interrupt();

    uart_write_str_raw("\r\n-------- enter default_handler -------- ");
    uart_write_str_raw("\r\ntype = ");
    uart_write_str_raw(entry_error_messages[type]);
    // print out interruption type
    // switch(type) {
    //     case 0: uart_write_str_raw("Synchronous"); break;
    //     case 1: uart_write_str_raw("IRQ"); break;
    //     case 2: uart_write_str_raw("FIQ"); break;
    //     case 3: uart_write_str_raw("SError"); break;
    // }

    // decode exception type (some, not all. See ARM DDI0487B_b chapter D10.2.28)
    uart_write_str_raw("; exception type = ");
    switch(esr>>26) {
        case 0b000000: uart_write_str_raw("Unknown"); break;
        case 0b000001: uart_write_str_raw("Trapped WFI/WFE"); break;
        case 0b00000010: uart_write_str_raw("Trapped MCR or MRC access with (coproc==0b1111) (AArch32)"); break;
        case 0b001110: uart_write_str_raw("Illegal execution"); break;
        case 0b010101: uart_write_str_raw("System call"); break;
        case 0b100000: uart_write_str_raw("Instruction abort, lower EL"); break;
        case 0b100001: uart_write_str_raw("Instruction abort, same EL"); break;
        case 0b100010: uart_write_str_raw("Instruction alignment fault"); break;
        case 0b100100: uart_write_str_raw("Data abort, lower EL"); break;
        case 0b100101: uart_write_str_raw("Data abort, same EL"); break;
        case 0b100110: uart_write_str_raw("Stack alignment fault"); break;
        case 0b101100: uart_write_str_raw("Floating point"); break;
        default: uart_write_str_raw("Unknown"); break;
    }

    // decode data abort cause
    uart_write_str_raw("; cause = ");
    if(esr>>26==0b100100 || esr>>26==0b100101) {
        uart_write_str_raw(", ");
        switch((esr>>2)&0x3) {
            case 0: uart_write_str_raw("Address size fault"); break;
            case 1: uart_write_str_raw("Translation fault"); break;
            case 2: uart_write_str_raw("Access flag fault"); break;
            case 3: uart_write_str_raw("Permission fault"); break;
        }
        switch(esr&0x3) {
            case 0: uart_write_str_raw(" at level 0"); break;
            case 1: uart_write_str_raw(" at level 1"); break;
            case 2: uart_write_str_raw(" at level 2"); break;
            case 3: uart_write_str_raw(" at level 3"); break;
        }
    }

    // dump registers
    uart_write_str_raw("\r\nESR_EL1 ");
    uart_write_hex_raw(esr);
    uart_write_str_raw(" ELR_EL1 ");
    uart_write_hex_raw(elr);
    uart_write_str_raw("\r\nSPSR_EL1 ");
    uart_write_hex_raw(spsr);
    uart_write_str_raw(" FAR_EL1 ");
    uart_write_hex_raw(far);

    while(1){};

}


///////////////////////////// task related //////////////////////////////
void task_enqueue(void (*callback)(void *), void *arg, int priority) {
    // 1. 找一個空的 task slot
    task_t *new_task = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!task_used[i]) {
            task_used[i] = 1;
            new_task = &task_pool[i];
            break;
        }
    }
    if (!new_task) {
        uart_write_str_raw("\r\n[ERROR] No free task slot");
        return;
    }

    // 2. 設定 task 的內容
    new_task->callback = callback;
    new_task->arg = arg;
    new_task->priority = priority;
    new_task->next = 0;

    // 3. 插入排序：priority 小的在前
    if (!task_queue || priority < task_queue->priority) {
        new_task->next = task_queue;
        task_queue = new_task;
    } else {
        task_t *cur = task_queue;
        while (cur->next && cur->next->priority <= priority) {
            cur = cur->next;
        }
        new_task->next = cur->next;
        cur->next = new_task;
    }


    // 如果有比現在執行中的 task 還高 priority，則主動執行 task loop
    // 因為在 task_execute_loop() 裡面有 disable interrupt 才去動 task_queue，所以不會重複做
    // 這邊直接用 task_execute_loop() 的話，有可能會出現 task2->task1->task3->task2 的情況(pri3 的反而比 pri2 早完成)
    if (executing_task && priority < current_priority) {
        task_execute_one();
    }
}


void task_execute_loop() {
    while (task_queue) {
        // 1. 關閉 IRQ 以保護 queue，不然怕 enqueue 那邊同時被叫到直接會出事
        disable_interrupt();

        // 2. 取出最高優先權的 task
        task_t *task = task_queue;
        task_queue = task_queue->next;

        // 3. 執行前先釋放 pool slot
        for (int i = 0; i < MAX_TASKS; i++) {
            if (&task_pool[i] == task) {
                task_used[i] = 0;
                break;
            }
        }

        current_priority = task->priority;
        // 4. 開啟 IRQ 以支援 nested interrupt / preempt
        enable_interrupt();

        // 5. 執行該任務（可能會再次 enqueue）
        task->callback(task->arg);
    }
}

void task_execute_one() {
    if (!task_queue) return;

    disable_interrupt();
    task_t *task = task_queue;
    task_queue = task->next;

    for (int i = 0; i < MAX_TASKS; i++) {
        if (&task_pool[i] == task) {
            task_used[i] = 0;
            break;
        }
    }

    current_priority = task->priority;
    enable_interrupt();

    task->callback(task->arg);
}
