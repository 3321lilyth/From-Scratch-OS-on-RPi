#include "exception.h"

void enable_interrupt() { asm volatile("msr DAIFClr, 0xf"); }   // 0xf = 0b1111，代表清除 D, A, I, F，允許所有中斷發生
void disable_interrupt() { asm volatile("msr DAIFSet, 0xf"); }  // 0xf = 0b1111，代表設置 D, A, I, F，禁用所有中斷
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


void handle_el1_irq() {

    // 1. 讀取 `CORE0_INTERRUPT_SOURCE`
    unsigned int core_irq_source = *(volatile unsigned int*) CORE0_INTERRUPT_SOURCE;

    // 2. 判斷是否是 `Core Timer IRQ`
    if (core_irq_source & CORE_TIMER_IRQ_BIT) {
        uart_write_str("\r\n[IRQ] Core Timer Interrupt detected.");
        two_second_core_timer_handler();
        return;  // Core Timer IRQ 已處理完畢，直接返回
    }

    // 3. 讀取 `Pending IRQ Registers`
    unsigned int pending_1 = *(volatile unsigned int*) IRQ_PENDING_1;

    // 4. 判斷是否是 `Mini UART IRQ`
    if (pending_1 & IRQ_AUX) {
        uart_write_str("\r\n[IRQ] Mini UART Interrupt detected.");
        //uart_irq_handler(); //lab3 later part
        return;  // UART IRQ 已處理完畢，直接返回
    }

    // 5. 若都不是已知 IRQ，輸出錯誤訊息
    uart_write_str("\r\n[IRQ] Unknown Interrupt Source.");
}




void lower_sync_handler(unsigned long type, unsigned long esr, unsigned long elr, unsigned long spsr){
    uart_write_str("\r\n-------- enter lower_sync_handler -------- ");
    uart_write_str("\r\ntype = ");
    uart_write_str(entry_error_messages[type]);
    // print out interruption type
    // switch(type) {
    //     case 0: uart_write_str("Synchronous"); break;
    //     case 1: uart_write_str("IRQ"); break;
    //     case 2: uart_write_str("FIQ"); break;
    //     case 3: uart_write_str("SError"); break;
    // }

    // decode exception type (some, not all. See ARM DDI0487B_b chapter D10.2.28)
    uart_write_str("; exception type = ");
    switch(esr>>26) {
        case 0b000000: uart_write_str("Unknown"); break;
        case 0b000001: uart_write_str("Trapped WFI/WFE"); break;
        case 0b001110: uart_write_str("Illegal execution"); break;
        case 0b010101: uart_write_str("System call"); break;
        case 0b100000: uart_write_str("Instruction abort, lower EL"); break;
        case 0b100001: uart_write_str("Instruction abort, same EL"); break;
        case 0b100010: uart_write_str("Instruction alignment fault"); break;
        case 0b100100: uart_write_str("Data abort, lower EL"); break;
        case 0b100101: uart_write_str("Data abort, same EL"); break;
        case 0b100110: uart_write_str("Stack alignment fault"); break;
        case 0b101100: uart_write_str("Floating point"); break;
        default: uart_write_str("Unknown"); break;
    }

    // decode data abort cause
    uart_write_str("; cause = ");
    if(esr>>26==0b100100 || esr>>26==0b100101) {
        uart_write_str(", ");
        switch((esr>>2)&0x3) {
            case 0: uart_write_str("Address size fault"); break;
            case 1: uart_write_str("Translation fault"); break;
            case 2: uart_write_str("Access flag fault"); break;
            case 3: uart_write_str("Permission fault"); break;
        }
        switch(esr&0x3) {
            case 0: uart_write_str(" at level 0"); break;
            case 1: uart_write_str(" at level 1"); break;
            case 2: uart_write_str(" at level 2"); break;
            case 3: uart_write_str(" at level 3"); break;
        }
    }

    // dump registers
    uart_write_str("\r\nESR_EL1 ");
    uart_write_hex(esr);
    uart_write_str("\r\nELR_EL1 ");
    uart_write_hex(elr);
    uart_write_str("\r\nSPSR_EL1 ");
    uart_write_hex(spsr);
}

//lab3: The design of system calls is left to the next lab. Now, your kernel 
//only needs to print the content of spsr_el1, elr_el1, and esr_el1 in the exception handler.
void default_handler(unsigned long type, unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long far){
    uart_write_str("\r\n-------- enter default_handler -------- ");
    uart_write_str("\r\ntype = ");
    uart_write_str(entry_error_messages[type]);
    // print out interruption type
    // switch(type) {
    //     case 0: uart_write_str("Synchronous"); break;
    //     case 1: uart_write_str("IRQ"); break;
    //     case 2: uart_write_str("FIQ"); break;
    //     case 3: uart_write_str("SError"); break;
    // }

    // decode exception type (some, not all. See ARM DDI0487B_b chapter D10.2.28)
    uart_write_str("; exception type = ");
    switch(esr>>26) {
        case 0b000000: uart_write_str("Unknown"); break;
        case 0b000001: uart_write_str("Trapped WFI/WFE"); break;
        case 0b001110: uart_write_str("Illegal execution"); break;
        case 0b010101: uart_write_str("System call"); break;
        case 0b100000: uart_write_str("Instruction abort, lower EL"); break;
        case 0b100001: uart_write_str("Instruction abort, same EL"); break;
        case 0b100010: uart_write_str("Instruction alignment fault"); break;
        case 0b100100: uart_write_str("Data abort, lower EL"); break;
        case 0b100101: uart_write_str("Data abort, same EL"); break;
        case 0b100110: uart_write_str("Stack alignment fault"); break;
        case 0b101100: uart_write_str("Floating point"); break;
        default: uart_write_str("Unknown"); break;
    }

    // decode data abort cause
    uart_write_str("; cause = ");
    if(esr>>26==0b100100 || esr>>26==0b100101) {
        uart_write_str(", ");
        switch((esr>>2)&0x3) {
            case 0: uart_write_str("Address size fault"); break;
            case 1: uart_write_str("Translation fault"); break;
            case 2: uart_write_str("Access flag fault"); break;
            case 3: uart_write_str("Permission fault"); break;
        }
        switch(esr&0x3) {
            case 0: uart_write_str(" at level 0"); break;
            case 1: uart_write_str(" at level 1"); break;
            case 2: uart_write_str(" at level 2"); break;
            case 3: uart_write_str(" at level 3"); break;
        }
    }

    // dump registers
    uart_write_str("\r\nESR_EL1 ");
    uart_write_hex(esr);
    uart_write_str(" ELR_EL1 ");
    uart_write_hex(elr);
    uart_write_str("\r\n SPSR_EL1 ");
    uart_write_hex(spsr);
    uart_write_str(" FAR_EL1 ");
    uart_write_hex(far);

    while(1){};

}