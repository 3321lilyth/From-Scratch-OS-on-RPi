#include "mini_uart.h"
#include "mailbox.h"
#include "utli.h"
#include "reboot.h" 
#include "devicetree.h"
#include "cpio.h" 
#include "simple_alloc.h" 
#include "exception.h" 
#include "timer.h" 
#include "buddy.h"
#include "dynamic_alloc.h"
#include "memory_test.h"
#include "thread.h"
#include "thread_functions.h"
#include "file_system/initfs.h"


int uart_interrupt_enabled = 0; // only change "help command (write)" and "enter cmd (read)"

size_t prase_number(const char *cmd) {
    char *endptr;
    
    // 找到 "malloc " 之後的數字部分
    while (*cmd && *cmd != ' ') {
        cmd++;  // 跳過 "malloc" 或者 buddy_alloc 或者 chunk_alloc
    }
    while (*cmd == ' ') {
        cmd++;  // 跳過空格
    }

    // 使用 strtol ，from string to int，並允許不同進位
    size_t size = (size_t)strtol(cmd, &endptr, 0);

    // 確保 size 是有效的正整數
    if (size == 0 || *endptr != '\0') {
        return 0;  // 解析失敗，回傳 0
    }

    return size;
}

//給 lab3 用的 callback function，只要能印出 log 就好
void timeout_callback_log(void *msg) {
    uart_write_str("\r\n[TIMEOUT] ");
    uart_write_str((char *)msg);
}

void parse_settimeout(char *cmd) {
    // 預設格式: setTimeout hello 2
    char *msg = cmd + 10;
    while (*msg == ' ') msg++;
    char *sec_str = msg;

    while (*sec_str && *sec_str != ' ') sec_str++;
    if (*sec_str) {
        *sec_str = '\0'; // 分割字串
        sec_str++;
    }

    // 轉換秒數
    unsigned int seconds = 0;
    while (*sec_str >= '0' && *sec_str <= '9') {
        seconds = seconds * 10 + (*sec_str - '0');
        sec_str++;
    }
    unsigned int milliseconds = 1000*seconds;

    // 呼叫 timer 插入函式
    add_timer(timeout_callback_log, "timeout_callback_log", (void*)msg, milliseconds);
}

void print_help(){
    if (uart_interrupt_enabled == 0){
        uart_write_str("\r\n    -------------------  basic cmd  ----------------------------------");        
        uart_write_str("\r\n    help           : print this help menu");
        uart_write_str("\r\n    hello          : print Hello World!");
        uart_write_str("\r\n    mailbox        : print mailbox info");
        uart_write_str("\r\n    reboot         : reboot the device");              
        uart_write_str("\r\n    ------------------- file related ---------------------------------");        
        uart_write_str("\r\n    ls             : list all file");        
        uart_write_str("\r\n    cat <filename> : cat one file");        
        uart_write_str("\r\n    exec <program> : execute user program");        
        uart_write_str("\r\n    ------------------- interrupt related ----------------------------");        
        uart_write_str("\r\n    timer_on               : enable timer, and interrupt every 2 second");
        uart_write_str("\r\n    setTimeout <msg> <sec> : print msg after sec seconds");
        uart_write_str("\r\n    async_on       : change to async mode(for read cmd and write 'help' cmd)");
        uart_write_str("\r\n    async_off      : change to sync mode");
        uart_write_str("\r\n    async                  : test async uart");
        uart_write_str("\r\n    ------------------- memory system related -------------------------");      
        uart_write_str("\r\n    kmalloc <size>            : allocate <size> bytes(in dec) memory");
        uart_write_str("\r\n    kfree <address>           : free memory");
        uart_write_str("\r\n    memory_test               : exec test case directly");  
        uart_write_str("\r\n    buddy_allocated_nodes     : show buddy system allocated pages");
        uart_write_str("\r\n    buddy_free_blocks <order> : show buddy system free blocks at target <order>");
        uart_write_str("\r\n    chunk_info                : show dynamic allocator pages of 5 pools");
        uart_write_str("\r\n    ------------------- thread related -------------------------");      
        uart_write_str("\r\n    foo_sync_preempt          : lab5 test, sync uart + preempt on + all kernel thread");
        uart_write_str("\r\n    foo_sync_preempt_user     : lab5 test, sync uart + preempt on + foo() is user thread");
    }else{
        uart_write_str_async("\r\n    -------------------  basic cmd  ----------------------------------");        
        uart_write_str_async("\r\n    help           : print this help menu");
        uart_write_str_async("\r\n    hello          : print Hello World!");
        uart_write_str_async("\r\n    mailbox        : print mailbox info");
        uart_write_str_async("\r\n    reboot         : reboot the device");           
        uart_write_str_async("\r\n    ------------------- file related ---------------------------------");        
        uart_write_str_async("\r\n    ls             : list all file");        
        uart_write_str_async("\r\n    cat <filename> : cat one file");        
        uart_write_str_async("\r\n    exec <program> : execute user program");        
        uart_write_str_async("\r\n    ------------------- interrupt related ----------------------------");        
        uart_write_str_async("\r\n    timer_on       : enable timer, and interrupt every 2 second");
        uart_write_str_async("\r\n    setTimeout <msg> <sec> : print msg after sec seconds");
        uart_write_str_async("\r\n    async_on       : change to async mode(for read cmd and write 'help' cmd)");
        uart_write_str_async("\r\n    async_off      : change to sync mode");
        uart_write_str_async("\r\n    async          : test async uart");
        uart_write_str_async("\r\n    ------------------- memory system related -------------------------");      
        uart_write_str_async("\r\n    kmalloc <size>            : allocate <size> bytes(in dec) memory");
        uart_write_str_async("\r\n    kfree <address>           : free memory");
        uart_write_str_async("\r\n    memory_test               : exec test case directly");  
        uart_write_str_async("\r\n    buddy_allocated_nodes     : show buddy system allocated pages");
        uart_write_str_async("\r\n    buddy_free_blocks <order> : show buddy system free blocks at target <order>");
        uart_write_str_async("\r\n    chunk_info                : show dynamic allocator pages of 5 pools");
        uart_write_str_async("\r\n    ------------------- thread related -------------------------");      
        uart_write_str_async("\r\n    foo_sync_preempt          : lab5 part1 thread test case");
    }
}

