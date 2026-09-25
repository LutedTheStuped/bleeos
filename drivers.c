/* BleeOS drivers implementation. */
#include "drivers.h"

static u16 pit_count(void);         /* PIT channel-0 latch (lazily declared) */
static void pit_advance(u16 cur);   /* log-clock tick, fed by every wait loop */

/* ================= VGA ================= */
#define VGA_BUF ((volatile u16*)0xB8000)

static u8 cur_row, cur_col, cur_color = 0x07;

static void cursor_update(void) {
    u16 pos = (u16)(cur_row * VGA_WIDTH + cur_col);
    outb(0x3D4, 0x0F); outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

void vga_clear(void) {
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_BUF[i] = (u16)(' ' | (cur_color << 8));
    cur_row = 0; cur_col = 0;
    cursor_update();
}

static void scroll(void) {
    for (u32 r = 0; r < VGA_HEIGHT - 1; r++)
        for (u32 c = 0; c < VGA_WIDTH; c++)
            VGA_BUF[r * VGA_WIDTH + c] = VGA_BUF[(r + 1) * VGA_WIDTH + c];
    for (u32 c = 0; c < VGA_WIDTH; c++)
        VGA_BUF[(VGA_HEIGHT - 1) * VGA_WIDTH + c] = (u16)(' ' | (cur_color << 8));
}

void vga_putc(char c) {
    if (c == '\n') {
        cur_col = 0;
        if (++cur_row >= VGA_HEIGHT) { scroll(); cur_row = VGA_HEIGHT - 1; }
    } else if (c == '\b') {
        if (cur_col > 0) {
            cur_col--;
            VGA_BUF[cur_row * VGA_WIDTH + cur_col] = (u16)(' ' | (cur_color << 8));
        }
    } else {
        VGA_BUF[cur_row * VGA_WIDTH + cur_col] = (u16)((u8)c | (cur_color << 8));
        if (++cur_col >= VGA_WIDTH) {
            cur_col = 0;
            if (++cur_row >= VGA_HEIGHT) { scroll(); cur_row = VGA_HEIGHT - 1; }
        }
    }
    cursor_update();
}

void vga_print(const char *s) { while (*s) vga_putc(*s++); }
void vga_setcolor(u8 color) { cur_color = color; }
u8   vga_getcolor(void) { return cur_color; }
u8   vga_row(void) { return cur_row; }
u8   vga_col(void) { return cur_col; }

void vga_setcursor(u8 row, u8 col) {
    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
    if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;
    cur_row = row; cur_col = col;
    cursor_update();
}

void vga_clear_eol(void) {
    u8 saved = cur_color;
    for (u8 c = cur_col; c < VGA_WIDTH; c++)
        VGA_BUF[cur_row * VGA_WIDTH + c] = (u16)(' ' | (saved << 8));
}

void vga_write_at(u8 row, u8 col, const char *s, u8 attr) {
    u8 r = cur_row, c = cur_col;
    while (*s && col < VGA_WIDTH) {
        if (*s == '\n') break;
        VGA_BUF[row * VGA_WIDTH + col] = (u16)((u8)*s | (attr << 8));
        s++; col++;
    }
    cur_row = r; cur_col = c;
    cursor_update();
}

/* ================= keyboard ================= */
static const char sc_normal[58] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',
    0,'*', 0,' ',
};
static const char sc_shift[58] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',
    0,'*', 0,' ',
};

static int shift_on, caps_on, ctrl_on;

