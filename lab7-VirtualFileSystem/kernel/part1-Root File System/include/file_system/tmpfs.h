#pragma once
#include "file_system/vfs.h"
#include "type.h"
#include "utli.h"
#include "memory_test.h"
#include "mini_uart.h"
#define MAX_CHILDREN 16   //spec: at most 16 entries for a directory
#define MAX_DATA_LEN 4096   //spec: at most 4096 bytes for a file


struct tmpfs_node {
    char name[MAX_NAME_LEN];
    int is_dir;
    struct tmpfs_node* parent;
    struct vnode* vnode;                        // tmpfs_node <-> vnode 是 一一對應且互相連結
    struct tmpfs_node* children[MAX_CHILDREN];  // 只有 dir 有意義
    char data[MAX_DATA_LEN];                    // 只有 normal file 有意義
    size_t data_size;
};

int tmpfs_setup_mount(struct filesystem* fs, struct mount* mount);
void tmpfs_debug_vnode(struct vnode* v);
void tmpfs_ls();