#include "../include/reboot.h"

//NOTE: This snippet of code only works on real rpi3, not on QEMU.
//直接複製助教的 code 而已
//參考網站 : https://github.com/rsta2/circle/blob/master/include/circle/bcm2835.h
#define PM_PASSWORD 0x5a000000	//網站上是 #define ARM_PM_PASSWD		(0x5A << 24)
#define PM_RSTC 0x3F10001c		//網站上是 #define ARM_PM_RSTC		(ARM_PM_BASE + 0x1C)
#define PM_WDOG 0x3F100024		//網站上是 #define ARM_PM_WDOG		(ARM_PM_BASE + 0x24)

void set(long addr, unsigned int value) {
    volatile unsigned int* point = (unsigned int*)addr;
    *point = value;
}

void reset(int tick) {                 // reboot after watchdog timer expire
    set(PM_RSTC, PM_PASSWORD | 0x20);  // full reset
    set(PM_WDOG, PM_PASSWORD | tick);  // number of watchdog tick
}
//PM_WDOG 是 watch dog  regisyter ，系統發生錯誤的時候，會負責對系統發出重設或關閉的訊號。
//本身是一個計時器，倒數完成之後，就會依照我們對  PM_RSTC 的設置來進行重新開機
//PM_RSTC : reset control register
