/* Zax-Launcher.dll: starts Zax directly in a map, without the main menu, and gives the game menu
 * (Esc) its "Load Map" button back.
 *
 * Loaded into Zax.exe by Zax-Launcher.exe. Reads from the command line
 *   /Map "Act 1/M1M2/01 Main"   map (path below Levels/ without extension); without /Map normal start
 *   /Spawn "Game Start"         start point; default First Map Spawn Point from Rules.txt
 *   /Difficulty Normal          name of a file in Resources/Dificulty Types; default Normal
 *   /NoLoadMap                  no "Load Map" button in the game menu
 *   /Cheats                     put the exe's cheat routines on Ctrl+F1 ... Ctrl+F9
 *   /DebugLog                   write the exe's section log to Zax-Debug.log
 *   /ShowFPS                    turn on the exe's frame counter (like Show Frame Rate=1 in Zax.CFG)
 *   /NetGraphs                  draw the exe's network traffic graphs in a LAN game (the byte 0x6d00ec,
 *                               which nothing in the exe sets)
 *   /DebugDraw                  the AIs' and parts' debug drawing (trigger shapes, patrol paths, ranges, ...):
 *                               the draw pass calls each part's slot 29 after its slot 30, which the release
 *                               never does (docs/EXE_FINDINGS.md, "Debug drawing of the AIs and parts")
 *   /ShowHidden                 the draw pass also gathers parts without "Visible" (byte 0x6c1cf5)
 *   /DrawOutlines               the draw queue's strip outlines (byte 0x6c1cf9, World_DrawQueueOutlines)
 *   /AltDirtyDraw               the second mode of drawing the dirty tiles (byte 0x6c1cf7; the same picture,
 *                               docs/EXE_FINDINGS.md)
 *   /NoEntryDraw                no queue entry drawn in the dirty tiles (byte 0x6c1cf8)
 * When the main menu is created for the first time (at the end of loading the game data,
 * App_LoadGameData 0x5a01c0 -> World_OpenMainMenu 0x59bab0), the DLL does what a click on a
 * difficulty does (Zax.exe 0x5bfbc1), only without the movie and with the selected map, in the
 * first step of the main loop, before the menu is drawn (the application's step, vtable slot 3,
 * runs before slot 4 draws, App_RunSteps 0x4cdd80). A 300 ms timer stays as a fallback.
 * "Load Map": the game menu's click handler knows the button [+0x13c] including the dialog
 * "Select Map", but the constructor never creates it. The DLL makes up for that.
 * Cheats: the exe still contains the routines but the release build never registers them
 * (docs/EXE_FINDINGS.md). The DLL redirects DispatchMessageA of the main loop and calls the
 * routine for the key directly.
 * DebugLog: the exe reports loading sections ("Layer Loading", "Start New Game", ...) to
 * 0x4a2650/0x4a2670, which would write to a file that is never opened. The DLL replaces both
 * functions and writes itself, unbuffered, so nothing is lost in a crash.
 * All addresses only apply to the Zax.exe described in docs/EXE_FINDINGS.md; if the bytes at the
 * call sites do not match, the DLL changes nothing.
 * Log: Zax-Launcher.log next to Zax.exe. */
#include <windows.h>
#include <stdarg.h>
#include "mini.h"

#define THISCALL __attribute__((thiscall))
#define STR_(x) #x
#define STR(x) STR_(x)

/* Zax.exe (image base 0x400000, no ASLR) */
#define MAINMENU_CTOR   0x5be4a0   /* CMainMenu::CMainMenu(arg), ret 4 */
#define WORLD           0x6c2080   /* [WORLD] = world object */
#define GAME            0x713f14   /* [GAME]: target of difficulty and game mode */
#define DIFF_CATALOG    0x6c2a00   /* catalogue CLevelOfDificulty */
#define DIFF_COUNT      0x6c2a14
#define SET_DIFFICULTY  0x5bb780   /* thiscall [[GAME]+0x34c], (Index), ret 4 */
#define SET_MODE        0x5a3180   /* thiscall [GAME], (char *mode), ret 4 */
#define PREPARE_START   0x51e450   /* thiscall [[WORLD]+0x334], ret */
#define CSTRING_CSTR    0x4e13a0   /* thiscall CString*, returns char* */
#define SET_RETURN_WIN  0x404180   /* thiscall window, (window *), ret 4 */
#define QUEST_MODE      0x6179f0   /* "QuestMode" */
#define VT_START_GAME   0x13c      /* world: StartNewGame(1, map, start point, 0) */
#define VT_CLOSE        0x14       /* close a window (deletes the object) */
#define APP_VT_STEP     0x602dd4   /* application vtable 0x602dc8, slot 3: the step (dt), ret 4 */
#define APP_STEP        0x59d140   /* CZaxWorld::Frame, the slot's original target */

