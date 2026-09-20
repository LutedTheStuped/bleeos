# BleeOS — tiny x86 OS with ASM bootloader + shell

16-bit real-mode OS. MBR bootloader (`boot.asm`) loads the kernel (`kernel.asm`) which provides a small shell.

## Layout

- `boot.asm` — 512-byte MBR bootloader. Prints status, reads 16 sectors from disk (sector 2+) to `0x7E00` via BIOS `int 0x13`, jumps to kernel.
- `kernel.asm` — kernel + shell at `ORG 0x7E00`. Uses BIOS `int 0x10` (video) / `int 0x16` (keyboard).
- `Makefile` — builds `os.img` (1.44M floppy image).

## Shell commands

- `help` — list commands
- `echo <text>` — print text
- `clear` — clear screen
- `info` / `ver` — system info
- `reboot` — reboot via BIOS
- `halt` — halt CPU

## Build & run

Requires `nasm` + `qemu-system-i386`.

```sh
make
make run
```

Type `help` at the `bleeos>` prompt.
