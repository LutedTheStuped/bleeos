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
- `wm.h/.c` — tiny window manager: overlap, focus, drag, close button,
  live `wm_resize` (layout changes keep the window on screen).
- `apps.h/.c` — demo apps: click Counter, live SysInfo (RTC clock).

## Shell commands

`help man echo printf clear uname whoami hostname ver pwd ls cd mkdir touch rm cat env export unset sleep uptime date history true false test exit reboot halt poweroff gui vgaregs`

`exit` (or Ctrl+D on an empty line) drops back to the boot manager.

## Hard disk (`install` command)
ATA PIO driver (primary bus, LBA28, polled). The kernel reports a
detected primary master at boot; `install` is a TUI wizard that
confirms with YES, writes the boot sector + kernel (161 sectors,
snapshotted from RAM) to LBA 0, and verifies by read-back.
Tested end-to-end: `make run-install` (floppy + blank `hdd.img`),
`install`, then `make run-hdd` boots BleeOS from the hard disk.

## Users (TUI login + database)
Boot drops to a `hostname login:` prompt checked against
`/etc/passwd` + `/etc/shadow` (salted/iterated hashes — hobby
grade, not real security). Default login is `root` / `root`.
`logout` returns to the prompt (`#` for root, `$` for users);
`useradd`/`userdel` (root only), `passwd`, `su`, `users`,
`whoami` manage the session. The GUI login uses the same DB.
ramfs is volatile: added users vanish on reboot.

## USB (`usb` command)
Stub UHCI detector: PCI-probes the controller and reports per-port
attach state at boot and via `usb`. No resets, no transfers — the
BIOS-owned controller is left alone and PS/2 stays the input path.
Full UHCI enumeration was attempted and dropped (TDs never complete
on QEMU's UHCI); the stub keeps the door open without the risk.

## GUI (`gui` command)

Ly-style login (any password — no user DB yet), then a 640x480x32
Bochs VBE desktop (needs `-vga std`, already in `make run`).
Left click focuses/drags windows, right click opens the menu
(Display settings, Calculator, Reboot, Power off, Log out),
X button closes. `Esc` in the desktop logs out to the login
screen; `Esc` at login returns to the shell.
Demo apps: **Counter** (click +1), **SysInfo** (live CMOS clock),
**Calculator** (integer), **Display** settings (640x480, 800x600,
1024x768 presets plus a Custom editor — type any `W`x`H` within
320-1920 x 200-1200, `W` a multiple of 8 — applied live). The
`Show all screen types` checkbox swaps the presets for all 24
supported screen types in a 3-column grid (the window grows to
fit and is kept on screen by `wm_resize`). Taskbar shows `user@bleeos` + live clock.
Compositor is double-buffered (1MB shadow buffer, one `rep movsl`
blit per frame — no tearing) and redraws on input or RTC second
change, not on a fixed tick.

## Build & run

Requires `nasm`, 32-bit-capable `gcc`, `binutils`, `qemu-system-i386`.

```sh
make
make run
```

## Testing input (PS/2) and screenshots

`make run-debug` starts the VM with HMP and QMP sockets. PS/2 keyboard
and mouse are QEMU's default pc devices — no extra flags needed. (Do not
use `-nographic` for input tests: stdio is wired to the serial port,
which BleeOS doesn't drive, so keystrokes would never reach the guest.)

HMP monitor (`/tmp/opencode/qemu-mon`), e.g. with `socat`:

```sh
# keyboard (NOTE: the space key is called `spc`, not `space`)
sendkey u
sendkey n
sendkey a
sendkey m
sendkey e
sendkey spc
sendkey minus
sendkey a
sendkey ret
# screenshot (PPM; convert with pnmtopng/ffmpeg)
screendump /tmp/opencode/shot.ppm
# read guest RAM (quote the path: unquoted / is division)
pmemsave 0xb8000 4000 "/tmp/opencode/vga.bin"
```

QMP mouse (`/tmp/opencode/qmp.sock`): handshake first, then events.

```json
{"execute": "qmp_capabilities"}
{"execute": "input-send-event", "arguments": {"events": [
  {"type": "rel", "data": {"axis": "x", "value": -190}},
  {"type": "rel", "data": {"axis": "y", "value": -82}}]}}
{"execute": "input-send-event", "arguments": {"events": [
  {"type": "btn", "data": {"down": true, "button": "left"}}]}}
{"execute": "input-send-event", "arguments": {"events": [
  {"type": "btn", "data": {"down": false, "button": "left"}}]}}
```

Full command if not using the Makefile target:

```sh
qemu-system-i386 -accel kvm:tcg -vga std -display none \
  -monitor unix:/tmp/opencode/qemu-mon,server,nowait \
  -qmp unix:/tmp/opencode/qmp.sock,server,nowait \
  -drive file=os.img,format=raw,if=floppy -boot order=a,strict=on -net none
```