/* game menu "Zax Menu" (constructor 0x5bfc40, click handler 0x5bff00) */
#define MENU_PATCH      0x5bfe14   /* mov ecx,0x138 / call ALLOC: start of "End Game" */
#define MENU_RESUME     0x5bfe19   /* return address behind mov ecx,0x138 */
#define MENU_HANDLER_CMP 0x5bffad  /* cmp eax,[esi+0x13c] -> dialog "Select Map" */
#define MENU_LOAD_MAP   0x13c      /* button the handler knows */
#define ALLOC           0x417710   /* operator new, size in ecx */
#define BUTTON_SIZE     0x138
#define BUTTON_CTOR     0x40fa60   /* thiscall button, (parent, CString *text, 0), ret 0xc */
#define ADD_CONTROL     0x40c7c0   /* thiscall window, (button, placement, 0, x, y, 0), ret 0x18 */
#define LABEL_LOAD_MAP  0x7144d4   /* CString "Load &Map (F9)", created in the exe but unused */
#define VT_LOCAL_PLAYER 0x04       /* world: object for the multiplayer query */
#define VT_IS_CLIENT    0x34       /* true as multiplayer client: then no "Save Game" either */

/* Cheats */
#define IAT_DISPATCH    0x5ea234   /* import entry DispatchMessageA, only call 0x599b62 */
#define GET_PLAYER      0x4f5d80   /* thiscall [GAME], returns the player character or 0 */

/* debug log */
#define SECTION_BEGIN   0x4a2650   /* name in ecx: write "*** name ***", depth + 1 */
#define SECTION_END     0x4a2670   /* depth - 1, then "section done" or "END of all active section" */
#define SECTION_DEPTH   0x6bdf68   /* depth, not used anywhere else by the exe */

/* frame counter: test eax,eax / setne cl / mov [ebp+0x1b],cl after reading "Show Frame Rate" */
#define SHOW_FPS_SETNE  0x4cdbf7
#define NET_GRAPHS_ON   0x6d00ec   /* byte, read only at 0x520153 (NetTrafficGraph_Update 0x520150) */
#define NET_GRAPHS_READ 0x520153
/* The draw pass Layer_DrawPass 0x4e8900: its loop over the parts after drawing, slot 30 of each with the frame
 * (edi) pushed; nothing in the exe writes its debug bytes */
#define DRAW_PASS_LOOP  0x4e8b31   /* mov ecx,[esp+0x58]; push edi; mov edx,[ecx+4]; mov ecx,[edx+esi*4] */
#define DRAW_PASS_SLOT  0x4e8b3c   /* mov eax,[ecx]; call [eax+0x78]: the part's slot 30 (frame), ret 4 */
#define MINI_MAP_RENDER 0x6c2908   /* byte: set while the layer render draws the mini map (0x4f4ae0) */
#define SHOW_HIDDEN     0x6c1cf5
#define ALT_DIRTY_DRAW  0x6c1cf7
#define NO_ENTRY_DRAW   0x6c1cf8
#define DRAW_OUTLINES   0x6c1cf9

