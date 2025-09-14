.section ".text"
//.global user_getpid_program
.global _start


.align 4

_start:
//user_getpid_program:
    // Step 1: 呼叫 sys_getpid (system call 0)
    mov x8, #0            // system call number 0: sys_getpid
    svc #0                // 呼叫 system call

    // 現在 x0 = pid

    // Step 2: 把 pid 轉成字串 (簡單轉 1 位數的處理)
    // buffer: "PID: X\n"
    adr x1, pid_str      // x1 = buffer base
    add x2, x1, #5        // x2 = &buf[5]
    mov x3, x0            // x3 = pid

    // 將 x3 + '0'，放進 buffer[5]
    add x3, x3, #'0'
    strb w3, [x2]

    // Step 3: 呼叫 sys_uart_write (system call 1)
    mov x0, x1            // x0 = buffer 地址
    mov x1, #7            // x1 = 長度 7 bytes
    mov x8, #2            // system call number 2: sys_uart_write
    svc #0

    // Step 4: 結束
    mov x8, #5            // sys_exit (system call 5)
    svc #0

.section ".data"
.align 4
pid_str:
    .ascii "\r\nPID: X\n"     // 7 bytes，包括 '\n'
