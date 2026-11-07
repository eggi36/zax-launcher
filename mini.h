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

/* Switch such as /help: case-insensitive, preceded by the start or a space,
 * followed by a space or the end */
static inline int has_switch(const char *args, const char *name)
{
    int n = lstrlenA(name);
    for (const char *p = args; (p = str_find(p, name)) != NULL; p++)
        if ((p == args || p[-1] == ' ' || p[-1] == '\t') && (!p[n] || p[n] == ' ' || p[n] == '\t'))
            return 1;
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
