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
void video_player();  //助教提供的 img
#endif