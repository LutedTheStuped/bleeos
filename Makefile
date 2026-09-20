AS=nasm
QEMU=qemu-system-i386

all: os.img

boot.bin: boot.asm
	$(AS) -f bin boot.asm -o boot.bin

kernel.bin: kernel.asm
	$(AS) -f bin kernel.asm -o kernel.bin

os.img: boot.bin kernel.bin
	cat boot.bin kernel.bin > os.img
	truncate -s 1440K os.img
	@echo "Built os.img ($$(stat -c%s os.img) bytes)"

run: os.img
	$(QEMU) -drive file=os.img,format=raw,if=floppy -boot a

run-nographic: os.img
	$(QEMU) -drive file=os.img,format=raw,if=floppy -boot a -nographic

clean:
	rm -f boot.bin kernel.bin os.img

.PHONY: all run run-nographic clean