/* key with Ctrl, routine (cdecl, no arguments), name in the exe, first bytes for checking */
static const struct {
    UINT key;
    DWORD routine;
    const char *name;
    BYTE head[6];
} CHEATS[] = {
    {VK_F1, 0x5a2500, "giveall",   {0x83, 0xec, 0x08, 0x56, 0x57, 0x8b}},  /* every item */
    {VK_F2, 0x5a22b0, "loadup",    {0x81, 0xec, 0x1c, 0x01, 0x00, 0x00}},  /* weapons, ammo, keys */
    {VK_F3, 0x5a2540, "cash",      {0x83, 0xec, 0x1c, 0xb8, 0xd4, 0xb7}},  /* ore and crystals */
    {VK_F4, 0x5a2630, "GunIt",     {0x83, 0xec, 0x18, 0xb8, 0xd0, 0x51}},  /* Twin Disruptor, battery */
    {VK_F5, 0x5a2670, "chargeit",  {0x83, 0xec, 0x14, 0xb8, 0xd0, 0x51}},  /* 4 x Battery Charge */
    {VK_F6, 0x5a26a0, "mine",      {0x83, 0xec, 0x2c, 0xb8, 0xe0, 0x51}},  /* 10 x Proximity Mine */
    {VK_F7, 0x5a2250, "medic",     {0xa1, 0x14, 0x3f, 0x71, 0x00, 0xc6}},  /* full health */
    {VK_F8, 0x59a600, "fixgun",    {0x8b, 0x0d, 0x14, 0x3f, 0x71, 0x00}},  /* unjam the weapon */
    {VK_F9, 0x5a2590, "generator", {0x56, 0x8b, 0x35, 0x14, 0x3f, 0x71}},  /* Materials Generator */
};

static const DWORD CALL_SITES[] = {0x50eab7, 0x59bace, 0x59d320, 0x5a102d, 0x5ba8ef, 0x5bff30};

typedef void *(THISCALL *ctor_fn)(void *self, void *arg);
typedef void (THISCALL *int_fn)(void *self, int value);
typedef void (THISCALL *ptr_fn)(void *self, const void *value);
typedef void (THISCALL *void_fn)(void *self);
typedef const char *(THISCALL *cstr_fn)(void *self);
typedef const char *(THISCALL *name_fn)(void *self, int index);
typedef void (THISCALL *start_fn)(void *self, int flag, const char *map, const char *spawn, int zero);
typedef void *(__attribute__((fastcall)) *alloc_fn)(DWORD size);
typedef void *(THISCALL *button_fn)(void *self, void *parent, void *label, int zero);
typedef void (THISCALL *add_fn)(void *self, void *control, int place, int zero, int x, int y, int zero2);
typedef void *(THISCALL *get_fn)(void *self);
typedef void (THISCALL *step_fn)(void *self, float dt);
typedef char (THISCALL *bool_fn)(void *self);

static char g_map[MAX_PATH], g_spawn[MAX_PATH], g_difficulty[64] = "Normal";
static void *g_menu;
static int g_done, g_started;
static UINT_PTR g_timer;   /* the fallback timer, killed once the first step started the map */
static HANDLE g_log = INVALID_HANDLE_VALUE;

static void log_line(const char *fmt, ...)
{
    if (g_log == INVALID_HANDLE_VALUE)
        return;
    char buf[1100];
    va_list ap;
    va_start(ap, fmt);
    int n = wvsprintfA(buf, fmt, ap);   /* at most 1024 characters */
    va_end(ap);
    buf[n++] = '\r', buf[n++] = '\n';
    DWORD written;
    WriteFile(g_log, buf, (DWORD)n, &written, NULL);
}

static DWORD rd(DWORD addr) { return *(DWORD *)addr; }

/* Value after /Name on the command line, also in quotes. */
static int arg_value(const char *cmd, const char *name, char *out, int size)
{
    int n = lstrlenA(name);
    for (const char *p = cmd; *p; p++) {
        if (*p != '/' || str_nieq(p + 1, name, n) != 0 || (p[1 + n] != ' ' && p[1 + n] != '\t'))
            continue;
        const char *v = p + 1 + n;
        while (*v == ' ' || *v == '\t')
            v++;
        char end = ' ';
        if (*v == '"')
            end = *v++;
        int i = 0;
        while (v[i] && v[i] != end && i + 1 < size)
            out[i] = v[i], i++;
        out[i] = 0;
        return i > 0;
    }
    return 0;
}

