# BleeOS 0.3 — tiny x86 OS: ASM MBR + C boot manager + C shell

Boot flow: `boot.asm` (16-bit ASM MBR) → `kernel_entry.asm` (ASM: A20, GDT,
`CR0.PE`, far-jump to 32-bit) → `bootmenu.c` (C boot manager) →
`kernel.c` + `shell.c` (C kernel shell). No BIOS calls after the mode switch.

## Layout

- `boot.asm` — 512-byte MBR. LBA→CHS loop over BIOS `int 0x13` (1 sector/call,
  reset+retry) loads 64 sectors (stage 2) to `0x7E00`, then jumps there.
- `kernel_entry.asm` — real→protected trampoline + flat GDT, linked at `0x7E00`.
  Calls `boot_main()`.
- `bootmenu.c` — BleeOS Boot Manager (own thing, GRUB/systemd-boot-style):
  entries, 5 s timeout, Up/Down or j/k, 1-4, Enter, `E` edits the kernel
  command line per entry. Passes `boot_info_t` (magic, entry, cmdline,
  boot time) to the kernel. Shell `exit` returns to the menu.
- `drivers.h/.c` — VGA text, PS/2 keyboard (incl. arrows), PIT sleep,
  CMOS RTC, reboot/halt. Shared by menu and kernel.
- `kernel.c` — banner, `verbose` cmdline parsing, runs the shell.
- `shell.c/.h` — POSIX-style shell: ramfs (`/motd`, `/version`, `/etc/hostname`),
  env vars, history (Up/Down), line editing, quoting, `$VAR $? $$`,
  `; && ||` lists, `> >> <` redirection, exit statuses, Ctrl+C/D.
- `linker.ld` — links stage 2 at `0x7E00`.

## Shell commands

`help man echo printf clear uname whoami hostname ver pwd ls cd mkdir touch rm cat env export unset sleep uptime date history true false test exit reboot halt poweroff`

`exit` (or Ctrl+D on an empty line) drops back to the boot manager.

## Build & run

Requires `nasm`, 32-bit-capable `gcc`, `binutils`, `qemu-system-i386`.

```sh
make
make run
```
