#include "file_system/initramfs.h"

static struct vnode_operations initramfs_v_ops;
static struct file_operations initramfs_f_ops;
static int initramfs_lookup(struct vnode* dir_node, struct vnode** target, const char* name);
static int initramfs_mkdir(struct vnode* dir_node, struct vnode** target, const char* name);
static int initramfs_create(struct vnode* dir_node, struct vnode** target, const char* name, void* data, size_t size);



int walk_and_build_path(struct vnode* root, const char* path, struct vnode** parent, char* final_name) {
    char buf[MAX_NAME_LEN];
    const char* p = path;
    struct vnode* curr = root;
    
    while (*p) {
        int i = 0;
        while (*p == '/') p++;
        while (*p && *p != '/' && i < MAX_NAME_LEN - 1) {
            buf[i++] = *p++;
        }
        buf[i] = '\0';

        if (*p == '/') {
            struct vnode* next;
            if (curr->v_ops->lookup(curr, &next, buf) < 0) {
                if (curr->v_ops->mkdir(curr, &next, buf) < 0) return -1;
            }
            curr = next;
        } else {
            strncpy(final_name, buf, MAX_NAME_LEN);
            *parent = curr;
            return 0;
        }
    }
    return -1;
}


void traverse_cpio_and_build(struct vnode* root_vnode) {
    extern uint8_t* CURRENT_PLATFORM_CPIO_BASE;
    struct cpio_newc_header* header = (struct cpio_newc_header*)CURRENT_PLATFORM_CPIO_BASE;

    while (1) {
        //1. check magic
        if (memcmp(header->c_magic, CPIO_MAGIC, MAGIC_SIZE) != 0) break;

        int namesize = atoi(header->c_namesize, 8);
        char* filename = (char*)header + sizeof(struct cpio_newc_header);
        
        //2. check CPIO_END
        if (memcmp(filename, CPIO_END, strlen(CPIO_END)) == 0) break;

        uint32_t filesize = (atoi(header->c_filesize, 8) + 3) & ~3;
        uint32_t entrysize = (sizeof(struct cpio_newc_header) + namesize + 3) & ~3;
        
        // 根據 pathname 分割出 parent 路徑與檔名
        struct vnode* parent;
        char name_buf[MAX_NAME_LEN];
        if (walk_and_build_path(root_vnode, filename, &parent, name_buf) < 0) {
            uart_write_str_raw("\r\n[initramfs] skip invalid file: ");
            uart_write_str_raw(filename);
            header = (struct cpio_newc_header*)((uint8_t*)header + filesize + entrysize);
            continue;
        }

        // 決定是檔案還是資料夾（根據 c_mode 判斷是否為目錄）
        uint32_t mode = atoi(header->c_mode, 8);
        struct vnode* node;
        if ((mode & 0040000) == 0040000) { // S_IFDIR
            uart_write_str_raw("\r\n    [initramfs_mount] try to create dir ");
            uart_write_str_raw(name_buf);
            uart_write_str_raw(" under parent ");
            uart_write_str_raw(((struct initramfs_node*)parent->internal)->name);
            initramfs_mkdir(parent, &node, name_buf);
        } else {
            void* file_data = (uint8_t*)header + entrysize;
            uart_write_str_raw("\r\n    [initramfs_mount] try to create normal file ");
            uart_write_str_raw(name_buf);
            uart_write_str_raw(" under parent ");
            uart_write_str_raw(((struct initramfs_node*)parent->internal)->name);
            initramfs_create(parent, &node, name_buf, file_data, filesize);
        }

        header = (struct cpio_newc_header*)((uint8_t*)header + filesize + entrysize);
    }
}


// setup_mount：建立根目錄 vnode，並遍歷 cpio 結構產生 vnode tree
int initramfs_setup_mount(struct filesystem* fs, struct mount* mount) {
    uart_write_str_raw("\r\n    [initramfs] initramfs_setup_mount......");
    struct initramfs_node* root_node = kmalloc(sizeof(struct initramfs_node));
    memset(root_node, 0, sizeof(struct initramfs_node));
    strcpy(root_node->name, "/");
    root_node->is_dir = 1;

    struct vnode* root_vnode = kmalloc(sizeof(struct vnode));
    memset(root_vnode, 0, sizeof(struct vnode));
    root_vnode->internal = root_node;
    root_vnode->v_ops = &initramfs_v_ops;
    root_vnode->f_ops = &initramfs_f_ops;
    root_vnode->mount = NULL;
    
    root_node->vnode = root_vnode;
    mount->root = root_vnode;
    mount->fs = fs;

    traverse_cpio_and_build(root_vnode);
    return 0;
}







