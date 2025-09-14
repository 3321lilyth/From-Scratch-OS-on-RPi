.section ".text.boot"

.global _start

.extern from_el2_to_el1
.extern vectors
.extern core_timer_enable
.extern core_timer_handler

_start:
    mov     x21, x0             //part6 把 x0 的值，也就是 dtb 的 address 保留起來

    mrs     x1, mpidr_el1       // read cpu id, mpidr_el1 is Multiprocessor Affinity Register
    and     x1, x1, #3          // # is mmediate value, 所以這條代表保留 x1 最低的兩位數
    cbz     x1, master_cpu      //Compare and Branch on Zero，只有 CPU id 為 0 時才會跳到 master

// cpu id > 0, stop
hang:  
    wfe
    b       hang                //<label>f: forward, <label>b: backward

// cpu id == 0
master_cpu:  
    //lab3
    bl     from_el2_to_el1
    
    //lab3: set_exception_vector_table
    adr x0, vector
    msr vbar_el1, x0

    //lab1: stack pointer setting
    ldr     x1, =_start         //把 _start 的地址載入 x1。
    mov     sp, x1              //讓 stack 從 0x80000 開始往 low mem 增長

    //lab1: clear bss, __bss_start 跟 __bss_end 都是在 linker script 裡定義 
    ldr     x2, =__bss_start
    ldr     x1, =__bss_end
    sub     x1, x1, x2          //get bss size (in bit)

memzero:  
    cbz     x1, run
    str     xzr, [x2], #8   
        //Store Register，將register的值存入記憶體，xzr是zero register
        //Post-index addressing: 寫入x2指向的位址後，x2自動加 8（8 bytes per word）。
    sub     x1, x1, #8          //count-- （8 bytes per word）
    cbnz    x1, memzero         //Compare and Branch if NonZero

run:  
    bl      main                //Branch with Link
    b       hang                // for failsafe, halt this core too
