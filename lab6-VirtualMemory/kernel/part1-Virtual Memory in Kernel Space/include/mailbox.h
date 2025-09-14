#ifndef _MAILBOX_H
#define _MAILBOX_H 

#include "mini_uart.h"
#include "simple_alloc.h"
#include "peripherals/gpio.h"
//下面這些設定是直接抄助教的(https://nycu-caslab.github.io/OSC2025/labs/hardware/mailbox.html)
#define MAILBOX_BASE    MMIO_BASE + 0xb880

#define MAILBOX_READ    (unsigned int*)(MAILBOX_BASE)
#define MAILBOX_STATUS  (unsigned int*)(MAILBOX_BASE + 0x18)
#define MAILBOX_WRITE   (unsigned int*)(MAILBOX_BASE + 0x20)

#define MAILBOX_EMPTY   0x40000000
#define MAILBOX_FULL    0x80000000		//MAILBOX_STATUS (MAILBOX_BASE + 0x18) 中的一個標誌（bit field），用來表示 mailbox 是否滿了


#define MAILBOX_RESPONSE 0x80000000		//跟 MAILBOX_FULL 是不同 register，mailbox[1] 的一部分，表示 mailbox 傳回的狀態

//tag 是代表我們想要拿到甚麼資訊，他才知道要給你 BOARD_REVISION 跟 GET_ARM_MEMORY
//這些值要參考助教給的連結: https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
#define GET_BOARD_REVISION  0x00010002
#define GET_ARM_MEMORY      0x00010005
#define REQUEST_CODE        0x00000000
#define REQUEST_SUCCEED     0x80000000
#define REQUEST_FAILED      0x80000001
#define TAG_REQUEST_CODE    0x00000000
#define END_TAG             0x00000000


extern uintptr_t mem_base;
extern uintptr_t mem_size;



typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t isrgb;
    uint8_t *buffer;
    uint32_t fb_size;
    uint32_t depth;
} framebuffer_info_t;



//mail box 
int mailbox_call(unsigned char channel, unsigned int *mbox);
void get_board_info();
void mailbox_get_memory();
//frame buffer related
int init_framebuffer(framebuffer_info_t *fb_info, uint32_t width, uint32_t height, uint32_t depth, uint32_t pixel_order);
void check_framebuffer();

void draw_test_pattern();
#endif