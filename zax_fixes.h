/* Zax-Launcher.dll: the fixes of fixes.h (/Fix Name), made in the running Zax.exe. Each one first compares the bytes
 * at its sites with those of the original (build 1308; build 1309 has the same bytes at every site, only DrawHudCrash
 * differs: 1309 has the test already) and stays off, with a line in Zax-Launcher.log, when they differ. A fix changes
 * nothing else: without its switch the game runs the original code. The bugs, their addresses and the port's fixes
 * that these follow: docs/FIXES.md; zax-redone docs/ORIGINAL_BUGS.md.
 * Included by zax_launcher_dll.c after its helpers (log_line, CODE, STR, THISCALL). */
#pragma once
#include "fixes.h"

/* ---- Helpers ------------------------------------------------------------------------------------------------- */

static int code_is(DWORD site, const BYTE *expect, int n)
{
    for (int i = 0; i < n; i++)
        if (((BYTE *)site)[i] != expect[i])
            return 0;
    return 1;
}

static void write_code(DWORD site, const BYTE *bytes, int n)
{
    DWORD old;
    VirtualProtect((void *)site, (SIZE_T)n, PAGE_EXECUTE_READWRITE, &old);
    for (int i = 0; i < n; i++)
        ((BYTE *)site)[i] = bytes[i];
    VirtualProtect((void *)site, (SIZE_T)n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)site, (SIZE_T)n);
}

/* jmp (0xe9) or call (0xe8) to target at site, the rest of len bytes nops */
static void branch_to(DWORD site, BYTE op, const void *target, int len)
{
    BYTE b[16];
    b[0] = op;
    *(INT32 *)(b + 1) = (INT32)((DWORD)target - (site + 5));
    for (int i = 5; i < len; i++)
        b[i] = 0x90;
    write_code(site, b, len);
}

/* A call rel32 at site whose target is the given function */
static int calls(DWORD site, DWORD target)
{
    return ((BYTE *)site)[0] == 0xe8 && site + 5 + *(INT32 *)(site + 1) == target;
}

static int fix_mismatch(int fix)
{
    log_line("Fix %s: the code does not match, different Zax.exe? This fix stays off.", FIX[fix].name);
    return 0;
}

/* Where the caves jump back to: the edition's addresses, set before a cave is put in */
DWORD g_ret_hud, g_skip_hud, g_layer_fn, g_ret_minimap, g_end_minimap, g_init_fields, g_ret_shield, g_ret_pickup,
    g_end_pickup, g_item_definition, g_ret_fidget, g_ret_aim, g_ret_ground;

/* ---- MultiplayerCfg (ORIGINAL_BUGS 1.5): Display_Set640x480 0x590f00 jumps over its two INI writes -------------- */

static int fix_multiplayer_cfg(void)
{
    static const BYTE expect[] = {0x8b, 0x0d, 0xcc, 0x02, 0x6c, 0x00, 0x8b, 0x49, 0x04};   /* 0x590fc7 */
    static const BYTE end[] = {0x5e, 0x83, 0xc4, 0x08, 0xc3};                                /* 0x591012 */
    static const BYTE patch[] = {0xeb, 0x49};                                                /* jmp 0x591012 */
    if (!code_is(CODE(0x590fc7), expect, sizeof expect) || !code_is(CODE(0x591012), end, sizeof end))
        return fix_mismatch(FIX_MULTIPLAYER_CFG);
    write_code(CODE(0x590fc7), patch, sizeof patch);
    return 1;
}

/* ---- DrawHudCrash (ORIGINAL_BUGS 2.39): 1309's test of World::CurrentLayer 0x4f1050 before the pointer ---------- */

void hud_pointer_cave(void);
__asm__(".globl _hud_pointer_cave\n"
        "_hud_pointer_cave:\n"
        "  movl %ebx, %ecx\n"            /* ebx: the world (DrawHud's this) */
        "  call *_g_layer_fn\n"
        "  testl %eax, %eax\n"
        "  jz 1f\n"
        "  movl 0x713f14, %ecx\n"        /* the overwritten instruction */
        "  jmp *_g_ret_hud\n"
        "1:\n"
        "  jmp *_g_skip_hud\n");          /* past the pointer, where 0x59e849 jumps too */

