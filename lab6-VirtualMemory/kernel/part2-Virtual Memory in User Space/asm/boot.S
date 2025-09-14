.section ".text.boot"

.global _start

//.extern from_el2_to_el1
.extern core_timer_enable
.extern core_timer_handler

#include "vm_config.h"

_start:                         //開啟 virtual memory 後位於 VA=0xffff000000080000 
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

vm_setting:
    // TCR
    ldr x0, =TCR_CONFIG_DEFAULT
    msr tcr_el1, x0

    // MAIR
    ldr x0, =((MAIR_DEVICE_nGnRnE << (MAIR_IDX_DEVICE_nGnRnE * 8)) | \
              (MAIR_NORMAL_NOCACHE << (MAIR_IDX_NORMAL_NOCACHE * 8)))
    msr mair_el1, x0

    // PGD @ 0x1000, PUD @ 0x2000，我有在 buddy system 裡面 reserve 了
    ldr x0, =BOOT_PGD_ADDR
    ldr x1, =BOOT_PUD_ADDR
    // mov x0, 0x1000
    // mov x1, 0x2000

    // PGD[0] -> PUD
    ldr x2, =BOOT_PGD_ATTR
    orr x2, x1, x2          // x2 = x1|x2 = PUD 的地址 | 0b11, combine PA with attribute.
    str x2, [x0]

    // 設定 PUD[0]，low address space 時是把 VA 0x0000_0000~0x3FFF_FFFF → PA 0x00000000~0x3FFF_FFFF（1GB）
    // high address space 時是把 VA 0xffff000000000000 ~ 0xffff00003fffffff  → PA 0x00000000~0x3FFF_FFFF（1GB）
    ldr x2, =BOOT_PUD_ATTR
    mov x3, 0x00000000
    orr x3, x2, x3
    str x3, [x1]

    // 設定 PUD[1]，low address space 時是把 VA 0x4000_0000~0x7FFF_FFFF → PA 0x40000000~0x7FFF_FFFF（1GB）
    // high address space 時是 VA 0xffff0000_40000000 ~ 0xffff0000_7fffffff → PA 0x40000000~0x7FFF_FFFF（1GB）
    mov x3, 0x40000000
    orr x3, x2, x3
    str x3, [x1, #8]

    // TTBR0_EL1 載入 PGD
    msr ttbr0_el1, x0
    msr ttbr1_el1, x0 // also load PGD to the upper translation based register.

    // enable MMU
    mrs x2, sctlr_el1
    orr x2, x2, #1
    msr sctlr_el1, x2

    ldr x2, = other_master_cpu // indirect branch to the virtual address
    br x2

other_master_cpu:
    //lab3: set exception vector table
    adr x0, vector
    msr vbar_el1, x0

    //lab1: stack pointer setting
    ldr     x1, =_start         //把 _start 的地址載入 x1。有 MMU 後會是 VA 0xffff_0000_0008_0000
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
