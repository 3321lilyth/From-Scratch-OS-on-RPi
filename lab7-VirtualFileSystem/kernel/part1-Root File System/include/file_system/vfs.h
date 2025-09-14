#pragma once
#include "type.h"
#include "utli.h"
#include "memory_test.h"

struct vnode;
struct file;
struct mount;
struct filesystem;

// POSIX 定義的 flags
#define O_RDONLY    0x00
#define O_WRONLY    0x01
#define O_RDWR      0x02
#define O_CREAT     0x40
#define O_APPEND    0x08
#define O_TRUNC     0x10

#define MAX_NAME_LEN 64     //傳統 EXT2/3 的 NAME_MAX 是 255，這邊簡化

struct vnode_operations {
    int (*lookup)(struct vnode* dir_node, struct vnode** target, const char* component_name);
    int (*create)(struct vnode* dir_node, struct vnode** target, const char* component_name);
    int (*mkdir)(struct vnode* dir_node, struct vnode** target, const char* component_name);
};

struct file_operations {
    int (*write)(struct file* file, const void* buf, size_t len);
    int (*read)(struct file* file, void* buf, size_t len);
    // int (*open)(struct vnode* file_node, struct file** target);
    // int (*close)(struct file* file);
//   long lseek64(struct file* file, long offset, int whence);
};

struct vnode {
    struct mount* mount;           // 如果該 vnode 是 mount point，這裡指向掛載的 mount struct
    struct vnode_operations* v_ops;
    struct file_operations* f_ops;
    void* internal;                // 指向 fs-specific 的資料（如 tmpfs_node）
};

struct file {
    struct vnode* vnode;
    size_t f_pos;                  // read/write offset
    struct file_operations* f_ops;
    int flags;                     // O_CREAT ...
};

struct mount {
    struct vnode* root;
    struct filesystem* fs;
};

struct filesystem {
    const char* name;
    int (*setup_mount)(struct filesystem* fs, struct mount* mount);
};

int register_filesystem(struct filesystem* fs);

//vnode operation
int vfs_mount(const char* target, const char* fs_name);
int vfs_create(const char* pathname);
int vfs_mkdir(const char* path);

//file operation
int vfs_open(const char* pathname, int flags, struct file** target);
int vfs_close(struct file* file);
int vfs_write(struct file* file, const void* buf, size_t len);
int vfs_read(struct file* file, void* buf, size_t len);