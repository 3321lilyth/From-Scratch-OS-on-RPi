SECTIONS
{
    . = _start;
    .text :   {
        KEEP(*(.text.boot))     /*KEEP() 確保 .text.boot 這段程式碼不會被 Linker 優化刪除。*/
        *(.text) 
    }
    .rodata :{ *(.rodata) }
    .data : { *(.data) }
	. = ALIGN(8);
    .bss :
    {
        __bss_start = .;
            *(.bss)             /*收集所有 .o 檔案的 .bss 內容*/
            *(COMMON)           /*未初始化且未指定 static 的全域變數*/
        __bss_end = .;
    }
	. = ALIGN(8);
}
