#!/usr/bin/env bash
# Optional: recreate the supplied Windows binaries with Zig 0.13.0.
set -eu
cd "$(dirname "$0")"
: "${ZIG:=zig}"
mkdir -p bin
"$ZIG" rc /i resources /fo bin/app.res resources/app.rc
"$ZIG" c++ -target x86_64-windows-gnu -std=c++17 -O2 -g0 -Iindependent-opm/include \
    bin/app.res src/gui.cpp src/formats.cpp src/player.cpp monitor_core/ym2151.cpp \
    -lwinmm -lcomdlg32 -lshell32 -luser32 -lgdi32 -ladvapi32 -lcomctl32 -lshlwapi -luxtheme -lgdiplus -municode -Wl,--subsystem,windows \
    -o bin/IndependentMDX.exe
"$ZIG" c++ -target x86_64-windows-gnu -std=c++17 -O2 -g0 -Iindependent-opm/include \
    src/cli.cpp src/formats.cpp src/player.cpp monitor_core/ym2151.cpp \
    -lwinmm -municode -o bin/independent-mdx.exe
"$ZIG" c++ -target x86-windows-gnu -std=c++17 -O2 -g0 src/x68_helper.cpp -municode -o bin/IndependentMDX-X68Bridge32.exe
cp THIRD_PARTY_NOTICES.txt bin/THIRD_PARTY_NOTICES.txt