static int fix_draw_hud_crash(void)
{
    static const BYTE expect[] = {0x8b, 0x0d, 0x14, 0x3f, 0x71, 0x00, 0x8b, 0xb9, 0xe0, 0x02};   /* 0x59e856 */
    static const BYTE has_test[] = {0x8b, 0xcb, 0xe8, 0xf3, 0x27, 0xf5, 0xff, 0x85, 0xc0};       /* 1309 */
    static const BYTE skip[] = {0xe9, 0x30, 0x02, 0x00, 0x00};                                    /* 0x59e849 */
    if (g_ed.kind == EDITION_1309) {
        if (!code_is(0x59e856, has_test, sizeof has_test))
            return fix_mismatch(FIX_DRAW_HUD_CRASH);
        log_line("Fix DrawHudCrash: build 1309 has the test already, nothing to change");
        return 1;
    }
    if (!code_is(0x59e856, expect, sizeof expect) || !code_is(0x59e849, skip, sizeof skip))
        return fix_mismatch(FIX_DRAW_HUD_CRASH);
    g_layer_fn = 0x4f1050;
    g_ret_hud = 0x59e85c;
    g_skip_hud = 0x59ea7e;
    branch_to(0x59e856, 0xe9, hud_pointer_cave, 6);
    return 1;
}

/* ---- DisableMiniMap (ORIGINAL_BUGS 1.6): HudMiniMap_Draw 0x508180's single-player branch ends when the current
 * layer's "Disable Mini-Map" (CLayerSaveData +0x168 at layer +0x3c) is set ------------------------------------- */

void minimap_cave(void);
__asm__(".globl _minimap_cave\n"
        "_minimap_cave:\n"
        "  cmpb $0, 0x1a4(%ebp)\n"       /* ebp: the current layer */
        "  jne 1f\n"
        "  leal 0x3c(%ebp), %esi\n"      /* the overwritten instructions */
        "  pushl $0x6bb4e0\n"
        "  jmp *_g_ret_minimap\n"
        "1:\n"
        "  jmp *_g_end_minimap\n");       /* the epilogue: pop edi, esi, ebp, ebx */

static int fix_disable_minimap(void)
{
    static const BYTE expect[] = {0x8d, 0x75, 0x3c, 0x68, 0xe0, 0xb4, 0x6b, 0x00, 0x6a, 0x00};   /* 0x5082ad */
    static const BYTE layer[] = {0xe8, 0x51, 0x8e, 0xfe, 0xff, 0x8a, 0x0d, 0xb8, 0x2d, 0x6c, 0x00, 0x8b, 0xe8};
    static const BYTE end[] = {0x5f, 0x5e, 0x5d, 0x5b, 0x83, 0xc4, 0x54, 0xc2, 0x0c, 0x00};        /* 0x50847a */
    if (!code_is(CODE(0x5082ad), expect, sizeof expect) || !code_is(CODE(0x5081fa), layer, sizeof layer) ||
        !code_is(CODE(0x50847a), end, sizeof end))
        return fix_mismatch(FIX_DISABLE_MINIMAP);
    g_ret_minimap = CODE(0x5082b5);
    g_end_minimap = CODE(0x50847a);
    branch_to(CODE(0x5082ad), 0xe9, minimap_cave, 8);
    return 1;
}

/* ---- SoundChannels (ORIGINAL_BUGS 1.19): Sound_LoadOptions 0x57ee90 reads "Sounds Maxing Chanels" (the key the
 * dialog writes) after "Sounds Mixing Chanels"; the second wins when it is there ---------------------------------- */

typedef int (__attribute__((fastcall)) *ini_int_fn)(void *a, void *b, const char *key, int def);
static int __attribute__((fastcall)) sound_channels_read(void *a, void *b, const char *key, int def)
{
    ini_int_fn read = (ini_int_fn)CODE(0x4a4670);
    int mixing = read(a, b, key, def);
    int channels = read(a, b, (const char *)0x62242c, mixing);
    log_line("Fix SoundChannels: \"Sounds Mixing Chanels\" gives %d, \"Sounds Maxing Chanels\" %d", mixing, channels);
    return channels;
}

static int fix_sound_channels(void)
{
    static const BYTE expect[] = {0x6a, 0x20, 0x68, 0xe0, 0x23, 0x62, 0x00};   /* 0x57ef35: push 32, push "Sounds Mixing Chanels" */
    if (!code_is(CODE(0x57ef35), expect, sizeof expect) || !calls(CODE(0x57ef3c), CODE(0x4a4670)) ||
        lstrcmpA((const char *)0x62242c, "Sounds Maxing Chanels") != 0)
        return fix_mismatch(FIX_SOUND_CHANNELS);
    branch_to(CODE(0x57ef3c), 0xe8, (void *)sound_channels_read, 5);
    return 1;
}

/* ---- PlayOnceAfterLoad (ORIGINAL_BUGS 1.20): CEntityBehaviorPlaySequenceOnce's Create 0x4966d0 and CreateArray
 * 0x496720 mark its speed +0x18 unset (-1); Begin overwrites it as before; Update 0x497010 of an object still marked
 * (made by a load, which does not run Begin) takes Begin's values: the speed from "Speed To Play" +0x14, the play
 * ratio +0x10 1.0, "Stationary" +0x24 cleared (as the port's [Fix] PlayOnceAfterLoad) ----------------------------- */

