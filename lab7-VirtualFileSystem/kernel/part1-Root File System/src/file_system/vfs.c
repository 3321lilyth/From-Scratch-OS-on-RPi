#include "file_system/vfs.h"


#define MAX_FS 4
static struct filesystem* fs_list[MAX_FS];
struct mount* rootfs;

int register_filesystem(struct filesystem* fs) {
    //這邊把 FS 記下來，user 呼叫 mount 時才可以找到
    for (int i = 0; i < MAX_FS; i++) {
        if (!fs_list[i]) {
            uart_write_str_raw("\r\n[vfs] register FS ");
            uart_write_str_raw(fs->name);
            uart_write_str_raw(" to kernel");
            fs_list[i] = fs;
            return 0;
        }
    }
    return -1;
}








///// vnode operation
int vfs_mount(const char* target, const char* fs_name) {
    for (int i = 0; i < MAX_FS; i++) {
        if (fs_list[i] && strcmp((char*)fs_list[i]->name, (char*)fs_name)) {
            uart_write_str_raw("\r\n[vfs] mounting FS ");
            uart_write_str_raw(fs_name);
            uart_write_str_raw(" at ");
            uart_write_str_raw(target);
            struct mount* mnt = kmalloc(sizeof(struct mount));
            fs_list[i]->setup_mount(fs_list[i], mnt);
            
            
            // rootfs 只有在第一次掛載時會被設定
            if (strcmp((char*)target, "/"))
                rootfs = mnt;
            
            // 找到 / 路徑對應的 vnode，例如 /boot
            // struct vnode* mount_point;
            // vfs_lookup(target, &mount_point);
            // mount_point->mount = mnt;
            return 0;
        }
    }
    return -1;
}

//解析整條 /a/b/c，回傳 /a/b/c.txt 的 vnode。用於 open, read, write
//找完整 path，每層都呼叫 FS lookup，遇到 mount 就切換。
int vfs_lookup(const char* pathname, struct vnode** target) {
    uart_write_str_raw("\r\n[vfs_lookup] resolving path: ");
    uart_write_str_raw(pathname);

    struct vnode* curr = rootfs->root;
    const char* p = pathname;
    char name_buf[MAX_NAME_LEN];

    while (*p != '\0') {
        // 讀下一層 component
        int i = 0;
        while (*p == '/') p++; // 跳過多餘的 '/'
        while (*p != '/' && *p != '\0' && i < MAX_NAME_LEN - 1) {
            name_buf[i++] = *p++;
        }
        name_buf[i] = '\0';

        if (name_buf[0] == '\0') break;

        // 若目前 vnode 被 mount，則切進子 filesystem 的根節點
        if (curr->mount) {
            curr = curr->mount->root;
        }

        // 呼叫對應 FS 的 lookup
        if (curr->v_ops->lookup(curr, &curr, name_buf) < 0) {
            uart_write_str_raw("\r\n[vfs_lookup] lookup failed at ");
            uart_write_str_raw(name_buf);
            return -1;
        }
    }

    *target = curr;
    return 0;
}


//解析出 /a/b/c 中的 /a/b 的 vnode，以及 c 作為 final_name。回傳 /a/b 的 vnode + "c.txt"。用於 create/mkdir
int vfs_lookup_dir_and_name(const char* pathname, struct vnode** parent_dir, char* final_name) {
    uart_write_str_raw("\r\n[vfs_lookup_dir_and_name] resolving path: ");
    uart_write_str_raw(pathname);

    struct vnode* curr = rootfs->root;
    const char* p = pathname;
    char name_buf[MAX_NAME_LEN];

    while (*p != '\0') {
        int i = 0;
        while (*p == '/') p++;
        while (*p != '/' && *p != '\0' && i < MAX_NAME_LEN - 1) {
            name_buf[i++] = *p++;
        }
        name_buf[i] = '\0';

        if (*p == '\0') {
            // 最後一層，回傳目前 vnode 和最後 component name
            strncpy(final_name, name_buf, MAX_NAME_LEN);
            *parent_dir = curr;
            return 0;
        }

        if (curr->mount)
            curr = curr->mount->root;

        if (curr->v_ops->lookup(curr, &curr, name_buf) < 0) {
            uart_write_str_raw("\r\n[vfs_lookup_dir_and_name] lookup failed at ");
            uart_write_str_raw(name_buf);
            return -1;
        }
    }

    return -1; // path 不合法
}



int vfs_create(const char* pathname) {
    uart_write_str_raw("\r\n[vfs_create] create a file at ");
    uart_write_str_raw(pathname);
    struct vnode* dir;
    char name[MAX_NAME_LEN];
    
    if (vfs_lookup_dir_and_name(pathname, &dir, name) < 0) return -1;
    
    struct vnode* new_node; //創建 vnode 是 FS 的職責，不是 VFS 的，所以這邊不用 kmalloc，而是等FS的 create function 自己創好丟過來
    return dir->v_ops->create(dir, &new_node, name);
}


int vfs_mkdir(const char* path) {
    struct vnode* dir;
    char name[MAX_NAME_LEN];
    if (vfs_lookup_dir_and_name(path, &dir, name) < 0) return -1;
    struct vnode* node;
    return dir->v_ops->mkdir(dir, &node, name);
}









//// file operation 
int vfs_open(const char* pathname, int flags, struct file** target) {
    struct vnode* node;

    //如果檔案已經存在就 open 沒問題
    if (vfs_lookup(pathname, &node) == 0) {
        struct file* f = kmalloc(sizeof(struct file));
        f->vnode = node;
        f->f_pos = 0;
        f->f_ops = node->f_ops;
        f->flags = flags;
        *target = f;
        return 0;
    }
    //如果檔案尚未存在，但是有設定 O_CREAT flag，那就直接幫他 create
    else if (flags & O_CREAT) {
        uart_write_str_raw("\r\n[vfs_open] open file faild but O_CREAT, creating");
        if (vfs_create(pathname) == 0 && vfs_lookup(pathname, &node) == 0) {
            struct file* f = kmalloc(sizeof(struct file));
            f->vnode = node;
            f->f_pos = 0;
            f->f_ops = node->f_ops;
            f->flags = flags;
            *target = f;
            return 0;
        }
    }
    return -1;
}

int vfs_close(struct file* file) {
    //這邊不應該釋放 vnode 和 internal，因為多個 file 可能指向同一 vnode
    //f_ops/v_ops 是static function pointers，不需釋放
    //mount 是在 unmount 才釋放
    //所以這邊的確只需要釋放 file handle 即可
    kfree((uintptr_t)file);
    return 0;
}
int vfs_write(struct file* file, const void* buf, size_t len) {
    return file->f_ops->write(file, buf, len);
}

int vfs_read(struct file* file, void* buf, size_t len) {
    return file->f_ops->read(file, buf, len);
}