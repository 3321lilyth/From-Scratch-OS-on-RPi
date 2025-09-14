#include "mailbox.h"

//__attribute__((aligned(16))) 是 GCC 的編譯器屬性 (attribute)，表示這塊記憶體的起始位址 (address) 必須是16的倍數(對齊16)。
// GPU 才能正確地讀取資料
// volatile unsigned int __attribute__((aligned(16))) mailbox[36];  // Mailbox buffer

int mailbox_call(unsigned char channel, unsigned int *mbox){
	//1. Combine the message address (upper 28 bits) with channel number (lower 4 bits)
	// unsigned int msg = ((unsigned int)((unsigned long)&mbox) & ~0xF) | (channel & 0xF);  //如果你的 mailbox 是全域變數那條就要用這個
    unsigned int msg = ((unsigned int)((unsigned long)mbox) & ~0xF) | (channel & 0xF);
		//uint 轉 ulong 是因為 AArch64 是 64bit 位址，而 uint 只有 32 bit 放不下
		//0xF 的二進制是 0b1111，取反 (~0xF) 就變成 0xFFFFFFF0 (最低 4 位元清零)。
		//& ~0xF 的作用是 確保 mailbox 位址的最低 4 個 bit 是 0，確保該位址是 16-byte 對齊。
		//ch & 0xF 確保 ch 只佔用最低 4 個 bit（避免溢出影響 mailbox 位址）。
    
	
	//2. Check if Mailbox 0 status register’s full flag is set.
    while (*MAILBOX_STATUS & MAILBOX_FULL) {asm volatile("nop");};
    
    //3. If not full, then you can write to Mailbox 1 Read/Write register.
    *MAILBOX_WRITE = msg;

    while (1) {
        //4. Check if Mailbox 0 status register’s empty flag is set.
        while (*MAILBOX_STATUS & MAILBOX_EMPTY) {asm volatile("nop");};

        //5. If not, then you can read from Mailbox 0 Read/Write register.
        //6. Check if the value is the same as you wrote in step 1.
        if (msg == *MAILBOX_READ){ 	
			// uart_write_str("\r\ncheck Mailbox address: ");
			// uart_write_hex((unsigned int)((unsigned long)&mailbox));
            return 1;  // 檢查回應是否成功
		}
    }
	return 0;
}

void get_board_info(){
    unsigned int __attribute__((aligned(16))) mailbox[36];
    mailbox[0] = 8 * 4;                 // buffer size in bytes
    mailbox[1] = REQUEST_CODE;

	// 這些值要參考助教給的連結: https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
	// 取得 Board revision
    // tags begin
    mailbox[2] = GET_BOARD_REVISION;    // tag identifier
    mailbox[3] = 4;                     // maximum of request and response value buffer's length.
										// 因為網址寫說Get board revision的request length=0；response length=4
    mailbox[4] = TAG_REQUEST_CODE;
    mailbox[5] = 0;                     // value buffer (output)
	

	// 取得 ARM memory base address and size
	mailbox[6] = GET_ARM_MEMORY;  		// Tag: ARM memory
    mailbox[7] = 8;						//因為網址寫說Get ARM memory的request length=0；response length=8
										//那因為是8，所以下面才要給他 mailbox[9] + mailbox[10] 共8B的空間給他
    mailbox[8] = TAG_REQUEST_CODE;
    mailbox[9] = 0;  					// value buffer (output)
    mailbox[10] = 0; 					// value buffer (output)

    mailbox[11] = END_TAG;  			// End tag

    if (mailbox_call(8, mailbox)){
		uart_write_str("\r\n");
		uart_write_str("Board revision: ");
		uart_write_hex(mailbox[5]);		// it should be 0xa020d3 for rpi3 b+
		uart_write_str("\r\n");
		uart_write_str("ARM memory base address : ");
		uart_write_hex(mailbox[9]);
		uart_write_str("\r\n");
		uart_write_str("ARM memory size : ");
		uart_write_hex(mailbox[10]);

	}else{
		uart_write_str("\r\nMailbox call failed.");
	}
}


// 從 mailbox 裡面讀出記憶體的範圍，雖然這邊我們已知是 0x0~0x3C000000  就是了
uintptr_t mem_base = 0;
uintptr_t mem_size = 0;

void mailbox_get_memory() {
    unsigned int __attribute__((aligned(16))) mailbox[36];
    mailbox[0] = 7 * 4;
    mailbox[1] = REQUEST_CODE;

    mailbox[2] = GET_ARM_MEMORY;
    mailbox[3] = 8;
    mailbox[4] = TAG_REQUEST_CODE;
    mailbox[5] = 0;
    mailbox[6] = 0;

    mailbox[7] = END_TAG;

    if (mailbox_call(8, mailbox)) {
        mem_base = (uintptr_t)mailbox[5];
        mem_size = (uintptr_t)mailbox[6];
    } else {
        uart_write_str("\r\n[mailbox] ERROR: Failed to get memory info");
    }
}





















