/* Users database on top of the shell ramfs (see shell_fread/fwrite).
 * Line-based files, rewritten whole on change; small user counts only. */
#include "users.h"
#include "shell.h"

#define UMAX 16   /* max login name length */
#define PMAX 32   /* max password length (checked, not stored) */

/* salted + iterated FNV-1a, hex digest. Deters eyeballing, not attacks. */
static u32 uhash(const char *salt, const char *pass) {
    u32 h = 2166136261u;
    for (int r = 0; r < 1000; r++) {
        const char *s = salt;
        while (*s) { h ^= (u8)*s++; h *= 16777619u; }
        s = pass;
        while (*s) { h ^= (u8)*s++; h *= 16777619u; }
        h ^= (u32)r;
        h *= 16777619u;
    }
    return h;
}

static void tohex(u32 v, char out[9]) {
    static const char *d = "0123456789abcdef";
    for (int i = 0; i < 8; i++) out[i] = d[(v >> (28 - i * 4)) & 15];
    out[8] = 0;
}

/* 4-hex-char salt from RTC + counter (unique enough per boot session) */
static void mksalt(char out[5]) {
    static const char *d = "0123456789abcdef";
    static u32 n;
    u32 v = rtc_seconds() + (n++ * 0x9E3779B9u);
    for (int i = 0; i < 4; i++) out[i] = d[(v >> (12 - i * 4)) & 15];
    out[4] = 0;
}

int users_validname(const char *name) {
    int i = 0;
    if (!name || !name[0]) return 0;
    if ((name[0] < 'a' || name[0] > 'z') && name[0] != '_') return 0;
    while (name[i]) {
        char c = name[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                 c == '_' || c == '-';
        if (!ok || i >= UMAX) return 0;
        i++;
    }
    return 1;
}

/* find "name:" at a line start; returns line offset, -1 if absent */
static int find_line(const char *file, const char *name) {
    int i = 0, n = 0;
    while (name[n]) n++;
    while (file[i]) {
        int ls = i;
        int k = 0;
        while (k < n && file[i] == name[k]) { i++; k++; }
        if (k == n && file[i] == ':') return ls;
        while (file[i] && file[i] != '\n') i++;
        if (file[i] == '\n') i++;
    }
    return -1;
}

/* parse decimal uid from a passwd line ("name:uid:gid"); -1 on error */
static int line_uid(const char *file, int off) {
    int i = off;
    while (file[i] && file[i] != ':') i++;
    if (!file[i]) return -1;
    i++;
    int v = 0, digits = 0;
    while (file[i] >= '0' && file[i] <= '9') {
        v = v * 10 + file[i] - '0';
        digits++;
        i++;
    }
    return digits ? v : -1;
}

/* split "salt$hash" from a shadow line; 0 ok */
static int line_cred(const char *file, int off,
                     char *salt, char *hash) {
    int i = off;
    while (file[i] && file[i] != ':') i++;
    if (!file[i]) return -1;
    i++;
    int k = 0;
    while (file[i] && file[i] != '$' && k < 4) salt[k++] = file[i++];
    salt[k] = 0;
    if (file[i] != '$') return -1;
    i++;
    k = 0;
    while (file[i] && file[i] != '\n' && k < 8) hash[k++] = file[i++];
    hash[k] = 0;
    return (k == 8) ? 0 : -1;
}

void users_init(void) {
    char buf[768];
    if (shell_fread("/etc/passwd", buf, sizeof(buf)) >= 0 &&
        shell_fread("/etc/shadow", buf, sizeof(buf)) >= 0)
        return;   /* DB already present (never on fresh boot) */
    shell_fwrite("/etc/passwd", "root:0:0\n", 9);
    char salt[5], hash[9], line[64];
    mksalt(salt);
    tohex(uhash(salt, "root"), hash);
    int k = 0, i = 0;
    const char *p = "root:";
    while (*p) line[k++] = *p++;
    for (i = 0; i < 4; i++) line[k++] = salt[i];
    line[k++] = '$';
    for (i = 0; i < 8; i++) line[k++] = hash[i];
    line[k++] = '\n';
    line[k] = 0;
    shell_fwrite("/etc/shadow", line, (u32)k);
}

int users_auth(const char *name, const char *pass) {
    char buf[768], salt[5], hash[9], mine[9];
    if (!users_validname(name) || !pass) return 0;
    if (shell_fread("/etc/shadow", buf, sizeof(buf)) < 0) return 0;
    int off = find_line(buf, name);
    if (off < 0) return 0;
    if (line_cred(buf, off, salt, hash)) return 0;
    tohex(uhash(salt, pass), mine);
    for (int i = 0; i < 8; i++)
        if (mine[i] != hash[i]) return 0;
    return 1;
}

int users_uid(const char *name) {
    char buf[768];
    if (!users_validname(name)) return -1;
    if (shell_fread("/etc/passwd", buf, sizeof(buf)) < 0) return -1;
    int off = find_line(buf, name);
    if (off < 0) return -1;
    return line_uid(buf, off);
}

/* append "line" to file; -1 when it would overflow */
static int append_line(const char *path, const char *line) {
    char buf[768];
    int n = shell_fread(path, buf, sizeof(buf) - 1);
    if (n < 0) n = 0;
    int k = 0;
    while (line[k]) k++;
    if (n + k >= 768) return -1;
    for (int i = 0; i < k; i++) buf[n + i] = line[i];
    buf[n + k] = 0;
    return shell_fwrite(path, buf, (u32)(n + k));
}

/* drop the line at off (to '\n' inclusive) */
static void cut_line(char *buf, int off) {
    int e = off;
    while (buf[e] && buf[e] != '\n') e++;
    if (buf[e] == '\n') e++;
    int i = off;
    while (buf[e]) buf[i++] = buf[e++];
    buf[i] = 0;
}

int users_add(const char *name, const char *pass) {
    char buf[768], line[64];
    int k, i, uid;
    if (!users_validname(name) || !pass || !pass[0]) return -1;
    if (slen(pass) > PMAX) return -1;
    if (users_uid(name) >= 0) return -1;   /* exists */
    /* next uid: max + 1 */
    uid = 0;
    if (shell_fread("/etc/passwd", buf, sizeof(buf)) >= 0) {
        int o = 0;
        while (buf[o]) {
            int u = line_uid(buf, o);
            if (u >= uid) uid = u + 1;
            while (buf[o] && buf[o] != '\n') o++;
            if (buf[o] == '\n') o++;
        }
    }
    k = 0;
    while (name[k]) { line[k] = name[k]; k++; }
    line[k++] = ':';
    char t[12];
    int n = 0, v = uid;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) line[k++] = t[--n];
    line[k++] = ':';
    n = 0; v = uid;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) line[k++] = t[--n];
    line[k++] = '\n';
    line[k] = 0;
    if (append_line("/etc/passwd", line)) return -1;
    /* shadow entry */
    {
        char salt[5], hash[9];
        mksalt(salt);
        tohex(uhash(salt, pass), hash);
        k = 0;
        while (name[k]) { line[k] = name[k]; k++; }
        line[k++] = ':';
        for (i = 0; i < 4; i++) line[k++] = salt[i];
        line[k++] = '$';
        for (i = 0; i < 8; i++) line[k++] = hash[i];
        line[k++] = '\n';
        line[k] = 0;
        if (append_line("/etc/shadow", line)) {
            /* roll back passwd half */
            if (shell_fread("/etc/passwd", buf, sizeof(buf)) >= 0) {
                int off = find_line(buf, name);
                if (off >= 0) {
                    cut_line(buf, off);
                    int m = 0;
                    while (buf[m]) m++;
                    shell_fwrite("/etc/passwd", buf, (u32)m);
                }
            }
            return -1;
        }
    }
    return 0;
}

