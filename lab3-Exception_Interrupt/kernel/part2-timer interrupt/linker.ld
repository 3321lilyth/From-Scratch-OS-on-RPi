SECTIONS
{
    . = 0x80000;
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
    . = ALIGN(0x8);
    .heap :{
        __heap_start = .;
    }
    _end = .;
}