static int find_difficulty(void)
{
    void *catalog = (void *)DIFF_CATALOG;
    name_fn name_of = (name_fn)(*(DWORD **)catalog)[1];
    int count = (int)rd(DIFF_COUNT), found = -1;
    int n = lstrlenA(g_difficulty);
    for (int i = 0; i < count; i++) {
        const char *name = name_of(catalog, i);
        int bad = name == NULL || IsBadStringPtrA(name, 256);
        log_line("Difficulty %d: %s", i, bad ? "(unreadable)" : name);
        if (bad)
            continue;
        const char *base = name;  /* the name comes with its folder: "Dificulty Types/Normal" */
        for (const char *p = name; *p; p++)
            if (*p == '/' || *p == '\\')
                base = p + 1;
        if (found < 0 && str_nieq(base, g_difficulty, n) == 0 && (base[n] == 0 || base[n] == '.'))
            found = i;
    }
    return found;
}

static void start_map(void)
{
    if (g_started)
        return;
    g_started = 1;
    void *world = (void *)rd(WORLD), *game = (void *)rd(GAME);
    log_line("World %08lX, game %08lX, menu %08lX", (DWORD)world, (DWORD)game, (DWORD)g_menu);
    if (!world || !game) {
        log_line("Aborted: game not ready");
        return;
    }
    int diff = find_difficulty();
    if (diff >= 0)
        ((int_fn)SET_DIFFICULTY)((void *)rd((DWORD)game + 0x34c), diff);
    else
        log_line("Difficulty \"%s\" not found, keeping the game default", g_difficulty);
    ((ptr_fn)SET_MODE)(game, (const char *)QUEST_MODE);
    ((void_fn)PREPARE_START)((void *)rd((DWORD)world + 0x334));

    const char *spawn = g_spawn;
    if (!*spawn) {  /* like StartNewGame without a map: First Map Spawn Point from CRules */
        DWORD rules = rd((DWORD)world + 0x2e0);
        spawn = ((cstr_fn)CSTRING_CSTR)((void *)(rules + 0xc));
    }
    log_line("StartNewGame(1, \"%s\", \"%s\", 0)", g_map, spawn);
    start_fn start = (start_fn)(*(DWORD **)world)[VT_START_GAME / 4];
    start(world, 1, g_map, spawn, 0);

    /* like the difficulty dialog: clear the return target, close the window */
    ((ptr_fn)SET_RETURN_WIN)(g_menu, NULL);
    ((void_fn)(*(DWORD **)g_menu)[VT_CLOSE / 4])(g_menu);
    log_line("done");
}

static void CALLBACK on_timer(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
    (void)hwnd, (void)msg, (void)time;
    KillTimer(NULL, id);
    g_timer = 0;
    if (!g_started)
        log_line("Starting map from the timer (the first step did not come)");
    start_map();
}

static void set_step_slot(DWORD target)
{
    DWORD old;
    VirtualProtect((void *)APP_VT_STEP, 4, PAGE_READWRITE, &old);
    *(DWORD *)APP_VT_STEP = target;
    VirtualProtect((void *)APP_VT_STEP, 4, old, &old);
}

/* The application's first step after the main menu: put the slot back, start the map, then the step. */
static void THISCALL first_step(void *self, float dt)
{
    set_step_slot(APP_STEP);
    if (g_timer)
        KillTimer(NULL, g_timer), g_timer = 0;
    log_line("First step, starting map");
    start_map();
    ((step_fn)APP_STEP)(self, dt);
}

typedef LRESULT (WINAPI *dispatch_fn)(const MSG *msg);
static dispatch_fn g_dispatch;

static void run_cheat(int i)
{
    void *game = (void *)rd(GAME);
    if (!game || !((get_fn)GET_PLAYER)(game)) {
        log_line("Cheat %s: no player, ignored", CHEATS[i].name);
        return;
    }
    log_line("Cheat %s", CHEATS[i].name);
    ((void (*)(void))CHEATS[i].routine)();
}

