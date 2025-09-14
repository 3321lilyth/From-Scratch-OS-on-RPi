#include "exception.h"
void enable_interrupt() { asm volatile("msr DAIFClr, 0xf"); }   // 0xf = 0b1111，代表清除 D, A, I, F，允許所有中斷發生
void disable_interrupt() { asm volatile("msr DAIFSet, 0xf"); }  // 0xf = 0b1111，代表設置 D, A, I, F，禁用所有中斷


//lab3: The design of system calls is left to the next lab. Now, your kernel 
//only needs to print the content of spsr_el1, elr_el1, and esr_el1 in the exception handler.
// void default_handler(){
//     disable_interrupt();
//     unsigned long esr, elr, spsr;
//     asm volatile("mrs %0, esr_el1" : "=r"(esr)); // 讀取 ESR_EL1，Exception Syndrome Register ESR- 保存觸發exection的原因
//     asm volatile("mrs %0, elr_el1" : "=r"(elr)); // 讀取 ELR_EL1，Exception Link Registers，保存例外返回的 addr
//     asm volatile("mrs %0, spsr_el1" : "=r"(spsr)); // 讀取 SPSR_EL1，(Saved Program Status Registers) - 在例外發生時用來儲存PE的狀態



//     //在 lab3 中印出來應該要是
//     // spsr_el1: 0x000003c0，這是助教讓我們設定的，refer to cpio.c
//     // elr_el1: 0x00200000，這是我們自己設定的 user program 位置
//     // esr_el1: 0x02000000，MSB[31:26] 的 EC=0x15=21 就代表 svc instruction；ISS = 0，代表 svc 0，也就是 System Call ID 為 0
//     uart_write_str("\r\nspsr_el1: ");
//     uart_write_hex(spsr);
//     uart_write_str("\r\nelr_el1: ");
//     uart_write_hex(elr);
//     uart_write_str("\r\nesr_el1: ");
//     uart_write_hex(esr);
//     enable_interrupt();
// }

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

void handle_irq(void){
    uart_write_str("\r\n-------- enter handle_irq -------- ");
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