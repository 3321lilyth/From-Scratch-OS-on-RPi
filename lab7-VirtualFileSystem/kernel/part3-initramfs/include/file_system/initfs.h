#pragma once
#include "file_system/vfs.h"
#include "file_system/tmpfs.h"
#include "file_system/initramfs.h"
#include "utli.h"
#include "mini_uart.h"


void init_rootfs();
void test_rootfs();
void test_vfs_errors();
void test_initramfs();