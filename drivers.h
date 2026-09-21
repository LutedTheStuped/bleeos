/* BleeOS low-level drivers: ports, VGA text, PS/2 keyboard, PIT, CMOS RTC.
 * Freestanding: no libc. Shared by the boot menu and the kernel. */
#ifndef DRIVERS_H
#define DRIVERS_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

/* ---------- ports ---------- */
static inline u8 inb(u16 port) {
    u8 v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outb(u16 port, u8 v) {
    __asm__ volatile ("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline void io_wait(void) { outb(0x80, 0); }
static inline void cli(void) { __asm__ volatile ("cli"); }
static inline void sti(void) { __asm__ volatile ("sti"); }
static inline void hlt(void) { __asm__ volatile ("hlt"); }

/* ---------- VGA (80x25, buffer at 0xB8000) ---------- */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

void vga_clear(void);
void vga_putc(char c);
void vga_print(const char *s);
void vga_setcolor(u8 color);
u8   vga_getcolor(void);
u8   vga_row(void);
u8   vga_col(void);
void vga_setcursor(u8 row, u8 col);
void vga_clear_eol(void);
/* direct cell write (menu UI, flicker-free updates) */
void vga_write_at(u8 row, u8 col, const char *s, u8 attr);

/* ---------- keyboard: key events ---------- */
/* 0..255 = ASCII/CTRL char, -1 = no data (non-blocking only), else: */
enum {
    KEY_UP = 0x100, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_DEL
};
int kbd_trykey(void);   /* -1 if no key waiting */
int kbd_getkey(void);   /* blocking */
/* simple blocking line editor for the boot menu (Enter returns len, Esc -1) */
int kbd_readline(char *buf, u32 cap);

/* ---------- timing / RTC ---------- */
void sleep_ms(u32 ms);
u32  rtc_seconds(void);             /* approximate epoch seconds (for uptime) */
void rtc_format(char *out);         /* "YYYY-MM-DD HH:MM:SS" (needs 20 bytes) */

/* ---------- machine control ---------- */
void reboot(void);
void halt_cpu(void);                /* cli + hlt loop, never returns */

#endif