int init_framebuffer(framebuffer_info_t *fb_info, uint32_t width, uint32_t height, uint32_t depth, uint32_t pixel_order) {
    unsigned int __attribute__((aligned(16))) mbox[36];
    // 準備 mailbox buffer
    mbox[0] = 35 * 4;    // total size
    mbox[1] = 0;         // request

    mbox[2] = 0x00048003; mbox[3] = 8; mbox[4] = 8;      // physical width/height
    mbox[5] = width;
    mbox[6] = height;

    mbox[7] = 0x00048004; mbox[8] = 8; mbox[9] = 8;      // virtual width/height
    mbox[10] = width;
    mbox[11] = height;

    mbox[12] = 0x00048005; mbox[13] = 8; mbox[14] = 8;   // depth
    mbox[15] = depth;

    mbox[16] = 0x00048006; mbox[17] = 8; mbox[18] = 8;   // pixel order
    mbox[19] = pixel_order;

    mbox[20] = 0x00040001; mbox[21] = 8; mbox[22] = 8;   // allocate framebuffer
    mbox[23] = 16;   // alignment = 16
    mbox[24] = 0;    // will be filled by GPU: framebuffer addr

    mbox[25] = 0x00040008; mbox[26] = 4; mbox[27] = 4;   // get pitch
    mbox[28] = 0;    // will be filled by GPU: pitch

    mbox[29] = 0;  // end tag

    if (mailbox_call(8, mbox)) {
        if (mbox[20] != 0) {
            fb_info->width = mbox[5];
            fb_info->height = mbox[6];
            fb_info->pitch = mbox[28];
            fb_info->isrgb = (mbox[19] == 0);  // 1 = RGB, 0 = BGR
            fb_info->buffer = (uint8_t*)((uintptr_t)(mbox[24] & 0x3FFFFFFF));  
                // GPU 會回傳一個「高位有 cache 標記」的地址，所以要 bitwise AND 0x3FFFFFFF 才會拿到真正的實體 address
            
            uart_write_str_raw("\r\n[framebuffer] Init OK: addr=");
            uart_write_hex_raw((uintptr_t)fb_info->buffer);
            uart_write_str_raw(", width=");
            uart_write_int_raw(fb_info->width);
            uart_write_str_raw(", height=");
            uart_write_int_raw(fb_info->height);
            uart_write_str_raw(", pitch=");
            uart_write_int_raw(fb_info->pitch);
            return 0;  // OK
        }
    }

    uart_write_str_raw("\r\n[framebuffer] Init FAIL");
    return -1;
}


// usage: 
// if (init_framebuffer(&fb_info, 1024, 768, 32, 0) == 0) {
//     // framebuffer ok, 可以用 fb_info.buffer 畫 pixel
// }
void draw_pixel(framebuffer_info_t *fb, int x, int y, uint32_t color) {
    uint32_t *pixel = (uint32_t*)(fb->buffer + y * fb->pitch + x * 4);
    *pixel = color;
}