int kbd_trykey(void) {
    static int ext = 0;
    pit_advance(pit_count());       /* the prompt spin also drives the log clock */
    u8 st = inb(0x64);
    if (!(st & 0x01)) return -1;
    if (st & 0x20) return -1;   /* AUX (mouse) byte: leave it */
    u8 sc = inb(0x60);

    if (!ext && sc == 0xE0) { ext = 1; return -1; }
    if (ext) {
        ext = 0;
        if (sc & 0x80) return -1;       /* extended release */
        switch (sc) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
            case 0x47: return KEY_HOME;
            case 0x4F: return KEY_END;
            case 0x53: return KEY_DEL;
            default: return -1;
        }
    }
    if (sc & 0x80) {                    /* release */
        u8 mk = (u8)(sc & 0x7F);
        if (mk == 0x2A || mk == 0x36) shift_on = 0;
        else if (mk == 0x1D) ctrl_on = 0;
        return -1;
    }
    if (sc == 0x2A || sc == 0x36) { shift_on = 1; return -1; }
    if (sc == 0x1D) { ctrl_on = 1; return -1; }
    if (sc == 0x3A) { caps_on = !caps_on; return -1; }
    if (sc == 0x0E) return '\b';
    if (sc == 0x1C) return '\n';
    if (ctrl_on && sc == 0x20) return 4;    /* Ctrl+D = EOT */
    if (ctrl_on && sc == 0x2E) return 3;    /* Ctrl+C = ETX */
    if (sc >= 58) return -1;
    char c = shift_on ? sc_shift[sc] : sc_normal[sc];
    if (!c) return -1;
    if (caps_on && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        c = (char)(c ^ 0x20);
    return (int)(u8)c;
}

int kbd_getkey(void) {
    int k;
    while ((k = kbd_trykey()) == -1) { /* spin */ }
    return k;
}

/* minimal editor: printable chars, backspace, Enter; Esc cancels (-1) */
int kbd_readline(char *buf, u32 cap) {
    u32 len = 0;
    for (;;) {
        int k = kbd_getkey();
        if (k == '\n') { vga_putc('\n'); buf[len] = 0; return (int)len; }
        if (k == 27) { buf[0] = 0; vga_putc('\n'); return -1; }
        if (k == '\b' || k == KEY_DEL) {
            if (len > 0) { len--; vga_putc('\b'); }
            continue;
        }
        if (k >= 32 && k < 127 && len + 1 < cap) {
            buf[len++] = (char)k;
            vga_putc((char)k);
        }
    }
}

/* ================= PIT (busy-wait sleep, no interrupts needed) ================= */
static u16 pit_count(void) {
    outb(0x43, 0x00);               /* latch channel 0 */
    u8 lo = inb(0x40), hi = inb(0x40);
    return (u16)lo | ((u16)hi << 8);
}

/* log clock: the kernel never sleeps in an idle loop, so every place that
 * waits (PIT busy-wait, keyboard spin) feeds the 16-bit counter deltas into
 * the uptime kept for the log timestamps (see the kernel-log section) */
static void pit_advance(u16 cur);

void sleep_ms(u32 ms) {
    u32 need = ms * 1193;           /* 1193182 ticks/sec */
    u32 acc = 0;
    u16 prev = pit_count();
    while (acc < need) {
        u16 cur = pit_count();
        acc += (u16)(prev - cur);   /* handles 16-bit wrap */
        prev = cur;
        pit_advance(cur);           /* keep the log clock moving while waiting */
    }
}

/* ================= misc ================= */
u32 slen(const char *s) { u32 n = 0; while (s[n]) n++; return n; }
int scmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(u8)*a - (int)(u8)*b;
}
char *utoa10(u32 v, char *buf) {
    char t[11];
    int n = 0, k = 0;
    if (!v) t[n++] = '0';
    while (v && n < 11) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) buf[k++] = t[--n];
    buf[k] = 0;
    return buf;
}

/* ================= CMOS RTC ================= */
static u8 cmos_read(u8 reg) {
    outb(0x70, reg & 0x7F);   /* keep NMI enabled; boot runs with IRQs off */
    io_wait();
    return inb(0x71);
}
static u8 bcd(u8 v) { return (u8)((v & 0x0F) + ((v >> 4) * 10)); }