int users_del(const char *name) {
    char buf[768];
    int m;
    if (!users_validname(name)) return -1;
    if (scmp(name, "root") == 0) return -1;
    if (users_uid(name) < 0) return -1;
    if (shell_fread("/etc/passwd", buf, sizeof(buf)) >= 0) {
        int off = find_line(buf, name);
        if (off >= 0) {
            cut_line(buf, off);
            m = 0;
            while (buf[m]) m++;
            shell_fwrite("/etc/passwd", buf, (u32)m);
        }
    }
    if (shell_fread("/etc/shadow", buf, sizeof(buf)) >= 0) {
        int off = find_line(buf, name);
        if (off >= 0) {
            cut_line(buf, off);
            m = 0;
            while (buf[m]) m++;
            shell_fwrite("/etc/shadow", buf, (u32)m);
        }
    }
    return 0;
}

int users_setpass(const char *name, const char *pass) {
    char buf[768], line[64];
    int k, i;
    if (!users_validname(name) || !pass || !pass[0]) return -1;
    if (slen(pass) > PMAX) return -1;
    if (users_uid(name) < 0) return -1;
    if (shell_fread("/etc/shadow", buf, sizeof(buf)) < 0) return -1;
    {
        int off = find_line(buf, name);
        if (off < 0) return -1;
        cut_line(buf, off);
    }
    {
        char salt[5], hash[9];
        mksalt(salt);
        tohex(uhash(salt, pass), hash);
        k = 0;
        while (name[k]) { line[k] = name[k]; k++; }
        line[k++] = ':';
        for (i = 0; i < 4; i++) line[k++] = salt[i];
        line[k++] = '$';
        for (i = 0; i < 8; i++) line[k++] = hash[i];
        line[k++] = '\n';
        line[k] = 0;
        if (append_line("/etc/shadow", line)) {
            /* original entry already cut; report failure */
            return -1;
        }
    }
    return 0;
}