//看 GPU 回的地址是不是 0（0 = 沒有 allocate）
void check_framebuffer() {
    unsigned int __attribute__((aligned(16))) mbox[36];  // 比原本大，因為我們會重複用

    // (1) Get Framebuffer Addr
    mbox[0] = 8 * 4;
    mbox[1] = 0;
    mbox[2] = 0x00040001;
    mbox[3] = 8;
    mbox[4] = 0;
    mbox[5] = 16;
    mbox[6] = 0;
    mbox[7] = 0;

    if (mailbox_call(8, mbox)) {
        uint32_t fb_addr = mbox[5];
        uart_write_str_raw("\r\n[fb check] Addr: ");
        uart_write_hex_raw(fb_addr);
        if (fb_addr == 0)
            uart_write_str_raw(" (NOT allocated)");
        else
            uart_write_str_raw(" (allocated)");
    }

    // (2) Get Physical Width/Height
    mbox[0] = 8 * 4;
    mbox[1] = 0;
    mbox[2] = 0x00040003;
    mbox[3] = 8;
    mbox[4] = 0;
    mbox[5] = 0;  // width
    mbox[6] = 0;  // height
    mbox[7] = 0;

    if (mailbox_call(8, mbox)) {
        uart_write_str_raw("\r\n[fb check] Physical Width: ");
        uart_write_int_raw(mbox[5]);
        uart_write_str_raw(", Height: ");
        uart_write_int_raw(mbox[6]);
    }

    // (3) Get Virtual Width/Height
    mbox[0] = 8 * 4;
    mbox[1] = 0;
    mbox[2] = 0x00040004;
    mbox[3] = 8;
    mbox[4] = 0;
    mbox[5] = 0;  // width
    mbox[6] = 0;  // height
    mbox[7] = 0;

    if (mailbox_call(8, mbox)) {
        uart_write_str_raw("\r\n[fb check] Virtual Width: ");
        uart_write_int_raw(mbox[5]);
        uart_write_str_raw(", Height: ");
        uart_write_int_raw(mbox[6]);
    }

    // (4) Get Depth
    mbox[0] = 7 * 4;
    mbox[1] = 0;
    mbox[2] = 0x00040005;
    mbox[3] = 4;
    mbox[4] = 0;
    mbox[5] = 0;
    mbox[6] = 0;

    if (mailbox_call(8, mbox)) {
        uart_write_str_raw("\r\n[fb check] Depth: ");
        uart_write_int_raw(mbox[5]);
    }

    // (5) Get Pixel Order
    mbox[0] = 7 * 4;
    mbox[1] = 0;
    mbox[2] = 0x00040006;
    mbox[3] = 4;
    mbox[4] = 0;
    mbox[5] = 0;
    mbox[6] = 0;

    if (mailbox_call(8, mbox)) {
        uart_write_str_raw("\r\n[fb check] Pixel Order: ");
        if (mbox[5] == 0) {
            uart_write_str_raw("RGB");
        } else if (mbox[5] == 1) {
            uart_write_str_raw("BGR");
        } else {
            uart_write_str_raw("Unknown (");
            uart_write_int_raw(mbox[5]);
            uart_write_str_raw(")");
        }
    }

    // (6) Get Pitch
    mbox[0] = 7 * 4;
    mbox[1] = 0;
    mbox[2] = 0x00040008;
    mbox[3] = 4;
    mbox[4] = 0;
    mbox[5] = 0;
    mbox[6] = 0;

    if (mailbox_call(8, mbox)) {
        uart_write_str_raw("\r\n[fb check] Pitch: ");
        uart_write_int_raw(mbox[5]);
    }
}


void draw_test_pattern() {
    unsigned int __attribute__((aligned(16))) mbox[36];

    // (1) Get Framebuffer Addr
    mbox[0] = 8 * 4;
    mbox[1] = 0;
    mbox[2] = 0x00040001;
    mbox[3] = 8;
    mbox[4] = 0;
    mbox[5] = 16;  // alignment
    mbox[6] = 0;
    mbox[7] = 0;

    if (!mailbox_call(8, mbox)) {
        uart_write_str_raw("\r\n[draw] Failed to get framebuffer addr");
        return;
    }
    uint32_t fb_addr = mbox[5] & 0x3FFFFFFF;  // 注意 GPU 會回傳 bus addr，要 & 0x3FFFFFFF

    // (2) Get Physical Width/Height
    mbox[0] = 8 * 4;
    mbox[1] = 0;
    mbox[2] = 0x00040003;
    mbox[3] = 8;
    mbox[4] = 0;
    mbox[5] = 0;
    mbox[6] = 0;
    mbox[7] = 0;

    mailbox_call(8, mbox);
    uint32_t width = mbox[5];
    uint32_t height = mbox[6];

    // (3) Get Pitch
    mbox[0] = 7 * 4;
    mbox[1] = 0;
    mbox[2] = 0x00040008;
    mbox[3] = 4;
    mbox[4] = 0;
    mbox[5] = 0;
    mbox[6] = 0;

    mailbox_call(8, mbox);
    uint32_t pitch = mbox[5];

    // (4) Get Pixel Order
    mbox[0] = 7 * 4;
    mbox[1] = 0;
    mbox[2] = 0x00040006;
    mbox[3] = 4;
    mbox[4] = 0;
    mbox[5] = 0;
    mbox[6] = 0;

    mailbox_call(8, mbox);
    uint32_t pixel_order = mbox[5];

    // (5) 開始畫
    uint16_t *pixel;

    for (uint32_t y = 0; y < height; y++) {
        pixel = (uint16_t *)((uintptr_t)fb_addr + y * pitch);
        for (uint32_t x = 0; x < width; x++) {
            uint8_t r = (x * 31) / width;
            uint8_t g = (y * 63) / height;
            uint8_t b = ((x + y) * 31) / (width + height);

            uint16_t color;
            if (pixel_order == 0) {  // RGB
                color = (r << 11) | (g << 5) | (b);
            } else {  // BGR
                color = (b << 11) | (g << 5) | (r);
            }

            pixel[x] = color;
        }
    }

    uart_write_str_raw("\r\n[draw] Finished drawing test pattern.");
}
