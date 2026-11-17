/* Zax-Launcher.exe: starts Zax.exe from the same folder with Zax-Launcher.dll.
 *
 *   Zax-Launcher.exe [/Map "Act 1/M1M2/01 Main" [/Spawn "Game Start"] [/Difficulty Normal]]
 *                 [/LoadMap] [/SkipMovies] [/Cheats] [/DebugLog] [/ShowFPS] [/NetGraphs] [/DebugDraw] [/ShowHidden]
 *                 [/DrawOutlines] [/AltDirtyDraw] [/NoEntryDraw] [-host | -connect <address>] [-name <player>]
 *                 [/NoPack Name.RED] [/Fix Name ...] [/LoadDll Name.dll]
 *
 * Without arguments a start window appears: choose map (from Data\Levels and from Data.dat), start
 * point, difficulty, resolution (only 640 x 480 or 800 x 600, 8 or 16 bit), switches, a LAN game
 * (the exe's own -host / -connect / -name), the packs (*.RED) to load and the fixes (/Fix); the selection is
 * remembered in Zax-Launcher.ini next to the exe.
 * /? or /help shows the help in a scrollable window; in the start window every control has a tooltip.
 * All arguments are passed to Zax.exe unchanged (e.g. /ScreenWidth 800, /SkipMovies), nothing is added: without
 * switches the game starts as without the launcher (movies, main menu, no "Load Map" button).
 * The launcher loads no DLL into Zax.exe but its own, whatever lies in the folder; only /LoadDll Name.dll (a file
 * next to Zax.exe, for developers) loads another one after it, and that switch is the only argument not passed on.
 * The edition of Zax.exe (edition.h) is shown in the start window's title; a copy-protected Zax.exe (SecuROM)
 * is not supported and not started.
 * Zax.exe itself is not modified. */
#include <windows.h>
#include <commctrl.h>
#include "mini.h"
#include "zipdir.h"
#include "edition.h"
#include "fixes.h"

static int fail(const char *what)
{
    char msg[600];
    wsprintfA(msg, "%s (error %lu)", what, GetLastError());
    MessageBoxA(NULL, msg, "Zax-Launcher", MB_ICONERROR);
    return 1;
}

/* Help for the help window (English like the whole start window) */
static const char HELP[] =
    "Zax-Launcher starts Zax.exe from the same folder and adds\n"
    "- starting directly into a map (no main menu, no intro),\n"
    "- the \"Load Map\" button in the game menu (Esc), which loads any map under Levels,\n"
    "- the developer cheats and switches that are still in Zax.exe,\n"
    "- leaving packs (*.RED) out of a start without touching the files,\n"
    "- fixes of bugs of the original game (/Fix).\n"
    "Each is off unless its switch is given; without switches the game starts as\n"
    "usual (movies, main menu). Zax.exe itself is not modified.\n\n"
    "Without arguments a launcher window opens. With arguments the game starts\n"
    "right away:\n"
    "  Zax-Launcher.exe [/Map \"map\"] [/Spawn \"spawn point\"] [/Difficulty level]\n\n"
    "/Map \"Act 1/M1M2/01 Main\"\n"
    "    Map to start in: path under Data\\Levels without .zax (/ or \\).\n"
    "    The map may also be inside Data.dat; a file under Data\\Levels wins.\n"
    "    Without /Map the game starts normally with the main menu.\n"
    "/Spawn \"Game Start\"\n"
    "    Spawn point in the map (name of a Spawn Point). Default is\n"
    "    First Map Spawn Point from Rules.txt, which the map must then contain.\n"
    "/Difficulty Normal\n"
    "    Difficulty: Easy, Normal or Hard. Default Normal.\n"
    "/LoadMap\n"
    "    the \"Load Map\" button in the game menu (Esc).\n"
    "/SkipMovies\n"
    "    Zax.exe's own switch: no movies at the start.\n"
    "/Cheats\n"
    "    enables the cheats in the game (hold Ctrl):\n"
    "    F1 giveall: every item     F2 loadup: weapons, ammo, keys\n"
    "    F3 cash: ore and crystals  F4 GunIt: Twin Disruptor and battery\n"
    "    F5 chargeit: 4 battery charges  F6 mine: 10 proximity mines\n"
    "    F7 medic: full health      F8 fixgun: unjam the weapon\n"
    "    F9 generator: opens the Materials Generator\n"
    "/DebugLog\n"
    "    writes the section log of Zax.exe (\"Layer Loading\", \"Start New Game\",\n"
    "    ...) to Zax-Debug.log next to Zax.exe, with milliseconds since start.\n"
    "    The last open section shows where a crash happened.\n"
    "/ShowFPS\n"
    "    shows the frame counter of Zax.exe at the top of the screen\n"
    "    (same as Show Frame Rate=1 under [Options] in Zax.CFG).\n"
    "/NetGraphs\n"
    "    in a LAN game: draws the network traffic graphs that are still in\n"
    "    Zax.exe but never switched on, at the bottom right: per player the\n"
    "    bytes sent (yellow) and received (green) with \"Max, Cur\", with more\n"
    "    than two players also \"All Non-Local Traffic\".\n"
    "/DebugDraw\n"
    "    the developers' debug drawing that is still in Zax.exe but never\n"
    "    called: trigger areas, patrol paths, turret ranges, the radii of\n"
    "    ambient sounds, particle start shapes, the AIs' target lines.\n"
    "/ShowHidden\n"
    "    with /DebugDraw: its debug shapes also for the parts that are not\n"
    "    visible in the game (e.g. a hidden trigger's area). The hidden parts\n"
    "    themselves stay invisible: the game gathers them but never draws them.\n"
    "    Without /DebugDraw it is accepted but changes nothing on screen, so\n"
    "    the start window offers it only while \"Debug drawing\" is ticked.\n"
    "/DrawOutlines\n"
    "    outlines of the draw order's strips (red rectangles, light lines).\n"
    "/AltDirtyDraw\n"
    "    a leftover second way of redrawing the changed tiles: the moving\n"
    "    objects are drawn whole instead of tile run by tile run. Technical;\n"
    "    the picture stays the same.\n"
    "/NoEntryDraw\n"
    "    a leftover mode of redrawing the changed tiles without the objects\n"
    "    on them (technical; objects may vanish).\n"
    "-host\n"
    "    LAN game (Zax.exe's own switch): opens the host options window,\n"
    "    where you pick the map and start the game. Multiplayer always runs\n"
    "    at 640 x 480.\n"
    "-connect 198.51.100.10\n"
    "    LAN game (Zax.exe's own switch): joins the game of that host.\n"
    "-name \"Player\"\n"
    "    the player name for -host or -connect (otherwise the one of the\n"
    "    multiplayer options).\n"
    "/NoPack Name.RED\n"
    "    leaves that pack (a *.RED next to Zax.exe) out of this start; repeatable.\n"
    "    Zax.exe loads every *.RED next to it after Data.dat, in name order, and a\n"
    "    later one replaces the same files of an earlier one. The file itself is\n"
    "    not renamed, moved or changed. The Packs page of the launcher window\n"
    "    lists them with a check box each.\n"
    "/Fix Name\n"
    "    a fix of a bug of the original game, made in the running Zax.exe\n"
    "    (repeatable; /Fix:Name is the same). Off unless given; the start\n"
    "    window's Fixes page ticks DPlaySlot100 by default. The names:\n"
    "    MultiplayerCfg     LAN games keep the resolution in Zax.CFG\n"
    "    DisableMiniMap     obey levels' \"Disable Mini-Map\" field\n"
    "    SoundChannels      remember the sound channel choice\n"
    "    PlayOnceAfterLoad  saved animations resume at their speed\n"
    "    DrawHudCrash       no crash at the HUD's direction pointer (1308)\n"
    "    CompanionSpot      companions keep their computed spot\n"
    "    SmoothTurning      obey \"Smooth Turning\" off for chasers\n"
    "    ShieldOverflow     shield lets excess damage through (harder)\n"
    "    PickupRefused      refused pickups stay on the ground (mods)\n"
    "    UseItems           using several items uses different ones (mods)\n"
    "    OldPatrolSeed      old patrol AI does not reset randomness\n"
    "    FidgetNext         fidget animation order 5 advances (mods)\n"
    "    DPlaySlot100       LAN: a new player's slot stays in range\n"
    "    AimCentre          aiming centre follows the game's size\n"
    "    NoWriteBack        do not rewrite definition files on load\n"
    "    QuitDuringMovie    no crash when closing during a movie\n"
    "    GroundBlend        smoother 16-bit plasma ground\n"
    "    AmplifyGreen       exact green in Amplify Light (16-bit)\n"
    "    MenuBackground     menu background follows any new size\n"
    "    OggToWav           missing .ogg music falls back to .wav (mods)\n"
    "    GoToAICross        debug drawing: GoTo AI's target cross red\n"
    "    ListenerCentre     3D sound heard from the view's centre\n"
    "    Hardening          all of the following (modded or broken data):\n"
    "    AmplifyEdges       Amplify Light soft edges\n"
    "    GlyphRange         characters past a font's end\n"
    "    BeamParts          light beams on odd parts\n"
    "    ItemData           projectiles without their data\n"
    "    EmptyLists         item lists without records\n"
    "    RleGetColor        colour reads of RLE frames\n"
    "    OffsetGetColor     offset frame colour reads\n"
    "    Some change how the game plays (CompanionSpot, SmoothTurning,\n"
    "    ShieldOverflow, OldPatrolSeed); the tooltips of the Fixes page and\n"
    "    docs/FIXES.md say what each does.\n"
    "/LoadDll Name.dll\n"
    "    for developers: also loads that DLL from the folder of Zax.exe into\n"
    "    Zax.exe, after Zax-Launcher.dll (repeatable, at most 4). Without it the\n"
    "    launcher loads no other DLL, whatever lies in the folder. The switch\n"
    "    and its value are not passed to Zax.exe.\n"
    "/? or /help\n"
    "    shows this help.\n\n"
    "All other arguments are passed to Zax.exe unchanged, e.g.\n"
    "/ScreenWidth 800 /ScreenHeight 600 /ScreenBitDepth 16. The game only runs at\n"
    "640 x 480 or 800 x 600 with 8 bit (256 colors) or 16 bit; at 24 or 32 bit it stops\n"
    "before the level with a hidden error box (the game data has no 24/32-bit images).\n\n"
    "Examples:\n"
    "  Zax-Launcher.exe /Map \"Eigene/Level 1\"\n"
    "  Zax-Launcher.exe /Map \"Act 1/M1M2/01 Main\" /Spawn \"Game Start\" /Difficulty Hard\n"
    "  Zax-Launcher.exe -connect 198.51.100.10 -name Player /NetGraphs\n"
    "  Zax-Launcher.exe /NoPack GermanFix.RED\n"
    "  Zax-Launcher.exe /Fix QuitDuringMovie /Fix ShieldOverflow\n\n"
    "Shortcut: right-click Zax-Launcher.exe, Create shortcut, then append the\n"
    "arguments to \"Target\" in its Properties. The launcher window shows the\n"
    "matching command line for the current selection.\n"
    "Zax-Launcher.exe and Zax-Launcher.dll belong in the folder of Zax.exe.\n"
    "Supported: Zax.exe build 1308 (Zax 1.0) and 1309 (Zax 1.1) without copy\n"
    "protection; with another Zax.exe the switches stay off.\n"
    "Log: Zax-Launcher.log next to Zax.exe.";