static int g_play_once_reports;
void __cdecl play_once_report(void *self)
{
    if (g_play_once_reports < 20 && ++g_play_once_reports)
        log_line("Fix PlayOnceAfterLoad: behaviour %08lX resumes at its own speed (no Begin since the load)", (DWORD)self);
}

void play_once_ctor_cave(void);
__asm__(".globl _play_once_ctor_cave\n"
        "_play_once_ctor_cave:\n"
        "  call *_g_init_fields\n"       /* the overwritten call: Object_InitFields (ecx = esi, the object) */
        "  movl $-1, 0x18(%esi)\n"
        "  ret\n");

void play_once_update_cave(void);
__asm__(".globl _play_once_update_cave\n"
        "_play_once_update_cave:\n"
        "  cmpl $-1, 0x18(%edi)\n"       /* edi: the behaviour */
        "  jne 1f\n"
        "  movl 0x14(%edi), %eax\n"
        "  movl %eax, 0x18(%edi)\n"
        "  movl $0x3f800000, 0x10(%edi)\n"
        "  movb $0, 0x24(%edi)\n"
        "  pushal\n"
        "  pushl %edi\n"
        "  call _play_once_report\n"
        "  addl $4, %esp\n"
        "  popal\n"
        "1:\n"
        "  flds 0x18(%edi)\n"            /* the overwritten instructions; their flags reach fnstsw behind the call */
        "  fcomps 0x5ebed0\n"
        "  ret\n");

static int fix_play_once_after_load(void)
{
    static const BYTE vt_create[] = {0xc7, 0x06, 0xa0, 0x2c, 0x5f, 0x00};   /* mov dword [esi], 0x5f2ca0 */
    static const BYTE update[] = {0xd9, 0x47, 0x18, 0xd8, 0x1d, 0xd0, 0xbe, 0x5e, 0x00, 0xdf, 0xe0};   /* 0x497024 */
    static const BYTE head[] = {0xa1, 0xfc, 0xdd, 0x6b, 0x00, 0x83, 0xec, 0x0c, 0x40, 0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9};
    const DWORD init = CODE(0x4173c0);
    if (!code_is(CODE(0x496707), vt_create, sizeof vt_create) || !calls(CODE(0x49670d), init) ||
        !code_is(CODE(0x496776), vt_create, sizeof vt_create) || !calls(CODE(0x49677c), init) ||
        !code_is(CODE(0x497010), head, sizeof head) || !code_is(CODE(0x497024), update, sizeof update))
        return fix_mismatch(FIX_PLAY_ONCE_AFTER_LOAD);
    g_init_fields = init;
    branch_to(CODE(0x49670d), 0xe8, play_once_ctor_cave, 5);
    branch_to(CODE(0x49677c), 0xe8, play_once_ctor_cave, 5);
    branch_to(CODE(0x497024), 0xe8, play_once_update_cave, 9);
    return 1;
}

/* ---- CompanionSpot (ORIGINAL_BUGS 1.7): CompanionAI_PickSpot 0x46ead0 keeps the computed sine part ------------- */

static int fix_companion_spot(void)
{
    static const BYTE expect[] = {0xd8, 0x44, 0x24, 0x0c, 0x89, 0x46, 0x04, 0xb0, 0x01};   /* 0x46eb66 */
    static const BYTE nops[] = {0x90, 0x90, 0x90};                                          /* no mov [esi+4], eax */
    if (!code_is(CODE(0x46eb66), expect, sizeof expect))
        return fix_mismatch(FIX_COMPANION_SPOT);
    write_code(CODE(0x46eb6a), nops, sizeof nops);
    return 1;
}

/* ---- SmoothTurning (ORIGINAL_BUGS 1.8): CEntityBehaviorMoveToPos::Update 0x430f20 tests the real flags ---------- */

static int fix_smooth_turning(void)
{
    static const BYTE expect[] = {0x8b, 0x9e, 0xa8, 0x00, 0x00, 0x00, 0x83, 0xcb, 0x08};   /* 0x430f2e */
    static const BYTE nops[] = {0x90, 0x90, 0x90};                                          /* no or ebx, 8 */
    if (!code_is(CODE(0x430f2e), expect, sizeof expect))
        return fix_mismatch(FIX_SMOOTH_TURNING);
    write_code(CODE(0x430f34), nops, sizeof nops);
    return 1;
}

/* ---- ShieldOverflow (ORIGINAL_BUGS 1.10): CShieldAI::Damage 0x56da70 (its vtable slot at 0x5ffb14, the only
 * reference). The cave in its branch for a hit at or above the charge notes the scaled damage and the charge; the
 * slot's wrapper then returns the part above the charge in the units the damage came in, as the port does --------- */

DWORD g_shield_seen, g_shield_scaled, g_shield_charge;
static DWORD g_shield_damage_fn;