///////////////////////// vnode_operations  /////////////////////////
int initramfs_lookup(struct vnode* dir_node, struct vnode** target, const char* name) {
    uart_write_str_raw("\r\n    [initramfs_lookup] looking for  ");
    uart_write_str_raw(name);
    initramfs_node_t* dir = dir_node->internal;
    if (!dir->is_dir) return ERR_NOT_DIR;
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (dir->children[i] && strcmp((char*)dir->children[i]->name, (char*)name)) {
            *target = dir->children[i]->vnode;
            return 0;
        }
    }
    return ERR_NO_ENTRY;
}
int initramfs_create(struct vnode* dir_node, struct vnode** target, const char* name, void* data, size_t size) {
    struct initramfs_node* dir = dir_node->internal;
    if (!dir->is_dir) return ERR_NOT_DIR;

    for (int i = 0; i < MAX_CHILDREN; ++i) {
        if (dir->children[i] && strcmp((char*)dir->children[i]->name, (char*)name)) return ERR_EXIST;
    }

    for (int i = 0; i < MAX_CHILDREN; ++i) {
        if (!dir->children[i]) {
            uart_write_str_raw("\r\n    [initramfs_create] creating file ");
            uart_write_str_raw(name);
            uart_write_str_raw(" under dir ");
            uart_write_str_raw(dir->name);
            
            struct initramfs_node* node = kmalloc(sizeof(struct initramfs_node));
            memset(node, 0, sizeof(struct initramfs_node));
            strncpy(node->name, name, MAX_NAME_LEN);
            node->is_dir = 0;
            node->data = data;
            node->data_size = size;

            struct vnode* v = kmalloc(sizeof(struct vnode));
            v->internal = node;
            v->v_ops = &initramfs_v_ops;
            v->f_ops = &initramfs_f_ops;
            v->mount = NULL;
            node->vnode = v;

            dir->children[i] = node;
            *target = v;
            return 0;
        }
    }

    return ERR_NO_SPACE;
}

int initramfs_mkdir(struct vnode* dir_node, struct vnode** target, const char* name) {
    struct initramfs_node* dir = dir_node->internal;
    if (!dir->is_dir) return ERR_NOT_DIR;

    for (int i = 0; i < MAX_CHILDREN; ++i) {
        if (dir->children[i] && strcmp((char*)dir->children[i]->name, (char*)name)) return ERR_EXIST;
    }

    for (int i = 0; i < MAX_CHILDREN; ++i) {
        if (!dir->children[i]) {
            uart_write_str_raw("\r\n    [initramfs_mkdir] creating directory ");
            uart_write_str_raw(name);

            struct initramfs_node* node = kmalloc(sizeof(struct initramfs_node));
            memset(node, 0, sizeof(struct initramfs_node));
            strncpy(node->name, name, MAX_NAME_LEN);
            node->is_dir = 1;

            struct vnode* v = kmalloc(sizeof(struct vnode));
            v->internal = node;
            v->v_ops = &initramfs_v_ops;
            v->f_ops = &initramfs_f_ops;
            v->mount = NULL;
            node->vnode = v;

            dir->children[i] = node;
            *target = v;
            return 0;
        }
    }

    return ERR_NO_SPACE;
}




//對外並不支援 create 跟 mkdir，所以弄這兩個 function 給 VFS 呼叫
int initramfs_create_out(struct vnode* dir_node, struct vnode** target, const char* name){
    uart_write_str_raw("\r\n    [initramfs_create_out] invalid !!!");
    return ERR_INVALID_OPERATION;
}
int initramfs_mkdir_out(struct vnode* dir_node, struct vnode** target, const char* name){
    uart_write_str_raw("\r\n    [initramfs_mkdir_out] invalid !!!");
    return ERR_INVALID_OPERATION;
}








///////////////////////// file_operations  /////////////////////////
int initramfs_read(struct file* f, void* buf, size_t len) {
    if (!f || !f->vnode || !f->vnode->internal || !buf) return ERR_INVALID_POINTER;
    initramfs_node_t* node = f->vnode->internal;
    if (f->f_pos >= node->data_size) return 0;
    if (f->f_pos + len > node->data_size) len = node->data_size - f->f_pos;
    simple_memcpy(buf, node->data + f->f_pos, len);
    f->f_pos += len;
    return len;
}


//不允許 write
int initramfs_write(struct file* f, const void* buf, size_t len) {
    return ERR_INVALID_OPERATION;
}




//////////////////// helper function ///////////////////////////////
void initramfs_ls_node(struct initramfs_node* dir, int depth) {
    if (!dir || !dir->is_dir) return;

    for (int i = 0; i < MAX_CHILDREN; ++i) {
        if (dir->children[i]) {
            uart_write_str_raw("\r\n");
            for (int j = 0; j <= depth; ++j)
                uart_write_str_raw("  ");
            uart_write_str_raw(dir->children[i]->name);

            if (dir->children[i]->is_dir) {
                uart_write_str_raw(" [DIR]");
                initramfs_ls_node(dir->children[i], depth + 1);
            } else {
                uart_write_str_raw(" [FILE]");
            }
        }
    }
}


//需傳入 initramfs 的 root，而不是 tmpfs 的 /initramfs 資料夾的 vnode 喔，要是 mount 後的
void initramfs_ls(struct vnode* initramfs_mount_vnode) {
    struct initramfs_node* root = (struct initramfs_node*)initramfs_mount_vnode->internal;
    
    if (!root || !root->is_dir) {
        uart_write_str_raw("\r\n[initramfs_ls] Invalid root vnode.");
        return;
    }

    uart_write_str_raw("\r\n[initramfs_ls] listing contents of initramfs:\r\n");
    uart_write_str_raw(root->name);
    initramfs_ls_node(root, 0);
}

int initramfs_is_dir(struct vnode* vn) {
    struct initramfs_node* node = vn->internal;
    return node && node->is_dir;
}


static struct vnode_operations initramfs_v_ops = {
    .lookup = initramfs_lookup,
    .create = initramfs_create_out,     // 不支援
    .mkdir  = initramfs_mkdir_out,      //不支援
    .is_dir   = initramfs_is_dir,
};
static struct file_operations initramfs_f_ops = {
    .read = initramfs_read,
    .write = initramfs_write,       //不支援
};