/* Command line without the own program name */
static const char *own_args(void)
{
    const char *p = GetCommandLineA();
    if (*p == '"') {
        for (p++; *p && *p != '"'; p++)
            ;
        if (*p)
            p++;
    } else {
        while (*p && *p != ' ' && *p != '\t')
            p++;
    }
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

/* Load a DLL with LoadLibraryA in a thread of the (still suspended) process */
static int inject(HANDLE process, const char *dll)
{
    DWORD len = (DWORD)lstrlenA(dll) + 1;
    void *remote = VirtualAllocEx(process, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    LPTHREAD_START_ROUTINE load =
        (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    HANDLE th = NULL;
    if (remote && WriteProcessMemory(process, remote, dll, len, NULL))
        th = CreateRemoteThread(process, NULL, 0, load, remote, 0, NULL);
    DWORD loaded = 0;
    if (th) {
        WaitForSingleObject(th, 15000);
        GetExitCodeThread(th, &loaded);
        CloseHandle(th);
    }
    if (remote)
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    return loaded != 0;
}

/* /LoadDll Name.dll: the DLLs a developer asks for (plain file names next to Zax.exe, ending in .dll) into names,
 * and args without the switches and their values (and the blanks before them) into rest, so Zax.exe gets the same
 * command line as without them. The number of DLLs, -1 with a message for a bad value. */
enum { LOAD_DLL_MAX = 4 };

static int take_load_dlls(const char *args, char *rest, int rest_size, char names[LOAD_DLL_MAX][MAX_PATH])
{
    int count = 0, len = 0;
    const char *from = args, *prev_end = NULL, *e;
    rest[0] = 0;
    for (const char *t = next_token(args, &e); t; t = next_token(e, &e)) {
        if (!token_is(t, e, "/LoadDll")) {
            prev_end = e;
            continue;
        }
        const char *v = next_token(e, &e);
        char name[MAX_PATH];
        int n = 0, bad = !v;
        for (; v && v < e; v++) {
            if (*v == '"')
                continue;
            if (*v == '\\' || *v == '/' || *v == ':' || n + 1 >= (int)sizeof name)
                bad = 1;
            else
                name[n++] = *v;
        }
        name[n] = 0;
        if (bad || n < 5 || str_nieq(name + n - 4, ".dll", 4) != 0 || token_is(name, name + n, "Zax-Launcher.dll") ||
            count == LOAD_DLL_MAX) {
            SetLastError(0);
            fail("/LoadDll needs the name of a DLL next to Zax.exe, e.g. /LoadDll Name.dll (at most 4)");
            return -1;
        }
        lstrcpyA(names[count++], name);
        /* cut from the end of the token before (or, at the start, up to the next token) to the end of the value */
        const char *cut = prev_end ? prev_end : t;
        if (from < cut) {
            if (len + (cut - from) >= rest_size)
                return SetLastError(0), fail("Command line too long"), -1;
            memcpy(rest + len, from, (size_t)(cut - from));
            len += (int)(cut - from);
        }
        from = e;
        if (!prev_end)
            while (*from == ' ' || *from == '\t')
                from++;
    }
    if (len + lstrlenA(from) >= rest_size)
        return SetLastError(0), fail("Command line too long"), -1;
    lstrcpyA(rest + len, from);
    return count;
}

static int launch(const char *all_args)
{
    static char dir[MAX_PATH], exe[MAX_PATH + 16], dll[MAX_PATH + 16], cmd[8192], args[8192];
    static char extra[LOAD_DLL_MAX][MAX_PATH];
    const int extras = take_load_dlls(all_args, args, sizeof args, extra);
    if (extras < 0)
        return 1;
    exe_dir(dir);
    lstrcpyA(exe, dir);
    lstrcatA(exe, "Zax.exe");
    lstrcpyA(dll, dir);
    lstrcatA(dll, "Zax-Launcher.dll");
    if (GetFileAttributesA(exe) == INVALID_FILE_ATTRIBUTES)
        return fail("Zax.exe not found; Zax-Launcher.exe belongs in the folder of Zax.exe");
    if (GetFileAttributesA(dll) == INVALID_FILE_ATTRIBUTES)
        return fail("Zax-Launcher.dll not found");
    if (lstrlenA(exe) + lstrlenA(args) + 32 > (int)sizeof cmd)
        return fail("Command line too long");

    lstrcpyA(cmd, "\"");
    lstrcatA(cmd, exe);
    lstrcatA(cmd, "\" ");
    lstrcatA(cmd, args);

    for (int i = 0; i < extras; i++) {
        lstrcpyA(dll, dir);
        lstrcatA(dll, extra[i]);
        if (GetFileAttributesA(dll) == INVALID_FILE_ATTRIBUTES) {
            SetLastError(0);
            lstrcpyA(cmd, "/LoadDll: not found next to Zax.exe: ");
            lstrcatA(cmd, extra[i]);
            return fail(cmd);
        }
    }
    lstrcpyA(dll, dir);
    lstrcatA(dll, "Zax-Launcher.dll");

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(exe, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, dir, &si, &pi))
        return fail("Could not start Zax.exe");

    /* Load the DLLs before Zax.exe continues: Zax-Launcher.dll, then only those /LoadDll asked for (never one just
     * because it lies in the folder) */
    if (!inject(pi.hProcess, dll)) {
        int rc = fail("Could not load Zax-Launcher.dll into Zax.exe");
        TerminateProcess(pi.hProcess, 1);
        return rc;
    }
    for (int i = 0; i < extras; i++) {
        lstrcpyA(dll, dir);
        lstrcatA(dll, extra[i]);
        if (!inject(pi.hProcess, dll)) {
            lstrcpyA(cmd, "Could not load ");
            lstrcatA(cmd, extra[i]);
            lstrcatA(cmd, " into Zax.exe");
            int rc = fail(cmd);
            TerminateProcess(pi.hProcess, 1);
            return rc;
        }
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

/* ---- Help window: HELP in a read-only, scrollable text field (a MessageBox cuts it off) --- */

enum { ID_HELP_TEXT = 1, ID_HELP_CLOSE };

static LRESULT CALLBACK help_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        MoveWindow(GetDlgItem(wnd, ID_HELP_TEXT), 8, 8, w - 16, h - 50, TRUE);
        MoveWindow(GetDlgItem(wnd, ID_HELP_CLOSE), w - 96, h - 34, 88, 26, TRUE);
        return 0;
    }
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize.x = 320;
        ((MINMAXINFO *)lp)->ptMinTrackSize.y = 200;
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_HELP_CLOSE || LOWORD(wp) == IDCANCEL)
            DestroyWindow(wnd);
        return 0;
    case WM_CLOSE:
        DestroyWindow(wnd);
        return 0;
    }
    return DefWindowProcA(wnd, msg, wp, lp);
}

