SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

ASMS = $(wildcard *.S)
ASM_OBJS = $(ASMS:.S=.o)


CFLAGS = -Wall -Wextra -Wpedantic -Werror -O0 -ffreestanding -nostdinc -nostdlib -nostartfiles 

all: kernel8.img

# 告訴 make，當它遇到 .S 時，應該使用這條規則來生成 .o。
# $< 代表依賴的文件（此處是 .S），而 $@ 代表目標文件（此處是 .o）

%.o: %.S
	aarch64-linux-gnu-gcc $(CFLAGS) -c $< -o $@

%.o: %.c
	aarch64-linux-gnu-gcc $(CFLAGS) -c $< -o $@

kernel8.img: $(ASM_OBJS) $(OBJS)
	@echo "SRC = $(SRCS)"
	@echo "ASM = $(ASMS)"
	aarch64-linux-gnu-ld $(ASM_OBJS) $(OBJS) -T linker.ld -o kernel8.elf
	aarch64-linux-gnu-objcopy -O binary kernel8.elf kernel8.img




clean:
	rm kernel8.elf kernel8.img *.o >/dev/null 2>/dev/null || true


# 不好用，還是直接 gdb 比較好
run_asm:
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -d in_asm

# qemu 直接跑完，不會卡住 PC 等你看
run:
	qemu-system-aarch64 -M raspi3b -serial null -serial stdio -display none -kernel kernel8.img

objdump:
	aarch64-linux-gnu-objdump -h kernel8.elf

# 用 objdump 顯示 kernel8.elf 文件的反組譯內容
disassembly:
	aarch64-linux-gnu-objdump -d kernel8.elf

# qemu debug mode，-S 會卡住 PC 一行一行執行，-s 會開一個 port 1234 讓你用另一個終端開 gdb 連進去
debug:
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -S -s

# c: continue, b: break point, quit 退出
# layout asm ->break _start -> continue -> si 逐條執行（不進入函數）
# info registers 或指定 p $r0, p $pc
gdb:
	gdb-multiarch -ex "set architecture aarch64" -ex "target remote :1234" kernel8.elf
	