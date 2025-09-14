.section ".text.boot"

.global _start
.extern uart_init
.extern bootloader_main
.extern __bss_start
.extern __bss_end

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
	//這邊 code 就是 lab1 再做一樣的事情
    //stack pointer setting
    ldr     x1, =0x60000 
    mov     sp, x1              //讓 stack 從 0x60000 開始往 low mem 增長，而不是原本的 0x80000，不然可能會跟 bootloader 撞到

    // clear bss, __bss_start 跟 __bss_end 都是在 linker script 裡定義 
    ldr     x2, =__bss_start
    ldr     x1, =__bss_end
    sub     x1, x1, x2          //get bss size (in bit)

    
memzero:  
    cbz     x1, uart_initial
    str     xzr, [x2], #8   
        //Store Register，將register的值存入記憶體，xzr是zero register
        //Post-index addressing: 寫入x0指向的位址後，x0自動加 8（8 bytes per word）。
    sub     x1, x1, #8          //count-- （8 bytes per word）
    cbnz    x1, memzero         //Compare and Branch if NonZero

uart_initial:
    bl      uart_init           //relocation 之前先確保 uart 可以 init

    // 設定 relocation 的來源 (0x80000) 和目標地址 (0x60000)
    ldr     x4, =__begin         	// 原始 bootloader 起始位置 0x80000
    ldr     x1, =0x60000       		// 目標位置 0x60000
    ldr     x2, =__end           	// bootloader 結束位置
    sub     x2, x2, x4         	 	// 計算 bootloader 大小 

relocation_loop:
    ldr     x3, [x4], #8        // 讀取 8 bytes (64-bit) 資料
    str     x3, [x1], #8        // 將 8 bytes 寫入新的記憶體位置(0x60000開始)
    sub     x2, x2, #8          // 減少剩餘的資料大小
    cbnz    x2, relocation_loop // 若還有資料，繼續搬移


run:  
    ldr     x3, =bootloader_main			//524924 = 0x8020c
    ldr     x1, =__begin		//524288 = 0x80000
    ldr     x2, =0x60000		//393216 = 0x60000
    sub     x3, x3, x1 			//計算一開始load到0x80000時，_begin 到 main 的距離是多少
    add     x3, x3, x2			//從 0x60000 開始偏移相同的量，得到現在 bootloader 的 main 的位置
    br      x3
	//bl      bootloader_main - 0x20000
    b       hang				// for failsafe, halt this core too