void shield_cave(void);
__asm__(".globl _shield_cave\n"
        "_shield_cave:\n"
        "  movl 0x20(%esp), %eax\n"      /* the damage, scaled for a player (eax is free here: the fnstsw result) */
        "  movl %eax, _g_shield_scaled\n"
        "  movl 0x24(%esp), %eax\n"      /* the charge (kept in the type's argument slot) */
        "  movl %eax, _g_shield_charge\n"
        "  movl $1, _g_shield_seen\n"
        "  movl 0x10(%esi), %ecx\n"      /* the overwritten instructions */
        "  pushl $0x62191c\n"
        "  jmp *_g_ret_shield\n");

static float f32(DWORD bits)
{
    union { DWORD d; float f; } u;
    u.d = bits;
    return u.f;
}

typedef float (THISCALL *shield_fn)(void *self, void *e, float damage, DWORD type);
static float THISCALL shield_damage(void *self, void *e, float damage, DWORD type)
{
    DWORD outer = g_shield_seen;
    g_shield_seen = 0;
    float r = ((shield_fn)g_shield_damage_fn)(self, e, damage, type);
    if (g_shield_seen) {
        float scaled = f32(g_shield_scaled), charge = f32(g_shield_charge);
        if (scaled > charge && scaled > 0) {
            r = damage * (scaled - charge) / scaled;
            log_line("Fix ShieldOverflow: hit %d, charge %d, %d passes through", (int)scaled, (int)charge, (int)r);
        }
    }
    g_shield_seen = outer;
    return r;
}

static int fix_shield_overflow(void)
{
    static const BYTE expect[] = {0xf6, 0xc4, 0x01, 0x75, 0x2a, 0x8b, 0x4e, 0x10, 0x68, 0x1c, 0x19, 0x62, 0x00};   /* 0x56db8b */
    static const BYTE charge[] = {0x8b, 0x4b, 0x18, 0x89, 0x4c, 0x24, 0x24};                                     /* 0x56db37 */
    static const BYTE result[] = {0xd9, 0x44, 0x24, 0x20, 0xd8, 0x64, 0x24, 0x0c, 0x5e, 0x5d, 0x5b};             /* 0x56dc2a */
    DWORD *slot = (DWORD *)0x5ffb14;
    if (*slot != CODE(0x56da70) || !code_is(CODE(0x56db8b), expect, sizeof expect) ||
        !code_is(CODE(0x56db37), charge, sizeof charge) || !code_is(CODE(0x56dc2a), result, sizeof result))
        return fix_mismatch(FIX_SHIELD_OVERFLOW);
    g_shield_damage_fn = *slot;
    g_ret_shield = CODE(0x56db98);
    branch_to(CODE(0x56db90), 0xe9, shield_cave, 8);
    DWORD old;
    VirtualProtect(slot, 4, PAGE_READWRITE, &old);
    *slot = (DWORD)shield_damage;
    VirtualProtect(slot, 4, old, &old);
    return 1;
}

/* ---- PickupRefused (ORIGINAL_BUGS 1.11): CPickupAI::Collect 0x53e110 ends when the item refused and there is no
 * alternate item (ebx 0; the original reads address 0 there) or the alternate item refuses too ------------------ */

void pickup_cave(void);
__asm__(".globl _pickup_cave\n"
        "_pickup_cave:\n"
        "  testl %ebx, %ebx\n"           /* ebx: the alternate item */
        "  jz 1f\n"
        "  movl (%ebx), %eax\n"          /* the overwritten instructions and the call they set up: its slot 32 */
        "  pushl %ebp\n"
        "  movl %ebx, %ecx\n"
        "  call *0x80(%eax)\n"
        "  testb %al, %al\n"
        "  jz 1f\n"
        "  jmp *_g_ret_pickup\n"         /* taken: on as in the original */
        "1:\n"
        "  jmp *_g_end_pickup\n");        /* the end: the local item destroyed, nothing given, the pickup stays */

static int fix_pickup_refused(void)
{
    static const BYTE expect[] = {0x8b, 0x03, 0x55, 0x8b, 0xcb, 0xff, 0x90, 0x80, 0x00, 0x00, 0x00, 0x84, 0xc0, 0x74, 0x32};   /* 0x53e346 */
    static const BYTE end[] = {0x8d, 0x4c, 0x24, 0x14, 0xe8, 0x56, 0xbe, 0xee, 0xff, 0x5f, 0x5e, 0x5b, 0x5d};                 /* 0x53e4a1 */
    static const BYTE jump[] = {0x0f, 0x84, 0xa6, 0x01, 0x00, 0x00};                                                          /* 0x53e2f5: je 0x53e4a1 */
    if (!code_is(CODE(0x53e346), expect, sizeof expect) || !code_is(CODE(0x53e4a1), end, sizeof end) ||
        !code_is(CODE(0x53e2f5), jump, sizeof jump))
        return fix_mismatch(FIX_PICKUP_REFUSED);
    g_ret_pickup = CODE(0x53e355);
    g_end_pickup = CODE(0x53e4a1);
    branch_to(CODE(0x53e346), 0xe9, pickup_cave, 5);
    return 1;
}

