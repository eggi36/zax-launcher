/* Which Zax.exe this is: build, version and copy protection, from a few bytes of the exe.
 * Used by Zax-Launcher.exe (on the file, before starting it) and Zax-Launcher.dll (on the loaded image).
 *
 * Supported (docs of zax-decomp: docs/EXE_FINDINGS.md, "Exe variants" and "Build 1308 -> 1309"):
 *   build 1308 ("Zax 1.0") without copy protection: the German and Russian exe, the English noCD exe, the exe of
 *     the demo of 2001-08-06; their code is the same byte for byte (apart from the GameSpy port).
 *   build 1309 ("Zax 1.1") without copy protection: the Polish exe. Its code from 0x59fc70 on lies 0x10 higher
 *     than in 1308 (calls into it, vtable slots, jump tables); every other address, all data, is the same.
 * Recognised but not supported: the editions whose entry point lies in the section .cms_t (SecuROM; the English
 * original 1308, the Spanish 1309). The launcher patches nothing in them and does not start them.
 * Anything else (other builds, the older demos, the Chinese 1314) is unknown: nothing is patched. */
#pragma once
#include <windows.h>

enum { EDITION_UNKNOWN, EDITION_1308, EDITION_1309, EDITION_SECUROM };

struct edition {
    int kind;
    int build;           /* 1308, 1309, 0 if unknown */
    DWORD shift;         /* 0x10 for 1309: added to code addresses of 1308 in [SHIFT_FROM, SHIFT_END) */
    char name[64];       /* "Zax 1.0 Build 1308", "Zax 1.1 Build 1309 (Polish)", ... */
};

#define IMAGE_BASE  0x400000
#define SHIFT_FROM  0x59fc70   /* the first function that lies 0x10 higher in 1309 */
#define SHIFT_END   0x5e9c80   /* the free bytes at the end of .text (Polish 1309: its new strings) */
#define BUILD_FUNC  0x5908a0   /* mov eax, build; ret */
#define VERSION_TXT 0x623660   /* "1.0" / "1.1" */
#define ENTRY_1308  0x1d328f   /* entry point (RVA) of the unprotected 1308; 1309: + 0x10 */
#define MENU_CALL   0x50eab7   /* call CMainMenu::CMainMenu 0x5be4a0 (1309: 0x5be4b0), from below the shift */
#define POLISH_PUSH 0x59cc54   /* Game_DefaultKeyBindings: push of the first Polish weapon name 0x5e9c80 */

/* The 1308 address of code in this edition */
static inline DWORD edition_code(const struct edition *ed, DWORD addr)
{
    return addr >= SHIFT_FROM && addr < SHIFT_END ? addr + ed->shift : addr;
}

struct pe_view {
    const BYTE *base;
    DWORD size;
    int mapped;   /* 1: a loaded image (offset = RVA); 0: the file (offset from the section table) */
    const IMAGE_SECTION_HEADER *sections;
    int count;
};

/* n bytes at the virtual address va, or NULL outside the image or the file */
static const BYTE *pe_at(const struct pe_view *pe, DWORD va, DWORD n)
{
    if (va < IMAGE_BASE)
        return NULL;
    DWORD rva = va - IMAGE_BASE;
    if (pe->mapped)
        return rva + n <= pe->size ? pe->base + rva : NULL;
    for (int i = 0; i < pe->count; i++) {
        const IMAGE_SECTION_HEADER *s = &pe->sections[i];
        if (rva >= s->VirtualAddress && rva + n <= s->VirtualAddress + s->SizeOfRawData) {
            DWORD off = s->PointerToRawData + (rva - s->VirtualAddress);
            return off + n <= pe->size ? pe->base + off : NULL;
        }
    }
    return NULL;
}

static int bytes_are(const struct pe_view *pe, DWORD va, const BYTE *expect, DWORD n)
{
    const BYTE *p = pe_at(pe, va, n);
    if (!p)
        return 0;
    for (DWORD i = 0; i < n; i++)
        if (p[i] != expect[i])
            return 0;
    return 1;
}

/* base: the file's bytes (mapped 0) or the loaded module (mapped 1, size = its SizeOfImage or 0 for "from the header") */
static void edition_detect(struct edition *ed, const BYTE *base, DWORD size, int mapped)
{
    ed->kind = EDITION_UNKNOWN, ed->build = 0, ed->shift = 0;
    lstrcpyA(ed->name, "unknown Zax.exe");
    if (!base || (!mapped && size < 0x400))
        return;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || (DWORD)dos->e_lfanew > 0x1000 || (!mapped && (DWORD)dos->e_lfanew + 0x200 > size))
        return;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.ImageBase != IMAGE_BASE)
        return;
    struct pe_view pe = {base, mapped ? nt->OptionalHeader.SizeOfImage : size, mapped,
                         (const IMAGE_SECTION_HEADER *)((const BYTE *)&nt->OptionalHeader + nt->FileHeader.SizeOfOptionalHeader),
                         nt->FileHeader.NumberOfSections};
    if (pe.count > 32 || (!mapped && (DWORD)((const BYTE *)(pe.sections + pe.count) - base) > size))
        return;

    /* build number and version text, the same in every edition of 1308 and 1309 */
    const BYTE *f = pe_at(&pe, BUILD_FUNC, 6), *v = pe_at(&pe, VERSION_TXT, 4);
    int build = f && f[0] == 0xb8 && f[3] == 0 && f[4] == 0 && f[5] == 0xc3 ? f[1] | f[2] << 8 : 0;
    const char *version = build == 1308 ? "1.0" : build == 1309 ? "1.1" : NULL;
    if (!version || !v || v[0] != version[0] || v[1] != version[1] || v[2] != version[2] || v[3])
        return;
    ed->build = build;
    wsprintfA(ed->name, "Zax %s Build %d", version, build);

    /* SecuROM: the entry point lies in its loader section (the noCD exes keep the sections, but start in .text) */
    static const BYTE cms_t[IMAGE_SIZEOF_SHORT_NAME] = ".cms_t";
    DWORD entry = nt->OptionalHeader.AddressOfEntryPoint;
    for (int i = 0; i < pe.count; i++) {
        const IMAGE_SECTION_HEADER *s = &pe.sections[i];
        int named = 1;
        for (int k = 0; k < IMAGE_SIZEOF_SHORT_NAME; k++)
            named &= s->Name[k] == cms_t[k];
        if (named && entry >= s->VirtualAddress && entry < s->VirtualAddress + s->Misc.VirtualSize) {
            ed->kind = EDITION_SECUROM;
            lstrcatA(ed->name, " (SecuROM)");
            return;
        }
    }

    /* the code layout: entry point and a call from below the shift into it */
    DWORD shift = build == 1309 ? 0x10 : 0;
    BYTE call[5] = {0xe8};
    *(INT32 *)(call + 1) = (INT32)(0x5be4a0 + shift - (MENU_CALL + 5));
    if (entry != ENTRY_1308 + shift || !bytes_are(&pe, MENU_CALL, call, 5)) {
        lstrcatA(ed->name, " (unknown variant)");
        ed->build = 0;
        return;
    }
    ed->kind = build == 1309 ? EDITION_1309 : EDITION_1308;
    ed->shift = shift;
    static const BYTE polish[] = {0x68, 0x80, 0x9c, 0x5e, 0x00};
    if (build == 1309 && bytes_are(&pe, POLISH_PUSH, polish, 5))
        lstrcatA(ed->name, " (Polish)");
}
