
// x0 = prev thread_t* (第一個 element 就是 sp)
// x1 = next thread_t*(第一個 element 就是 sp)
.global switch_to
switch_to:
    cbz x0, panic_switch_null
    cbz x1, panic_switch_null
    
    //注意!!! 這段被我搬到 save_all_preempt 裡面去了!
    //這樣 handler 裡面的 function call 造成的 stack 變動後的 sp 才不會被儲存到 thread_t 裡面
    //如果 prev thread_t 裡面的是 "handler 過後的 sp 而不是 save_all_preempt 一結束的sp"，那 load_all 時就會從錯誤的地方開始 load register 
    // Save callee-saved registers into prev
    // mov x9, sp
    // str x9, [x0]        // 存到 thread_t* offset 0 的地方，也就是 sp (thread_t 第一個 element 就是 sp)

    
    // Load callee-saved registers from next
    ldr x9, [x1]
    cbz x9, panic_switch_invalid_sp
    mov sp, x9

    // set current thread
    msr tpidr_el1, x1
    ret
    
// .global switch_to2
// switch_to2:
//     stp x19, x20, [x0, 16 * 0]
//     stp x21, x22, [x0, 16 * 1]
//     stp x23, x24, [x0, 16 * 2]
//     stp x25, x26, [x0, 16 * 3]
//     stp x27, x28, [x0, 16 * 4]
//     stp fp, lr,   [x0, 16 * 5]
//     mov x9, sp
//     str x9, [x0, 16 * 6]
// 
//     ldp x19, x20, [x1, 16 * 0]
//     ldp x21, x22, [x1, 16 * 1]
//     ldp x23, x24, [x1, 16 * 2]
//     ldp x25, x26, [x1, 16 * 3]
//     ldp x27, x28, [x1, 16 * 4]
//     ldp fp, lr,   [x1, 16 * 5]
//     ldr x9, [x1, 16 * 6]
//     mov sp, x9
//     msr tpidr_el1, x1
//     ret



