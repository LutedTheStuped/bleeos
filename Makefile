AS=nasm
CC=gcc
LD=ld
OBJCOPY=objcopy
QEMU=qemu-system-i386

# MBR loads this many sectors (must cover the whole stage2 binary)
STAGE2_SECTORS=160

CFLAGS=-m32 -march=i386 -mno-mmx -mno-sse -mno-sse2 -ffreestanding -nostdlib -nostartfiles -nodefaultlibs \
       -fno-builtin -fno-stack-protector -fno-pie -no-pie \
       -Wall -Wextra -O2 -std=gnu11

OBJS=kernel_entry.o drivers.o bootmenu.o shell.o kernel.o vbe.o gfx.o mouse.o wm.o apps.o login.o ata.o users.o uhci.o usb.o

all: os.img

boot.bin: boot.asm
	$(AS) -f bin boot.asm -o boot.bin

kernel_entry.o: kernel_entry.asm
	$(AS) -f elf32 kernel_entry.asm -o kernel_entry.o

drivers.o: drivers.c drivers.h
	$(CC) $(CFLAGS) -c drivers.c -o drivers.o

bootmenu.o: bootmenu.c drivers.h boot.h
	$(CC) $(CFLAGS) -c bootmenu.c -o bootmenu.o

shell.o: shell.c shell.h drivers.h
	$(CC) $(CFLAGS) -c shell.c -o shell.o

kernel.o: kernel.c drivers.h boot.h shell.h
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

vbe.o: vbe.c vbe.h drivers.h
	$(CC) $(CFLAGS) -c vbe.c -o vbe.o

gfx.o: gfx.c gfx.h drivers.h
	$(CC) $(CFLAGS) -c gfx.c -o gfx.o

mouse.o: mouse.c mouse.h drivers.h
	$(CC) $(CFLAGS) -c mouse.c -o mouse.o

wm.o: wm.c wm.h vbe.h gfx.h mouse.h drivers.h
	$(CC) $(CFLAGS) -c wm.c -o wm.o

apps.o: apps.c apps.h wm.h gfx.h drivers.h
	$(CC) $(CFLAGS) -c apps.c -o apps.o

login.o: login.c login.h gfx.h drivers.h shell.h wm.h users.h
	$(CC) $(CFLAGS) -c login.c -o login.o

ata.o: ata.c ata.h drivers.h
	$(CC) $(CFLAGS) -c ata.c -o ata.o

users.o: users.c users.h shell.h drivers.h
	$(CC) $(CFLAGS) -c users.c -o users.o

uhci.o: uhci.c uhci.h drivers.h
	$(CC) $(CFLAGS) -c uhci.c -o uhci.o

usb.o: usb.c usb.h uhci.h drivers.h
	$(CC) $(CFLAGS) -c usb.c -o usb.o

kernel.elf: $(OBJS) linker.ld
	$(LD) -m elf_i386 -T linker.ld -o kernel.elf $(OBJS)

# stage2 = flat menu+kernel image, padded to whole sectors
kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	@size=$$(stat -c%s kernel.bin); \
	max=$$(( $(STAGE2_SECTORS) * 512 )); \
	if [ $$size -gt $$max ]; then \
		echo "ERROR: stage2 $$size bytes > $$max (grow STAGE2_SECTORS)"; exit 1; \
	fi; \
	padded=$$(( (size + 511) / 512 * 512 )); \
	truncate -s $$padded kernel.bin; \
	echo "stage2: $$size -> $$padded bytes ($$(($$padded / 512))/$(STAGE2_SECTORS) sectors)"

os.img: boot.bin kernel.bin
	cat boot.bin kernel.bin > os.img
	truncate -s 1440K os.img
	@echo "Built os.img ($$(stat -c%s os.img) bytes)"

run: os.img
	$(QEMU) -vga std -drive file=os.img,format=raw,if=floppy -boot order=a,strict=on -net none

run-hd: os.img
	$(QEMU) -vga std -drive file=os.img,format=raw,if=ide -boot order=c,strict=on -net none

run-nographic: os.img
	$(QEMU) -drive file=os.img,format=raw,if=floppy -boot order=a,strict=on -net none -nographic

# USB bring-up rig: UHCI + USB keyboard/mouse. Enumeration only;
# PS/2 stays the input path (see `usb` command).
run-usb: os.img
	$(QEMU) -accel kvm:tcg -vga std -display none \
		-monitor unix:/tmp/opencode/qemu-mon,server,nowait \
		-qmp unix:/tmp/opencode/qmp.sock,server,nowait \
		-device piix3-usb-uhci -device usb-kbd -device usb-mouse \
		-drive file=os.img,format=raw,if=floppy -boot order=a,strict=on -net none

# HDD test rig: blank disk on IDE primary master. Boot the floppy,
# run `install`, then boot the disk itself with run-hdd.
hdd.img:
	qemu-img create -f raw hdd.img 100M

run-install: os.img hdd.img
	$(QEMU) -accel kvm:tcg -vga std -display none \
		-monitor unix:/tmp/opencode/qemu-mon,server,nowait \
		-qmp unix:/tmp/opencode/qmp.sock,server,nowait \
		-drive file=os.img,format=raw,if=floppy -boot order=a,strict=on \
		-drive file=hdd.img,format=raw,if=ide -net none

run-hdd: hdd.img
	$(QEMU) -accel kvm:tcg -vga std -display none \
		-monitor unix:/tmp/opencode/qemu-mon,server,nowait \
		-qmp unix:/tmp/opencode/qmp.sock,server,nowait \
		-drive file=hdd.img,format=raw,if=ide -boot order=c,strict=on -net none

# Debug/test VM: HMP monitor + QMP sockets for sendkey, mouse events,
# screendump/pmemsave. PS/2 kbd+mouse are the default pc devices.
# NOTE: do NOT use -nographic here: stdio goes to the serial port,
# which BleeOS doesn't drive, so typed keys would never reach the guest.
run-debug: os.img
	$(QEMU) -accel kvm:tcg -vga std -display none \
		-monitor unix:/tmp/opencode/qemu-mon,server,nowait \
		-qmp unix:/tmp/opencode/qmp.sock,server,nowait \
		-drive file=os.img,format=raw,if=floppy -boot order=a,strict=on -net none

clean:
	rm -f boot.bin $(OBJS) kernel.elf kernel.bin os.img

.PHONY: all run run-nographic clean