/* ---- UseItems (ORIGINAL_BUGS 1.12): Part_UseItems 0x426930's search skips the items already marked for deletion
 * (+0x11): for those the definition call answers -1, which no definition is -------------------------------------- */

void use_items_cave(void);
__asm__(".globl _use_items_cave\n"
        "_use_items_cave:\n"
        "  cmpb $0, 0x11(%ecx)\n"
        "  jne 1f\n"
        "  jmp *_g_item_definition\n"
        "1:\n"
        "  orl $-1, %eax\n"
        "  ret\n");

static int fix_use_items(void)
{
    static const BYTE expect[] = {0x3b, 0x44, 0x24, 0x10, 0x74, 0x0a};   /* 0x426a7e: cmp eax, [esp+0x10]; je */
    static const BYTE mark[] = {0xc6, 0x46, 0x11, 0x01};                   /* 0x426aa4: the mark */
    if (!calls(CODE(0x426a79), CODE(0x482de0)) || !code_is(CODE(0x426a7e), expect, sizeof expect) ||
        !code_is(CODE(0x426aa4), mark, sizeof mark))
        return fix_mismatch(FIX_USE_ITEMS);
    g_item_definition = CODE(0x482de0);
    branch_to(CODE(0x426a79), 0xe8, use_items_cave, 5);
    return 1;
}

/* ---- OldPatrolSeed (ORIGINAL_BUGS 1.14): COldBadPatrolAI's constructor, inlined in Create 0x53a990 and CreateArray
 * 0x53aa00, no longer reseeds the generator 0x6d06c0 (the call's pushed seed is dropped instead) ---------------- */

static int fix_old_patrol_seed(void)
{
    static const BYTE seed[] = {0x68, 0xc6, 0xbd, 0x23, 0x00};   /* push 2342342, 7 bytes before each call */
    static const BYTE drop[] = {0x83, 0xc4, 0x04, 0x90, 0x90};   /* add esp, 4 */
    static const DWORD site[] = {0x53a9da, 0x53aa6b};
    for (int i = 0; i < 2; i++)
        if (!calls(CODE(site[i]), CODE(0x55c390)) || !code_is(CODE(site[i]) - 12, seed, sizeof seed))
            return fix_mismatch(FIX_OLD_PATROL_SEED);
    for (int i = 0; i < 2; i++)
        write_code(CODE(site[i]), drop, sizeof drop);
    return 1;
}

/* ---- FidgetNext (ORIGINAL_BUGS 1.15): CFidgetData::PickFidget 0x4a0680, order 5 counts its index on --------------- */

void fidget_cave(void);
__asm__(".globl _fidget_cave\n"
        "_fidget_cave:\n"
        "  movl (%esi), %edx\n"          /* the overwritten instructions */
        "  incl (%esi)\n"                /* the next one next time (the wrap above takes it back to 0) */
        "  movl 0x4(%edi), %eax\n"
        "  jmp *_g_ret_fidget\n");

static int fix_fidget_next(void)
{
    static const BYTE expect[] = {0xc7, 0x06, 0x00, 0x00, 0x00, 0x00, 0x8b, 0x16, 0x8b, 0x47, 0x04, 0x8b, 0x0c, 0x90};   /* 0x4a081b */
    if (!code_is(CODE(0x4a081b), expect, sizeof expect) || *(DWORD *)(0x4a0848 + 5 * 4) != CODE(0x4a080e))
        return fix_mismatch(FIX_FIDGET_NEXT);
    g_ret_fidget = CODE(0x4a0826);
    branch_to(CODE(0x4a0821), 0xe9, fidget_cave, 5);
    return 1;
}

/* ---- DPlaySlot100 (ORIGINAL_BUGS 1.16): the DirectPlay message handler 0x47ff50's search for a new player's slot
 * starts at 99, the array's last, instead of 100 (one past it) ------------------------------------------------- */

static int fix_dplay_slot100(void)
{
    static const BYTE expect[] = {0xb8, 0x64, 0x00, 0x00, 0x00, 0x8d, 0x8b, 0x04, 0x09, 0x00, 0x00,
                                  0xba, 0x01, 0x00, 0x00, 0x00, 0x80, 0x79, 0xf8, 0x00};   /* 0x4801cf */
    static const BYTE patch[] = {0xb8, 0x63, 0x00, 0x00, 0x00, 0x8d, 0x8b, 0xf8, 0x08, 0x00, 0x00};   /* 99, +0x8f8 */
    if (!code_is(CODE(0x4801cf), expect, sizeof expect))
        return fix_mismatch(FIX_DPLAY_SLOT100);
    write_code(CODE(0x4801cf), patch, sizeof patch);
    return 1;
}

