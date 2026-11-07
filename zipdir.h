/* Read the directory of a zip file such as Data.dat (uncompressed, no Zip64). Works on buffers
 * only, without Windows calls (so it can also be tested on the Mac). */
#pragma once

static inline unsigned zip_u16(const unsigned char *p) { return p[0] | p[1] << 8; }
static inline unsigned zip_u32(const unsigned char *p) { return zip_u16(p) | (unsigned)zip_u16(p + 2) << 16; }

#define ZIP_TAIL_MAX (22 + 65535)  /* end of central directory including the longest comment */
#define ZIP_LOCAL_HEAD 30

/* Find the end of the central directory in the file tail (n bytes): position and size of the
 * central directory. 0 if there is none. Signature "PK\5\6" or "RE\5\6": the game's Data.dat
 * ends like that, Zax.exe accepts both (0x5c22e5). */
static inline int zip_eocd(const unsigned char *tail, unsigned n, unsigned *cd_off, unsigned *cd_size)
{
    for (unsigned i = n >= 22 ? n - 22 + 1 : 0; i-- > 0;) {
        const unsigned char *p = tail + i;
        if ((zip_u32(p) == 0x06054b50 || zip_u32(p) == 0x06054552) && i + 22 + zip_u16(p + 20) == n) {
            *cd_size = zip_u32(p + 12);
            *cd_off = zip_u32(p + 16);
            return 1;
        }
    }
    return 0;
}

struct zip_entry {
    const char *name;
    int name_len;
    unsigned method, size, local_off;  /* method 0 = stored; local_off = local header */
};

/* Central directory entry at p (n bytes left): length of the entry, 0 at the end */
static inline unsigned zip_entry_at(const unsigned char *p, unsigned n, struct zip_entry *e)
{
    if (n < 46 || zip_u32(p) != 0x02014b50)
        return 0;
    unsigned len = 46 + zip_u16(p + 28) + zip_u16(p + 30) + zip_u16(p + 32);
    if (len > n)
        return 0;
    e->method = zip_u16(p + 10);
    e->size = zip_u32(p + 20);
    e->local_off = zip_u32(p + 42);
    e->name = (const char *)p + 46;
    e->name_len = (int)zip_u16(p + 28);
    return len;
}

/* Distance from the local header (ZIP_LOCAL_HEAD bytes) to the data, 0 for a wrong header */
static inline unsigned zip_local_data(const unsigned char *head)
{
    if (zip_u32(head) != 0x04034b50)
        return 0;
    return ZIP_LOCAL_HEAD + zip_u16(head + 26) + zip_u16(head + 28);
}

static inline char zip_lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

/* Compare a name with prefix, ignoring case and / vs. \: length of prefix or 0 */
static inline int zip_prefix(const char *s, int n, const char *prefix)
{
    int i = 0;
    for (; prefix[i]; i++) {
        char a = s[i] == '\\' ? '/' : zip_lower(s[i]);
        if (i >= n || a != prefix[i])
            return 0;
    }
    return i;
}

/* Map name of an entry as in the start window list: "[Data/]Levels/<path>.zax" becomes
 * "<path>" with /. Length or -1 if the entry is not a map or does not fit. */
static inline int zip_map_name(const char *name, int n, char *out, int cap)
{
    int skip = zip_prefix(name, n, "data/");
    name += skip, n -= skip;
    skip = zip_prefix(name, n, "levels/");
    if (!skip)
        return -1;
    name += skip, n -= skip;
    if (n < 5 || !zip_prefix(name + n - 4, 4, ".zax") || n - 4 >= cap)
        return -1;
    n -= 4;
    for (int i = 0; i < n; i++)
        out[i] = name[i] == '\\' ? '/' : name[i];
    out[n] = 0;
    return n;
}
