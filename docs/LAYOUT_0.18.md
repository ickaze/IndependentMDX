> Historical release note. These details describe the named earlier version; see README.md and docs/VALIDATION.md for the current distribution.

# IndependentMDX 0.18

## Control layout

The existing file-opening, PDX, license and language controls remain at the top; the browser retains its navigation, filters, bookmarks, columns and sorting. Central rows are grouped as follows:

| Group | Controls |
|---|---|
| Playback | Play / Pause / Stop / WAV, position/time, loops, auto-next, PDX preview, speed, key and key mode |
| Volume | Master slider/value, FM and PCM gain |
| Output | Output device, FM/PCM delay, measured timing and YM3012 skew |
| MIDI | MIDI device, Mono/Poly, aftertouch, instrument viewer and MML export |

View and Carrier pitch occupy a right-aligned toolbar above the keyboard pane. Label columns and slider/value endpoints are shared. Both language captions are measured, including translated dropdown alternatives; geometry is independent of the selected language. Dropdowns use their widest option plus arrow padding. Longer device names and high DPI can require the existing horizontal/vertical scrolling. The browser still receives the remaining vertical area with a minimum list height. Settings and event bindings use the existing control IDs.

## Runtime DLL and retained source

Source archive for the runtime update: independent-opm-0.11b-windows-x64.zip.

- bin/independent_opm.dll SHA-256: e24a3356edc62e7cd677a89de13ee1db67fc4b37fc9e9ce1e4d13fbc91d861dd
- independent-opm/include/independent_opm.h SHA-256: 9fe60b782106fa4e332d363b73a9cca220583e3985cb8bf34469648c5b1f2093
- independent-opm/LICENSE.txt SHA-256: 35618964ed5a420911eb3797fd0f576e8158abff4d481c6f3a4a19932b3ddeff

The original release DLL is copied unchanged. Its header, license, API.md and README (renamed DLL_README.md) are included. Root/bin notices carry the release license, including Copyright (C) 2026 by I.C.KaZe. The source-subset THIRD_PARTY_NOTICES is retained from the previous source archive; its reference to upstream docs/PROVENANCE.md points outside this intentionally trimmed distribution.

The binary package contains no implementation source. Build sources and the private waveform monitor therefore retain the 0.17 source revision and existing monitor patch. This is a runtime DLL update, not a claim that those retained sources reproduce the new DLL bit-for-bit. Upstream tests and validation bundles are not added. The application's own tests remain available.

## Verification limits

GUI, CLI and GUI test executables cross-compile. GUI geometry assertions have not been executed, and there is no native Windows audio/DPI/visual validation in this environment. Prior software-engine and hardware-related validation retains the scope stated in VALIDATION.md.
