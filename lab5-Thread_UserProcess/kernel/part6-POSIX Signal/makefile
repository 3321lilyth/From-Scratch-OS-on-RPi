SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)

ASMS = $(wildcard asm/*.S)
ASM_OBJS = $(ASMS:.S=.o)


# CFLAGS = -Iinclude -Wall -Wextra -Wpedantic -O0 -ffreestanding -nostdinc -nostdlib -nostartfiles 
COPS = -Wall -nostdlib -nostartfiles -ffreestanding -Iinclude -mgeneral-regs-only
ASMOPS = -Iinclude 

DTB_PATH = ../../rpi_firmware/bcm2710-rpi-3-b-plus.dtb
INITRAMFS_PATH = ../../rpi_firmware/initramfs.cpio

############################# all #################################################
all: kernel8.img

# 告訴 make，當它遇到 .S 時，應該使用這條規則來生成 .o。
# $< 代表依賴的文件（此處是 .S），而 $@ 代表目標文件（此處是 .o）

# %.o: %.S
asm/%.o: asm/%.S
	aarch64-linux-gnu-gcc $(ASMOPS) -c $< -o $@

%.o: %.c
	aarch64-linux-gnu-gcc $(COPS) -c $< -o $@

kernel8.img: $(ASM_OBJS) $(OBJS)
	@echo "SRC = $(SRCS)"
	@echo "ASM = $(ASMS)"
	@echo "OBJS = $(OBJS)"
	@echo "ASM_OBJS = $(ASM_OBJS)"
	aarch64-linux-gnu-ld $(OBJS) $(ASM_OBJS) -T linker.ld -o kernel8.elf
	aarch64-linux-gnu-objcopy -O binary kernel8.elf kernel8.img

# file initramfs.cpio 應該要看到  ASCII cpio archive (SVR4 with no CRC) 
# cpio -idv < initramfs.cpio 應該可以看到你的資料夾結構
cpio:
	cd rootfs && find . | cpio -o -H newc > ../initramfs.cpio
###################################### run qemu #########################################
# qemu 直接跑完，不會卡住 PC 等你看
# for lab2 part3, add initramfs.cpio
# for lab2 part6, add -dtb bcm2710-rpi-3-b-plus.dtb
run:
	qemu-system-aarch64 -M raspi3b -serial null -serial stdio -display none -kernel kernel8.img -initrd $(INITRAMFS_PATH) -dtb $(DTB_PATH)

#-device VGA 或者 -display gtk 改成 -display sdl都有可能
run_video: 
	qemu-system-aarch64 -M raspi3b -serial null -serial stdio -display gtk -kernel kernel8.img -initrd $(INITRAMFS_PATH) -dtb $(DTB_PATH)

################################ other utli #######################################
clean:
	rm kernel8.elf kernel8.img src/*.o asm/*.o >/dev/null 2>/dev/null || true

# 不好用，還是直接 gdb 比較好
run_asm:
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -d in_asm


objdump:
	aarch64-linux-gnu-objdump -h kernel8.elf

# 用 objdump 顯示 kernel8.elf 文件的反組譯內容
disassembly:
	aarch64-linux-gnu-objdump -d kernel8.elf

inverse_dtb:
	dtc -I dtb -O dts bcm2710-rpi-3-b-plus.dtb | grep -A2 chosen

################################## debug，常用 ###################################################
# qemu debug mode，-S 會卡住 PC 一行一行執行，-s 會開一個 port 1234 讓你用另一個終端開 gdb 連進去
debug:
	qemu-system-aarch64 -M raspi3b -serial null -serial stdio -display none -kernel kernel8.img -initrd $(INITRAMFS_PATH) -dtb $(DTB_PATH) -S -s

# c: continue, b: break point, quit 退出
# layout split ->break _start -> continue -> si 逐條執行（不進入函數）
# p $r0, p $pc
# 印出指定 address 儲存的內容 (x/[n][f][u] <address>, x)
# 查看 reg 的值: info register 或p/x $elr_el1
# watch: 監控 register 的改寫
gdb:
	gdb-multiarch -ex "set architecture aarch64" -ex "target remote :1234" kernel8.elf
	