/* Shows HELP and returns when the window is closed (Close, Esc or the X); owner is disabled meanwhile */
static void show_help(HWND owner)
{
    static char text[sizeof HELP * 2];  /* the edit control needs CR LF */
    char *t = text;
    for (const char *s = HELP; *s; s++) {
        if (*s == '\n')
            *t++ = '\r';
        *t++ = *s;
    }
    *t = 0;

    HINSTANCE inst = GetModuleHandleA(NULL);
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = help_proc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(1));
    if (!wc.hIcon)
        wc.hIcon = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "Zax-Launcher-Help";
    RegisterClassA(&wc);  /* fails harmlessly on the second call */
    /* fixed-width font: the help lines up the cheat keys and the examples in columns */
    HFONT font = CreateFontA(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
    HGDIOBJ use = font ? (HGDIOBJ)font : GetStockObject(ANSI_FIXED_FONT);
    /* as wide as the longest line, so nothing wraps (the edit control would wrap at the window's edge) */
    int width = 560;
    HDC dc = GetDC(NULL);
    HGDIOBJ old = SelectObject(dc, use);
    for (const char *line = HELP; *line;) {
        const char *e = line;
        while (*e && *e != '\n')
            e++;
        SIZE size;
        if (GetTextExtentPoint32A(dc, line, (int)(e - line), &size) && size.cx + 40 > width)
            width = size.cx + 40;  /* margins and the vertical scroll bar */
        line = *e ? e + 1 : e;
    }
    SelectObject(dc, old);
    ReleaseDC(NULL, dc);

    DWORD style = WS_OVERLAPPEDWINDOW & ~WS_MINIMIZEBOX;
    RECT r = {0, 0, width, 560};
    AdjustWindowRect(&r, style, FALSE);
    int w = r.right - r.left, h = r.bottom - r.top;
    int sw = GetSystemMetrics(SM_CXSCREEN) - 40, sh = GetSystemMetrics(SM_CYSCREEN) - 60;
    if (w > sw)
        w = sw;
    if (h > sh)
        h = sh;
    HWND wnd = CreateWindowExA(WS_EX_CONTROLPARENT, wc.lpszClassName, "Zax-Launcher - Help", style,
                               (GetSystemMetrics(SM_CXSCREEN) - w) / 2, (GetSystemMetrics(SM_CYSCREEN) - h) / 2,
                               w, h, owner, NULL, inst, NULL);
    if (!wnd) {
        if (font)
            DeleteObject(font);
        MessageBoxA(owner, HELP, "Zax-Launcher - Help", MB_ICONINFORMATION);
        return;
    }
    /* ES_AUTOHSCROLL: no wrapping; on a narrow screen the horizontal scroll bar reaches the rest */
    HWND edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", text,
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY |
                                    ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                                0, 0, 0, 0, wnd, (HMENU)ID_HELP_TEXT, inst, NULL);
    SendMessageA(edit, WM_SETFONT, (WPARAM)use, 0);
    HWND close = CreateWindowExA(0, "BUTTON", "Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                 0, 0, 0, 0, wnd, (HMENU)ID_HELP_CLOSE, inst, NULL);
    SendMessageA(close, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
    GetClientRect(wnd, &r);
    help_proc(wnd, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
    if (owner)
        EnableWindow(owner, FALSE);
    ShowWindow(wnd, SW_SHOW);
    SetFocus(edit);
    SendMessageA(edit, EM_SETSEL, 0, 0);

    MSG msg;
    int quit = 0;
    while (IsWindow(wnd)) {
        if (GetMessageA(&msg, NULL, 0, 0) <= 0) {
            quit = 1;
            break;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            DestroyWindow(wnd);
            continue;
        }
        if (!IsDialogMessageA(wnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    if (owner) {
        EnableWindow(owner, TRUE);
        SetActiveWindow(owner);
    }
    if (font)
        DeleteObject(font);
    if (quit)
        PostQuitMessage((int)msg.wParam);
}

/* ---- Start window -------------------------------------------------------------------- */

enum {
    ID_MAPS = 100, ID_SPAWN, ID_EASY, ID_NORMAL, ID_HARD, ID_RES, ID_DEPTH,
    ID_SKIP, ID_LOADMAP, ID_CHEATS, ID_DEBUGLOG, ID_SHOWFPS, ID_NETGRAPHS, ID_DEBUGDRAW, ID_SHOWHIDDEN, ID_OUTLINES, ID_ALTDIRTY,
    ID_NOENTRY, ID_CMDLINE, ID_HELP, ID_TABS,
    ID_SINGLE, ID_HOST, ID_JOIN, ID_NAME, ID_ADDRESS,
    ID_PACKS = 300, ID_PACKS_ALL, ID_PACKS_NOTE,
    ID_HARDENING = 390, ID_HARDENING_LIST,   /* the hardening group's box and the button that shows its fixes */
    ID_FIX_LIST, ID_HARD_LIST,               /* the scrolling lists that hold the fixes' boxes */
    ID_FIX = 400   /* ID_FIX + i: the box of FIX[i] (fixes.h) */
};
static const char *const LAN_MODE[] = {"Off", "Host", "Join"};
/* The switches of things Zax.exe still contains but never shows (Zax-Launcher.dll turns them on); the ini key is the
 * switch without its slash */
static const struct {
    int id;
    const char *arg, *text;
} DRAW_SWITCH[] = {
    {ID_NETGRAPHS, "/NetGraphs", "Network traffic graphs (LAN game)"},
    {ID_DEBUGDRAW, "/DebugDraw", "Debug drawing (AI areas, paths, ranges)"},
    {ID_SHOWHIDDEN, "/ShowHidden", "Hidden parts' overlays (with Debug drawing)"},
    {ID_OUTLINES, "/DrawOutlines", "Draw order outlines"},
    {ID_ALTDIRTY, "/AltDirtyDraw", "Alternative tile redraw"},
    {ID_NOENTRY, "/NoEntryDraw", "No objects in redrawn tiles"},
};
enum { DRAW_SWITCHES = sizeof DRAW_SWITCH / sizeof *DRAW_SWITCH };
static const char *const DIFFICULTY[] = {"Easy", "Normal", "Hard"};
static const char MAIN_MENU[] = "(Main menu: normal start)";

static HWND g_wnd;
/* The start window's tabs (so it fits a 640 x 480 desktop); the controls are children of the window itself, each
 * on one page (PAGE_ALL: below the tabs), shown or hidden when the tab changes */
enum { PAGE_START, PAGE_LAN, PAGE_DEVELOPER, PAGE_PACKS, PAGE_FIXES, PAGES, PAGE_ALL = -1 };
/* The Fixes page shows one of two lists below its text: the fixes, or the hardening fixes (g_hardening_shown) */
enum { PAGE_FIXES_OWN = PAGES, PAGE_FIXES_HARDENING };
static const char *const PAGE_NAME[PAGES] = {"Start", "LAN game", "Developer", "Packs", "Fixes"};
static struct {
    HWND wnd;
    int page;
} g_controls[160];
static int g_control_count, g_build_page = PAGE_ALL, g_page, g_hardening_shown;

static char g_dir[MAX_PATH], g_ini[MAX_PATH + 16], g_rules_spawn[256];
static char g_pool[256 * 1024];
/* Maps: path below Levels with / and without extension; from g_zip: local header position and size */
struct map {
    char *name;
    int in_zip;
    DWORD off, size;
};
static struct map g_maps[4096];
static char g_zip[MAX_PATH + 16];  /* Data.zip or Data.dat, empty without */
static int g_pool_used, g_map_count;
static char g_args[4096];

/* The Fixes page's scrolling lists, the fixes' and the hardening fixes' (two per row, sorted by label): the boxes are
 * their children, so item() looks there too */
enum { LIST_FIX, LIST_HARD, LISTS };
static struct {
    HWND wnd;
    int scroll, rows;
} g_lists[LISTS];
static HWND g_build_parent;   /* where add() puts a control: the window (NULL), or one of the lists */

static HWND item(int id)
{
    HWND c = GetDlgItem(g_wnd, id);
    for (int l = 0; !c && l < LISTS; l++)
        if (g_lists[l].wnd)
            c = GetDlgItem(g_lists[l].wnd, id);
    return c;
}

static void *read_file(const char *path, DWORD *size)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return NULL;
    DWORD n = GetFileSize(f, NULL), got = 0;
    char *buf = n != INVALID_FILE_SIZE ? HeapAlloc(GetProcessHeap(), 0, n + 1) : NULL;
    if (buf && (!ReadFile(f, buf, n, &got, NULL) || got != n)) {
        HeapFree(GetProcessHeap(), 0, buf);
        buf = NULL;
    }
    CloseHandle(f);
    if (buf)
        buf[n] = 0, *size = n;
    return buf;
}

/* Pass the lines of a game text file (CRLF) one by one to fn, without line ending */
static void each_line(char *text, DWORD size, void (*fn)(char *line, void *ctx), void *ctx)
{
    char *p = text, *end = text + size;
    while (p < end) {
        char *e = p;
        while (e < end && *e != '\r' && *e != '\n')
            e++;
        char keep = *e;
        *e = 0;
        fn(p, ctx);
        *e = keep;
        p = e;
        if (p < end && *p == '\r')
            p++;
        if (p < end && *p == '\n')
            p++;
    }
}

static const char *after(const char *line, const char *prefix)
{
    int n = lstrlenA(prefix);
    for (int i = 0; i < n; i++)
        if (line[i] != prefix[i])
            return NULL;
    return line + n;
}

static void trim_right(char *s)
{
    int n = lstrlenA(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = 0;
}

static void rules_line(char *line, void *ctx)
{
    (void)ctx;
    const char *v = after(line, "\tFirst Map Spawn Point=");
    if (v) {
        lstrcpynA(g_rules_spawn, v, sizeof g_rules_spawn);
        trim_right(g_rules_spawn);
    }
}

/* Add a map unless it is already there (ignoring case, like the game); 0 if there is no room
 * left */
static int add_map(const char *name, int len, int in_zip, DWORD off, DWORD size)
{
    if (g_map_count >= (int)(sizeof g_maps / sizeof *g_maps) || g_pool_used + len + 1 > (int)sizeof g_pool)
        return 0;
    char *m = g_pool + g_pool_used;
    for (int i = 0; i < len; i++)
        m[i] = name[i] == '\\' ? '/' : name[i];
    m[len] = 0;
    for (int i = 0; i < g_map_count; i++)
        if (!lstrcmpiA(g_maps[i].name, m))
            return 1;
    g_pool_used += len + 1;
    struct map *e = &g_maps[g_map_count++];
    e->name = m;
    e->in_zip = in_zip;
    e->off = off;
    e->size = size;
    return 1;
}

/* Maps on disk: all .zax below Data\Levels */
static void scan_maps(const char *rel)
{
    char pattern[MAX_PATH * 2];
    wsprintfA(pattern, "%sData\\Levels\\%s*", g_dir, rel);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (fd.cFileName[0] == '.')
            continue;
        char sub[MAX_PATH];
        if (lstrlenA(rel) + lstrlenA(fd.cFileName) + 2 > MAX_PATH)
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            wsprintfA(sub, "%s%s\\", rel, fd.cFileName);
            scan_maps(sub);
            continue;
        }
        int n = lstrlenA(fd.cFileName);
        if (n < 5 || str_nieq(fd.cFileName + n - 4, ".zax", 4) != 0)
            continue;
        wsprintfA(sub, "%s%s", rel, fd.cFileName);
        if (!add_map(sub, lstrlenA(sub) - 4, 0, 0, 0))
            break;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* n bytes from off of an open file */
static int read_at(HANDLE f, DWORD off, void *buf, DWORD n)
{
    DWORD got = 0;
    return SetFilePointer(f, (LONG)off, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
           ReadFile(f, buf, n, &got, NULL) && got == n;
}

/* Maps in the zip file next to the exe: Zax.exe (0x4a0c40) looks for Data.ZIP first, then
 * Data.DAT. Only uncompressed entries, the game does not read others. Entries that are also on
 * disk are loaded from disk by the game (zax-formats/docs/FORMATS.md 1); that is why scan_maps runs first. */
static void scan_zip(void)
{
    static const char *const NAMES[] = {"Data.zip", "Data.dat"};
    HANDLE f = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 2 && f == INVALID_HANDLE_VALUE; i++) {
        wsprintfA(g_zip, "%s%s", g_dir, NAMES[i]);
        f = CreateFileA(g_zip, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    }
    if (f == INVALID_HANDLE_VALUE) {
        g_zip[0] = 0;
        return;
    }
    HANDLE heap = GetProcessHeap();
    DWORD size = GetFileSize(f, NULL);
    unsigned cd_off, cd_size;
    DWORD n = size != INVALID_FILE_SIZE && size > ZIP_TAIL_MAX ? ZIP_TAIL_MAX : size;
    unsigned char *tail = n != INVALID_FILE_SIZE ? HeapAlloc(heap, 0, n + 1) : NULL;
    unsigned char *cd = NULL;
    if (tail && read_at(f, size - n, tail, n) && zip_eocd(tail, n, &cd_off, &cd_size) &&
        cd_off <= size && cd_size <= size - cd_off && (cd = HeapAlloc(heap, 0, cd_size + 1)) != NULL &&
        read_at(f, cd_off, cd, cd_size)) {
        struct zip_entry e;
        char name[MAX_PATH];
        for (DWORD p = 0, len; (len = zip_entry_at(cd + p, cd_size - p, &e)) != 0; p += len) {
            int k = zip_map_name(e.name, e.name_len, name, sizeof name);
            /* sizes beyond the file are a damaged entry: left out */
            if (e.local_off >= size || e.size > size - e.local_off)
                continue;
            if (k > 0 && e.method == 0 && !add_map(name, k, 1, e.local_off, e.size))
                break;
        }
    }
    if (cd)
        HeapFree(heap, 0, cd);
    if (tail)
        HeapFree(heap, 0, tail);
    CloseHandle(f);
}

/* Content of a map from disk or from the zip file, 0-terminated; HeapFree afterwards */
static char *read_map(const struct map *m, DWORD *size)
{
    if (!m->in_zip) {
        char path[MAX_PATH * 2];
        wsprintfA(path, "%sData\\Levels\\%s.zax", g_dir, m->name);
        return read_file(path, size);
    }
    HANDLE f = CreateFileA(g_zip, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return NULL;
    unsigned char head[ZIP_LOCAL_HEAD];
    char *buf = NULL;
    DWORD skip;
    if (read_at(f, m->off, head, sizeof head) && (skip = zip_local_data(head)) != 0 &&
        (buf = HeapAlloc(GetProcessHeap(), 0, m->size + 1)) != NULL) {
        if (read_at(f, m->off + skip, buf, m->size))
            buf[m->size] = 0, *size = m->size;
        else
            HeapFree(GetProcessHeap(), 0, buf), buf = NULL;
    }
    CloseHandle(f);
    return buf;
}

/* Start points of a map as in the editor (zax-formats/zaxtools/transition.py spawn_points): named
 * objects with model Editor/Spawn Point or Activity CSpawnPointAI */
struct spawn_scan {
    HWND combo;
    char name[256], team[256];
    int is_spawn, have_team;
};

static void spawn_flush(struct spawn_scan *s)
{
    if (s->name[0] && s->is_spawn && SendMessageA(s->combo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)s->name) == CB_ERR)
        SendMessageA(s->combo, CB_ADDSTRING, 0, (LPARAM)s->name);
    s->name[0] = 0;
    s->is_spawn = 0;
}

static void spawn_line(char *line, void *ctx)
{
    struct spawn_scan *s = ctx;
    const char *v;
    if (after(line, "\t\tLevel Part="))
        spawn_flush(s);
    else if ((v = after(line, "\t\t\tName=")) != NULL)
        lstrcpynA(s->name, v, sizeof s->name);
    else if (!lstrcmpA(line, "\t\t\tModel=Editor/Spawn Point") || !lstrcmpA(line, "\t\t\t\tActivity=CSpawnPointAI"))
        s->is_spawn = 1;
    else if (!s->have_team && (v = after(line, "\t\t\tSpawn Point Name=")) != NULL) {
        lstrcpynA(s->team, v, sizeof s->team);
        trim_right(s->team);
        s->have_team = 1;
    }
}

static int select_string(HWND combo, const char *text)
{
    if (!text[0])
        return 0;
    LRESULT i = SendMessageA(combo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)text);
    if (i == CB_ERR)
        return 0;
    SendMessageA(combo, CB_SETCURSEL, (WPARAM)i, 0);
    return 1;
}

static const struct map *selected(void)
{
    LRESULT i = SendMessageA(item(ID_MAPS), LB_GETCURSEL, 0, 0);
    return i > 0 && i <= g_map_count ? &g_maps[i - 1] : NULL;
}

static const char *selected_map(void)
{
    const struct map *m = selected();
    return m ? m->name : NULL;
}

/* Start points of the selected map; preselected: wish, else First Map Spawn Point from
 * Rules.txt, else the start point from the map's Team Info, else the first one */
static void fill_spawns(const char *wish)
{
    HWND combo = item(ID_SPAWN);
    SendMessageA(combo, CB_RESETCONTENT, 0, 0);
    const struct map *map = selected();
    if (map) {
        DWORD size;
        char *text = read_map(map, &size);
        if (text) {
            struct spawn_scan s;
            memset(&s, 0, sizeof s);
            s.combo = combo;
            each_line(text, size, spawn_line, &s);
            spawn_flush(&s);
            HeapFree(GetProcessHeap(), 0, text);
            if (!select_string(combo, wish) && !select_string(combo, g_rules_spawn) && !select_string(combo, s.team))
                SendMessageA(combo, CB_SETCURSEL, 0, 0);
        }
    }
    EnableWindow(combo, map != NULL);
    for (int id = ID_EASY; id <= ID_HARD; id++)
        EnableWindow(item(id), map != NULL);
}

static int checked(int id) { return SendMessageA(item(id), BM_GETCHECK, 0, 0) == BST_CHECKED; }

static int combo_data(int id)
{
    LRESULT i = SendMessageA(item(id), CB_GETCURSEL, 0, 0);
    return i == CB_ERR ? 0 : (int)SendMessageA(item(id), CB_GETITEMDATA, (WPARAM)i, 0);
}

static void add_arg(const char *fmt, const char *value)
{
    char part[600];
    wsprintfA(part, fmt, value);
    if (lstrlenA(g_args) + lstrlenA(part) + 2 < (int)sizeof g_args) {
        if (g_args[0])
            lstrcatA(g_args, " ");
        lstrcatA(g_args, part);
    }
}

/* ---- Packs page: the *.RED beside Zax.exe -----------------------------------------------------------------------
 * Zax.exe mounts every *.RED of its folder after Data.dat, in _stricmp name order, and a later pack wins file by file
 * (zax-decomp docs/DECOMPILATION.md, "Mounting at start"). The page lists them in that order with a check box each,
 * all ticked by default (the game as without the launcher); every unticked one becomes /NoPack Name.RED, which
 * Zax-Launcher.dll leaves out of the game's mount loop. No file in the game folder is renamed, moved or changed.
 * Remembered in Zax-Launcher.ini, [Packs]: one line per pack, Name.RED=1 or 0; a pack not named there is ticked. */
enum { PACKS_MAX = 64, PACK_NAME = 0x50 };   /* the game keeps each name in 0x50 bytes */
static struct {
    char name[PACK_NAME];
    DWORD size;
} g_packs[PACKS_MAX];
static int g_pack_count, g_packs_ready;

/* _stricmp: the ASCII letters compared as lower case */
static int pack_cmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        const char x = lower(*a), y = lower(*b);
        if (x != y)
            return (unsigned char)x < (unsigned char)y ? -1 : 1;
        if (!x)
            return 0;
    }
}

/* What the game's own search finds (FindFirstFileA over *.RED in its folder, files only, no name starting with '.'),
 * sorted */
static void scan_packs(void)
{
    char pattern[MAX_PATH + 16];
    wsprintfA(pattern, "%s*.RED", g_dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.' ||
            lstrlenA(fd.cFileName) >= PACK_NAME || g_pack_count == PACKS_MAX)
            continue;
        int at = g_pack_count++;
        for (; at > 0 && pack_cmp(g_packs[at - 1].name, fd.cFileName) > 0; at--)
            g_packs[at] = g_packs[at - 1];
        lstrcpyA(g_packs[at].name, fd.cFileName);
        g_packs[at].size = fd.nFileSizeLow;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static int pack_on(int i)
{
    return g_packs_ready ? ListView_GetCheckState(item(ID_PACKS), i) != 0 : 1;
}

/* The unticked packs as /NoPack, and the note below the list */
static void pack_args(void)
{
    int off = 0;
    for (int i = 0; i < g_pack_count; i++)
        if (!pack_on(i)) {
            add_arg(str_find(g_packs[i].name, " ") ? "/NoPack \"%s\"" : "/NoPack %s", g_packs[i].name);
            off++;
        }
    if (!g_packs_ready)
        return;
    char note[96];
    if (!g_pack_count)
        lstrcpyA(note, "No *.RED next to Zax.exe.");
    else if (!off)
        lstrcpyA(note, "All ticked: the game loads them as without the launcher.");
    else
        wsprintfA(note, "%d of %d left out of this start.", off, g_pack_count);
    SetWindowTextA(item(ID_PACKS_NOTE), note);
}

static void save_packs(void)
{
    for (int i = 0; i < g_pack_count; i++)
        WritePrivateProfileStringA("Packs", g_packs[i].name, pack_on(i) ? "1" : "0", g_ini);
}

/* ---- Fixes page (fixes.h): one box per fix, remembered in [Fixes] of Zax-Launcher.ini, /Fix Name when ticked ---- */

static HWND add(DWORD ex, const char *cls, const char *text, DWORD style, int x, int y, int w, int h, int id);

static int hardening_ticked(void)   /* how many of the hardening fixes are ticked */
{
    int n = 0;
    for (int i = FIX_FIRST_HARDENING; i < FIXES; i++)
        n += checked(ID_FIX + i);
    return n;
}

/* The group's box: ticked when all its fixes are, grey when some are */
static void sync_hardening(void)
{
    const int n = hardening_ticked();
    SendMessageA(item(ID_HARDENING), BM_SETCHECK, n == FIXES - FIX_FIRST_HARDENING ? BST_CHECKED : n ? BST_INDETERMINATE
                                                                                                  : BST_UNCHECKED, 0);
}

/* A click on the group's box ticks all its fixes, or unticks them when all were ticked */
static void click_hardening(void)
{
    const int all = hardening_ticked() == FIXES - FIX_FIRST_HARDENING;
    for (int i = FIX_FIRST_HARDENING; i < FIXES; i++)
        SendMessageA(item(ID_FIX + i), BM_SETCHECK, all ? BST_UNCHECKED : BST_CHECKED, 0);
    sync_hardening();
}

static void fixes_args(void)
{
    for (int i = 0; i < FIX_FIRST_HARDENING; i++)
        if (checked(ID_FIX + i))
            add_arg("/Fix %s", FIX[i].name);
    if (hardening_ticked() == FIXES - FIX_FIRST_HARDENING)
        add_arg("/Fix %s", FIX_HARDENING);
    else
        for (int i = FIX_FIRST_HARDENING; i < FIXES; i++)
            if (checked(ID_FIX + i))
                add_arg("/Fix %s", FIX[i].name);
}

static void save_fixes(void)
{
    for (int i = 0; i < FIXES; i++)
        WritePrivateProfileStringA("Fixes", FIX[i].name, checked(ID_FIX + i) ? "1" : "0", g_ini);
}

/* A list: a child window with a vertical scroll bar, its boxes two per row (LIST_ROW pixels); the wheel and the bar
 * move them, a box's clicks go on to the window (wnd_proc) as if it were the window's own */
enum { LIST_ROW = 20 };

static int list_of(HWND wnd)
{
    for (int l = 0; l < LISTS; l++)
        if (g_lists[l].wnd == wnd)
            return l;
    return 0;
}

static void list_scroll_to(HWND list, int pos)
{
    const int l = list_of(list);
    RECT r;
    GetClientRect(list, &r);
    const int page = r.bottom / LIST_ROW, max = g_lists[l].rows - page > 0 ? g_lists[l].rows - page : 0;
    pos = pos < 0 ? 0 : pos > max ? max : pos;
    if (pos != g_lists[l].scroll) {
        ScrollWindowEx(list, 0, (g_lists[l].scroll - pos) * LIST_ROW, NULL, NULL, NULL, NULL,
                       SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
        g_lists[l].scroll = pos;
    }
    SCROLLINFO si = {sizeof si, SIF_RANGE | SIF_PAGE | SIF_POS, 0, g_lists[l].rows - 1, (UINT)page, pos, 0};
    SetScrollInfo(list, SB_VERT, &si, TRUE);
}

static LRESULT CALLBACK list_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    const int l = list_of(wnd);
    switch (msg) {
    case WM_COMMAND:
        /* a box that takes the focus (the Tab key; BS_NOTIFY) or is clicked scrolls into view */
        if (HIWORD(wp) == BN_SETFOCUS || HIWORD(wp) == BN_CLICKED) {
            RECT b, r;
            GetWindowRect((HWND)lp, &b);
            MapWindowPoints(NULL, wnd, (POINT *)&b, 2);
            GetClientRect(wnd, &r);
            if (b.top < 0)
                list_scroll_to(wnd, g_lists[l].scroll + (b.top - LIST_ROW + 1) / LIST_ROW);
            else if (b.bottom > r.bottom)
                list_scroll_to(wnd, g_lists[l].scroll + (b.bottom - r.bottom + LIST_ROW - 1) / LIST_ROW);
        }
        return SendMessageA(g_wnd, msg, wp, lp);
    case WM_VSCROLL: {
        SCROLLINFO si = {sizeof si, SIF_ALL, 0, 0, 0, 0, 0};
        GetScrollInfo(wnd, SB_VERT, &si);
        int pos = g_lists[l].scroll;
        switch (LOWORD(wp)) {
        case SB_LINEUP: pos--; break;
        case SB_LINEDOWN: pos++; break;
        case SB_PAGEUP: pos -= (int)si.nPage; break;
        case SB_PAGEDOWN: pos += (int)si.nPage; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: pos = si.nTrackPos; break;
        case SB_TOP: pos = 0; break;
        case SB_BOTTOM: pos = g_lists[l].rows; break;
        }
        list_scroll_to(wnd, pos);
        return 0;
    }
    case WM_MOUSEWHEEL:   /* a box passes the wheel on to its parent (DefWindowProc) */
        list_scroll_to(wnd, g_lists[l].scroll - (short)HIWORD(wp) / WHEEL_DELTA * 3);
        return 0;
    }
    return DefWindowProcA(wnd, msg, wp, lp);
}

/* List l at x, y (w x h) with the boxes of FIX[first] to FIX[last - 1], sorted by label (case ignored; fixes.h keeps
 * its own order) and filled row by row, two per row in columns of 262 pixels; each ticked as remembered in the ini
 * or by its default */
static void add_list(int l, int x, int y, int w, int h, int id, int first, int last)
{
    WNDCLASSA lc;
    memset(&lc, 0, sizeof lc);
    lc.lpfnWndProc = list_proc;
    lc.hInstance = GetModuleHandleA(NULL);
    lc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    lc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    lc.lpszClassName = "Zax-Launcher-List";
    RegisterClassA(&lc);  /* fails harmlessly on a second call */
    g_lists[l].wnd = add(WS_EX_CLIENTEDGE | WS_EX_CONTROLPARENT, lc.lpszClassName, "", WS_VSCROLL | WS_CLIPCHILDREN,
                         x, y, w, h, id);
    int order[FIXES], n = 0;
    for (int i = first; i < last; i++) {
        int k = n++;   /* insertion by label */
        for (; k > 0 && lstrcmpiA(FIX[order[k - 1]].label, FIX[i].label) > 0; k--)
            order[k] = order[k - 1];
        order[k] = i;
    }
    g_build_parent = g_lists[l].wnd;
    for (int k = 0; k < n; k++) {
        const int i = order[k];
        HWND box = add(0, "BUTTON", FIX[i].label, WS_TABSTOP | BS_AUTOCHECKBOX | BS_NOTIFY, 8 + (k % 2) * 268,
                       2 + k / 2 * LIST_ROW, 262, 18, ID_FIX + i);
        const int on = (int)GetPrivateProfileIntA("Fixes", FIX[i].name, FIX[i].on_in_window, g_ini);
        SendMessageA(box, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    g_build_parent = NULL;
    g_lists[l].rows = (n + 1) / 2;
    g_lists[l].scroll = 0;
    list_scroll_to(g_lists[l].wnd, 0);
}

/* Built on its own page: the fixes in a scrolling list, below it the Hardening box and the button that shows the
 * hardening fixes (in a list of their own) in the fixes' place */
static void create_fixes_page(void)
{
    g_build_page = PAGE_FIXES;
    add(0, "STATIC", "Fixes of bugs of the original game, made in the running Zax.exe (/Fix). Unticked: as the "
                     "original.", 0, 20, 40, 556, 16, -1);
    g_build_page = PAGE_FIXES_OWN;
    add_list(LIST_FIX, 20, 60, 556, 224, ID_FIX_LIST, 0, FIX_FIRST_HARDENING);
    g_build_page = PAGE_FIXES_HARDENING;
    add(0, "STATIC", "These guard against crashes on unusual or modded data; the shipped game reaches none of them. "
                     "Each one also has its own switch (/Fix Name); the box below ticks or unticks them all.",
        0, 20, 60, 556, 32, -1);
    add_list(LIST_HARD, 20, 96, 556, 188, ID_HARD_LIST, FIX_FIRST_HARDENING, FIXES);
    g_build_page = PAGE_FIXES;
    add(0, "BUTTON", HARDENING_LABEL, WS_TABSTOP | BS_3STATE, 20, 292, 400, 20, ID_HARDENING);
    add(0, "BUTTON", "Show them", WS_TABSTOP, 452, 290, 124, 24, ID_HARDENING_LIST);
    sync_hardening();
}

static void add_fix_tooltips(HWND tip)
{
    for (int i = 0; i < FIXES; i++) {
        TTTOOLINFOA ti;
        memset(&ti, 0, sizeof ti);
        ti.cbSize = TTTOOLINFOA_V2_SIZE;
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.uId = (UINT_PTR)item(ID_FIX + i);
        ti.hwnd = GetParent((HWND)ti.uId);   /* its list */
        ti.lpszText = (LPSTR)FIX[i].tip;
        SendMessageA(tip, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    }
    static const struct {
        int id;
        const char *text;
    } GROUP[] = {
        {ID_HARDENING, HARDENING_TIP},
        {ID_HARDENING_LIST, "Shows the hardening fixes one by one in place of the other fixes, and back."},
    };
    for (unsigned i = 0; i < sizeof GROUP / sizeof *GROUP; i++) {
        TTTOOLINFOA ti;
        memset(&ti, 0, sizeof ti);
        ti.cbSize = TTTOOLINFOA_V2_SIZE;
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = g_wnd;
        ti.uId = (UINT_PTR)item(GROUP[i].id);
        ti.lpszText = (LPSTR)GROUP[i].text;
        SendMessageA(tip, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    }
}

/* Selection in the window as arguments for launch() and for display */
static void build_args(void)
{
    char buf[300];
    g_args[0] = 0;
    const int lan = checked(ID_HOST) ? 1 : checked(ID_JOIN) ? 2 : 0;
    const char *map = lan ? NULL : selected_map();   /* a LAN game picks its map in the host options */
    if (lan == 1)
        add_arg("%s", "-host");
    if (lan == 2 && GetWindowTextA(item(ID_ADDRESS), buf, sizeof buf) > 0)
        add_arg("-connect %s", buf);
    if (lan && GetWindowTextA(item(ID_NAME), buf, sizeof buf) > 0)
        add_arg(str_find(buf, " ") ? "-name \"%s\"" : "-name %s", buf);
    if (map) {
        add_arg("/Map \"%s\"", map);
        if (GetWindowTextA(item(ID_SPAWN), buf, sizeof buf) > 0)
            add_arg("/Spawn \"%s\"", buf);
        for (int i = 0; i < 3; i++)
            if (checked(ID_EASY + i))
                add_arg("/Difficulty %s", DIFFICULTY[i]);
    }
    int res = combo_data(ID_RES), depth = combo_data(ID_DEPTH);
    if (res) {
        wsprintfA(buf, "/ScreenWidth %d /ScreenHeight %d", LOWORD(res), HIWORD(res));
        add_arg("%s", buf);
    }
    if (depth) {
        wsprintfA(buf, "/ScreenBitDepth %d", depth);
        add_arg("%s", buf);
    }
    if (checked(ID_LOADMAP))
        add_arg("%s", "/LoadMap");
    if (checked(ID_SKIP))
        add_arg("%s", "/SkipMovies");
    if (checked(ID_CHEATS))
        add_arg("%s", "/Cheats");
    if (checked(ID_DEBUGLOG))
        add_arg("%s", "/DebugLog");
    if (checked(ID_SHOWFPS))
        add_arg("%s", "/ShowFPS");
    for (int i = 0; i < DRAW_SWITCHES; i++)
        if (checked(DRAW_SWITCH[i].id) && (DRAW_SWITCH[i].id != ID_SHOWHIDDEN || checked(ID_DEBUGDRAW)))
            add_arg("%s", DRAW_SWITCH[i].arg);
    pack_args();
    fixes_args();

    static char shown[sizeof g_args + 32];
    wsprintfA(shown, "Zax-Launcher.exe %s", g_args);
    SetWindowTextA(item(ID_CMDLINE), shown);
}

static void save_settings(void)
{
    char buf[300];
    const char *map = selected_map();
    WritePrivateProfileStringA("Launcher", "Map", map ? map : "", g_ini);
    GetWindowTextA(item(ID_SPAWN), buf, sizeof buf);
    WritePrivateProfileStringA("Launcher", "Spawn", buf, g_ini);
    for (int i = 0; i < 3; i++)
        if (checked(ID_EASY + i))
            WritePrivateProfileStringA("Launcher", "Difficulty", DIFFICULTY[i], g_ini);
    int res = combo_data(ID_RES);
    wsprintfA(buf, "%d", res ? LOWORD(res) : 0);
    WritePrivateProfileStringA("Launcher", "ScreenWidth", buf, g_ini);
    wsprintfA(buf, "%d", res ? HIWORD(res) : 0);
    WritePrivateProfileStringA("Launcher", "ScreenHeight", buf, g_ini);
    wsprintfA(buf, "%d", combo_data(ID_DEPTH));
    WritePrivateProfileStringA("Launcher", "ScreenBitDepth", buf, g_ini);
    WritePrivateProfileStringA("Launcher", "SkipMovies", checked(ID_SKIP) ? "1" : "0", g_ini);
    WritePrivateProfileStringA("Launcher", "LoadMap", checked(ID_LOADMAP) ? "1" : "0", g_ini);
    WritePrivateProfileStringA("Launcher", "Cheats", checked(ID_CHEATS) ? "1" : "0", g_ini);
    WritePrivateProfileStringA("Launcher", "DebugLog", checked(ID_DEBUGLOG) ? "1" : "0", g_ini);
    WritePrivateProfileStringA("Launcher", "ShowFPS", checked(ID_SHOWFPS) ? "1" : "0", g_ini);
    for (int i = 0; i < DRAW_SWITCHES; i++)
        WritePrivateProfileStringA("Launcher", DRAW_SWITCH[i].arg + 1, checked(DRAW_SWITCH[i].id) ? "1" : "0", g_ini);
    for (int i = 0; i < 3; i++)
        if (checked(ID_SINGLE + i))
            WritePrivateProfileStringA("Launcher", "LAN", LAN_MODE[i], g_ini);
    GetWindowTextA(item(ID_NAME), buf, sizeof buf);
    WritePrivateProfileStringA("Launcher", "PlayerName", buf, g_ini);
    GetWindowTextA(item(ID_ADDRESS), buf, sizeof buf);
    WritePrivateProfileStringA("Launcher", "HostAddress", buf, g_ini);
    wsprintfA(buf, "%d", g_page);
    WritePrivateProfileStringA("Launcher", "Page", buf, g_ini);
    save_packs();
    save_fixes();
}

/* A LAN game picks its map in the host options: the map, spawn and difficulty only without one; the host
 * address only for Join */
static void enable_lan(void)
{
    const int lan = !checked(ID_SINGLE);
    EnableWindow(item(ID_MAPS), !lan);
    EnableWindow(item(ID_SPAWN), !lan);
    for (int i = 0; i < 3; i++)
        EnableWindow(item(ID_EASY + i), !lan);
    EnableWindow(item(ID_NAME), lan);
    EnableWindow(item(ID_ADDRESS), checked(ID_JOIN));
}

/* /ShowHidden only gathers the hidden parts; the game never draws them, so it shows something only through
 * /DebugDraw's shapes: offered only while Debug drawing is ticked, unticked with it */
static void enable_hidden(void)
{
    const int debug = checked(ID_DEBUGDRAW);
    if (!debug)
        SendMessageA(item(ID_SHOWHIDDEN), BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(item(ID_SHOWHIDDEN), debug);
}

static int ini_int(const char *key, int def) { return (int)GetPrivateProfileIntA("Launcher", key, def, g_ini); }

/* Only what the game can handle (verified in the game): 640 x 480 or 800 x 600, 256 colours
 * or 16 bit; other values crash Zax.exe */
static void add_mode(HWND combo, const char *text, int data, int want)
{
    LRESULT k = SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)text);
    SendMessageA(combo, CB_SETITEMDATA, (WPARAM)k, data);
    if (data == want)
        SendMessageA(combo, CB_SETCURSEL, (WPARAM)k, 0);
}

static void fill_modes(void)
{
    HWND res = item(ID_RES), depth = item(ID_DEPTH);
    int want_res = MAKELONG(ini_int("ScreenWidth", 0), ini_int("ScreenHeight", 0));
    int want_depth = ini_int("ScreenBitDepth", 0);
    add_mode(res, "as set in the game", 0, -1);
    SendMessageA(res, CB_SETCURSEL, 0, 0);
    add_mode(res, "640 x 480", MAKELONG(640, 480), want_res);
    add_mode(res, "800 x 600", MAKELONG(800, 600), want_res);
    add_mode(depth, "as set in the game", 0, -1);
    SendMessageA(depth, CB_SETCURSEL, 0, 0);
    add_mode(depth, "8 bit (256 colors)", 8, want_depth);
    add_mode(depth, "16 bit", 16, want_depth);
}

static HWND add(DWORD ex, const char *cls, const char *text, DWORD style, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExA(ex, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, g_build_parent ? g_build_parent : g_wnd,
                             (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
    /* a box in a list is shown and hidden with the list */
    if (!g_build_parent && g_control_count < (int)(sizeof g_controls / sizeof *g_controls)) {
        g_controls[g_control_count].wnd = c;
        g_controls[g_control_count++].page = g_build_page;
    }
    return c;
}

static void show_page(int page)
{
    g_page = page;
    SendMessageA(item(ID_TABS), TCM_SETCURSEL, (WPARAM)page, 0);
    const int fixes_list = page != PAGE_FIXES ? -2 : g_hardening_shown ? PAGE_FIXES_HARDENING : PAGE_FIXES_OWN;
    for (int i = 0; i < g_control_count; i++)
        if (g_controls[i].page != PAGE_ALL)
            ShowWindow(g_controls[i].wnd,
                       g_controls[i].page == page || g_controls[i].page == fixes_list ? SW_SHOW : SW_HIDE);
    if (page == PAGE_FIXES)
        SetWindowTextA(item(ID_HARDENING_LIST), g_hardening_shown ? "Back to the fixes" : "Show them");
}

/* The Packs page: a list with a check box per pack in load order, its size, and "Tick all" */
static void create_packs_page(void)
{
    g_build_page = PAGE_PACKS;
    add(0, "STATIC", "Zax.exe loads every *.RED next to it after Data.dat, in this order; a later pack replaces the same\n"
                     "files of an earlier one. Untick a pack to leave it out of this start (the file is not touched).",
        WS_GROUP, 20, 40, 556, 32, -1);
    HWND list = add(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                    WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER, 20, 78, 556, 206, ID_PACKS);
    SendMessageA(list, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT,
                 LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
    LVCOLUMNA col;
    memset(&col, 0, sizeof col);
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.pszText = (LPSTR)"Pack (load order)";
    col.cx = 420;
    SendMessageA(list, LVM_INSERTCOLUMNA, 0, (LPARAM)&col);
    col.fmt = LVCFMT_RIGHT;
    col.pszText = (LPSTR)"Size";
    col.cx = 110;
    SendMessageA(list, LVM_INSERTCOLUMNA, 1, (LPARAM)&col);
    for (int i = 0; i < g_pack_count; i++) {
        LVITEMA it;
        memset(&it, 0, sizeof it);
        it.mask = LVIF_TEXT;
        it.iItem = i;
        it.pszText = g_packs[i].name;
        SendMessageA(list, LVM_INSERTITEMA, 0, (LPARAM)&it);
        char size[32];
        wsprintfA(size, "%lu KB", (g_packs[i].size + 1023) / 1024);
        it.iSubItem = 1;
        it.pszText = size;
        SendMessageA(list, LVM_SETITEMTEXTA, (WPARAM)i, (LPARAM)&it);
        ListView_SetCheckState(list, i, GetPrivateProfileIntA("Packs", g_packs[i].name, 1, g_ini) != 0);
    }
    add(0, "BUTTON", "Tick all", WS_TABSTOP, 20, 292, 88, 24, ID_PACKS_ALL);
    add(0, "STATIC", "", 0, 120, 296, 456, 16, ID_PACKS_NOTE);   /* pack_args writes it */
    g_packs_ready = 1;
}

/* Tooltips of the start window: the short form of HELP next to each control */
static const struct {
    int id;
    const char *text;
} TIP[] = {
    {ID_MAPS, "The map to start in, without the main menu and the intro. Maps under Data\\Levels win over those in "
              "Data.dat. Double-click starts. \"Main menu\" starts the game normally."},
    {ID_SPAWN, "Spawn point in the map. The default is First Map Spawn Point from Rules.txt."},
    {ID_EASY, "Difficulty of the started map."},
    {ID_NORMAL, "Difficulty of the started map."},
    {ID_HARD, "Difficulty of the started map."},
    {ID_RES, "Screen size for this start only (Zax.CFG stays as it is). The game runs at 640 x 480 or 800 x 600."},
    {ID_DEPTH, "Color depth for this start only. At 24 or 32 bit the game stops before the level (the data has no 24/32-bit images), so only 8 and "
               "16 bit are offered."},
    {ID_SKIP, "Skips the movies at the start (/SkipMovies, a switch of Zax.exe)."},
    {ID_LOADMAP, "Adds a \"Load Map\" button to the game menu (Esc) that loads any map under Levels."},
    {ID_CHEATS, "The developer cheats still in Zax.exe, hold Ctrl: F1 every item, F2 weapons/ammo/keys, F3 ore and "
                "crystals, F4 Twin Disruptor, F5 battery charges, F6 mines, F7 full health, F8 unjam the weapon, "
                "F9 Materials Generator."},
    {ID_DEBUGLOG, "Writes the section log of Zax.exe to Zax-Debug.log next to Zax.exe. The last open section shows "
                  "where a crash happened."},
    {ID_SHOWFPS, "Frame counter at the top of the screen (same as Show Frame Rate=1 in Zax.CFG)."},
    {ID_NETGRAPHS, "LAN game only: the network traffic graphs still in Zax.exe, at the bottom right; per player the "
                   "bytes sent (yellow) and received (green)."},
    {ID_DEBUGDRAW, "The developers' debug drawing: trigger areas, patrol paths, turret ranges, sound radii, particle "
                   "shapes, the AIs' target lines."},
    {ID_SHOWHIDDEN, "Needs Debug drawing: its debug shapes also for the parts that are invisible in the game (e.g. a "
                    "hidden trigger's area). The hidden parts themselves stay invisible."},
    {ID_OUTLINES, "Outlines of the draw order's strips (red rectangles, light lines)."},
    {ID_ALTDIRTY, "A leftover second way of redrawing the changed tiles: moving objects drawn whole, not tile run by "
                   "tile run (technical; the picture stays the same)."},
    {ID_NOENTRY, "Redraws the changed tiles without the objects on them (technical; objects may vanish)."},
    {ID_SINGLE, "No LAN game: single player with the settings above."},
    {ID_HOST, "Hosts a LAN game (-host): the game opens its host options, where you pick the map. Multiplayer always "
              "runs at 640 x 480; map, spawn point and difficulty do not apply."},
    {ID_JOIN, "Joins the LAN game of the host below (-connect). Multiplayer always runs at 640 x 480."},
    {ID_NAME, "Your player name in the LAN game (-name). Empty: the one of the game's multiplayer options."},
    {ID_ADDRESS, "IP address or computer name of the host to join."},
    {ID_CMDLINE, "The same start as a command line: append it to \"Target\" of a shortcut to Zax-Launcher.exe."},
    {ID_PACKS, "The packs (*.RED) next to Zax.exe in the order the game loads them; a later one wins. Unticked ones are left out "
               "of this start (/NoPack Name.RED); the files stay as they are."},
    {ID_PACKS_ALL, "Ticks every pack: the game loads them all, as without the launcher."},
    {ID_HELP, "All switches, the cheat keys and examples."},
    {IDOK, "Starts Zax.exe with this selection (remembered in Zax-Launcher.ini)."},
};

static void add_tooltips(void)
{
    HWND tip = CreateWindowExA(WS_EX_TOPMOST, TOOLTIPS_CLASSA, NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, g_wnd, NULL,
                               GetModuleHandleA(NULL), NULL);
    if (!tip)
        return;  /* without tooltips the help button still explains everything */
    SendMessageA(tip, TTM_SETMAXTIPWIDTH, 0, 360);  /* wrap long texts */
    SendMessageA(tip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 30000);
    for (unsigned i = 0; i < sizeof TIP / sizeof *TIP; i++) {
        TTTOOLINFOA ti;
        memset(&ti, 0, sizeof ti);
        ti.cbSize = TTTOOLINFOA_V2_SIZE;  /* the full size is rejected without a comctl32 6 manifest (XP) */
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = g_wnd;
        ti.uId = (UINT_PTR)item(TIP[i].id);
        ti.lpszText = (LPSTR)TIP[i].text;
        SendMessageA(tip, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    }
    add_fix_tooltips(tip);
}

static void create_controls(void)
{
    /* below the controls in the z-order, so they paint over it */
    HWND tabs = add(0, WC_TABCONTROLA, "", WS_TABSTOP | WS_CLIPSIBLINGS, 8, 8, 584, 324, ID_TABS);
    for (int i = 0; i < PAGES; i++) {
        TCITEMA tab;
        memset(&tab, 0, sizeof tab);
        tab.mask = TCIF_TEXT;
        tab.pszText = (LPSTR)PAGE_NAME[i];
        SendMessageA(tabs, TCM_INSERTITEMA, (WPARAM)i, (LPARAM)&tab);
    }
    SetWindowPos(tabs, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    g_build_page = PAGE_START;
    add(0, "STATIC", "Map (Data\\Levels and Data.dat):", 0, 20, 40, 300, 16, -1);
    HWND maps = add(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    20, 58, 300, 262, ID_MAPS);
    add(0, "STATIC", "Spawn point:", 0, 336, 40, 240, 16, -1);
    add(0, "COMBOBOX", "", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_SORT, 336, 58, 240, 300, ID_SPAWN);
    add(0, "STATIC", "Difficulty:", 0, 336, 90, 240, 16, -1);
    for (int i = 0; i < 3; i++)
        add(0, "BUTTON", DIFFICULTY[i], BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP | WS_TABSTOP : 0),
            340 + 80 * i, 108, 76, 18, ID_EASY + i);
    add(0, "STATIC", "Resolution:", WS_GROUP, 336, 136, 240, 16, -1);
    add(0, "COMBOBOX", "", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 336, 154, 240, 300, ID_RES);
    add(0, "STATIC", "Color depth:", 0, 336, 184, 240, 16, -1);
    add(0, "COMBOBOX", "", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 336, 202, 240, 200, ID_DEPTH);
    add(0, "BUTTON", "Skip start movies", WS_TABSTOP | BS_AUTOCHECKBOX, 336, 238, 240, 18, ID_SKIP);
    add(0, "BUTTON", "\x93Load Map\x94 button in game menu (Esc)", WS_TABSTOP | BS_AUTOCHECKBOX,
        336, 260, 240, 18, ID_LOADMAP);

    g_build_page = PAGE_LAN;
    add(0, "STATIC", "A LAN game with Zax.exe's own -host / -connect / -name switches.", WS_GROUP, 20, 44, 556, 16, -1);
    for (int i = 0; i < 3; i++)
        add(0, "BUTTON", LAN_MODE[i], BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP | WS_TABSTOP : 0),
            28 + 80 * i, 70, 72, 18, ID_SINGLE + i);
    add(0, "STATIC", "Player name:", WS_GROUP, 20, 105, 84, 16, -1);
    add(WS_EX_CLIENTEDGE, "EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL, 104, 102, 236, 20, ID_NAME);
    add(0, "STATIC", "Host address:", 0, 20, 131, 84, 16, -1);
    add(WS_EX_CLIENTEDGE, "EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL, 104, 128, 236, 20, ID_ADDRESS);
    add(0, "STATIC", "Multiplayer always runs at 640 x 480. The host picks the map in the game's host options, so\n"
                     "the map, spawn point and difficulty of the Start tab do not apply.",
        0, 20, 166, 556, 32, -1);

    g_build_page = PAGE_DEVELOPER;
    add(0, "BUTTON", "Cheats (Ctrl+F1 ... Ctrl+F9, see Help)", WS_TABSTOP | BS_AUTOCHECKBOX,
        20, 44, 300, 18, ID_CHEATS);
    add(0, "BUTTON", "Debug log (Zax-Debug.log)", WS_TABSTOP | BS_AUTOCHECKBOX, 20, 66, 300, 18, ID_DEBUGLOG);
    add(0, "BUTTON", "Frame counter (FPS)", WS_TABSTOP | BS_AUTOCHECKBOX, 20, 88, 300, 18, ID_SHOWFPS);
    add(0, "STATIC", "Still in Zax.exe, but never shown:", 0, 20, 120, 556, 16, -1);
    for (int i = 0; i < DRAW_SWITCHES; i++)
        add(0, "BUTTON", DRAW_SWITCH[i].text, WS_TABSTOP | BS_AUTOCHECKBOX, 20, 140 + 22 * i, 300, 18, DRAW_SWITCH[i].id);

    create_packs_page();
    create_fixes_page();

    g_build_page = PAGE_ALL;
    add(0, "STATIC", "Shortcut target:", 0, 12, 344, 90, 16, -1);
    add(WS_EX_CLIENTEDGE, "EDIT", "", WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL, 104, 340, 484, 22, ID_CMDLINE);
    add(0, "BUTTON", "Help", WS_TABSTOP, 12, 372, 88, 26, ID_HELP);
    add(0, "BUTTON", "Start", WS_TABSTOP | BS_DEFPUSHBUTTON, 404, 372, 88, 26, IDOK);
    add(0, "BUTTON", "Exit", WS_TABSTOP, 500, 372, 88, 26, IDCANCEL);

    /* Maps sorted, preceded by the normal start */
    for (int i = 1; i < g_map_count; i++)
        for (int k = i; k > 0 && lstrcmpiA(g_maps[k - 1].name, g_maps[k].name) > 0; k--) {
            struct map t = g_maps[k];
            g_maps[k] = g_maps[k - 1];
            g_maps[k - 1] = t;
        }
    SendMessageA(maps, LB_ADDSTRING, 0, (LPARAM)MAIN_MENU);
    for (int i = 0; i < g_map_count; i++)
        SendMessageA(maps, LB_ADDSTRING, 0, (LPARAM)g_maps[i].name);

    /* last selection from Zax-Launcher.ini */
    char map[300], spawn[300], diff[32];
    GetPrivateProfileStringA("Launcher", "Map", "", map, sizeof map, g_ini);
    GetPrivateProfileStringA("Launcher", "Spawn", "", spawn, sizeof spawn, g_ini);
    GetPrivateProfileStringA("Launcher", "Difficulty", "Normal", diff, sizeof diff, g_ini);
    LRESULT sel = map[0] ? SendMessageA(maps, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)map) : 0;
    SendMessageA(maps, LB_SETCURSEL, sel == LB_ERR ? 0 : (WPARAM)sel, 0);
    int d = 1;
    for (int i = 0; i < 3; i++)
        if (!lstrcmpiA(diff, DIFFICULTY[i]))
            d = i;
    SendMessageA(item(ID_EASY + d), BM_SETCHECK, BST_CHECKED, 0);
    SendMessageA(item(ID_SKIP), BM_SETCHECK, ini_int("SkipMovies", 0) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(item(ID_LOADMAP), BM_SETCHECK, ini_int("LoadMap", 0) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(item(ID_CHEATS), BM_SETCHECK, ini_int("Cheats", 0) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(item(ID_DEBUGLOG), BM_SETCHECK, ini_int("DebugLog", 0) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(item(ID_SHOWFPS), BM_SETCHECK, ini_int("ShowFPS", 0) ? BST_CHECKED : BST_UNCHECKED, 0);
    for (int i = 0; i < DRAW_SWITCHES; i++)
        SendMessageA(item(DRAW_SWITCH[i].id), BM_SETCHECK, ini_int(DRAW_SWITCH[i].arg + 1, 0) ? BST_CHECKED : BST_UNCHECKED, 0);
    char lan[16], text[300];
    GetPrivateProfileStringA("Launcher", "LAN", "Off", lan, sizeof lan, g_ini);
    int m = 0;
    for (int i = 0; i < 3; i++)
        if (!lstrcmpiA(lan, LAN_MODE[i]))
            m = i;
    SendMessageA(item(ID_SINGLE + m), BM_SETCHECK, BST_CHECKED, 0);
    SendMessageA(item(ID_NAME), EM_LIMITTEXT, 31, 0);
    SendMessageA(item(ID_ADDRESS), EM_LIMITTEXT, 255, 0);
    GetPrivateProfileStringA("Launcher", "PlayerName", "", text, sizeof text, g_ini);
    SetWindowTextA(item(ID_NAME), text);
    GetPrivateProfileStringA("Launcher", "HostAddress", "", text, sizeof text, g_ini);
    SetWindowTextA(item(ID_ADDRESS), text);
    enable_lan();
    enable_hidden();
    fill_modes();
    fill_spawns(spawn);
    build_args();
    add_tooltips();
    int page = ini_int("Page", PAGE_START);
    show_page(page >= 0 && page < PAGES ? page : PAGE_START);
    if (g_page == PAGE_START)
        SetFocus(maps);
}

static int g_start;
static struct edition g_edition;   /* of the Zax.exe next to the launcher */

static LRESULT CALLBACK wnd_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_NOTIFY:
        if (((NMHDR *)lp)->idFrom == ID_TABS && ((NMHDR *)lp)->code == TCN_SELCHANGE)
            show_page((int)SendMessageA(item(ID_TABS), TCM_GETCURSEL, 0, 0));
        if (((NMHDR *)lp)->idFrom == ID_PACKS && ((NMHDR *)lp)->code == LVN_ITEMCHANGED && g_packs_ready)
            build_args();
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == ID_MAPS && code == LBN_SELCHANGE)
            fill_spawns("");
        if (id >= ID_SINGLE && id <= ID_JOIN)
            enable_lan();
        if (id == ID_DEBUGDRAW)
            enable_hidden();
        if (id == IDOK || (id == ID_MAPS && code == LBN_DBLCLK)) {
            if (checked(ID_JOIN) && GetWindowTextLengthA(item(ID_ADDRESS)) == 0) {
                show_page(PAGE_LAN);
                MessageBoxA(wnd, "Join needs the host's address (IP address or computer name).", "Zax-Launcher",
                            MB_ICONWARNING);
                SetFocus(item(ID_ADDRESS));
                return 0;
            }
            build_args();
            save_settings();
            g_start = 1;
            DestroyWindow(wnd);
            return 0;
        }
        if (id == IDCANCEL) {
            DestroyWindow(wnd);
            return 0;
        }
        if (id == ID_HELP)
            show_help(wnd);
        if (id == ID_PACKS_ALL)
            for (int i = 0; i < g_pack_count; i++)
                ListView_SetCheckState(item(ID_PACKS), i, TRUE);
        if (id == ID_HARDENING && code == BN_CLICKED)
            click_hardening();
        if (id >= ID_FIX + FIX_FIRST_HARDENING && id < ID_FIX + FIXES)
            sync_hardening();
        if (id == ID_HARDENING_LIST) {
            g_hardening_shown = !g_hardening_shown;
            show_page(PAGE_FIXES);
        }
        if (id != ID_CMDLINE)
            build_args();
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(wnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(wnd, msg, wp, lp);
}

static int launcher(void)
{
    exe_dir(g_dir);
    lstrcpyA(g_ini, g_dir);
    lstrcatA(g_ini, "Zax-Launcher.ini");
    char rules[MAX_PATH + 32];
    wsprintfA(rules, "%sData\\Rules.txt", g_dir);
    DWORD size;
    char *text = read_file(rules, &size);
    if (text) {
        each_line(text, size, rules_line, NULL);
        HeapFree(GetProcessHeap(), 0, text);
    }
    scan_maps("");
    scan_zip();
    scan_packs();

    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hIcon = LoadIconA(wc.hInstance, MAKEINTRESOURCEA(1));  /* from build.sh, missing without Zax.exe */
    if (!wc.hIcon)
        wc.hIcon = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "Zax-Launcher";
    RegisterClassA(&wc);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r = {0, 0, 600, 406};  /* with the frame below 640 x 450: a 640 x 480 desktop with its task bar */
    AdjustWindowRect(&r, style, FALSE);
    int w = r.right - r.left, h = r.bottom - r.top;
    char title[96];
    wsprintfA(title, "Zax-Launcher - %s", g_edition.name);
    if (g_edition.kind == EDITION_UNKNOWN)
        lstrcatA(title, ": the switches stay off");
    g_wnd = CreateWindowExA(WS_EX_CONTROLPARENT, "Zax-Launcher", title, style,
                            (GetSystemMetrics(SM_CXSCREEN) - w) / 2, (GetSystemMetrics(SM_CYSCREEN) - h) / 2,
                            w, h, NULL, NULL, wc.hInstance, NULL);
    if (!g_wnd)
        return fail("Could not create the launcher window");
    INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_WIN95_CLASSES};  /* tabs and tooltips */
    InitCommonControlsEx(&icc);
    create_controls();
    if (!g_map_count)
        MessageBoxA(g_wnd, "No maps found under Data\\Levels or in Data.dat; Zax-Launcher.exe belongs in the folder of Zax.exe.",
                    "Zax-Launcher", MB_ICONWARNING);
    ShowWindow(g_wnd, SW_SHOW);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        /* Ctrl+Tab and Ctrl+Shift+Tab switch the tabs, as in a property sheet */
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB && GetKeyState(VK_CONTROL) < 0) {
            show_page((g_page + (GetKeyState(VK_SHIFT) < 0 ? PAGES - 1 : 1)) % PAGES);
            continue;
        }
        if (!IsDialogMessageA(g_wnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    return g_start ? launch(g_args) : 0;
}

/* The edition of the Zax.exe next to the launcher (a missing Zax.exe is reported when starting it) */
static void detect_edition(void)
{
    char path[MAX_PATH + 16];
    exe_dir(path);
    lstrcatA(path, "Zax.exe");
    DWORD size = 0;
    BYTE *image = read_file(path, &size);
    edition_detect(&g_edition, image, size, 0);
    if (image)
        HeapFree(GetProcessHeap(), 0, image);
}

static int run(void)
{
    const char *args = own_args();
    if (has_switch(args, "/?") || has_switch(args, "/help") || has_switch(args, "-?") || has_switch(args, "-help")) {
        show_help(NULL);
        return 0;
    }
    detect_edition();
    if (g_edition.kind == EDITION_SECUROM) {
        char text[160];
        wsprintfA(text, "This Zax.exe (%s) is copy-protected and not supported by the launcher.", g_edition.name);
        MessageBoxA(NULL, text, "Zax-Launcher", MB_ICONINFORMATION);
        return 1;
    }
    if (!*args)
        return launcher();
    return launch(args);
}

void start(void)
{
    ExitProcess((UINT)run());
}
