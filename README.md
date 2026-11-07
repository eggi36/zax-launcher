# zax-launcher

Zax-Launcher.exe, a launcher for the original Zax.exe of "Zax: The Alien Hunter" (Reflexive Entertainment / JoWooD, 2001). It starts Zax.exe from the same folder with Zax-Launcher.dll, which patches the running exe in memory; Zax.exe itself is not modified.

    Zax-Launcher.exe [/Map "Act 1/M1M2/01 Main" [/Spawn "Game Start"] [/Difficulty Normal]]
                     [/NoLoadMap] [/Movies] [/Cheats] [/DebugLog] [/ShowFPS] [/NetGraphs] [/DebugDraw] [/ShowHidden]
                     [/DrawOutlines] [/AltDirtyDraw] [/NoEntryDraw] [/Original] [/Resolution 1024x768]
                     [-host | -connect <address>] [-name <player>]

- Start directly into a map (no main menu, no intro), at a start point and difficulty of your choice.
- A "Load Map" button in the game menu (Esc) that loads any map under `Levels`.
- The developer switches still present in Zax.exe: debug log, frame rate, network graphs, the AIs' debug drawing, hidden parts, outlines, the developer cheats.
- Without arguments a start window opens (map, start point, difficulty, resolution, switches, LAN game); `/?` or `/help` shows the help.

The launcher works with the original Zax.exe alone (German build 1308; the addresses it patches are that build's). It knows nothing of the port: the decompilation (zax-decomp) and the port (zax-redone) are separate repositories; if the decompilation's `Zax-Reimpl.dll` sits next to Zax.exe, the launcher loads it as well (`/Original` leaves out its replacement functions). How each switch works in the exe, with addresses and evidence: zax-decomp's `docs/EXE_FINDINGS.md` and `docs/DECOMPILATION.md`.

## Build

macOS or Linux with [zig](https://ziglang.org) (`brew install zig`): `./build.sh [output dir]` (default `out/`) builds `Zax-Launcher.exe` and `Zax-Launcher.dll` for 32-bit Windows (XP and later), without a C runtime: they need only kernel32, user32, gdi32 and comctl32.

The icon is taken from your own Zax.exe at build time (`ZAX_EXE`, else the Zax.exe next to `ZAX_DATA`; `make_icon.py` needs `pip install -r requirements.txt`); without it the exe gets the default Windows icon. Nothing of the game is part of this repository.

## Install

Copy `Zax-Launcher.exe` and `Zax-Launcher.dll` next to `Zax.exe` of an installed game and start `Zax-Launcher.exe`.
