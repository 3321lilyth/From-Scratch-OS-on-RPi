#include "file_system/tmpfs.h"

static struct vnode_operations tmpfs_v_ops;
static struct file_operations tmpfs_f_ops;


/// for initial /////
int tmpfs_setup_mount(struct filesystem* fs, struct mount* mount) {
    uart_write_str_raw("\r\n    [tmpfs_mount] tmpfs mount setup.......");
    struct tmpfs_node* root_node = kmalloc(sizeof(struct tmpfs_node));
    memset(root_node, 0, sizeof(struct tmpfs_node));
    strcpy(root_node->name, "/");
    root_node->is_dir = 1;
    root_node->magic = TMPFS_NODE_MAGIC;

    struct vnode* root_vnode = kmalloc(sizeof(struct vnode));
    root_vnode->mount = NULL;
    root_vnode->v_ops = &tmpfs_v_ops;
    root_vnode->f_ops = &tmpfs_f_ops;
    root_vnode->internal = root_node;
    
    root_node->vnode = root_vnode;

    mount->root = root_vnode;
    mount->fs = fs;
    return 0;
}


//////// vnode operation
//dir_node 是目前所在的 vnode，也可能是 root；target 是 結果 vnode pointer：如果找到就寫進來
//這邊是 FS 內部的 lookup，所以是透過找 tmpfs_node 來找到 vnode。
//因為 vfs_lookup 會幫忙解析 '/'，所以這邊只要負責找單層的 child 裡面有沒有對應檔案就好
int tmpfs_lookup(struct vnode* dir_node, struct vnode** target, const char* name) {
    uart_write_str_raw("\r\n    [tmpfs_lookup] looking for ");
    uart_write_str_raw(name);

    struct tmpfs_node* dir = dir_node->internal;
    if (!dir->is_dir) return -1;

    for (int i = 0; i < MAX_CHILDREN; ++i) {
        if (dir->children[i] && strcmp((char*)dir->children[i]->name, (char*)name)) {
            *target = dir->children[i]->vnode;
            return 0;
        }
    }

    return -1;
}


//在指定 dir 內創建 regular file，不負責解析路徑
int tmpfs_create(struct vnode* dir_node, struct vnode** target, const char* name) {
    struct tmpfs_node* dir = dir_node->internal;
    if (!dir->is_dir) return ERR_NOT_DIR;

    // 檢查是否已存在同名檔案或資料夾
    for (int i = 0; i < MAX_CHILDREN; ++i) {
        if (dir->children[i] && strcmp((char*)dir->children[i]->name, (char*)name)) {
            uart_write_str_raw("\r\n    [tmpfs_create] error: file already exists: ");
            uart_write_str_raw(name);
            return ERR_EXIST;
        }
    }

    //尋找空格創建
    for (int i = 0; i < MAX_CHILDREN; ++i) {
        if (!dir->children[i]) {
            uart_write_str_raw("\r\n    [tmpfs_create] creating file ");
            uart_write_str_raw(name);
            uart_write_str_raw(" under dir ");
            uart_write_str_raw(dir->name);

            struct tmpfs_node* node = kmalloc(sizeof(struct tmpfs_node));
            memset(node, 0, sizeof(struct tmpfs_node));
            strncpy(node->name, name, MAX_NAME_LEN);
            node->parent = dir;
            node->is_dir = 0;
            node->magic = TMPFS_NODE_MAGIC;

            struct vnode* v = kmalloc(sizeof(struct vnode));
            v->internal = node;
            v->v_ops = &tmpfs_v_ops;
            v->f_ops = &tmpfs_f_ops;
            v->mount = NULL;

            node->vnode = v;
            dir->children[i] = node;
            *target = v;
            return 0;
        }
    }

    return ERR_NO_SPACE;
}



