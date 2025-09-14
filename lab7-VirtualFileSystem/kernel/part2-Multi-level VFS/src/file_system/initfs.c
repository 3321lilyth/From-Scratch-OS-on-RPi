#include "file_system/initfs.h"
struct filesystem tmpfs = {
    .name = "tmpfs",
    .setup_mount = tmpfs_setup_mount,
};
static struct filesystem tmpfs_dev = { 
    .name = "tmpfs_dev", 
    .setup_mount = tmpfs_setup_mount
};


//這是測試 PART1 和 PART2 用的，看能不能創建多個檔案系統，都能掛載、mkdir 和讀寫檔案
void init_rootfs1() {
    uart_write_str_raw("\r\n---------- [init] Step 1: register tmpfs ----------");
    register_filesystem(&tmpfs);

    uart_write_str_raw("\r\n----------[init] Step 2: mount tmpfs to / ----------");
    vfs_mount("/", "tmpfs");

    uart_write_str_raw("\r\n----------[init] Step 3: create directory /root_dir ----------");
    vfs_mkdir("root_dir");

    uart_write_str_raw("\r\n----------[init] Step 4: open /root_dir/root_file.txt ----------");
    static struct file* f;
    vfs_open("root_dir/root_file.txt", O_CREAT, &f);

    uart_write_str_raw("\r\n----------[init] Step 5: write to /root_dir/root_file.txt ----------");
    vfs_write(f, "ROOT!", 5);
    vfs_close(f);


    register_filesystem(&tmpfs_dev);
    uart_write_str_raw("\r\n----------[init] Step 6: create mount point /dev ----------");
    vfs_mkdir("dev");

    uart_write_str_raw("\r\n----------[init] Step 7: mount tmpfs_dev to /dev ----------");
    vfs_mount("dev", "tmpfs_dev");

    uart_write_str_raw("\r\n----------[init] Step 8: create /dev/dev_dir ----------");
    vfs_mkdir("dev/dev_dir");

    uart_write_str_raw("\r\n----------[init] Step 9: open /dev/dev_dir/dev_file.txt ----------");
    vfs_open("dev/dev_dir/dev_file.txt", O_CREAT, &f);

    uart_write_str_raw("\r\n----------[init] Step 10: write to /dev/dev_dir/dev_file.txt ----------");
    vfs_write(f, "DEVICE", 6);
    vfs_close(f);

    uart_write_str_raw("\r\n----------[init] Step 11: read back /root_dir/root_file.txt ----------");
    vfs_open("root_dir/root_file.txt", 0, &f);
    static char buf1[10] = {0};
    vfs_read(f, buf1, 5);
    uart_write_str_raw("\r\nread result: ");
    uart_write_str_raw(buf1);
    vfs_close(f);

    uart_write_str_raw("\r\n----------[init] Step 12: read back /dev/dev_dir/dev_file.txt ----------");
    vfs_open("dev/dev_dir/dev_file.txt", 0, &f);
    static char buf2[10] = {0};
    vfs_read(f, buf2, 6);
    uart_write_str_raw("\r\nread result: ");
    uart_write_str_raw(buf2);
    vfs_close(f);

    uart_write_str_raw("\r\n----------[init] Step 13: tmpfs_ls ----------");
    tmpfs_ls();
}