static void rtc_wait_update(void) {
    /* Bounded wait: UIP should clear within ~244us. If CMOS is broken
       (e.g. returns 0xFF forever), bail out instead of hanging boot. */
    for (volatile int i = 0; i < 200000; i++) {
        if (!(cmos_read(0x0A) & 0x80)) return;
    }
}

static void rtc_get(u8 *sec, u8 *min, u8 *hour, u8 *day, u8 *mon, u16 *year) {
    u8 s, m, h, d, mo, y, b;
    rtc_wait_update();
    s = cmos_read(0x00); m = cmos_read(0x02); h = cmos_read(0x04);
    d = cmos_read(0x07); mo = cmos_read(0x08); y = cmos_read(0x09);
    b = cmos_read(0x0B);
    if (!(b & 0x04)) { s = bcd(s); m = bcd(m); h = bcd(h); d = bcd(d); mo = bcd(mo); y = bcd(y); }
    if (!(b & 0x02) && (h & 0x80)) h = (u8)(((h & 0x7F) % 12) + 12); /* 12h -> 24h */
    h &= 0x7F;
    *sec = s; *min = m; *hour = h; *day = d; *mon = mo; *year = (u16)(2000 + y);
}

static const u8 mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

u32 rtc_seconds(void) {
    u8 s, mi, h, d, mo; u16 y;
    rtc_get(&s, &mi, &h, &d, &mo, &y);
    u32 days = 0;
    for (u16 yy = 1970; yy < y; yy++)
        days += ((yy % 4 == 0 && yy % 100 != 0) || yy % 400 == 0) ? 366 : 365;
    for (u8 mm = 1; mm < mo; mm++) {
        days += mdays[mm - 1];
        if (mm == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) days++;
    }
    days += (u32)(d - 1);
    return days * 86400 + (u32)h * 3600 + (u32)mi * 60 + s;
}

static void put2(char *out, u8 v) {
    out[0] = (char)('0' + v / 10);
    out[1] = (char)('0' + v % 10);
}

void rtc_format(char *out) {
    u8 s, mi, h, d, mo; u16 y;
    rtc_get(&s, &mi, &h, &d, &mo, &y);
    out[0] = (char)('0' + (y / 1000) % 10);
    out[1] = (char)('0' + (y / 100) % 10);
    out[2] = (char)('0' + (y / 10) % 10);
    out[3] = (char)('0' + y % 10);
    out[4] = '-'; put2(out + 5, mo); out[7] = '-'; put2(out + 8, d);
    out[10] = ' '; put2(out + 11, h); out[13] = ':'; put2(out + 14, mi);
    out[16] = ':'; put2(out + 17, s); out[19] = 0;
}

/* ================= machine control ================= */
void reboot(void) {
    klogf(KLOG_WARN, "machine: reboot (8042 reset, PCI reset, triple fault)");
    u8 good = 0x02;
    while (good & 0x02) good = inb(0x64);   /* drain input buffer */
    outb(0x64, 0xFE);                        /* 8042 reset */
    io_wait();
    outb(0xCF9, 0x0E);                       /* PCI reset fallback */
    cli();
    __asm__ volatile ("lidt 0(%%eax)" : : "a"(0));  /* triple-fault fallback */
    __asm__ volatile ("int $3");
    for (;;) hlt();
}

void halt_cpu(void) {
    klogf(KLOG_WARN, "machine: CPU halted (interrupts off)");
    cli();
    for (;;) hlt();
}

/* ================= serial port (COM1 0x3F8, polled, 38400 8N1) ================= */
void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);            /* DLAB on */
    outb(0x3F8 + 0, 0x03);            /* divisor 3 = 38400 */
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);            /* 8N1, DLAB off */
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}
void serial_putc(char c) {
    if (c == '\n') serial_putc('\r');
    while (!(inb(0x3F8 + 5) & 0x20)) ;   /* THR empty; 0xFF (no port) passes */
    outb(0x3F8, (u8)c);
}
void serial_print(const char *s) { while (*s) serial_putc(*s++); }
void klog(const char *s) { vga_print(s); serial_print(s); }

