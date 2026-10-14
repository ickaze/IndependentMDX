# IndependentMDX 0.31 — validation and release scope

## Implemented

MRL 1.2 loop metadata, loop-aware playback, finite WAV export, seek-progress callback/UI, missing-PDX fallback, ZIP filename titles, All files filtering, sidecar DOC icons, a modeless text viewer and Explorer copy drags. Existing INI values are preserved; ShowAll is a new setting defaulting to off. The bundled Independent OPM 0.14 DLL is unchanged.

## Executed

Native Linux Release CTest: 5/5 suites passed. New checks exercise MRL finite/infinite playback, position wrapping, requested capture-count retention, loop bounds, progress callbacks, missing embedded-bank fallback and archive materialization for copying. An end-to-end CLI MRZ-to-WAV check with --loops 0 produced two sections plus the requested 0.1-second fade (12.1101 seconds, within one output block of the expected duration). All 80 original LFO diagnostic MDX inputs exported successfully with one loop in legacy MRL format.

Random-wave and missing-voice investigations are documented separately in LFO_COMPARISON_0.31.md. They do not establish a new exact RNG or an alternative undefined-voice behavior. The earlier MRL_VALIDATION.html remains the 0.30 sequencer comparison report.

## Build-only / not verified

Windows GUI/CLI and the x86 X68Sound bridge are cross-built. Windows-only GUI regression tests include DOC icons, viewer toggling without stopping playback and the All files filter; they are compiled but not executed here. Actual Explorer drag/drop, text rendering, DPI layout, MIDI, C86CTL and X68Sound require Windows runtime verification. Native core tests do not constitute a Windows visual/device test.

PE inspection checks no CodeView/PDB/build paths and no VC++ runtime DLL imports in the application executables. Windows system libraries remain required. DLL SHA-256 is recorded in LICENSE_AUDIT.md. SHA256SUMS.txt covers the distribution.
