//檔名之所以叫做 exception_s.S 是因為如果步加上 _s，那 exception.S 跟 exception.c 產生的 obj 都會叫做 exception.o，會被覆蓋
//要在 .S 檔案裡面 include 的話，header file 裡面只能有 #define，不能有 C function 宣告和 typedef
#include "ventry.h"
.section ".text"

.extern  default_handler
.extern  lower_sync_handler
.extern  handle_el1_irq
.extern  syscall_handler


// 沒有傳入參數，並且因為觸發 exception 時 CPU 會自動把 sp 切換到 current thread 的 sp_el1，所以不用我手動切
// save general registers to stack (這邊應該是指 SP_EL0)
.macro save_all_preempt
    sub     sp, sp, #18*16

    // Save general purpose registers x0 - x30
    stp x0, x1, [sp ,16 * 0]        // 從 stack 的 lowest mem 開始存入 x0, x1
    stp x2, x3, [sp ,16 * 1]          
    stp x4, x5, [sp ,16 * 2]
    stp x6, x7, [sp ,16 * 3]
    stp x8, x9, [sp ,16 * 4]
    stp x10, x11, [sp ,16 * 5]
    stp x12, x13, [sp ,16 * 6]
    stp x14, x15, [sp ,16 * 7]
    stp x16, x17, [sp ,16 * 8]
    stp x18, x19, [sp ,16 * 9]
    stp x20, x21, [sp ,16 * 10]
    stp x22, x23, [sp ,16 * 11]
    stp x24, x25, [sp ,16 * 12]
    stp x26, x27, [sp ,16 * 13]
    stp x28, x29, [sp ,16 * 14]
    str x30, [sp, 16*15]       // save x30, lr

    mov x9, #1
    str x9, [sp, 16*15+8]      // save is_preemp

    mrs x10, spsr_el1
    str x10, [sp, #16 * 16]          // Save spsr_el1
    mrs x11, elr_el1
    str x11, [sp, #16 * 16+8]          // Save elr_el1
    mrs x12, sp_el0
    str x12, [sp, #16 * 17]          // Save sp_el0

    // Save callee-saved registers into prev，更新當前 thread_t->kernel_sp 為 sp (sp_el1)
    mrs x14, tpidr_el1
    mov x15, sp
    str x15, [x14]
    
    //以下在儲存 return_via_eretx 的值
    // 取得 thread_t->is_user_mode
    ldr w16, [x14, #48]
    cmp w16, #1
    b.eq 1f

    // 如果是 kernel thread，那 return_via_eret 就取決於 is_preempt (x9)
    str x9, [sp, 16 * 17 + 8]
    b 2f

    //如果是 user thread，那 return_via_eret 無條件 = 1
    1:
    mov x17, #1
    str x17, [sp, 16 * 17 + 8] 
    2: 

.endm


// 沒有傳入餐數
// 只要 save_all_preempt 的時候有保留好 spsr 和 sp_el0，那 load all 只要負責 load 回來後 eret 就可以了，就會自動跳去 EL0 並且使用 sp_el0
// load general registers from stack (這邊應該是指 SP_EL0)
.global load_all_preempt
load_all_preempt:
    //ldp: load pair, 從記憶體中同時加載兩個值到寄存器
    // 從 lowest mem (stack 頂端) 開始把東西拉回來，最後再把 sp 復原
    // Restore general-purpose registers
    ldp x0, x1, [sp ,16 * 0]
    ldp x2, x3, [sp ,16 * 1]
    ldp x4, x5, [sp ,16 * 2]
    ldp x8, x9, [sp ,16 * 4]
    ldp x10, x11, [sp ,16 * 5]
    ldp x12, x13, [sp ,16 * 6]
    ldp x14, x15, [sp ,16 * 7]
    ldp x16, x17, [sp ,16 * 8]
    ldp x18, x19, [sp ,16 * 9] 
    ldp x20, x21, [sp ,16 * 10]
    ldp x22, x23, [sp ,16 * 11]
    ldp x24, x25, [sp ,16 * 12]
    ldp x26, x27, [sp ,16 * 13]
    ldp x28, x29, [sp ,16 * 14]
    ldp x30, x2, [sp, 16 * 15]        //get lr, is_preempt

    ldr x3, [sp, #16 * 16]
    msr spsr_el1, x3             // Restore spsr_el1
    ldr x4, [sp, #16 * 16+8]
    msr elr_el1, x4             // Restore elr_el
    ldr x5, [sp, #16 * 17]
    msr sp_el0, x5             // Restore sp_el0
    ldr x6, [sp, #16 * 17+8]        //restore return_via_eret


    add sp, sp, #18*16          // Reclaim stack space

    // 判斷 return_via_eret 是 0 還是 1
    cbz x6, 1f                   // if (return_via_eret == 0) jump to label 1
    eret                         // return_via_eret != 0，直接用 eret，也就是用 elr_el1
1:
    ret                          // return_via_eret == 0，用 ret，也就是用 lr





.macro handle_invalid_entry type
	save_all_preempt
	mov	    x0, #\type
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
	bl	default_handler
	b	err_hang
.endm

.macro	ventry	label
	.align	7
	b	\label
.endm


.global get_el_s
get_el_s:
    #  CurrentEl 是 AArch64中的一個 System Register，用來取得目前的 Exception Level
    # 返回 EL 數值 (0, 1, 2, 3)
    mrs x0, CurrentEl
    lsr x0, x0, #2                 // 右移兩位，讓 EL 值對齊到 x0[1:0]，以便正確解析。
    ret

.global from_el2_to_el1
from_el2_to_el1:
    //為甚麼都是操作 el2 register? 因為我們還位於 EL2，需要透過 EL2 來 config EL1 並切換到 EL1
    
    //1. 設定 HCR_EL2，Hypervisor Configuration Register，用來控制 EL2 對 EL1 和 EL0 的行為
    mov x0, (1 << 31)   //HCR_EL2.RW = 1 表示 EL1 使用 AArch64，而非 AArch32。
    msr hcr_el2, x0     //將 x0 的值放到 system register(hcr_el2) 裡面


    //2. Saved Program Status Registers 設定為 0x3c5 (0b0011 1100 0101)
    mov x0, 0x3c5               //basic part
                            // M[3:0] 是 0101 代表 return 後要使用 EL1h。
                            // 010 是 EL1 的意思
                            //而 LSB 的 1 代表 SPSel = 1，也就是要使用 SP_ELx 作為 SP (在這邊是切換到 EL1 所以就是 SP_EL1)
                        // 中間的 1111 代表 DAIF(Debug, SError, IRQ, FIQ) 四種異常通通 disabled
    msr spsr_el2, x0


    //3. 設定 ELR (Exception Link Registers) - 保存例外返回的位置
    msr elr_el2, lr     //lr 是 link register，AArch64 中也就是 x30
                        //當 from_el2_to_el1 被呼叫時，硬體會自動把返回位址(就是 bl from_el2_to_el1 的下一行指令拉)儲存在 lr（X30）中
                        //因此在執行 msr elr_el2, lr 時，lr 已經包含了返回 EL1 後要執行的指令位址。
    eret                // return to EL1 (也)
















.globl enable_irq
enable_irq:
    msr    daifclr, #2
    ret

.globl disable_irq
disable_irq:
    msr    daifset, #2
    ret
/*
 * Exception vectors.
*/
// .align 11 : 確保下一條指令的地址是 2^11 (0x800) 的倍數，.align will pad 0
.align 11                   // vector table should be aligned to 0x800，這是 AArch64 的規定
.global vector
vector:
    //Exception from the currentEL while using SP_EL0, 當 CPU 本來就在 EL1，而且是用 SP_EL0，發生 synchronous exception 時會跳到這個 entry。
	ventry	sync_invalid_el1t			// Synchronous EL1t
	ventry	irq_invalid_el1t			// IRQ EL1t
	ventry	fiq_invalid_el1t			// FIQ EL1t
	ventry	error_invalid_el1t			// Error EL1t

    //Exception from the currentEL while using SP_ELx
	ventry	sync_invalid_el1h			// Synchronous EL1h
	ventry	general_irq_handler			// IRQ EL1h, include EL1->EL1 的 timer!!
	ventry	fiq_invalid_el1h			// FIQ EL1h
	ventry	error_invalid_el1h			// Error EL1h

    //Exception from a lower EL and at least one lower EL is AARCH64.
	ventry	sync_el0_64			        // Synchronous 64-bit EL0, include svc!!
	ventry	general_irq_handler			// IRQ 64-bit EL0, include EL0->EL1 的 timer interrupt，原本是 irq_invalid_el0_64 財隊但我懶得拆成兩組寫 timer
	ventry	fiq_invalid_el0_64			// FIQ 64-bit EL0
	ventry	error_invalid_el0_64		// Error 64-bit EL0

    //Exception from a lower EL and at least one lower EL is AARCH32.
	ventry	sync_invalid_el0_32			// Synchronous 32-bit EL0
	ventry	irq_invalid_el0_32			// IRQ 32-bit EL0
	ventry	fiq_invalid_el0_32			// FIQ 32-bit EL0
	ventry	error_invalid_el0_32		// Error 32-bit EL0

sync_invalid_el1t:
	handle_invalid_entry  SYNC_INVALID_EL1t
irq_invalid_el1t:
	handle_invalid_entry  IRQ_INVALID_EL1t
fiq_invalid_el1t:
	handle_invalid_entry  FIQ_INVALID_EL1t
error_invalid_el1t:
	handle_invalid_entry  ERROR_INVALID_EL1t

sync_invalid_el1h:
	handle_invalid_entry  SYNC_INVALID_EL1h
general_irq_handler:
    //handle_invalid_entry  IRQ_INVALID_EL1h
    save_all_preempt
    bl handle_el1_irq           // function in exception.c
    bl load_all_preempt
fiq_invalid_el1h:
	handle_invalid_entry  FIQ_INVALID_EL1h
error_invalid_el1h:
	handle_invalid_entry  ERROR_INVALID_EL1h

sync_el0_64:
    // 判斷是不是 svc（system call）
    mrs x9, esr_el1
    lsr x9, x9, #26       // 把 EC (Exception Class) 移到最低 bit
    cmp x9, #0x15         // 0x15 是 SVC from AArch64
    b.ne other_el0_sync   // 如果不是 system call，跳去其他處理

    // 如果是 system call，呼叫 save_all_preempt
    save_all_preempt

    // 把 sp (剛 save 完的) 傳給 handler
    mov x0, sp
    bl syscall_handler

    // 處理完後 restore
    bl load_all_preempt

other_el0_sync:
    save_all_preempt
    mov     x0, #0
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    bl lower_sync_handler    // function in exception.c
    bl load_all_preempt

irq_invalid_el0_64:         //其實沒必要，因為跟 EL1->EL1 的 general_irq_handler 合併了。但我其實不確定應該要分兩組寫比較好還是像我這樣跳到同一個地方
	handle_invalid_entry  IRQ_INVALID_EL0_64
fiq_invalid_el0_64:
	handle_invalid_entry  FIQ_INVALID_EL0_64
error_invalid_el0_64:
	handle_invalid_entry  ERROR_INVALID_EL0_64

sync_invalid_el0_32:
	handle_invalid_entry  SYNC_INVALID_EL0_32
irq_invalid_el0_32:
	handle_invalid_entry  IRQ_INVALID_EL0_32
fiq_invalid_el0_32:
	handle_invalid_entry  FIQ_INVALID_EL0_32
error_invalid_el0_32:
	handle_invalid_entry  ERROR_INVALID_EL0_32




err_hang:
    wfe
    b err_hang