SECTIONS
{
    . = 0xffff000000000000; /*kernel space is in high address space of virtual memory */
    . += 0x80000;           /*kernel load address */
    _kernel_start = .;
    
    .text :   {
        KEEP(*(.text.boot))     /*KEEP() 確保 .text.boot 這段程式碼不會被 Linker 優化刪除。*/
        *(.text) 
    }
    . = ALIGN(0x8);
    .rodata :{ *(.rodata) }
    .data : { *(.data) }
    . = ALIGN(0x8);
    .bss :
    {
        __bss_start = .;
            *(.bss)             /*收集所有 .o 檔案的 .bss 內容*/
            *(COMMON)           /*未初始化且未指定 static 的全域變數*/
        __bss_end = .;
    }
    . = ALIGN(0x1000);          /*對齊一個 page ，這樣我後面保留給 heap 的空間就也會對齊 page*/
    .heap :{
        __heap_start = .;
    }
    _end = .;
}
