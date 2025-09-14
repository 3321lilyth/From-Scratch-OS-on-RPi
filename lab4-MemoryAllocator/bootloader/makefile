SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

ASMS = $(wildcard *.S)
ASM_OBJS = $(ASMS:.S=.o)


CFLAGS = -g -Wall -Wextra -Wpedantic -Werror -O0 -ffreestanding -nostdinc -nostdlib -nostartfiles 


############################# all #################################################
all: bootloader.img

# 告訴 make，當它遇到 .S 時，應該使用這條規則來生成 .o。
# $< 代表依賴的文件（此處是 .S），而 $@ 代表目標文件（此處是 .o）

%.o: %.S
	aarch64-linux-gnu-gcc $(CFLAGS) -c $< -o $@

%.o: %.c
	aarch64-linux-gnu-gcc $(CFLAGS) -c $< -o $@

bootloader.img: $(ASM_OBJS) $(OBJS)
	@echo "SRC = $(SRCS)"
	@echo "ASM = $(ASMS)"
	aarch64-linux-gnu-ld $(ASM_OBJS) $(OBJS) -g -T linker.ld -o bootloader.elf
	aarch64-linux-gnu-objcopy -O binary bootloader.elf bootloader.img

# qemu 直接跑完，不會卡住 PC 等你看
# reate a pseudo TTY device and test your bootloader
run_tty:
	qemu-system-aarch64 -M raspi3b -serial null -serial pty -display none -kernel bootloader.img -initrd ../rpi_firmware/initramfs.cpio -dtb ../rpi_firmware/bcm2710-rpi-3-b-plus.dtb 


################################ other utli #######################################
clean:
	rm bootloader.elf bootloader.img *.o >/dev/null 2>/dev/null || true

# 不好用，還是直接 gdb 比較好
run_asm:
	qemu-system-aarch64 -M raspi3b -kernel bootloader.img -display none -d in_asm

objdump:
	aarch64-linux-gnu-objdump -h bootloader.elf

# 用 objdump 顯示 bootloader.elf 文件的反組譯內容
disassembly:
	aarch64-linux-gnu-objdump -d bootloader.elf



################################## debug，常用 ###################################################
# qemu debug mode，-S 會卡住 PC 一行一行執行，-s 會開一個 port 1234 讓你用另一個終端開 gdb 連進去
debug_tty:
	qemu-system-aarch64 -M raspi3b -kernel bootloader.img -serial null -serial pty -S -s -display none -initrd ../rpi_firmware/initramfs.cpio -dtb ../rpi_firmware/bcm2710-rpi-3-b-plus.dtb 

# c: continue, b: break point, quit 退出
# layout asm ->break _start -> continue -> si 逐條執行（不進入函數）
# info registers 或指定 p $r0(預設印出十進位), p $pc, p/x $x0 (印出16進位), print &symbol_name (印出 symbol addr)
# x/g 0x80000  # 以 8-byte giant 格式查看 address 裡面的值
gdb:
	gdb-multiarch -ex "set architecture aarch64" -ex "target remote :1234" bootloader.elf
