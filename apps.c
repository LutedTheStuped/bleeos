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
    gfx_text(cx + 12, cy + 10, "BleeOS 0.5 GUI", INK, GFX_TRANS);
    /* live resolution readout (tracks Display settings changes) */
    {
        char rb[20];
        int k = 0;
        int vals[2];
        vals[0] = gfx_w(); vals[1] = gfx_h();
        for (int f = 0; f < 2; f++) {
            char t[6];
            int n = 0, v = vals[f];
            if (v == 0) t[n++] = '0';
            while (v > 0 && n < 6) { t[n++] = (char)('0' + v % 10); v /= 10; }
            while (n > 0) rb[k++] = t[--n];
            rb[k++] = 'x';
        }
        rb[k++] = '3'; rb[k++] = '2'; rb[k++] = 0;
        gfx_text(cx + 12, cy + 26, rb, INK, GFX_TRANS);
    }
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

/* ---------- Calculator (integer) ---------- */
static long calc_acc, calc_cur;
static int calc_op, calc_fresh, calc_err;

static void calc_num(char *out, long v) {
    char tmp[12];
    int i = 0, neg = 0, k = 0;
    unsigned long u;
    if (v < 0) { neg = 1; u = (unsigned long)(-(v + 1)) + 1u; }
    else u = (unsigned long)v;
    if (!u) tmp[i++] = '0';
    while (u && i < 11) { tmp[i++] = (char)('0' + u % 10); u /= 10; }
    if (neg) out[k++] = '-';
    while (i) out[k++] = tmp[--i];
    out[k] = 0;
}
static long calc_apply(int op, long a, long b, int *err) {
    if (op == 1) return a + b;
    if (op == 2) return a - b;
    if (op == 3) return a * b;
    if (op == 4) { if (!b) { *err = 1; return 0; } return a / b; }
    return b;
}

static const char calc_keys[4][5] = {
    { '7', '8', '9', '/', 0 },
    { '4', '5', '6', '*', 0 },
    { '1', '2', '3', '-', 0 },
    { '0', 'C', '=', '+', 0 },
};
#define CALC_X0 16
#define CALC_Y0 44
#define CALC_CW 44
#define CALC_CH 26

static void calc_draw(win_t *w, int cx, int cy) {
    (void)w;
    char buf[14];
    if (calc_err) { buf[0] = 'E'; buf[1] = 'r'; buf[2] = 'r'; buf[3] = 0; }
    else calc_num(buf, calc_cur);
    gfx_fill(cx + 16, cy + 8, 176, 24, RGB(255, 255, 255));
    gfx_rect(cx + 16, cy + 8, 176, 24, RGB(20, 20, 20));
    gfx_text(cx + 176 + 16 - gfx_textw(buf) - 6, cy + 15, buf,
             RGB(20, 20, 20), GFX_TRANS);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = cx + CALC_X0 + c * (CALC_CW + 6);
            int by = cy + CALC_Y0 + r * (CALC_CH + 6);
            gfx_fill(bx, by, CALC_CW, CALC_CH, RGB(60, 120, 220));
            gfx_rect(bx, by, CALC_CW, CALC_CH, RGB(20, 20, 20));
            char s[2] = { calc_keys[r][c], 0 };
            gfx_text(bx + CALC_CW / 2 - 4, by + 9, s,
                     RGB(255, 255, 255), GFX_TRANS);
        }
    }
}
static void calc_press(char k) {
    if (k >= '0' && k <= '9') {
        if (calc_err) { calc_err = 0; calc_cur = 0; calc_acc = 0; calc_op = 0; }
        if (calc_fresh) { calc_cur = k - '0'; calc_fresh = 0; }
        else calc_cur = calc_cur * 10 + (k - '0');
    } else if (k == 'C') {
        calc_acc = 0; calc_cur = 0; calc_op = 0; calc_fresh = 1; calc_err = 0;
    } else {
        int op = k == '+' ? 1 : k == '-' ? 2 : k == '*' ? 3 : k == '/' ? 4 : 0;
        if (k == '=') {
            if (calc_op && !calc_err) {
                calc_cur = calc_apply(calc_op, calc_acc, calc_cur, &calc_err);
                calc_op = 0;
            }
            calc_fresh = 1;
        } else if (op) {
            if (calc_op && !calc_fresh && !calc_err)
                calc_acc = calc_apply(calc_op, calc_acc, calc_cur, &calc_err);
            else if (!calc_err)
                calc_acc = calc_cur;
            calc_op = op;
            calc_fresh = 1;
        }
    }
    wm_dirty();
}
static void calc_click(win_t *w, int x, int y, int btn) {
    (void)w; (void)btn;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            int bx = CALC_X0 + c * (CALC_CW + 6);
            int by = CALC_Y0 + r * (CALC_CH + 6);
            if (x >= bx && y >= by && x < bx + CALC_CW && y < by + CALC_CH) {
                calc_press(calc_keys[r][c]);
                return;
            }
        }
}
void apps_open_calc(void) {
    calc_acc = 0; calc_cur = 0; calc_op = 0; calc_fresh = 1; calc_err = 0;
    int w = 16 * 2 + 4 * 44 + 3 * 6 + 4, h = 44 + 4 * 26 + 3 * 6 + 44;
    wm_open("Calculator", 120, 100, w, h, calc_draw, calc_click, 0);
}

/* ---------- Display settings ---------- */
static const int disp_modes[3][2] = { { 640, 480 }, { 800, 600 }, { 1024, 768 } };

static void disp_draw(win_t *w, int cx, int cy) {
    (void)w;
    gfx_text(cx + 16, cy + 10, "Resolution:", RGB(20, 20, 20), GFX_TRANS);
    for (int i = 0; i < 3; i++) {
        char b[16];
        int v = disp_modes[i][0], k = 0, t[8], n = 0;
        if (!v) b[k++] = '0';
        while (v && n < 8) { t[n++] = v % 10; v /= 10; }
        while (n) b[k++] = (char)('0' + t[--n]);
        b[k++] = 'x';
        v = disp_modes[i][1]; n = 0;
        if (!v) b[k++] = '0';
        while (v && n < 8) { t[n++] = v % 10; v /= 10; }
        while (n) b[k++] = (char)('0' + t[--n]);
        b[k] = 0;
        int by = cy + 34 + i * 34;
        int cur = gfx_w() == disp_modes[i][0] && gfx_h() == disp_modes[i][1];
        gfx_fill(cx + 16, by, 200, 26, cur ? RGB(40, 160, 70) : RGB(60, 120, 220));
        gfx_rect(cx + 16, by, 200, 26, RGB(20, 20, 20));
        gfx_text(cx + 26, by + 9, b, RGB(255, 255, 255), GFX_TRANS);
    }
}
static void disp_click(win_t *w, int x, int y, int btn) {
    (void)w; (void)btn;
    for (int i = 0; i < 3; i++) {
        int by = 34 + i * 34;
        if (x >= 16 && y >= by && x < 216 && y < by + 26) {
            wm_set_resolution(disp_modes[i][0], disp_modes[i][1]);
            return;
        }
    }
}
void apps_open_display(void) {
    wm_open("Display", 180, 140, 248, 34 + 3 * 34 + 16, disp_draw, disp_click, 0);
}