int tmpfs_mkdir(struct vnode* dir_node, struct vnode** target, const char* name) {
    struct tmpfs_node* dir = dir_node->internal;
    if (!dir->is_dir) return ERR_NOT_DIR;

    for (int i = 0; i < MAX_CHILDREN; ++i) {
        if (dir->children[i] && strcmp((char*)dir->children[i]->name, (char*)name)) {
            uart_write_str_raw("\r\n    [tmpfs_mkdir] error: entry already exists: ");
            uart_write_str_raw(name);
            return ERR_EXIST;
        }
    }

    for (int i = 0; i < MAX_CHILDREN; ++i) {
        if (!dir->children[i]) {
            uart_write_str_raw("\r\n    [tmpfs_mkdir] creating directory ");
            uart_write_str_raw(name);

            struct tmpfs_node* node = kmalloc(sizeof(struct tmpfs_node));
            memset(node, 0, sizeof(struct tmpfs_node));
            strncpy(node->name, name, MAX_NAME_LEN);
            node->parent = dir;
            node->is_dir = 1;
            node->magic = TMPFS_NODE_MAGIC;

            struct vnode* v = kmalloc(sizeof(struct vnode));
            v->internal = node;
            v->v_ops = &tmpfs_v_ops;
            v->f_ops = &tmpfs_f_ops;
            v->mount = NULL;

            node->vnode = v;
            dir->children[i] = node;
            *target = v;
            return 0;
        }
    }

    return ERR_NO_SPACE;
}






////////////////////////// file operation  //////////////////////////
int tmpfs_write(struct file* f, const void* buf, size_t len) {
    struct tmpfs_node* node = f->vnode->internal;
    if (node->magic != TMPFS_NODE_MAGIC || !buf) {
        uart_write_str_raw("\r\n    [tmpfs_write] error: invalid node or buffer");
        return ERR_INVALID_POINTER;
    }

    if (node->data_size + len > MAX_DATA_LEN)
        len = MAX_DATA_LEN - node->data_size;

    uart_write_str_raw("\r\n    [tmpfs_write] writing file at ");
    uart_write_int_raw(f->f_pos);
    uart_write_str_raw(", len = ");
    uart_write_int_raw(len);

    simple_memcpy(node->data + f->f_pos, buf, len);
    f->f_pos += len;
    if (f->f_pos > node->data_size)
        node->data_size = f->f_pos;

    return len;
}


int tmpfs_read(struct file* f, void* buf, size_t len) {
    struct tmpfs_node* node = f->vnode->internal;
    if (node->magic != TMPFS_NODE_MAGIC || !buf) {
        uart_write_str_raw("\r\n    [tmpfs_read] error: invalid node or buffer");
        return ERR_INVALID_POINTER;
    }

    if (f->f_pos >= node->data_size) return 0;  // EOF
    if (f->f_pos + len > node->data_size)
        len = node->data_size - f->f_pos;

    uart_write_str_raw("\r\n    [tmpfs_read] reading file at ");
    uart_write_int_raw(f->f_pos);
    uart_write_str_raw(", len = ");
    uart_write_int_raw(len);

    simple_memcpy(buf, node->data + f->f_pos, len);
    f->f_pos += len;
    return len;
}


















////////////////////////// dump function //////////////////////////
void tmpfs_debug_vnode(struct vnode* v) {
    struct tmpfs_node* tn = v->internal;
    uart_write_str_raw("\r\n[tmpfs_debug_vnode] ");
    uart_write_str_raw("name=");
    uart_write_str_raw(tn->name);
    uart_write_str_raw(", is_dir=");
    uart_write_int_raw(tn->is_dir);
}
void tmpfs_ls_node(struct tmpfs_node* dir, int depth) {
    if (!dir || !dir->is_dir) return;


    // 遍歷 children
    for (int i = 0; i < MAX_CHILDREN; ++i) {
        if (dir->children[i]) {
            uart_write_str_raw("\r\n");
            for (int j = 0; j < depth + 1; ++j) uart_write_str_raw("  ");
            uart_write_str_raw(dir->children[i]->name);

            if (dir->children[i]->is_dir) {
                uart_write_str_raw(" [DIR]");
                tmpfs_ls_node(dir->children[i], depth + 1); // 遞迴印出子目錄
            } else {
                uart_write_str_raw(" [FILE]");
            }
        }
    }
}

void tmpfs_ls() {
    extern struct mount* rootfs;
    struct vnode* root_vnode = rootfs->root;
    struct tmpfs_node* root = root_vnode->internal;

    uart_write_str_raw("\r\n[tmpfs_ls] tmpfs ls");
    uart_write_str_raw("\r\n/");
    tmpfs_ls_node(root, 0);
}

int tmpfs_is_dir(struct vnode* vn) {
    struct tmpfs_node* node = vn->internal;
    return node && node->is_dir;
}


static struct vnode_operations tmpfs_v_ops = {
    .lookup = tmpfs_lookup,
    .create = tmpfs_create,
    .mkdir  = tmpfs_mkdir,
    .is_dir = tmpfs_is_dir,
};

static struct file_operations tmpfs_f_ops = {
    .write = tmpfs_write,
    .read = tmpfs_read,
};