CC = aarch64-linux-gnu-gcc
LD = aarch64-linux-gnu-ld
OBJCOPY = aarch64-linux-gnu-objcopy
CFLAGS = -Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles -g

ASM_FILES = $(wildcard *.S)
OBJS = $(ASM_FILES:%.S=%.o)

all: exec_test

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

exec_test: $(OBJS)
	$(LD)  -Ttext=0x0 -o exec_test.elf $(OBJS)
	$(OBJCOPY) exec_test.elf -O binary exec_test.img

clean:
	rm *.o *.elf *.img