/* ---- AimCentre (ORIGINAL_BUGS 1.18): ControlConfig::AnalogueVector 0x4708c0 takes the screen's middle (the game
 * frame's size, [[0x713f14]+0x2a8] slots 45 and 39, halved) instead of (320, 240) for the mouse's offset and for
 * putting the cursor back; the radius 200 stays --------------------------------------------------------------- */

int g_aim_cx = 320, g_aim_cy = 240;
float g_aim_fx = 320.0f, g_aim_fy = 240.0f;

typedef DWORD (THISCALL *size_fn)(void *self);
void __cdecl aim_update(void)
{
    BYTE *app = (BYTE *)rd(0x713f14);
    void *screen = app ? (void *)rd((DWORD)app + 0x2a8) : NULL;
    if (!screen)
        return;
    int cx = (int)(((size_fn)(*(DWORD **)screen)[45])(screen) >> 1);
    int cy = (int)(((size_fn)(*(DWORD **)screen)[39])(screen) >> 1);
    if (cx != g_aim_cx || cy != g_aim_cy)
        log_line("Fix AimCentre: the aiming centre is (%d, %d)", cx, cy);
    g_aim_cx = cx, g_aim_cy = cy;
    g_aim_fx = (float)cx, g_aim_fy = (float)cy;
}

void aim_cave(void);
__asm__(".globl _aim_cave\n"
        "_aim_cave:\n"
        "  pushl %ecx\n"
        "  pushl %edx\n"                 /* the mouse's x */
        "  pushl %eax\n"                 /* and y */
        "  call _aim_update\n"
        "  popl %eax\n"
        "  popl %edx\n"
        "  popl %ecx\n"
        "  subl _g_aim_cx, %edx\n"
        "  subl _g_aim_cy, %eax\n"
        "  jmp *_g_ret_aim\n");

static int fix_aim_centre(void)
{
    static const BYTE offset[] = {0x81, 0xc2, 0xc0, 0xfe, 0xff, 0xff, 0x05, 0x10, 0xff, 0xff, 0xff};   /* 0x4708e8: -320, -240 */
    static const BYTE back[] = {0xd9, 0x46, 0x60, 0xd8, 0x0d, 0xa0, 0xff, 0x5e, 0x00, 0xd8, 0x05, 0x9c, 0xff, 0x5e, 0x00,
                                0xd9, 0x5d, 0xf8, 0xd9, 0x45, 0xf8, 0xdb, 0x5d, 0xfc, 0xd9, 0x46, 0x5c, 0xd8, 0x0d,
                                0xa0, 0xff, 0x5e, 0x00, 0xd8, 0x05, 0x98, 0xff, 0x5e, 0x00};                  /* 0x470a6a */
    if (!code_is(CODE(0x4708e8), offset, sizeof offset) || !code_is(CODE(0x470a6a), back, sizeof back))
        return fix_mismatch(FIX_AIM_CENTRE);
    g_ret_aim = CODE(0x4708f3);
    branch_to(CODE(0x4708e8), 0xe9, aim_cave, 11);
    DWORD fy = (DWORD)&g_aim_fy, fx = (DWORD)&g_aim_fx;
    write_code(CODE(0x470a75), (const BYTE *)&fy, 4);   /* fadds [0x5eff9c] -> the centre's y */
    write_code(CODE(0x470a8d), (const BYTE *)&fx, 4);   /* fadds [0x5eff98] -> the centre's x */
    return 1;
}

/* ---- NoWriteBack (ORIGINAL_BUGS 4.8): Object_CreateFromFile 0x4164c0 ignores its write-back request ----------- */

static int fix_no_write_back(void)
{
    static const BYTE expect[] = {0x8a, 0x45, 0x08, 0x84, 0xc0, 0x74, 0x13, 0x6a, 0x00};   /* 0x41659f */
    static const BYTE patch[] = {0xeb, 0x13};                                               /* jmp past Object_WriteFile */
    if (!code_is(CODE(0x41659f), expect, sizeof expect) || !calls(CODE(0x4165b2), CODE(0x4167e0)))
        return fix_mismatch(FIX_NO_WRITE_BACK);
    write_code(CODE(0x4165a4), patch, sizeof patch);
    return 1;
}

/* ---- QuitDuringMovie (ORIGINAL_BUGS 1.21): App_Dtor 0x4cdcc0 closes every open movie of the Bink system 0x62eaf0
 * (records at +8, count at +0xc, 0x18 bytes each, the first dword set while open) with its CloseMovie 0x432080 before
 * Zip_UnmountAll 0x4a1120 ------------------------------------------------------------------------------------- */

