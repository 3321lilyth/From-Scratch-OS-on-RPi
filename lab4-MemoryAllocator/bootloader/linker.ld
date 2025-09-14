
SECTIONS
{
    . = 0x80000; 				/*bootloader 一開始還是放在 0x80000，這邊不能寫 __start ，為何?*/
	__begin = .;

    .text :   {
        KEEP(*(.text.boot))     /*KEEP() 確保 .text.boot 這段程式碼不會被 Linker 優化刪除。*/
        *(.text .text.*) 
    }
	. = ALIGN(0x8);  				/* 確保大小為 8 的倍數，不然在 boot.S 裡面 relocation 的時候會發現卡在 relocation loop 裡面出不來 */
    
	.rodata :{ *(.rodata .rodata.*)  } /*匹配 .rodata 以及所有以 .rodata. 開頭的 section*/
    .data : { *(.data .data.*) }
	. = ALIGN(0x8);
	
    .bss (NOLOAD):				/*NOLOAD: tell loader skip load this section, 值都是0了就別站空間了*/
    {
        __bss_start = .;
            *(.bss)             /*收集所有 .o 檔案的 .bss 內容*/
            *(COMMON)           /*未初始化且未指定 static 的全域變數*/
        __bss_end = .;
    }
	. = ALIGN(0x8);

    __end = .;
}