void async_on(){
    if (uart_interrupt_enabled == 0){
        uart_write_str("\r\nenable uart interrupt ...");
        enable_uart_interrupt();
        uart_interrupt_enabled = 1;
    }else{
        uart_write_str("\r\nalready in async mode");
    }
}


































void shell(){
	while(1){
		char cmd[256];
        if (uart_interrupt_enabled == 0){
		    uart_write_str("\r\nLily@Rpi3B+ (sync mode)> ");
		    readcmd(cmd);
        }else{
            uart_write_str_async("\r\nLily@Rpi3B+ (async mode)> ");
            readcmd_async(cmd);
        }


        // basic command
		if (strcmp(cmd, "help")){
            print_help();
		}else if (strcmp(cmd, "hello")){
			uart_write_str("\r\nHello World!");
		}else if (strcmp(cmd, "mailbox")){
		    get_board_info();
		}else if (strcmp(cmd, "reboot")){
		    uart_write_str("\r\nRebooting...");
		    reset(200);
            return;
		}
        
        //initramfs related
        else if (memcmp(cmd, "cat", 3) == 0){
            char filename[256];
            int i = 4;  // "cat " 的長度
            int j = 0;
            int ret_value = 1;
            while (cmd[i] != 0 && cmd[i] != ' ') {
                filename[j++] = cmd[i++];
            }
            filename[j] = '\0';
            ret_value = cpio_cat(filename);

            //沒有找到指定檔案
            if (ret_value == 0){
			    uart_write_str("\r\nNo file found");
            }
        }else if (strcmp(cmd, "ls")){
            cpio_ls();
        }else if (memcmp(cmd, "exec", 4) == 0){
            char program_name[256];
            int i = 5;  // "exec " 的長度
            int j = 0;
            int ret_value = 1;
            while (cmd[i] != 0 && cmd[i] != ' ') {
                program_name[j++] = cmd[i++];
            }
            program_name[j] = '\0';
            ret_value = cpio_exec_user_program(program_name);

            //沒有找到指定檔案
            if (ret_value == 0){
			    uart_write_str("\r\nNo Program found");
            }
        }
        




        // interrupt related
        // else if (strcmp(cmd, "timer_on")){ //lab3
        //     core_timer_enable();
        // }
        else if (memcmp(cmd, "setTimeout", 10) == 0){
            // 格式: setTimeout <message> <seconds>
            parse_settimeout(cmd);
        }else if (strcmp(cmd, "async")){
            char async_buf[BUFFER_MAX_SIZE];

            enable_uart_interrupt();

            uart_write_str_async("\r\nAsync I/O test:");
            readcmd_async(async_buf);
            uart_write_str_async("\r\nYou just typed:");
            uart_write_str_async(async_buf);
            // 這行要加，不然你的 uart_write_str_async 裡面還沒 enable TX，你這邊就 disable_uart_interrupt() 了
            uart_write_flush();

            disable_uart_interrupt();
        }
        // else if (strcmp(cmd, "async_on")){
        //     async_on();
        // }else if (strcmp(cmd, "async_off")){
        //     if (uart_interrupt_enabled == 1){
        //         uart_write_str("\r\ndisable uart interrupt ...");
        //         disable_uart_interrupt();
        //         uart_interrupt_enabled = 0;
        //     }else{
        //         uart_write_str("\r\nalready in sync mode");   
        //     }
        // }




        //memory related
        else if (memcmp(cmd, "memory_test", 10) == 0){
            //can choose from test1, test2, test3
            size_t test_case = prase_number(cmd);
            switch (test_case) {
                case 1: buddy_test1(); break;
                case 2: buddy_test2(); break;
                case 3: buddy_test3(); break;
                case 4: chunk_test1(); break;
                case 5: chunk_test2(); break;
                case 6: chunk_test3(); break;
                case 7: mix_test1(); break;
                case 8: mix_test2(); break;
                case 9: buddy_test4(); break;
                default:     uart_write_str("\r\n[main] ERROR: only have 6 test case now");     break;
            }
        }else if (memcmp(cmd, "kmalloc", 7) == 0){
            // 格式: alloc <size in bytes>
            size_t size = prase_number(cmd);
            kmalloc(size);

        }else if (memcmp(cmd, "kfree", 5) == 0){
            // 格式: buddy_free <address>
            char *endptr;
            uintptr_t addr = (uintptr_t)strtol(cmd + 5, &endptr, 0);
            if (*endptr != '\0') {
                uart_write_str("\r\n[main] Free invalid address");
            } else{
                kfree(addr);
            }
        }else if (strcmp(cmd, "buddy_allocated_nodes")){
            dump_allocated_nodes();
        }else if(memcmp(cmd, "buddy_free_blocks", 17) == 0){
            int order = prase_number(cmd);
            dump_free_blocks(order);
        }else if (strcmp(cmd, "chunk_info")){
            dump_chunk_pools();
        }



        //thread related
        // else if (strcmp(cmd, "thread_foo")){
        //     uart_write_str("\r\n=========================  create foo_sync threads  =========================");
        //     thread_create(idle, MAX_PRIORITY - 1);
        //     for(int i=0; i<5; i++){
        //         thread_create(foo_sync, 0);
        //     }
        //     uart_write_str("\r\n========================= after thread create =========================");
            
        //     asm volatile("msr tpidr_el1, xzr");
        //     core_timer_enable();
        //     // schedule(1);
        //     schedule((void*)1);
        // } 
        else if (strcmp(cmd, "foo_sync_preempt")){
            // 全部都是 kernel thread
            uart_write_str("\r\n=========================  create foo_sync_preempt threads  =========================");
            thread_create(idle, MAX_PRIORITY - 1, 0);
            for(int i=0; i<5; i++){
                thread_create(foo_sync_preempt, 0, 0);
            }
            uart_write_str("\r\n========================= after thread create =========================");
            ready_queue_dump();
            asm volatile("msr tpidr_el1, xzr");
            core_timer_enable();
            schedule_complete((void*)1);
        } 
        else if (strcmp(cmd, "foo_sync_preempt_user")){
            uart_write_str("\r\n=========================  create foo_sync_preempt_user threads  =========================");
            //只有 idle 是 kernel thread
            thread_create(idle, MAX_PRIORITY - 1, 0);
            //其他都是 user thread
            for(int i=0; i<5; i++){
                thread_create(foo_sync_preempt_user, 0, 1);
            }
            uart_write_str("\r\n========================= after thread create =========================");
            ready_queue_dump();
            asm volatile("msr tpidr_el1, xzr");
            core_timer_enable();
            schedule_complete((void*)1);
        }

        
        else{
			uart_write_str("\r\nWrong command, please type help");
		}
	}

}

