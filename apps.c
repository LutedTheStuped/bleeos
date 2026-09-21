/* Two test apps: a click counter and a live system-info panel. */
#include "apps.h"
#include "wm.h"
#include "gfx.h"
#include "drivers.h"

#define INK RGB(20, 20, 20)
#define BTN RGB(60, 120, 220)
#define BTN_T RGB(255, 255, 255)

/* ---------- Counter ---------- */
static int counter_n;

static void counter_draw(win_t *w, int cx, int cy) {
    (void)w;
    char buf[16];
    int n = counter_n, i = 0;
    char tmp[12];
    if (n == 0) tmp[i++] = '0';
    while (n > 0 && i < 11) { tmp[i++] = (char)('0' + n % 10); n /= 10; }
    for (int k = 0; k < i; k++) buf[k] = tmp[i - 1 - k];
    buf[i] = 0;
    gfx_text(cx + 16, cy + 12, "Count:", INK, GFX_TRANS);
    gfx_text(cx + 72, cy + 12, buf, INK, GFX_TRANS);
    /* button at client (20,60) size 140x32 */
    gfx_fill(cx + 20, cy + 60, 140, 32, BTN);
    gfx_rect(cx + 20, cy + 60, 140, 32, INK);
    gfx_text(cx + 20 + 52, cy + 60 + 12, "+1", BTN_T, GFX_TRANS);
}
static void counter_click(win_t *w, int x, int y, int btn) {
    (void)w; (void)btn;
    if (x >= 20 && y >= 60 && x < 160 && y < 92) {
        counter_n++;
        wm_dirty();
    }
}

/* ---------- SysInfo ---------- */
static void sysinfo_draw(win_t *w, int cx, int cy) {
    (void)w;
    char timeb[20];
    int mx, my;
    rtc_format(timeb);
    wm_mouse_xy(&mx, &my);
    gfx_text(cx + 12, cy + 10, "BleeOS 0.3 GUI demo", INK, GFX_TRANS);
    gfx_text(cx + 12, cy + 26, "640x480x32 VBE LFB", INK, GFX_TRANS);
    gfx_text(cx + 12, cy + 42, timeb, INK, GFX_TRANS);
    gfx_text(cx + 12, cy + 58, "UTC (CMOS RTC)", INK, GFX_TRANS);
    /* mouse readout */
    char mb[24];
    int i = 0;
    mb[i++] = 'm'; mb[i++] = 'x'; mb[i++] = '=';
    {
        char t[4];
        int n = 0, v = mx;
        if (v == 0) t[n++] = '0';
        while (v > 0 && n < 4) { t[n++] = (char)('0' + v % 10); v /= 10; }
        while (n > 0) mb[i++] = t[--n];
    }
    mb[i++] = ' '; mb[i++] = 'm'; mb[i++] = 'y'; mb[i++] = '=';
    {
        char t[4];
        int n = 0, v = my;
        if (v == 0) t[n++] = '0';
        while (v > 0 && n < 4) { t[n++] = (char)('0' + v % 10); v /= 10; }
        while (n > 0) mb[i++] = t[--n];
    }
    mb[i] = 0;
    gfx_text(cx + 12, cy + 74, mb, INK, GFX_TRANS);
    gfx_text(cx + 12, cy + 96, "Drag title. X closes.", INK, GFX_TRANS);
}

void apps_open_demo(void) {
    counter_n = 0;
    wm_open("Counter", 40, 60, 240, 150, counter_draw, counter_click, 0);
    wm_open("SysInfo", 320, 80, 260, 170, sysinfo_draw, 0, 0);
}