/* Replacement for DispatchMessageA: catch Ctrl+F keys, pass everything else on as usual. */
static LRESULT WINAPI dispatch_hook(const MSG *msg)
{
    if ((msg->message == WM_KEYDOWN || msg->message == WM_KEYUP) && (GetKeyState(VK_CONTROL) & 0x8000))
        for (int i = 0; i < (int)(sizeof CHEATS / sizeof *CHEATS); i++)
            if (msg->wParam == CHEATS[i].key) {
                /* only on the first press, not on key repeat */
                if (msg->message == WM_KEYDOWN && !(msg->lParam & 0x40000000))
                    run_cheat(i);
                return 0;  /* the game does not see the key (F9 would otherwise be "Load Map") */
            }
    return g_dispatch(msg);
}

static void install_cheats(void)
{
    for (size_t i = 0; i < sizeof CHEATS / sizeof *CHEATS; i++)
        for (int k = 0; k < 6; k++)
            if (((BYTE *)CHEATS[i].routine)[k] != CHEATS[i].head[k]) {
                log_line("Cheat %s does not match, different Zax.exe? Cheats stay off.", CHEATS[i].name);
                return;
            }
    DWORD *slot = (DWORD *)IAT_DISPATCH;
    FARPROC real = GetProcAddress(GetModuleHandleA("user32.dll"), "DispatchMessageA");
    if (!real || *slot != (DWORD)real) {
        log_line("DispatchMessageA import does not match, different Zax.exe? Cheats stay off.");
        return;
    }
    g_dispatch = (dispatch_fn)real;
    DWORD old;
    VirtualProtect(slot, 4, PAGE_READWRITE, &old);
    *slot = (DWORD)dispatch_hook;
    VirtualProtect(slot, 4, old, &old);
    log_line("Cheats enabled: Ctrl+F1 giveall, F2 loadup, F3 cash, F4 GunIt, F5 chargeit, "
             "F6 mine, F7 medic, F8 fixgun, F9 generator");
}

static HANDLE g_debug = INVALID_HANDLE_VALUE;
static DWORD g_debug_start;

/* line as in the exe ("*** %s ***"), preceded by the time since start in ms and indentation per depth */
static void debug_line(const char *text, int depth)
{
    char buf[1100];
    int n = wsprintfA(buf, "%7lu ", GetTickCount() - g_debug_start);
    for (int i = 0; i < depth && i < 20; i++)
        buf[n++] = ' ', buf[n++] = ' ';
    n += wsprintfA(buf + n, "*** %.1000s ***\r\n", text ? text : "(null)");
    DWORD written;
    WriteFile(g_debug, buf, (DWORD)n, &written, NULL);
}

static void __attribute__((fastcall)) section_begin(const char *name)
{
    int *depth = (int *)SECTION_DEPTH;
    debug_line(name, *depth);
    ++*depth;
}

static void section_end(void)
{
    int *depth = (int *)SECTION_DEPTH;
    --*depth;
    debug_line(*depth ? "section done" : "END of all active section", *depth);
}

static void jump_to(DWORD site, void *target)
{
    DWORD old;
    VirtualProtect((void *)site, 5, PAGE_EXECUTE_READWRITE, &old);
    *(BYTE *)site = 0xe9;
    *(INT32 *)(site + 1) = (INT32)((BYTE *)target - (BYTE *)(site + 5));
    VirtualProtect((void *)site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)site, 5);
}

static void install_debug_log(void)
{
    static const BYTE begin[] = {0xa1, 0x48, 0xdf, 0x6b, 0x00, 0x85};
    static const BYTE end[] = {0xa1, 0x68, 0xdf, 0x6b, 0x00, 0x48};
    for (int i = 0; i < 6; i++)
        if (((BYTE *)SECTION_BEGIN)[i] != begin[i] || ((BYTE *)SECTION_END)[i] != end[i]) {
            log_line("Debug log functions do not match, different Zax.exe? Debug log stays off.");
            return;
        }
    char path[MAX_PATH + 16];
    exe_dir(path);
    lstrcatA(path, "Zax-Debug.log");
    g_debug = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_debug == INVALID_HANDLE_VALUE) {
        log_line("Cannot create %s, debug log stays off.", path);
        return;
    }
    g_debug_start = GetTickCount();
    jump_to(SECTION_BEGIN, (void *)section_begin);
    jump_to(SECTION_END, (void *)section_end);
    log_line("Debug log enabled: %s", path);
}

