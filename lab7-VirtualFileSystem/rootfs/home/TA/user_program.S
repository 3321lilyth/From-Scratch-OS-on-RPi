.section ".text"
.global _start


_start:
    mov x0, 0       // 把 X0 設為 0，這是計數器 (counter)
1:
    add x0, x0, 1   // increase counter 
    svc 0           // 觸發 System Call 0 (sys_write?)，進入 EL1，而且是  Synchronous Exception，應該會對應到 VBAR_ELn + 0x200 那邊
                        // EC (Exception Class) 會被設為 0x15 (表示 SVC 例外)
                        // ESR_EL1 裡面德 ISS (Instruction Specific Syndrome) 部分的低 16-bit 會存 //imm 的值 (這裡是 0)
    cmp x0, 5
    blt 1b          // branch if less then, 若 X0 < 5，則往回跳回 `1:` 繼續執行
2:
    b 2b            // 無窮迴圈，讓 user program 停在這裡            