// x0 = next thread_t*
// 因為是第一個，所以進來的時候一定是 EL1
.global cxtsw_complete_first
cxtsw_complete_first:
    cbz x0, panic_switch_null
    ldr x9, [x0]            //因為 thread_t 的第一個 element 就是 sp，所以這就是把第一個要執行的 thread 的 sp 搬到 x9
                            //目前的 sp 是 kernel 的 sp，一開始在 boot.S 設定在 0x80000，所以應該會比 ox80000 小一點，比如 0x7fd50
                            

    cbz x9, panic_switch_invalid_sp
    mov sp, x9

    msr tpidr_el1, x0       // Update TPIDR_EL1 (current thread pointer)
    
    ldp x0, x1, [sp, 16*0]
    ldp x2, x3, [sp, 16*1]
    ldp x4, x5, [sp, 16*2]
    ldp x6, x7, [sp, 16*3]
    ldp x8, x9, [sp, 16*4]
    ldp x10, x11, [sp, 16*5]
    ldp x12, x13, [sp, 16*6]
    ldp x14, x15, [sp, 16*7]
    ldp x16, x17, [sp, 16*8]
    ldp x18, x19, [sp, 16*9]
    ldp x20, x21, [sp, 16*10]
    ldp x22, x23, [sp, 16*11]
    ldp x24, x25, [sp, 16*12]
    ldp x26, x27, [sp, 16*13]
    ldp x28, x29, [sp, 16*14]
    ldp x30, x2, [sp, 16 * 15]  // lr, get is_preempt to x2，其實沒必要拉，反正一開始一定是 ret 就好，func addr 是放在 lr

    ldr x3, [sp, #16 * 16]
    msr spsr_el1, x3             // Restore spsr_el1
    ldr x4, [sp, #16 * 16+8]
    msr elr_el1, x4             // Restore elr_el1
    ldr x5, [sp, #16 * 17]
    msr sp_el0, x5             // initial sp_el0
    ldr x6, [sp, #16 * 17 + 8]  //restore return_via_eret

    add sp, sp, #18*16          // Reclaim stack space
    
    // 判斷 return_via_eret 是 0 還是 1
    cbz x6, 1f                   // if (return_via_eret == 0) jump to label 1
    eret                         // return_via_eret != 0，直接用 eret，也就是用 elr_el1
1:
    ret                          // return_via_eret == 0，用 ret，也就是用 lr



// 把三個 fucntion 合併成一個的原因看 notion
// x0 = prev thread_t* (第一個 element 就是 sp)
// x1 = next thread_t*
// 因為 x0 x1 這兩個是參數，所以注意千萬不能當作暫時用的 register !!!!
// 我先預設進到這邊的時候已經會是 sp_el1 而不是 sp_el0，如果 user thread 要用到這些功能一定要透過 system call
.global cxtsw_complete
cxtsw_complete:
    cbz x0, panic_switch_null
    cbz x1, panic_switch_null
    
    sub sp, sp, #18*16                  //sp 一定要對齊 16，不可以只對齊 8 喔

    //1. 先做 save_all_complete 的工作，此時的 sp 還是舊的 sp    
    stp x0, x1, [sp, 16*0]
    stp x2, x3, [sp, 16*1]
    stp x4, x5, [sp, 16*2]
    stp x6, x7, [sp, 16*3]
    stp x8, x9, [sp, 16*4]
    stp x10, x11, [sp, 16*5]
    stp x12, x13, [sp, 16*6]
    stp x14, x15, [sp, 16*7]
    stp x16, x17, [sp, 16*8]
    stp x18, x19, [sp, 16*9]
    stp x20, x21, [sp, 16*10]
    stp x22, x23, [sp, 16*11]
    stp x24, x25, [sp, 16*12]
    stp x26, x27, [sp, 16*13]
    stp x28, x29, [sp, 16*14]
    str x30, [sp, 16*15]       // save x30, lr

    mov x9, #0
    str x9, [sp, 16 * 15 + 8]      //lr, is_preempt

    mrs x10, spsr_el1
    str x10, [sp, #16 * 16]          // Save spsr_el1
    mrs x11, elr_el1
    str x11, [sp, #16 * 16+8]          // Save elr_el1
    mrs x12, sp_el0
    str x12, [sp, #16 * 17]          // Save sp_el0


    //1.5 找到 return_via_eret 的值
    ldr w13, [x0, #48]          // w13 = prev->is_user_mode
    cmp w13, #1
    b.eq 1f

    // 如果是 kernel thread，那 return_via_eret 就取決於 is_preempt (x9)
    str x9, [sp, #16*17 + 8]
    b 2f

    //如果是 user thread，那 return_via_eret 無條件 = 1
    1:
    mov x14, #1
    str x14, [sp, #16*17 + 8]

    2:
    
    
    
    
    
    //2. 在來做 switch_to 的工作
    // Save sp(應該是sp_el1) to prev->kernel_sp
    mov x9, sp
    str x9, [x0]                     // 存到 thread_t* offset 0 的地方，也就是 sp (thread_t 第一個 element 就是 sp)
    // Load sp from next->sp
    ldr x9, [x1]
    cbz x9, panic_switch_invalid_sp
    mov sp, x9
    msr tpidr_el1, x1               // Update TPIDR_EL1 (current thread pointer)


    //3. 最後是 load_all_complete 的工作，這時候用的是新的 sp(sp_el1)
    ldp x0, x1, [sp, 16*0]
    ldp x2, x3, [sp, 16*1]
    ldp x4, x5, [sp, 16*2]
    ldp x6, x7, [sp, 16*3]
    ldp x8, x9, [sp, 16*4]
    ldp x10, x11, [sp, 16*5]
    ldp x12, x13, [sp, 16*6]
    ldp x14, x15, [sp, 16*7]
    ldp x16, x17, [sp, 16*8]
    ldp x18, x19, [sp, 16*9]
    ldp x20, x21, [sp, 16*10]
    ldp x22, x23, [sp, 16*11]
    ldp x24, x25, [sp, 16*12]
    ldp x26, x27, [sp, 16*13]
    ldp x28, x29, [sp, 16*14]
    ldp x30, x2, [sp, 16 * 15]        //get lr, is_preempt

    ldr x3, [sp, #16 * 16]
    msr spsr_el1, x3             // Restore spsr_el1
    ldr x4, [sp, #16 * 16+8]
    msr elr_el1, x4             // Restore elr_el1
    ldr x5, [sp, #16 * 17]
    msr sp_el0, x5             // Restore sp_el0
    ldr x6, [sp, #16 * 17 + 8]  //restore return_via_eret

    add sp, sp, #18*16          // Reclaim stack space

    // 判斷 return_via_eret(上一次離開這個 thread 的時候的情況) 是 0 還是 1
    cbz x6, 1f                   // if (return_via_eret == 0) jump to label 1
    eret                         // return_via_eret != 0，直接用 eret，也就是用 elr_el1
1:
    ret                          // return_via_eret == 0，用 ret，也就是用 lr




.global get_current
get_current:
    mrs x0, tpidr_el1
    ret
panic_switch_null:
    bl panic_null_thread

panic_switch_invalid_sp:
    bl panic_invalid_sp