void test_vfs_errors() {
    register_filesystem(&tmpfs);
    vfs_mount("/", "tmpfs");


    // 先創建一個檔案並寫入內容
    static struct file* f;
    vfs_open("errfile.txt", O_CREAT, &f);
    vfs_write(f, "ABCDEF", 6);   // 正常寫入
    vfs_close(f);

    // 測試 1: 嘗試讀超過檔案範圍
    uart_write_str_raw("\r\n\r\n[TEST1] ==== read exceed file range ====");
    vfs_open("errfile.txt", 0, &f);
    static char buf1[10] = {0};
    static int len1;
    len1 = vfs_read(f, buf1, 10);  // 實際檔案長度只有 6
    uart_write_str_raw("\r\n    [TEST] read result: ");
    uart_write_str_raw(buf1);
    uart_write_str_raw(" (len=");
    uart_write_int_raw(len1);
    uart_write_str_raw(")");
    vfs_close(f);

    // 測試 2: 嘗試寫入超過 buffer 限制
    uart_write_str_raw("\r\n\r\n[TEST2] ==== write exceed file range ====");
    vfs_open("errfile.txt", 0, &f);
    static char bigdata[MAX_DATA_LEN + 100]; // 超過上限
    for (int i = 0; i < sizeof(bigdata); ++i) bigdata[i] = 'X';
    static int len2;
    len2 = vfs_write(f, bigdata, sizeof(bigdata));  // 應該被截斷
    uart_write_str_raw("\r\n    [TEST] write big buffer, actual len = ");
    uart_write_int_raw(len2);
    vfs_close(f);

    // 測試 3: 重複創建同一個檔案
    uart_write_str_raw("\r\n\r\n[TEST3] ==== create same file ====");
    static int create_ret;
    create_ret = vfs_create("errfile.txt");  // 已存在
    uart_write_str_raw("\r\n    [TEST] duplicate create return = ");
    uart_write_int_raw(create_ret);  // 預期是 -1

    // 測試 4: open 不存在的檔案（沒設 O_CREAT）
    uart_write_str_raw("\r\n\r\n[TEST4] ==== open file does not exist ====");
    static int open_ret;
    open_ret = vfs_open("ghost.txt", 0, &f); // 沒有此檔案
    uart_write_str_raw("\r\n    [TEST] open ghost.txt (no O_CREAT) return = ");
    uart_write_int_raw(open_ret); // 預期是 -1

    // 測試 5: 同一檔案連續寫兩次 offset 是否有更新
    uart_write_str_raw("\r\n\r\n[TEST5] ==== test write offset ====");
    vfs_open("twice.txt", O_CREAT, &f);
    vfs_write(f, "123", 3);  // offset 應變成 3
    vfs_write(f, "456", 3);  // offset 應變成 6
    vfs_close(f);
    vfs_open("twice.txt", 0, &f);
    static char buf2[10] = {0};
    static int len3;
    len3 = vfs_read(f, buf2, 10);  // 預期讀到 "123456"
    uart_write_str_raw("\r\n    [TEST] read twice.txt result: ");
    uart_write_str_raw(buf2);
    uart_write_str_raw(" (len=");
    uart_write_int_raw(len3);
    uart_write_str_raw(")");
    vfs_close(f);
    

    // test6: 使用一個已經關閉的 file*
    uart_write_str_raw("\r\n\r\n[TEST6] ==== access closed file ====");
    vfs_open("closed.txt", O_CREAT, &f);
    vfs_write(f, "ALIVE", 5);
    vfs_close(f);  // 關閉後 f 指向的 file 已被釋放

    static int closed_r;
    closed_r = vfs_read(f, buf1, 5);  // 嘗試再次讀取
    static int closed_w;
    closed_w = vfs_write(f, "FAIL", 4); // 嘗試再次寫入
    uart_write_str_raw("\r\n    [TEST] result of accessing closed file: ");
    uart_write_str_raw(fs_strerror(closed_r));
    uart_write_str_raw(" / ");
    uart_write_str_raw(fs_strerror(closed_w));

    // test7: 在同一個資料夾下面開超過 MAX_CHILDREN 個檔案
    uart_write_str_raw("\r\n\r\n[TEST7] ==== test MAX_CHILDREN overflow ====");
    vfs_mkdir("overflow");

    static char path[64];
    static char suffix[16];
    static struct file* f7;
    static int fail_at = -1;

    for (int i = 0; i < MAX_CHILDREN + 2; ++i) {
        // 構造路徑：overflow/file0.txt, overflow/file1.txt, ...
        uart_write_str_raw("\r\n--------------------creating ");
        uart_write_int_raw(i);
        uart_write_str_raw(" th child file ---------------------------");

        strcpy(path, "overflow/file");
        // suffix = "_0.txt", "_1.txt", ...
        suffix[0] = '_';
        suffix[1] = '0' + (i / 10);
        suffix[2] = '0' + (i % 10);
        suffix[3] = '.';
        suffix[4] = 't';
        suffix[5] = 'x';
        suffix[6] = 't';
        suffix[7] = '\0';
        strcat(path, suffix);

        int ret = vfs_open(path, O_CREAT, &f7);
        if (ret < 0) {
            uart_write_str_raw("\r\n    [TEST] create failed at ");
            uart_write_str_raw(path);
            uart_write_str_raw(", error = ");
            uart_write_str_raw(fs_strerror(ret));
            fail_at = i;
            break;
        }
        vfs_close(f7);
    }

    if (fail_at == MAX_CHILDREN) {
        uart_write_str_raw("\r\n    [TEST] correct: overflow rejected at MAX_CHILDREN");
    } else {
        uart_write_str_raw("\r\n    [TEST] unexpected: fail_at = ");
        uart_write_int_raw(fail_at);
    }



}



