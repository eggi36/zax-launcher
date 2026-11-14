/* Small replacement functions so Zax-Launcher needs no C runtime library and only
 * kernel32/user32 (so it also runs on older Windows versions). */
#pragma once
#include <windows.h>

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;
    while (n--)
        *d++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

static inline char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

/* 0 if the first n characters are equal, ignoring case */
static inline int str_nieq(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        if (lower(a[i]) != lower(b[i]))
            return 1;
        if (!a[i])
            return 0;
    }
    return 0;
}

static inline const char *str_find(const char *s, const char *what)
{
    int n = lstrlenA(what);
    for (; *s; s++)
        if (str_nieq(s, what, n) == 0)
            return s;
    return NULL;
}

/* The next token of a command line from p: blanks skipped, a token ends at a blank outside double quotes
 * (a quote switches quoting on and off, as in Windows' own rules for plain arguments). Its start, NULL at the end;
 * *end is set after it. */
static inline const char *next_token(const char *p, const char **end)
{
    while (*p == ' ' || *p == '\t')
        p++;
    if (!*p)
        return NULL;
    int quoted = 0;
    const char *e = p;
    for (; *e && (quoted || (*e != ' ' && *e != '\t')); e++)
        if (*e == '"')
            quoted = !quoted;
    *end = e;
    return p;
}

/* Whether the token [t, e) is name, ignoring case */
static inline int token_is(const char *t, const char *e, const char *name)
{
    int n = lstrlenA(name);
    return e - t == n && str_nieq(t, name, n) == 0;
}

/* Switch such as /help: a whole token of the command line, case-insensitive; text inside a quoted value
 * (/Map "Eigene/x /Cheats y") is no switch */
static inline int has_switch(const char *args, const char *name)
{
    const char *e;
    for (const char *t = next_token(args, &e); t; t = next_token(e, &e))
        if (token_is(t, e, name))
            return 1;
    return 0;
}

/* The token after the switch /name (name without the slash) without its quotes, e.g. Act 1/M1M2/01 Main for
 * /Map "Act 1/M1M2/01 Main"; 0 without the switch or without a value */
static inline int arg_value(const char *args, const char *name, char *out, int size)
{
    char sw[64];
    if (lstrlenA(name) + 2 > (int)sizeof sw)
        return 0;
    sw[0] = '/';
    lstrcpyA(sw + 1, name);
    const char *e;
    for (const char *t = next_token(args, &e); t; t = next_token(e, &e)) {
        if (!token_is(t, e, sw))
            continue;
        const char *v = next_token(e, &e);
        int i = 0;
        for (; v && v < e && i + 1 < size; v++)
            if (*v != '"')
                out[i++] = *v;
        out[i] = 0;
        return i > 0;
    }
    return 0;
}

/* Folder of the running exe with a trailing \ */
static inline void exe_dir(char *out)
{
    DWORD n = GetModuleFileNameA(NULL, out, MAX_PATH);
    while (n > 0 && out[n - 1] != '\\' && out[n - 1] != '/')
        n--;
    out[n] = 0;
}
