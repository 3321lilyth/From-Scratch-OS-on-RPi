#include "file_system/vfs.h"


#define MAX_FS 4
static struct filesystem* fs_list[MAX_FS];
struct mount* rootfs;
extern thread_t* get_current();

//這邊把 FS 記下來，user 呼叫 mount 時才可以找到
int register_filesystem(struct filesystem* fs) {
    for (int i = 0; i < MAX_FS; i++) {
        if (fs_list[i] && strcmp((char*)fs_list[i]->name, (char*)fs->name)) {
            uart_write_str_raw("\r\n[vfs] FS already registered: ");
            uart_write_str_raw(fs->name);
            return -1;
        }
    }

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



/**
 * 從 pathname 中擷取下一層 path component 並寫入 buf
 * ex:  pathname = "a/b/c"
 *      第一次呼叫會擷取 "a"，返回剩下的 "b/c"
 *      第二次呼叫會擷取 "b"，返回剩下的 "c"
 * 傳回下一段的開頭位置，或 NULL 表示結束
 */
const char* get_next_path_component(const char* pathname, char* buf) {
    while (*pathname == '/') pathname++;  // skip leading slashes

    if (*pathname == '\0') return NULL;   // path 結束

    int i = 0;
    while (*pathname != '/' && *pathname != '\0') {
        if (i < MAX_NAME_LEN - 1)
            buf[i++] = *pathname;
        pathname++;
    }
    buf[i] = '\0';

    // skip repeated slashes
    while (*pathname == '/') pathname++;
    return pathname;
}



void resolve_absolute_path(const char* path, char* resolved_path) {
    char stack[16][MAX_NAME_LEN];
    int top = 0;

    if (path[0] != '/') {
        struct vnode* vnode = get_current()->cwd;
        struct tmpfs_node* node = (struct tmpfs_node*)(vnode->internal);

        while (node) {
            if (top < 16)
                strncpy(stack[top++], node->name, MAX_NAME_LEN);
            node = node->parent;
        }

        // stack 目前是從 leaf 到 root，反轉處理
        for (int i = 0; i < top / 2; ++i) {
            char tmp[MAX_NAME_LEN];
            strcpy(tmp, stack[i]);
            strcpy(stack[i], stack[top - i - 1]);
            strcpy(stack[top - i - 1], tmp);
        }
    }

    // 解析 path
    const char* p = path;
    char comp[MAX_NAME_LEN];
    while ((p = get_next_path_component(p, comp)) != NULL) {
        if (strcmp(comp, ".")) continue;
        if (strcmp(comp, "..")) {
            if (top > 0) top--;
        } else {
            if (top < 16)
                strncpy(stack[top++], comp, MAX_NAME_LEN);
        }
    }

    // 組 output path
    char* out = resolved_path;
    *out++ = '/';
    for (int i = 0; i < top; i++) {
        int len = strlen(stack[i]);
        simple_memcpy(out, stack[i], len);
        out += len;
        if (i < top - 1)
            *out++ = '/';
    }
    *out = '\0';
}

















/////////////////////////////////////////////// vnode operation  ///////////////////////////////////////////////
// int vfs_mount(const char* target, const char* fs_name) {
//     char abs_path[256];
//     resolve_absolute_path(target, abs_path);
//     for (int i = 0; i < MAX_FS; i++) {
//         if (fs_list[i] && strcmp((char*)fs_list[i]->name, (char*)fs_name)) {
//             uart_write_str_raw("\r\n[vfs] mounting FS ");
//             uart_write_str_raw(fs_name);
//             uart_write_str_raw(" at ");
//             uart_write_str_raw(abs_path);

//             struct mount* mnt = kmalloc(sizeof(struct mount));
//             fs_list[i]->setup_mount(fs_list[i], mnt);
            
//             // rootfs 只有在第一次掛載時會被設定
//             if (strcmp((char*)abs_path, "/"))
//                 rootfs = mnt;

            
//             //把他想要掛載的 mount point 找出來
//             struct vnode* mount_point;
//             if (vfs_lookup(abs_path, &mount_point) < 0) {
//                 uart_write_str_raw("\r\n[vfs_mount] Error: mount point not found");
//                 return -1;
//             }
            
//             // 設定掛載點的 mount 欄位，才可以從 /initramfs 切到新的 fs
//             mount_point->mount = mnt;

//             return 0;
//         }
//     }
//     return -1;
// }

int vfs_mount(const char* target, const char* fs_name) {
    char abs_path[256];
    resolve_absolute_path(target, abs_path);

    for (int i = 0; i < MAX_FS; i++) {
        if (fs_list[i] && strcmp((char*)fs_list[i]->name, (char*)fs_name)) {
            uart_write_str_raw("\r\n[vfs] mounting FS ");
            uart_write_str_raw(fs_name);
            uart_write_str_raw(" at ");
            uart_write_str_raw(abs_path);

            struct mount* mnt = kmalloc(sizeof(struct mount));

            // Special case: mount rootfs
            if (strcmp(abs_path, "/")) {
                fs_list[i]->setup_mount(fs_list[i], mnt);
                rootfs = mnt;
                uart_write_str_raw("\r\n[vfs] rootfs mounted.");
                return 0;
            }

            // Regular mount: 先找掛載點，再 setup + 掛上去
            struct vnode* mount_point;
            if (vfs_lookup(abs_path, &mount_point) < 0) {
                uart_write_str_raw("\r\n[vfs_mount] Error: mount point not found");
                return -1;
            }
            
            fs_list[i]->setup_mount(fs_list[i], mnt);

            // 設定掛載點的 mount 欄位，才可以從 /initramfs 切到新的 fs
            mount_point->mount = mnt;

            return 0;
        }
    }

    uart_write_str_raw("\r\n[vfs_mount] Error: fs_name not registered");
    return -1;
}



//解析整條 /a/b/c，回傳 /a/b/c.txt 的 vnode。用於 open, read, write
//找完整 path，每層都呼叫 FS lookup，遇到 mount 就切換。
//注意，如果是有被 mount 的 vnode，就應該要回傳 mount 過後的 vnode，而不是 tmpfs 原本的 vnode
// int vfs_lookup(const char* pathname, struct vnode** target) {
//     uart_write_str_raw("\r\n[vfs_lookup] resolving path: ");
//     uart_write_str_raw(pathname);

//     struct thread* t = get_current();
//     struct vnode* curr;
//     // struct vnode* curr = rootfs->root;
//     if (pathname[0] == '/') {
//         curr = rootfs->root;  // 絕對路徑
//         pathname++;  // skip leading '/'
//     } else {
//         curr = t->cwd;        // 相對路徑從 current thread 的 cwd 開始
//     }


//     const char* p = pathname;
//     char name_buf[MAX_NAME_LEN];

//     while (*p != '\0') {
//         // 讀下一層 component
//         int i = 0;
//         while (*p == '/') p++; // 跳過多餘的 '/'
//         while (*p != '/' && *p != '\0' && i < MAX_NAME_LEN - 1) {
//             name_buf[i++] = *p++;
//         }
//         name_buf[i] = '\0';

//         if (name_buf[0] == '\0') break;

//         // 若目前 vnode 被 moun t，則切進子 filesystem 的根節點
//         // 用 while 是因為有可能會有巢狀 mount，也就是同一個點上掛了多個檔案系統，就要以最後掛的為主
//         while (curr->mount && curr != curr->mount->root) {
//             curr = curr->mount->root;
//         }

//         // 呼叫對應 FS 的 lookup
//         if (curr->v_ops->lookup(curr, &curr, name_buf) < 0) {
//             uart_write_str_raw("\r\n[vfs_lookup] lookup failed at ");
//             uart_write_str_raw(name_buf);
//             return -1;
//         }
//     }

//     *target = curr;
//     return 0;
// }


// //解析出 /a/b/c 中的 /a/b 的 vnode，以及 c 作為 final_name。回傳 /a/b 的 vnode + "c.txt"。用於 create/mkdir
// int vfs_lookup_dir_and_name(const char* pathname, struct vnode** parent_dir, char* final_name) {
//     uart_write_str_raw("\r\n[vfs_lookup_dir_and_name] resolving path: ");
//     uart_write_str_raw(pathname);

//     // struct vnode* curr = rootfs->root;
//     struct thread* t = get_current();
//     struct vnode* curr;
//     if (pathname[0] == '/') {
//         curr = rootfs->root;  // 絕對路徑
//         pathname++;  // skip leading '/'
//     } else {
//         curr = t->cwd;        // 相對路徑從 current thread 的 cwd 開始
//     }

//     const char* p = pathname;
//     char name_buf[MAX_NAME_LEN];

//     while (*p != '\0') {
//         int i = 0;
//         while (*p == '/') p++;
//         while (*p != '/' && *p != '\0' && i < MAX_NAME_LEN - 1) {
//             name_buf[i++] = *p++;
//         }
//         name_buf[i] = '\0';

//         if (*p == '\0') {
//             // 最後一層，回傳目前 vnode 和最後 component name
//             strncpy(final_name, name_buf, MAX_NAME_LEN);
//             while (curr->mount && curr != curr->mount->root) {
//                 curr = curr->mount->root;
//             }
//             *parent_dir = curr;
//             return 0;
//         }

//         while (curr->mount && curr != curr->mount->root) {
//             curr = curr->mount->root;
//         }

//         if (curr->v_ops->lookup(curr, &curr, name_buf) < 0) {
//             uart_write_str_raw("\r\n[vfs_lookup_dir_and_name] lookup failed at ");
//             uart_write_str_raw(name_buf);
//             return -1;
//         }
//     }

//     return -1; // path 不合法
// }




int vfs_lookup(const char *pathname, struct vnode **target) {
    struct vnode *curr;
    if (pathname[0] == '/') {
        curr = rootfs->root;
        pathname++;
    } else {
        curr = get_current()->cwd;
    }

    char component[MAX_NAME_LEN];
    const char* p = pathname;

    while ((p = get_next_path_component(p, component)) != NULL) {
        // uart_write_str_raw("\r\n[debug] vfs_lookup looking for component: ");
        // uart_write_str_raw(component);
        if (strcmp(component, ".")) continue;
        if (strcmp(component, "..")) continue;
        
        // 修正點：先進入 mount，再呼叫 lookup
        // while (curr->mount && curr != curr->mount->root) {
        while (curr->mount) {
            curr = curr->mount->root;
        }
        
        struct vnode *next;
        if (curr->v_ops->lookup(curr, &next, component) < 0) {
            uart_write_str_raw("\r\n[vfs_lookup] lookup failed at ");
            uart_write_str_raw(component);
            return -1;
        }
        // uart_write_str_raw("\r\n[debug] vfs_lookup after FS lookup: ");
        // uart_write_str_raw(next->internal ? ((struct tmpfs_node*)next->internal)->name : "NULL");
        curr = next;
    }
    
    // 若最後是個 mount point，也要切進去
    // while (curr->mount && curr != curr->mount->root) {
    while (curr->mount) {
        // uart_write_str_raw("\r\n[debug] mount point 2");
        // uart_write_str_raw(curr->mount->fs->name);
        // uart_write_str_raw("\r\n[debug] mount point root: ");
        // uart_write_hex_raw((uintptr_t)curr->mount->root);
        curr = curr->mount->root;
    }
    
    *target = curr;
    return 0;
}


int vfs_lookup_dir_and_name(const char* pathname, struct vnode** parent_dir, char* final_name) {
    uart_write_str_raw("\r\n[vfs_lookup_dir_and_name] resolving path: ");
    uart_write_str_raw(pathname);
    
    struct vnode* curr;
    if (pathname[0] == '/') {
        curr = rootfs->root;
        pathname++;
    } else {
        curr = get_current()->cwd;
    }
    
    char component[MAX_NAME_LEN];
    const char* p = pathname;
    
    while ((p = get_next_path_component(p, component)) != NULL) {
        // uart_write_str_raw("\r\n[debug] vfs_lookup_dir_and_name looking for component: ");
        // uart_write_str_raw(component);
        if (*p == '\0') {
            // 最後一層 path component，直接返回
            strncpy(final_name, component, MAX_NAME_LEN);
            
            // 修正點：return 前也需要確認是否是 mount point
            // while (curr->mount && curr != curr->mount->root) {
            while (curr->mount) {
                curr = curr->mount->root;
            }
            
            *parent_dir = curr;
            return 0;
        }
        
        if (strcmp(component, ".")) continue;
        if (strcmp(component, "..")) continue;
        
        // 修正點：先切進 mount root，再 lookup
        // while (curr->mount && curr != curr->mount->root) {
        while (curr->mount) {
            curr = curr->mount->root;
        }

        struct vnode* next_node;
        if (curr->v_ops->lookup(curr, &next_node, component) < 0) {
            uart_write_str_raw("\r\n[vfs_lookup_dir_and_name] lookup failed at ");
            uart_write_str_raw(component);
            return -1;
        }

        curr = next_node;
    }

    return -1;
}

//這邊要吃絕對路徑，裡面的 vfs_lookup_dir_and_name 才會找的到正確的 parent node
int vfs_create(const char* pathname) {
    char abs_path[MAX_NAME_LEN];
    resolve_absolute_path(pathname, abs_path);
    uart_write_str_raw("\r\n[vfs_create] create a file at ");
    uart_write_str_raw(abs_path);
    struct vnode* dir;
    char name[MAX_NAME_LEN];
    
    if (vfs_lookup_dir_and_name(abs_path, &dir, name) < 0) return -1;
    
    struct vnode* new_node; //創建 vnode 是 FS 的職責，不是 VFS 的，所以這邊不用 kmalloc，而是等FS的 create function 自己創好丟過來
    return dir->v_ops->create(dir, &new_node, name);
}



//這邊要吃絕對路徑，裡面的 vfs_lookup_dir_and_name 才會找的到正確的 parent node
int vfs_mkdir(const char* pathname) {
    char abs_path[MAX_NAME_LEN];
    resolve_absolute_path(pathname, abs_path);
    uart_write_str_raw("\r\n[vfs_create] create a file at ");
    uart_write_str_raw(abs_path);

    struct vnode* dir;
    char name[MAX_NAME_LEN];
    if (vfs_lookup_dir_and_name(abs_path, &dir, name) < 0) return -1;
    struct vnode* node;
    return dir->v_ops->mkdir(dir, &node, name);
}


int vfs_chdir(const char* path) {
    char abs_path[256];
    resolve_absolute_path(path, abs_path);
    
    struct vnode* node;
    if (vfs_lookup(abs_path, &node) < 0) {
        uart_write_str_raw("\r\n[vfs_chdir] lookup failed.");
        return -1;
    }

    if (!node->v_ops || !node->v_ops->is_dir || !node->v_ops->is_dir(node)) {
        uart_write_str_raw("\r\n[vfs_chdir] not a directory.");
        return -1;
    }
    struct tmpfs_node* old = (struct tmpfs_node*)(get_current()->cwd->internal);
    struct tmpfs_node* new = (struct tmpfs_node*)(node->internal);
    uart_write_str_raw("\r\n[vfs_chdir] success. from ");
    uart_write_str_raw(old->name[0] ? old->name : "/");
    uart_write_str_raw(" to ");
    uart_write_str_raw(new->name[0] ? new->name : "/");
    get_current()->cwd = node;
    return 0;
}





/////////////////////////////////// file operation ///////////////////////////////////
int vfs_open(const char* pathname, int flags, struct file** target) {
    
    char abs_path[MAX_NAME_LEN];
    resolve_absolute_path(pathname, abs_path);
    uart_write_str_raw("\r\n[vfs_open] try to open file ");
    uart_write_str_raw(abs_path);
    
    struct vnode* node;
    //如果檔案已經存在就 open 沒問題
    if (vfs_lookup(abs_path, &node) == 0) {
        uart_write_str_raw("\r\n[vfs_open] file found");
        struct file* f = kmalloc(sizeof(struct file));
        f->vnode = node;
        f->f_pos = 0;
        f->f_ops = node->f_ops;
        f->flags = flags;
        f->magic = FILE_MAGIC;
        *target = f;
        return 0;
    }
    //如果檔案尚未存在，但是有設定 O_CREAT flag，那就直接幫他 create
    else if (flags & O_CREAT) {
        uart_write_str_raw("\r\n[vfs_open] open file faild but O_CREAT, creating");
        int create_ret = vfs_create(abs_path);
        if (create_ret == 0 && vfs_lookup(abs_path, &node) == 0) {
            struct file* f = kmalloc(sizeof(struct file));
            f->vnode = node;
            f->f_pos = 0;
            f->f_ops = node->f_ops;
            f->flags = flags;
            f->magic = FILE_MAGIC;
            *target = f;
            return 0;
        }else if (create_ret != 0){
            return create_ret;
        }
    }
    return -1;
}
int vfs_open_fd(const char* pathname, int flags) {
    struct file* f;
    int ret = vfs_open(pathname, flags, &f);  // 保留原本的實作邏輯
    if (ret < 0) return ret;

    // 找 fd table 空位
    struct thread* t = get_current();
    for (int fd = 0; fd < MAX_FD; fd++) {
        if (t->fd_table[fd] == NULL) {
            t->fd_table[fd] = f;
            return fd;
        }
    }

    uart_write_str_raw("\r\n[vfs_open] no available fd slot");
    vfs_close(f); // 沒有 slot 就釋放
    return -1;
}


int vfs_close(struct file* file) {
    //這邊不應該釋放 vnode 和 internal，因為多個 file 可能指向同一 vnode
    //f_ops/v_ops 是static function pointers，不需釋放
    //mount 是在 unmount 才釋放
    //所以這邊的確只需要釋放 file handle 即可
    
    uart_write_str_raw("\r\n[vfs_close] close a fd");
    file->magic = 0;
    kfree((uintptr_t)file);
    return 0;
}
int vfs_close_fd(int fd) {
    struct thread* t = get_current();
    if (fd < 0 || fd >= MAX_FD || !t->fd_table[fd]) {
        uart_write_str_raw("\r\n[vfs_close] invalid fd");
        return -1;
    }
    int res = vfs_close(t->fd_table[fd]);
    t->fd_table[fd] = NULL;
    return res;
}




int vfs_write(struct file* file, const void* buf, size_t len) {
    if (!file || file->magic != FILE_MAGIC ||
        !file->vnode || !file->vnode->internal) {
        uart_write_str_raw("\r\n[vfs_write] error: invalid file struct");
        return ERR_INVALID_POINTER;
    }
    return file->f_ops->write(file, buf, len);
}
int vfs_write_fd(int fd, const void* buf, size_t len) {
    struct thread* t = get_current();
    if (fd < 0 || fd >= MAX_FD || !t->fd_table[fd]) {
        uart_write_str_raw("\r\n[vfs_write] invalid fd");
        return -1;
    }
    return vfs_write(t->fd_table[fd], buf, len);
}




int vfs_read(struct file* file, void* buf, size_t len) {
    if (!file || file->magic != FILE_MAGIC ||
        !file->vnode || !file->vnode->internal) {
        uart_write_str_raw("\r\n[vfs_read] error: invalid file struct");
        return ERR_INVALID_POINTER;
    }
    return file->f_ops->read(file, buf, len);
}
int vfs_read_fd(int fd, void* buf, size_t len) {
    struct thread* t = get_current();
    if (fd < 0 || fd >= MAX_FD || !t->fd_table[fd]) {
        uart_write_str_raw("\r\n[vfs_read] invalid fd");
        return -1;
    }
    return vfs_read(t->fd_table[fd], buf, len);
}




////// helper function
const char* fs_strerror(int err) {
    switch (err) {
        case ERR_NO_ENTRY:  return "No such entry";
        case ERR_EXIST:     return "Already exists";
        case ERR_NOT_DIR:   return "Not a directory";
        case ERR_NO_SPACE:  return "No space in dir";
        case ERR_INVALID_POINTER:  return "Invalid pointer";
        case ERR_INVALID_OPERATION:  return "Invalid operation";
        default:            return "Unknown error";
    }
}
