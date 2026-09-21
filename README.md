# BleeOS 0.3 — tiny x86 OS: ASM MBR + C boot manager + C shell

Boot flow: `boot.asm` (16-bit ASM MBR) → `kernel_entry.asm` (ASM: A20, GDT,
`CR0.PE`, far-jump to 32-bit) → `bootmenu.c` (C boot manager) →
`kernel.c` + `shell.c` (C kernel shell). No BIOS calls after the mode switch.

## Layout

- `boot.asm` — 512-byte MBR. EDD LBA reads (`AH=0x42`, 32-sector chunks)
  with CHS fallback (1 sector/call, reset+retry); loads 96 sectors
  (stage 2) to `0x7E00`, then jumps there.
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
- `vbe.h/.c` — Bochs VBE driver (ports `0x1CE`/`0x1CF`, PCI BAR0 scan for
  the LFB, VGA text-mode + font save/restore across sessions).
- `gfx.h/.c` — software framebuffer (XRGB8888) + built-in 8x8 font.
- `mouse.h/.c` — PS/2 mouse (polled, bounded waits, init retries).
- `wm.h/.c` — tiny window manager: overlap, focus, drag, close button.
- `apps.h/.c` — demo apps: click Counter, live SysInfo (RTC clock).

## Shell commands

`help man echo printf clear uname whoami hostname ver pwd ls cd mkdir touch rm cat env export unset sleep uptime date history true false test exit reboot halt poweroff gui vgaregs`

`exit` (or Ctrl+D on an empty line) drops back to the boot manager.

## GUI (`gui` command)

640x480x32 Bochs VBE desktop (needs `-vga std`, already in `make run`).
Click focuses/drags windows, the X button closes, `Esc` returns to the
shell (text mode is reprogrammed, font restored, screen repainted).
Demo apps: **Counter** (click +1) and **SysInfo** (live CMOS clock).

## Build & run

Requires `nasm`, 32-bit-capable `gcc`, `binutils`, `qemu-system-i386`.

```sh
make
make run
```