/* setne cl -> mov cl,1: the frame counter is on, whatever Zax.CFG says */
static void install_show_fps(void)
{
    static const BYTE expect[] = {0x85, 0xc0, 0x0f, 0x95, 0xc1, 0x88, 0x4d, 0x1b};
    static const BYTE patch[] = {0xb1, 0x01, 0x90};
    BYTE *site = (BYTE *)SHOW_FPS_SETNE;
    for (int i = 0; i < 8; i++)
        if (site[i - 2] != expect[i]) {
            log_line("Frame rate switch does not match, different Zax.exe? Frame counter stays as in Zax.CFG.");
            return;
        }
    DWORD old;
    VirtualProtect(site, 3, PAGE_EXECUTE_READWRITE, &old);
    for (int i = 0; i < 3; i++)
        site[i] = patch[i];
    VirtualProtect(site, 3, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 3);
    log_line("Frame counter enabled");
}

/* The traffic graphs: NetTrafficGraph_Update draws one graph per player (and "All Non-Local Traffic" with
 * more than two) at the bottom right while the byte is set; its only reference in the exe is the read
 * mov al,[0x6d00ec] at 0x520153 (docs/DECOMPILATION.md 13, "Batch gaps-ui") */
static void install_net_graphs(void)
{
    static const BYTE expect[] = {0xa0, 0xec, 0x00, 0x6d, 0x00};
    for (int i = 0; i < 5; i++)
        if (((BYTE *)NET_GRAPHS_READ)[i] != expect[i]) {
            log_line("Traffic graph switch does not match, different Zax.exe? The graphs stay off.");
            return;
        }
    *(BYTE *)NET_GRAPHS_ON = 1;   /* uninitialised .data: writable, 0 at start */
    log_line("Network traffic graphs enabled");
}

/* The draw pass is the one of the exe described in docs/EXE_FINDINGS.md (its loop over the parts and the slot 30
 * call); the draw switches change nothing otherwise. Tested once, before /DebugDraw puts its call into these bytes
 * (until 2026-11-01 every switch tested them again, so /ShowHidden and /DrawOutlines after /DebugDraw stayed off) */
static int draw_pass_matches(void)
{
    static int matches = -1;
    static const BYTE expect[] = {0x8b, 0x4c, 0x24, 0x58, 0x57, 0x8b, 0x51, 0x04, 0x8b, 0x0c, 0xb2,
                                  0x8b, 0x01, 0xff, 0x50, 0x78};
    if (matches < 0) {
        matches = 1;
        for (int i = 0; i < (int)sizeof expect; i++)
            if (((BYTE *)DRAW_PASS_LOOP)[i] != expect[i])
                matches = 0;
    }
    return matches;
}

/* The debug drawing of one part after its slot 30: its slot 29 (frame), which is the empty 0x4186e0 for most parts;
 * CEntityBase 0x488560 draws the AIs of its model's "Activity" (AI_Draw 0x4fd0b0 -> each AI's slot 41),
 * CEntityAnimated 0x48f490 adds its behaviour's, CFreeRangePoly and CRenderablePolygon draw only with part flag
 * 0x400000. Then AI_Draw for each AI of the part itself (+0x3c data, +0x40 count): the triggers', spawn points',
 * patrols' and ambient sounds' AIs sit there, and no slot 29 draws them (the developers' loop over them is not in
 * the release; docs/EXE_FINDINGS.md, "Debug drawing of the AIs and parts") */
#define AI_DRAW         0x4fd0b0   /* thiscall AI, (part, frame), ret 8 */
#define PART_AIS        0x3c       /* the part's AI array: data, count at +0x40 */
typedef void (THISCALL *slot_fn)(void *self, void *frame);
typedef void (THISCALL *ai_draw_fn)(void *ai, void *part, void *frame);
void __cdecl debug_draw_part(void *part, void *frame)
{
    ((slot_fn)(*(DWORD **)part)[29])(part, frame);
    void **ais = *(void ***)((BYTE *)part + PART_AIS);
    DWORD n = *(DWORD *)((BYTE *)part + PART_AIS + 4);
    for (DWORD i = 0; ais && i < n; i++)
        if (ais[i])
            ((ai_draw_fn)AI_DRAW)(ais[i], part, frame);
}

