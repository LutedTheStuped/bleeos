# BleeOS — tiny x86 OS (32-bit protected mode + C)

MBR bootloader (`boot.asm`, 16-bit ASM) loads the kernel at `0x7E00`.
`kernel_entry.asm` (ASM) enables A20, installs a flat GDT, sets `CR0.PE` and
far-jumps into 32-bit protected mode, then calls `kernel_main()` in `kernel.c`,
which implements the VGA shell (no BIOS after the switch).

## Layout

- `boot.asm` — 512-byte MBR. BIOS `int 0x13` reads 32 sectors (sector 2+) to `0x7E00`, jumps to kernel.
- `kernel_entry.asm` — real→protected trampoline + GDT, linked at `0x7E00`.
- `kernel.c` — 32-bit C kernel: VGA driver (`0xB8000`), PS/2 keyboard polling (`0x60`/`0x64`), shell.
- `linker.ld` — links kernel at `0x7E00`.
- `Makefile` — builds `os.img` (1.44M floppy image).

## Shell commands

- `help` — list commands
- `echo <text>` — print text
- `clear` — clear screen
- `info` / `ver` — system info
- `reboot` — reboot (8042 controller + PCI reset + triple-fault fallback)
- `halt` — halt CPU

## Build & run

Requires `nasm`, 32-bit-capable `gcc`, `binutils`, `qemu-system-i386`.

```sh
make
make run
```

Type `help` at the `bleeos>` prompt.