/* ================= kernel log (COM1 + optional VGA mirror) ================= */
#define KLOG_MAX   256            /* assembled line (incl. NUL/newline) */
#define KLOG_TICKS 1193182u       /* PIT input frequency */

static struct {
    u8 inited;                    /* serial is up, time base is running */
    u8 mirror;                    /* boot arg: copy log lines to VGA text */
    u8 level;                     /* lowest level that gets emitted */
    u8 tvalid;                    /* first PIT sample taken */
    u16 prev;                     /* previous PIT channel-0 count */
    u32 sec;                      /* uptime seconds since klog_init() */
    u32 frac;                     /* PIT ticks inside the current second */
} lg;

static const char *const lg_tag[4] = { "DEBUG", "INFO ", "WARN ", "ERROR" };
static const u8 lg_col[4]         = { 0x08,   0x07,   0x0E,   0x0C  };

/* Channel 0 wraps every ~55 ms, so it must be sampled at least that often
 * or whole wraps are lost: sleep_ms() and the keyboard spin hand their
 * samples in here, klogf() takes one itself. Deltas are unsigned subtractions,
 * so the 16-bit wrap cancels out. */
static void pit_advance(u16 cur) {
    if (!lg.tvalid) { lg.prev = cur; lg.tvalid = 1; return; }
    lg.frac += (u16)(lg.prev - cur);
    lg.prev = cur;
    while (lg.frac >= KLOG_TICKS) { lg.frac -= KLOG_TICKS; lg.sec++; }
}

/* ---- line assembly: everything is appended to kline, then emitted ---- */
static char kline[KLOG_MAX];
static u32  klen;

static void kput(char c) {
    if (klen < KLOG_MAX - 1) kline[klen] = c;
    klen++;
}
static void kputs(const char *s) { while (*s) kput(*s++); }

/* digits of v in base (10 or 16), written reversed into t, count back */
static u32 kdigits(char *t, u32 v, u32 base, int upper) {
    const char *d = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    u32 k = 0;
    if (!v) t[k++] = '0';
    while (v) { t[k++] = d[v % base]; v /= base; }
    return k;
}

/* reversed digits + front padding, emitted right-aligned in `width` */
static void knum(u32 v, u32 base, int upper, u32 width, char pad) {
    char t[12];
    u32 k = kdigits(t, v, base, upper);
    while (k < width && k < sizeof(t)) t[k++] = pad;
    while (k) kput(t[--k]);
}

static void kvformat(const char *fmt, __builtin_va_list ap) {
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { kput(*p); continue; }
        if (*++p == 0) break;
        if (*p == '%') { kput('%'); continue; }
        int left = 0;
        if (*p == '-') { left = 1; p++; if (!*p) break; }
        char pad = ' ';
        if (*p == '0') { pad = '0'; p++; if (!*p) break; }
        u32 width = 0;
        while (*p >= '0' && *p <= '9') { width = width * 10 + (u32)(*p - '0'); p++; }
        switch (*p) {
        case 's': {
            const char *s = __builtin_va_arg(ap, const char *);
            if (!s) s = "(null)";
            u32 n = 0;
            while (s[n]) n++;
            if (!left) for (u32 i = n; i < width; i++) kput(' ');
            kputs(s);
            if (left) for (u32 i = n; i < width; i++) kput(' ');
            break;
        }
        case 'c':
            kput((char)__builtin_va_arg(ap, int));
            break;
        case 'd': case 'i': {
            int v = __builtin_va_arg(ap, int);
            if (v < 0) { kput('-'); knum((u32)(-(long)v), 10, 0, width ? width - 1 : 0, pad); }
            else knum((u32)v, 10, 0, width, pad);
            break;
        }
        case 'u':
            knum(__builtin_va_arg(ap, u32), 10, 0, width, pad);
            break;
        case 'x':
            knum(__builtin_va_arg(ap, u32), 16, 0, width, pad);
            break;
        case 'X':
            knum(__builtin_va_arg(ap, u32), 16, 1, width, pad);
            break;
        case 'p':
            kputs("0x");
            knum((u32)__builtin_va_arg(ap, void *), 16, 0, 8, '0');
            break;
        default:                    /* unknown conversion: print it raw */
            kput('%');
            kput(*p);
            break;
        }
    }
}

