#pragma once
#include "type.h"
#include "utli.h"
#include "memory_test.h"
#include "file_system/vfs.h"
#include "cpio.h"


typedef struct initramfs_node {
    char name[MAX_NAME_LEN];
    int is_dir;
    struct vnode* vnode;
    struct initramfs_node* children[MAX_CHILDREN];  // 只有 dir 有意義
    char* data;
    size_t data_size;
} initramfs_node_t;



int initramfs_setup_mount(struct filesystem* fs, struct mount* mount);
void initramfs_ls(struct vnode* initramfs_mount_vnode);