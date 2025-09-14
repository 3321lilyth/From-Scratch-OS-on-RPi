#ifndef THREAD_FUNCTIONS_H
#define THREAD_FUNCTIONS_H

#include "syscall.h"
#include "type.h"


void foo_sync();
void foo_sync_preempt();
void foo_sync_preempt_user();

//user thread
void user_shell();
void fork_test();
void exec_test1();
void exec_test2();
void kill_test();
void signal_kill_test();
void signal_handler_test();

//助教提供的 img
void video_player();    //lab6
void vfs_test_TA();     //lab7
#endif