/* At DRAW_PASS_SLOT, instead of the slot 30 call: slot 30 (frame) as before, then debug_draw_part unless the mini
 * map is being drawn; the frame is the argument the loop pushed, popped as slot 30 would (ret 4) */
void debug_draw_thunk(void);
__asm__(".globl _debug_draw_thunk\n"
        "_debug_draw_thunk:\n"
        "  pushl %ecx\n"
        "  pushl 8(%esp)\n"
        "  movl (%ecx), %eax\n"
        "  call *0x78(%eax)\n"
        "  popl %ecx\n"
        "  cmpb $0, " STR(MINI_MAP_RENDER) "\n"
        "  jne 1f\n"
        "  pushl 4(%esp)\n"
        "  pushl %ecx\n"
        "  call _debug_draw_part\n"
        "  addl $8, %esp\n"
        "1:\n"
        "  ret $4\n");

static void install_debug_draw(void)
{
    if (!draw_pass_matches()) {
        log_line("Draw pass does not match, different Zax.exe? Debug drawing stays off.");
        return;
    }
    BYTE *site = (BYTE *)DRAW_PASS_SLOT;
    DWORD old;
    VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
    site[0] = 0xe8;
    *(INT32 *)(site + 1) = (INT32)((BYTE *)debug_draw_thunk - (site + 5));
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    log_line("Debug drawing of the AIs and parts enabled");
}

/* One of the draw pass' debug bytes in .bss (0 at start, written nowhere by the exe) */
static void set_draw_byte(DWORD address, const char *what)
{
    if (!draw_pass_matches()) {
        log_line("Draw pass does not match, different Zax.exe? %s stays off.", what);
        return;
    }
    *(BYTE *)address = 1;
    log_line("%s enabled", what);
}

/* Replacement for the main menu constructor at all call sites. */
static void *THISCALL main_menu_ctor(void *self, void *arg)
{
    void *menu = ((ctor_fn)MAINMENU_CTOR)(self, arg);
    if (!g_done && menu) {
        g_done = 1;
        g_menu = menu;
        /* only run once the main loop is running: in its first step, before anything is drawn */
        if (rd(APP_VT_STEP) == APP_STEP)
            set_step_slot((DWORD)first_step);
        else
            log_line("Step slot %08lX does not match, starting map from the timer", rd(APP_VT_STEP));
        g_timer = SetTimer(NULL, 0, 300, on_timer);
        log_line("Main menu created, starting map in the first step");
    }
    return menu;
}

/* Called in the middle of the game menu constructor, before "End Game" (like "Save Game"). */
void __cdecl add_load_map(void *menu)
{
    void *world = (void *)rd(WORLD);
    void *obj = ((get_fn)(*(DWORD **)world)[VT_LOCAL_PLAYER / 4])(world);
    if (((bool_fn)(*(DWORD **)obj)[VT_IS_CLIENT / 4])(obj))
        return;
    void *button = ((alloc_fn)ALLOC)(BUTTON_SIZE);
    if (button)
        button = ((button_fn)BUTTON_CTOR)(button, menu, (void *)LABEL_LOAD_MAP, 0);
    *(void **)((BYTE *)menu + MENU_LOAD_MAP) = button;
    log_line("Game menu: button \"%s\" created", ((cstr_fn)CSTRING_CSTR)((void *)LABEL_LOAD_MAP));
    ((add_fn)ADD_CONTROL)(menu, button, 8, 0, -1000, -1000, 0);
}

/* Jump target at MENU_PATCH: esi = menu; afterwards execute the overwritten instruction. */
void menu_hook(void);
__asm__(".globl _menu_hook\n"
        "_menu_hook:\n"
        "  pushal\n"
        "  pushl %esi\n"
        "  call _add_load_map\n"
        "  addl $4, %esp\n"
        "  popal\n"
        "  movl $" STR(BUTTON_SIZE) ", %ecx\n"
        "  pushl $" STR(MENU_RESUME) "\n"
        "  ret\n");

