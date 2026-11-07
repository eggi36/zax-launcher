#!/bin/bash
# Builds Zax-Launcher.exe and Zax-Launcher.dll (32-bit Windows, XP and later) with zig, without a C
# runtime library: both only need kernel32, user32, gdi32 (fonts) and comctl32 (tooltips of the start window).
# Usage: ./build.sh [output dir]   (default: out/ of this repository)
# Icon: make_icon.py creates it from the icon of Zax.exe (ZAX_EXE, otherwise the Zax.exe next to $ZAX_DATA, the folder Data or Data.dat);
# without Zax.exe the exe gets the default Windows icon. The icon is not added to the repository.
set -e
cd "$(dirname "$0")"
OUT=$(mkdir -p "${1:-out}" && cd "${1:-out}" && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
TARGET=x86-windows.xp-gnu
CFLAGS="-target $TARGET -Os -Wall -Wextra -fno-stack-protector -mno-stack-arg-probe -fno-builtin"
LINK="-target $TARGET -lkernel32 -luser32 -lgdi32 -lcomctl32 -O ReleaseSmall -fstrip"
zig cc $CFLAGS -c zax_launcher.c -o "$TMP/zax_launcher.o"
RC=
ZAX_EXE=${ZAX_EXE:-${ZAX_DATA:+$(dirname "$ZAX_DATA")/Zax.exe}}
if [ -n "$ZAX_EXE" ] && [ -f "$ZAX_EXE" ]; then
  PY=.venv/bin/python; [ -x "$PY" ] || PY=python3   # make_icon.py needs pefile and Pillow (requirements.txt)
  "$PY" make_icon.py "$ZAX_EXE" "$TMP/Zax-Launcher.ico"
  printf '1 ICON "Zax-Launcher.ico"\r\n' > "$TMP/zax_launcher.rc"
  RC=zax_launcher.rc
else
  echo "Note: no Zax.exe (set ZAX_EXE or ZAX_DATA), building without icon" >&2
fi
zig cc $CFLAGS -c zax_launcher_dll.c -o "$TMP/zax_launcher_dll.o"
(cd "$TMP" &&
 zig build-exe zax_launcher.o $RC $LINK --subsystem windows -fentry=start --name Zax-Launcher &&
 zig build-lib -dynamic zax_launcher_dll.o $LINK -fentry=DllMain@12 -fno-dll-export-fns --name Zax-Launcher)
cp "$TMP/Zax-Launcher.exe" "$TMP/Zax-Launcher.dll" "$OUT/"
ls -l "$OUT"
