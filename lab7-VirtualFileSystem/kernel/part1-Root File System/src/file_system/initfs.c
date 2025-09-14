#include "file_system/initfs.h"
struct filesystem tmpfs = {
    .name = "tmpfs",
    .setup_mount = tmpfs_setup_mount,
};

void init_rootfs() {
    uart_write_str_raw("\r\n---------- [init_rootfs] step1 register_filesystem ------------");
    register_filesystem(&tmpfs);
    uart_write_str_raw("\r\n---------- [init_rootfs] step2 mount to vfs ------------");
    vfs_mount("/", "tmpfs");

    uart_write_str_raw("\r\n---------- [init_rootfs] step3 mkdir!!! ------------");
    vfs_mkdir("mydir");

    uart_write_str_raw("\r\n---------- [init_rootfs] step4 trying to open mydir/file.txt.txt ------------");
    struct file* f1;
    vfs_open("mydir/file.txt", O_CREAT, &f1);

    uart_write_str_raw("\r\n---------- [init_rootfs] step5 trying to write ------------");
    vfs_write(f1, "Hello", 5);
    vfs_close(f1);

    uart_write_str_raw("\r\n---------- [init_rootfs] step6 trying to open mydir/file.txt------------");
    struct file* f2;
    vfs_open("mydir/file.txt", 0, &f2);
    
    uart_write_str_raw("\r\n---------- [init_rootfs] step7 trying to read ------------");
    char buf[10] = {0};
    vfs_read(f2, buf, 5);
    uart_write_str_raw("\r\nread result = ");
    uart_write_str_raw(buf);
    vfs_close(f2);


    uart_write_str_raw("\r\n---------- [init_rootfs] step7 trying to open hello.txt ------------");
    struct file* f3;
    vfs_open("hello.txt", O_CREAT, &f3);

    uart_write_str_raw("\r\n\r\n");
    tmpfs_ls();

    // Now buf should contain "Hello"
}
