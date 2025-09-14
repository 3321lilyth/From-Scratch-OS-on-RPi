
#ifndef _DEVTREE_H
#define _DEVTREE_H

#include "type.h" 
#include "mini_uart.h" 
#include "utli.h" 

//1. dtb = header + 一堆 block，中間會穿插 free space。header 在下面第二點，阿 block 結構去看圖
//2. header 定義如下 all the header fields are 32-bit integers, stored in big-endian format.
struct fdt_header {
	uint32_t magic;			    /* magic word FDT_MAGIC */
	uint32_t totalsize;		    /* total size of DT block */
	uint32_t off_dt_struct;		 /* offset to structure，包含 node, property 跟 tag*/
        //node 應該就是 device, property 結構如下，token 描述 node 之間的層級關係，在上面的 define
	uint32_t off_dt_strings;	 /* offset to strings，裡面放所有出現的 property 的名字*/
	uint32_t off_mem_rsvmap;	 /* offset to memory reserve map，一般不會拿來當作記憶體分配，會拿來放不能被覆蓋的重要資料*/
	uint32_t version;		     /* format version */
	uint32_t last_comp_version;	 /* last compatible version */

	/* version 2 fields below */
	uint32_t boot_cpuid_phys;	 /* Which physical CPU id we're booting on */
	/* version 3 fields below */
	uint32_t size_dt_strings;	 /* size of the strings block */

	/* version 17 fields below */
	uint32_t size_dt_struct;	 /* size of the structure block */
};//總共 40B，也就是 0x28


//3. structure block 裡面有 node, property 跟 tag(好像也叫做token) 三種東西，tag 其實就是拿來隔開 node 跟 property 而已
    
//3-1. tag 的定義
#define FDT_HEADER_MAGIC    0xd00dfeed  //整個 dtb 最前面的 4B 為 magic
#define FDT_BEGIN_NODE      0x00000001  //代表 node 的起始(遇到新的 node)，後面會跟著 node name，root 的 node name 為空所以會填 4B 的 0
#define FDT_END_NODE        0x00000002  //代表 node 的結束
#define FDT_PROP            0x00000003  //代表 property 開始，後面會跟著 struct fdt_property
#define FDT_NOP             0x00000004  //廢話就是教你忽略
#define FDT_END             0x00000009  //end of structure block

//3-2. property 的定義
struct fdt_property {
	uint32_t len;
	uint32_t nameoff;   //相對於  off_dt_strings 的偏移量，因為 propertyname(null-terminate) 本身都存在 string block 裡面，這邊只存了他位於 string block 的哪裡
    //從這邊拿到nameoff後，就知道 name 是從 header position + off_dt_strings + nameoff 這個位址開始
    //假設取出來的 name 是 ”#address-cells”，fdt_property->data 的值為0x00000001，就知道原本長相是 "#address-cells=<0x00000001>"

};



extern uintptr_t dtb_start_addr;
extern uintptr_t dtb_end_addr;

//遍歷 Device Tree（DTB）並對找到的節點執行 callback 函數。
//void (*callback)(char *, char *, struct fdt_prop *) 是一個函數指標 (function pointer)
//也就是 fdt_traverse 是以 callback 這個 function 的 pointer 作為傳入參數
//callback 本身定義為 void callback(char *, char *, struct fdt_prop *); 是 fdt_traverse 遍歷到符合條件的 Device Tree 節點時，會執行的函數。
void fdt_traverse ( void (*callback)(char *, char *, struct fdt_property *, char *), uint64_t* dtb_ptr );

#endif