#define BINK_SYSTEM 0x62eaf0
typedef void (THISCALL *close_movie_fn)(void *self, DWORD index);
void __cdecl quit_unmount(void)
{
    BYTE *bink = (BYTE *)BINK_SYSTEM;
    DWORD records = rd((DWORD)bink + 8), count = rd((DWORD)bink + 0xc);
    for (DWORD i = 0; records && i < count; i++)
        if (rd(records + i * 0x18)) {
            log_line("Fix QuitDuringMovie: movie %lu closed before the archives are unmounted", i);
            ((close_movie_fn)CODE(0x432080))(bink, i);
        }
    ((void (*)(void))CODE(0x4a1120))();
}

static int fix_quit_during_movie(void)
{
    static const BYTE head[] = {0x53, 0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xe8, 0x73, 0x5f, 0x00};   /* 0x4cdcc0 */
    static const BYTE close[] = {0x8b, 0x44, 0x24, 0x04, 0x56, 0x57, 0x83, 0xf8, 0xff, 0x8b, 0xf9};   /* 0x432080 */
    if (!code_is(CODE(0x4cdcc0), head, sizeof head) || !calls(CODE(0x4cdcca), CODE(0x4a1120)) ||
        !code_is(CODE(0x432080), close, sizeof close))
        return fix_mismatch(FIX_QUIT_DURING_MOVIE);
    branch_to(CODE(0x4cdcca), 0xe8, (void *)quit_unmount, 5);
    return 1;
}

/* ---- GroundBlend (ORIGINAL_BUGS 1.1) and AmplifyGreen (1.3): the port's 16-bit formulas ----------------------- */

/* d towards s by w (0..255, above that as 255): per channel the rounded mean weighted by 255 (the port's
 * fix_blend565, zax-redone reimpl/src/port_fix.cpp) */
WORD __cdecl fix_blend565(DWORD d, DWORD s, DWORD w)
{
    if (w > 255)
        w = 255;
    const DWORD i = 255 - w, ds = d & 0xffff, ss = s & 0xffff;
#define MIX(a, b) (((a) * i + (b) * w + 127) / 255)
    return (WORD)(MIX(ds >> 11, ss >> 11) << 11 | MIX((ds >> 5) & 63, (ss >> 5) & 63) << 5 | MIX(ds & 31, ss & 31));
#undef MIX
}

/* The plasma ground's 16-bit loop (CPlasmaTileMap, 0x541519..0x5415f0): at 0x541541 the second texture's pixel is in
 * cx, the first's in dx, the weight in edi; the cave writes the blend and goes on where the original does after its
 * store, with the registers it reloads there */
void ground_cave(void);
__asm__(".globl _ground_cave\n"
        "_ground_cave:\n"
        "  pushl %edi\n"
        "  pushl %ecx\n"
        "  pushl %edx\n"
        "  call _fix_blend565\n"
        "  addl $12, %esp\n"
        "  movl %eax, %edx\n"
        "  movl 0x1c(%esp), %ebx\n"
        "  movl 0x40(%esp), %ebp\n"
        "  movl 0x2c(%esp), %esi\n"
        "  movl 0x18(%esp), %ecx\n"
        "  movl 0x20(%esp), %eax\n"
        "  movw %dx, (%ecx)\n"
        "  jmp *_g_ret_ground\n");

static int fix_ground_blend(void)
{
    static const BYTE load[] = {0x66, 0x8b, 0x0c, 0x10, 0x8b, 0x54, 0xbe, 0x64, 0x8b, 0xbc, 0xbe, 0x64, 0x0c, 0x00, 0x00,
                                0x66, 0x8b, 0x14, 0x02, 0x8b, 0xc1, 0x8b, 0xda};                                    /* 0x54152e */
    static const BYTE store[] = {0x8b, 0x5c, 0x24, 0x1c, 0x03, 0xcd, 0x8b, 0x6c, 0x24, 0x40};                       /* 0x54159f */
    static const BYTE tail[] = {0x8b, 0x74, 0x24, 0x2c, 0x83, 0xe2, 0x1f, 0x03, 0xc1, 0x8b, 0x4c, 0x24, 0x18, 0x03,
                                0xd0, 0x8b, 0x44, 0x24, 0x20, 0x66, 0x89, 0x11, 0xeb, 0x0b};                        /* 0x5415b4 */
    if (!code_is(CODE(0x54152e), load, sizeof load) || !code_is(CODE(0x54159f), store, sizeof store) ||
        !code_is(CODE(0x5415b4), tail, sizeof tail))
        return fix_mismatch(FIX_GROUND_BLEND);
    g_ret_ground = CODE(0x5415d7);
    branch_to(CODE(0x541541), 0xe9, ground_cave, 8);
    return 1;
}