void klogf(int level, const char *fmt, ...) {
    if (!lg.inited || level < lg.level || level > KLOG_ERROR) return;

    klen = 0;
    /* [   0.123] TAG message\n */
    pit_advance(pit_count());
    kput('[');
    knum(lg.sec, 10, 0, 5, ' ');
    kput('.');
    knum(lg.frac * 1000u / KLOG_TICKS, 10, 0, 3, '0');
    kputs("] ");
    kputs(lg_tag[level & 3]);
    kput(' ');
    {
        __builtin_va_list ap;
        __builtin_va_start(ap, fmt);
        kvformat(fmt, ap);
        __builtin_va_end(ap);
    }
    if (klen > KLOG_MAX - 2) klen = KLOG_MAX - 2;   /* room for the newline */
    if (!klen || kline[klen - 1] != '\n') kline[klen++] = '\n';

    /* serial always; the VGA text screen only when the boot arg asked */
    u8 saved = 0;
    if (lg.mirror) { saved = vga_getcolor(); vga_setcolor(lg_col[level & 3]); }
    for (u32 i = 0; i < klen; i++) {
        serial_putc(kline[i]);
        if (lg.mirror) vga_putc(kline[i]);
    }
    if (lg.mirror) vga_setcolor(saved);
}

void klog_init(void) {
    if (lg.inited) return;
    serial_init();
    lg.level = KLOG_INFO;
    lg.sec = 0;
    lg.frac = 0;
    lg.prev = pit_count();
    lg.tvalid = 1;
    lg.inited = 1;
    klogf(KLOG_INFO, "log: kernel log on COM1 0x3F8 (38400 8N1, polled), "
                     "uptime base started");
}

void klog_level(int level) {
    if (level < KLOG_DEBUG) level = KLOG_DEBUG;
    if (level > KLOG_ERROR) level = KLOG_ERROR;
    lg.level = (u8)level;
}
int klog_getlevel(void) { return lg.level; }
void klog_mirror(int on) { lg.mirror = on ? 1 : 0; }
int  klog_mirrored(void) { return lg.mirror; }
u32  klog_uptime_sec(void) { return lg.sec; }
u32  klog_uptime_msec(void) { return lg.frac * 1000u / KLOG_TICKS; }


/* ================= kernel panic ================= */
void panic(const char *msg) {
    cli();
    vga_setcolor(0x4F);
    vga_print("\n*** KERNEL PANIC ***\n");
    vga_print(msg);
    vga_putc('\n');
    serial_print("\n*** KERNEL PANIC ***\n");
    serial_print(msg);
    serial_putc('\n');
    for (;;) hlt();
}
void panic_at(const char *file, int line, const char *msg) {
    char b[12];
    cli();
    vga_setcolor(0x4F);
    vga_print("\n*** KERNEL PANIC ***\nASSERT ");
    vga_print(file);
    vga_putc(':');
    vga_print(utoa10((u32)line, b));
    vga_print(": ");
    vga_print(msg);
    vga_putc('\n');
    serial_print("\n*** KERNEL PANIC ***\nASSERT ");
    serial_print(file);
    serial_putc(':');
    serial_print(utoa10((u32)line, b));
    serial_print(": ");
    serial_print(msg);
    serial_putc('\n');
    for (;;) hlt();
}
