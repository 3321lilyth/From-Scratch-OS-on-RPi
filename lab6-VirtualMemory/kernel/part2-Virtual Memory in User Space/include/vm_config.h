#ifndef _VM_COMFIG_H
#define _VM_COMFIG_H



//tcr
#define TCR_CONFIG_REGION_48bit (((64 - 48) << 0) | ((64 - 48) << 16))  
    // (64 - 48) << 0 設定 TCR 裡面的 T0SZ，代表  ttbr0 控 48-bit 低位地址
    // (64 - 48) << 0 設定 TCR 裡面的 T1SZ，代表  ttbr1 控 48-bit 高位地址
#define TCR_CONFIG_4KB ((0b00 << 14) |  (0b10 << 30))                   // 4KB page + TTBR1 for high VA
#define TCR_CONFIG_DEFAULT (TCR_CONFIG_REGION_48bit | TCR_CONFIG_4KB)

//mair
#define MAIR_DEVICE_nGnRnE 0b00000000
#define MAIR_NORMAL_NOCACHE 0b01000100
#define MAIR_IDX_DEVICE_nGnRnE 0
#define MAIR_IDX_NORMAL_NOCACHE 1


//attribute
#define PD_TABLE 0b11
#define PD_BLOCK 0b01
#define PD_ACCESS (1L << 10)
#define PD_AF     (1L << 10)
#define BOOT_PGD_ATTR PD_TABLE
#define BOOT_PUD_ATTR (PD_ACCESS | (MAIR_IDX_DEVICE_nGnRnE << 2) | PD_BLOCK)


//address allocate (這是物理記憶體)
//注意 ttbr 裡面寫的一定要是 PA，這樣才能確保 MMU access 到 page table
//page table 裡面也應該要寫 PA
#define BOOT_PGD_ADDR 0x1000
#define BOOT_PUD_ADDR (BOOT_PGD_ADDR + 0x1000)


#endif