#define AMPLIFY_RED   0x6b7cf0   /* 32 x 32 words each, built by CBlendAmplifyLight_Ctor 0x433df0 */
#define AMPLIFY_GREEN 0x6b8928
#define AMPLIFY_BLUE  0x6b67f0
static WORD amplify565(DWORD d, DWORD s)
{
    const DWORD b = (d & 0x1f) << 5 | (s & 0x1f), r = ((d & 0xf800) | (s >> 5)) >> 6;
    const DWORD d6 = (d >> 5) & 63, s6 = (s >> 5) & 63, v = d6 + d6 * s6 / 63;
    return (WORD)(((v > 63 ? 63 : v) << 5) | ((const WORD *)AMPLIFY_BLUE)[b] | ((const WORD *)AMPLIFY_RED)[r]);
}
/* The original's value of a pixel (its three tables), for the log below */
static WORD amplify565_original(DWORD d, DWORD s)
{
    const DWORD g = ((d & 0x7c0) | ((s >> 5) & 0x3e)) >> 1, b = (d & 0x1f) << 5 | (s & 0x1f), r = ((d & 0xf800) | (s >> 5)) >> 6;
    return (WORD)(((const WORD *)AMPLIFY_GREEN)[g] | ((const WORD *)AMPLIFY_BLUE)[b] | ((const WORD *)AMPLIFY_RED)[r]);
}
/* The first 20000 pixels drawn are also computed the original's way and counted: how many get another value, and
 * whether any differs outside green (it must not); one log line, the evidence that the game draws through these
 * spans and what the fix changes */
static DWORD g_amplify_pixels, g_amplify_changed, g_amplify_other;
static WORD amplify_pixel(DWORD d, DWORD s)
{
    const WORD v = amplify565(d, s);
    if (g_amplify_pixels < 20000) {
        const WORD o = amplify565_original(d, s);
        g_amplify_changed += v != o;
        g_amplify_other += ((v ^ o) & ~0x7e0) != 0;
        if (++g_amplify_pixels == 20000)
            log_line("Fix AmplifyGreen: of the first %lu Amplify Light pixels %lu got another green than the original's, "
                     "%lu another red or blue", g_amplify_pixels, g_amplify_changed, g_amplify_other);
    }
    return v;
}
/* 0x434280 (dst, src; n) and 0x434310 (dst, value; n), fastcall, ret 4 */
static void __attribute__((fastcall)) amplify_literal(WORD *dst, const WORD *src, DWORD n)
{
    for (DWORD i = 0; i < n; i++)
        dst[i] = amplify_pixel(dst[i], src[i]);
}
static void __attribute__((fastcall)) amplify_fill(WORD *dst, DWORD value, DWORD n)
{
    for (DWORD i = 0; i < n; i++)
        dst[i] = amplify_pixel(dst[i], value & 0xffff);
}

static int fix_amplify_green(void)
{
    static const BYTE literal[] = {0x8b, 0x44, 0x24, 0x04, 0x85, 0xc0, 0x76, 0x79, 0x53, 0x55, 0x56, 0x57, 0x8b, 0xfa};
    static const BYTE fill[] = {0x8b, 0x44, 0x24, 0x04, 0x85, 0xc0, 0x76, 0x74, 0x53, 0x55, 0x56, 0x57, 0x8b, 0xfa};
    static const BYTE tables[] = {0x66, 0x8b, 0x14, 0x5d, 0x28, 0x89, 0x6b, 0x00, 0x0b, 0xc6, 0x83, 0xc1, 0x02, 0x66, 0x0b,
                                  0x14, 0x6d, 0xf0, 0x67, 0x6b, 0x00, 0xc1, 0xe8, 0x06, 0x66, 0x0b, 0x14, 0x45, 0xf0, 0x7c,
                                  0x6b, 0x00};                                                                   /* 0x4342ce */
    if (!code_is(CODE(0x434280), literal, sizeof literal) || !code_is(CODE(0x434310), fill, sizeof fill) ||
        !code_is(CODE(0x4342ce), tables, sizeof tables))
        return fix_mismatch(FIX_AMPLIFY_GREEN);
    branch_to(CODE(0x434280), 0xe9, (void *)amplify_literal, 5);
    branch_to(CODE(0x434310), 0xe9, (void *)amplify_fill, 5);
    return 1;
}

/* ---- The switches -------------------------------------------------------------------------------------------- */

static int (*const FIX_INSTALL[FIXES])(void) = {
    fix_multiplayer_cfg, fix_disable_minimap, fix_sound_channels, fix_play_once_after_load, fix_draw_hud_crash,
    fix_companion_spot, fix_smooth_turning, fix_shield_overflow, fix_pickup_refused, fix_use_items,
    fix_old_patrol_seed, fix_fidget_next, fix_dplay_slot100, fix_aim_centre, fix_no_write_back, fix_quit_during_movie,
    fix_ground_blend, fix_amplify_green,
};

static void install_fixes(const char *cmd)
{
    for (int i = 0; i < FIXES; i++)
        if (has_fix(cmd, FIX[i].name) && FIX_INSTALL[i]())
            log_line("Fix %s enabled", FIX[i].name);
}
