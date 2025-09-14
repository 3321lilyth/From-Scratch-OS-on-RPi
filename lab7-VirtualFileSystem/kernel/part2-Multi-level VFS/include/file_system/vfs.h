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

// limit
#define MAX_NAME_LEN 64     //傳統 EXT2/3 的 NAME_MAX 是 255，這邊簡化
#define MAX_CHILDREN 16   //spec: at most 16 entries for a directory
#define MAX_DATA_LEN 4096   //spec: at most 4096 bytes for a file
#define TMPFS_NODE_MAGIC  0x746D7066  // 'tmpfs node struct'
#define FILE_MAGIC        0x66696C65  // 'file struct'


// ERROR CODE
#define ERR_NO_ENTRY      -2   // 檔案不存在
#define ERR_EXIST         -3   // 檔案已存在
#define ERR_NOT_DIR       -4   // 不是資料夾
#define ERR_NO_SPACE      -5   // child 滿了
#define ERR_INVALID_POINTER      -6   //讀寫不存在的 file*
                                // 或者 f->vnode == NULL 或者 f->vnode->internal == NULL 或者 buf==NULL
#define ERR_INVALID_OPERATION      -7 //嘗試 write 一個 read only 區域，或者對 initramfs 做 mkdir 和 create 等等


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
    uint32_t magic;
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


//helper function
const char* fs_strerror(int err);