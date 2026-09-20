AS=nasm
CC=gcc
LD=ld
OBJCOPY=objcopy
QEMU=qemu-system-i386

CFLAGS=-m32 -ffreestanding -nostdlib -nostartfiles -nodefaultlibs \
       -fno-builtin -fno-stack-protector -fno-pie -no-pie \
       -Wall -Wextra -O2 -std=gnu11

all: os.img

boot.bin: boot.asm
	$(AS) -f bin boot.asm -o boot.bin

kernel_entry.o: kernel_entry.asm
	$(AS) -f elf32 kernel_entry.asm -o kernel_entry.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

kernel.elf: kernel_entry.o kernel.o linker.ld
	$(LD) -m elf_i386 -T linker.ld -o kernel.elf kernel_entry.o kernel.o

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	@size=$$(stat -c%s kernel.bin); \
	padded=$$(( (size + 511) / 512 * 512 )); \
	truncate -s $$padded kernel.bin; \
	echo "kernel.bin: $$size -> $$padded bytes ($$(($$padded / 512)) sectors)"

os.img: boot.bin kernel.bin
	cat boot.bin kernel.bin > os.img
	truncate -s 1440K os.img
	@echo "Built os.img ($$(stat -c%s os.img) bytes)"

run: os.img
	$(QEMU) -drive file=os.img,format=raw,if=floppy -boot a

run-nographic: os.img
	$(QEMU) -drive file=os.img,format=raw,if=floppy -boot a -nographic

clean:
	rm -f boot.bin kernel_entry.o kernel.o kernel.elf kernel.bin os.img

.PHONY: all run run-nographic clean
