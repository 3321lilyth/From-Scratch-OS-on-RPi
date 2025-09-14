#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "thread.h"
#include "mini_uart.h"
#include "mailbox.h"
#include "cpio.h"
#include "utli.h"
#include "file_system/vfs.h"


// system call numbers
#define SYS_GETPID     0
#define SYS_UART_READ  1
#define SYS_UART_WRITE 2
#define SYS_EXEC       3
#define SYS_FORK       4
#define SYS_EXIT       5
#define SYS_MBOX_CALL  6
#define SYS_KILL       7    // parent 殺掉 child
#define SYS_SIGNAL     8    // user thread 幫自己註冊某個 signal 編號的 handler
#define SYS_SIGNAL_KILL 9   // 任何 thread 都可以呼叫這個殺掉任何 pid 的 thread
#define SYS_SIGRETURN 10    // handler 做完後會跳到這邊，讓 kernel 把真正的 sp_el0 和 elr, spsr 設定回去S
#define SYS_OPEN 11
#define SYS_CLOSE 12
#define SYS_WRITE 13
#define SYS_READ 14
#define SYS_MKDIR 15
#define SYS_MOUNT 16
#define SYS_CHDIR 17



void syscall_handler(uint64_t* sp); //for system call
int sys_getpid();
size_t sys_uart_read(char buf[], size_t size);
size_t sys_uart_write(const char buf[], size_t size);
int sys_exec(const char* name_or_func, int is_func) ;
int sys_fork();
void sys_exit();
int sys_mbox_call(unsigned char ch, unsigned int *mbox);
void sys_kill(int pid);
void sys_signal(int signum, void (*handler)());
void sys_signal_kill(int pid, int signum);
void sys_sigreturn();

#endif
