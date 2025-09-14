#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "thread.h"
#include "mini_uart.h"
#include "mailbox.h"
#include "cpio.h"
#include "utli.h"


// system call numbers
#define SYS_GETPID     0
#define SYS_UART_READ  1
#define SYS_UART_WRITE 2
#define SYS_EXEC       3
#define SYS_FORK       4
#define SYS_EXIT       5
#define SYS_MBOX_CALL  6
#define SYS_KILL       7
#define SYS_SIGNAL     8
#define SYS_SIGNAL_KILL 9


void syscall_handler(uint64_t* sp); //for system call
int sys_getpid();
size_t sys_uart_read(char buf[], size_t size);
size_t sys_uart_write(const char buf[], size_t size);
int sys_exec(const char* name_or_func, int is_func) ;
int sys_fork();
void sys_exit();
int sys_mbox_call(unsigned char ch, unsigned int *mbox);
void sys_kill(int pid);

#endif
