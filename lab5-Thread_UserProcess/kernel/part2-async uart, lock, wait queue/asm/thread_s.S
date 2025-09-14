.global switch_to_first
.global switch_to
.global get_current



// x0 = next thread_t*
switch_to_first:
    cbz x0, panic_switch_null
    
    // 只 Load next，不儲存 prev，要跳去執行第一個 task 時要呼叫這個版本的 switch to
    ldp x19, x20, [x0, 16 * 0]
    ldp x21, x22, [x0, 16 * 1]
    ldp x23, x24, [x0, 16 * 2]
    ldp x25, x26, [x0, 16 * 3]
    ldp x27, x28, [x0, 16 * 4]
    ldp fp, lr,   [x0, 16 * 5]
    ldr x9, [x0, 16 * 6]

    cbz x9, panic_switch_invalid_sp
    mov sp, x9

    msr tpidr_el1, x0
    ret


// x0 = prev thread_t*
// x1 = next thread_t*
switch_to:
    cbz x0, panic_switch_null
    cbz x1, panic_switch_null
    
    // Save callee-saved registers into prev
    stp x19, x20, [x0, 16 * 0]
    stp x21, x22, [x0, 16 * 1]
    stp x23, x24, [x0, 16 * 2]
    stp x25, x26, [x0, 16 * 3]
    stp x27, x28, [x0, 16 * 4]
    stp fp, lr,   [x0, 16 * 5]
    mov x9, sp                  //複製sp到x9
    str x9, [x0, 16 * 6]        //push sp into thread stack(t->context.sp)

    // Load callee-saved registers from next
    ldp x19, x20, [x1, 16 * 0]
    ldp x21, x22, [x1, 16 * 1]
    ldp x23, x24, [x1, 16 * 2]
    ldp x25, x26, [x1, 16 * 3]
    ldp x27, x28, [x1, 16 * 4]
    ldp fp, lr,   [x1, 16 * 5]
    ldr x9, [x1, 16 * 6]

    cbz x9, panic_switch_invalid_sp
    mov sp, x9

    // set current thread
    msr tpidr_el1, x1
    ret                     //跳到 next thread 上一次被 switch out 時的 lr (link register) 位置執行
        //如果是剛從 thread_create() 創好的新 thread, 那 lr 是 func的entry地址（foo() 函式），所以第一次切換到這個 thread 就是從 foo() 開始。
        //如果是從 sleep()、或別的地方 switch_out 的 thread，會從當初被 switch out 的 下一條指令開始繼續。
get_current:
    mrs x0, tpidr_el1
    ret
panic_switch_null:
    bl panic_null_thread

panic_switch_invalid_sp:
    bl panic_invalid_sp