int main(){
    //1. uart init
    uart_init();

    //2. get dtb start address
    extern uintptr_t dtb_start_addr; // lab2 part6
    asm volatile("mov %0, x21" :  "=r"(dtb_start_addr));
    
    //3. get dtb end address & initramfs address
    //注意一定要在 startup_alloc_init 前呼叫，不然 startup_alloc_init 裡面就無法 reserve dtb
    fdt_traverse(initramfs_callback, (uint64_t *)dtb_start_addr);

    //4. get memory base and memory size (from dtb or mailbox)
    // fdt_traverse(memory_callback, dtb_address);          //我用 device tree 就是找不到沒辦法
    mailbox_get_memory();
    extern uintptr_t mem_base, mem_size;        //定義在 cpio.h，要從 device tree 裡面取出來
    
    //5. startup allocate memory for buddy & dynamic system
    startup_alloc_init(mem_base, mem_size);

    // asm volatile("msr tpidr_el1, xzr");
    // core_timer_enable();


    //6. initial tasks
    uart_write_str("\r\n============================= create idle task & shell task =============================");
    // thread_create(idle, MAX_PRIORITY - 1, 0);
    // thread_create(foo_sync_preempt_user, 0, 1);
    // thread_create(signal_handler_test, 0, 1);
    // thread_create(video_player, 0, 1);
    ready_queue_dump();
    
    //7. enable timer & enable EL0 access timer register
    // asm volatile("msr tpidr_el1, xzr");
    // core_timer_enable();
    // set_timeout();

    // 8. check frame buffer
    // framebuffer_info_t fb_info;
    // init_framebuffer(&fb_info, 640, 480, 16, 0);  我發現要不要初始化 frame buffer 都會動，應該是助教那邊有做
    // check_framebuffer();

    //9.start! 如果要跑我們自己的 shell 就要把 schedule_complete((void*)1); 先註解掉喔!!!
    uart_write_str("\r\n============================= start running tasks =============================");
    // init_rootfs1();
    test_vfs_errors();
    // schedule_complete((void*)1);
    // shell();
    return 0;
}