static void install_load_map(void)
{
    static const BYTE expect[] = {0xb9, 0x38, 0x01, 0x00, 0x00, 0xe8};
    static const BYTE handler[] = {0x3b, 0x86, 0x3c, 0x01, 0x00, 0x00};
    BYTE *site = (BYTE *)MENU_PATCH;
    for (int i = 0; i < 6; i++)
        if (site[i] != expect[i] || ((BYTE *)MENU_HANDLER_CMP)[i] != handler[i]) {
            log_line("Game menu does not match, different Zax.exe? Load Map stays off.");
            return;
        }
    if ((DWORD)(site + 10) + *(INT32 *)(site + 6) != ALLOC) {
        log_line("Game menu does not match, different Zax.exe? Load Map stays off.");
        return;
    }
    DWORD old;
    VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
    site[0] = 0xe9;
    *(INT32 *)(site + 1) = (INT32)((BYTE *)menu_hook - (site + 5));
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    log_line("Game menu: Load Map button enabled");
}

static void install(void)
{
    BYTE *hook = (BYTE *)main_menu_ctor;
    for (size_t i = 0; i < sizeof CALL_SITES / sizeof *CALL_SITES; i++) {
        BYTE *site = (BYTE *)CALL_SITES[i];
        if (site[0] != 0xe8 || (DWORD)(site + 5) + *(INT32 *)(site + 1) != MAINMENU_CTOR) {
            log_line("Call site %08lX does not match, different Zax.exe? No changes.", (DWORD)site);
            return;
        }
    }
    for (size_t i = 0; i < sizeof CALL_SITES / sizeof *CALL_SITES; i++) {
        BYTE *site = (BYTE *)CALL_SITES[i];
        DWORD old;
        VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
        *(INT32 *)(site + 1) = (INT32)(hook - (site + 5));
        VirtualProtect(site, 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), site, 5);
    }
    log_line("Main menu redirected (%u call sites)", (unsigned)(sizeof CALL_SITES / sizeof *CALL_SITES));
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;
    DisableThreadLibraryCalls(inst);
    char path[MAX_PATH + 16];
    exe_dir(path);
    lstrcatA(path, "Zax-Launcher.log");
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    const char *cmd = GetCommandLineA();
    log_line("Command line: %s", cmd);
    if (has_switch(cmd, "/NoLoadMap"))
        log_line("Game menu: Load Map button disabled (/NoLoadMap)");
    else
        install_load_map();
    if (has_switch(cmd, "/Cheats"))
        install_cheats();
    if (has_switch(cmd, "/DebugLog"))
        install_debug_log();
    if (has_switch(cmd, "/ShowFPS"))
        install_show_fps();
    if (has_switch(cmd, "/NetGraphs"))
        install_net_graphs();
    if (has_switch(cmd, "/DebugDraw"))
        install_debug_draw();
    if (has_switch(cmd, "/ShowHidden"))
        set_draw_byte(SHOW_HIDDEN, "Hidden parts");
    if (has_switch(cmd, "/DrawOutlines"))
        set_draw_byte(DRAW_OUTLINES, "Draw queue outlines");
    if (has_switch(cmd, "/AltDirtyDraw"))
        set_draw_byte(ALT_DIRTY_DRAW, "Alternative dirty tile drawing");
    if (has_switch(cmd, "/NoEntryDraw"))
        set_draw_byte(NO_ENTRY_DRAW, "No queue entries in the dirty tiles");
    if (!arg_value(cmd, "Map", g_map, sizeof g_map)) {
        log_line("No /Map given, game starts normally");
        return TRUE;
    }
    arg_value(cmd, "Spawn", g_spawn, sizeof g_spawn);
    arg_value(cmd, "Difficulty", g_difficulty, sizeof g_difficulty);
    for (char *p = g_map; *p; p++)  /* as in Rules.txt: paths with / */
        if (*p == '\\')
            *p = '/';
    log_line("Map \"%s\", spawn point \"%s\", difficulty \"%s\"",
             g_map, g_spawn[0] ? g_spawn : "(from Rules.txt)", g_difficulty);
    install();
    return TRUE;
}
