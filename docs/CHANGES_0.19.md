# IndependentMDX 0.19 — UI and rendering load

## Changes

- Right pane header: View / Carrier pitch / Waveforms on the left; instruction on the right, vertically aligned. The waveform checkbox defaults ON and uses the existing two-second settings save. The instruction has an ellipsis and full-text tooltip when space is limited.
- MML export suggests the current MDX basename with .mml; the overwrite guard remains.
- Speed (0–200), key (-24–24) and master (0–400) trackbars support absolute page clicks. Native thumb dragging, keyboard operation and live value handlers remain.
- FM delay, PCM delay and their next-playback notice are adjacent in that order. Title height is two font lines plus padding, retaining track count / PDX text.
- MIDI dropdown enumeration occurs on opening, including when No MIDI input is selected. Items store actual device IDs, restore selection by name, and fall back to None if the selected device is gone. Identical device names cannot uniquely identify physically identical units.
- Main-window language updates suppress parent/child redraw, including browser rebuilding, until caption changes and layout are complete. Unchanged transport text/positions are skipped.

## Identified load / contention

1. `refreshVoiceWindow` held `VoiceState::mutex` during list insertion and painting. Audio publishing also needs that mutex. It now copies revision/defined/used metadata while locked, then performs all list operations after releasing the lock.
2. Keyboard/piano-roll painting allocated/deleted GDI brushes for individual keys/notes each frame. It now uses DC_BRUSH with per-fill colors.
3. The private display-only FM core recalculated frequency with exp2 for all 32 operators at 62,500 Hz, even with unchanged registers/pitch modulation. Sustained attenuation also repeated pow. Exact-input caches reuse these results without approximation. Clock/state restores invalidate caches. The audio DLL and its sources are unchanged.
4. Waveforms OFF suppresses the private FM model advance for MDX and MIDI, and line drawing. Note/mask tracking remains active. When re-enabled, the visual model resets from current registers and held keys; its envelope/phase starts anew, while audible playback is uninterrupted.
5. Playback speed is sampled once per audio render block rather than once per native sample. At 48 kHz / 1,024 frames this preserves live response within a render block.

## Local benchmark

Linux, g++ -O2, synthetic eight-FM-channel MDX, ten seconds, external-FM mode to isolate display computation. One local run; includes sequence processing and waveform hashing, excludes Windows DLL, UI, USB and device scheduling.

| Mode | Rendering wall time |
|---|---:|
| 0.18 waveform ON | 0.861963 s |
| 0.19 waveform ON | 0.414324 s |
| 0.19 waveform OFF | 0.0576839 s |

ON-to-ON time decreased about 52%. The waveform hash matched exactly (18169114322324321390). A separate 2,000-step deterministic register/key/state-restore probe also matched (14770186152229520632). These are not measurements of overall Windows playback load or guarantees against dropouts. Expensive audio DLL synthesis, OS scheduling and hardware drivers can still limit real-time playback.

## Verification

63 native application test groups pass, including 1,000 malformed MDX mutations and a new live waveform OFF/ON test: audible samples remain identical and note reporting continues. Existing timing, LFO, seek, MIDI, PCM and C86 register-order regressions remain included.

Windows x64 GUI/CLI and GUI tests cross-compile. The unchanged x86 bridge is retained. GUI tests cover waveform control wiring/default, absolute slider targets including the negative key range, MIDI None retention on refresh, bilingual geometry and toolbar placement. They have NOT been executed. Windows font/DPI layout, flicker, physical MIDI insertion/removal, real-time audio and C86CTL/X68Sound devices must still be checked on Windows.

The Independent OPM DLL remains the byte-identical 0.18 release package DLL. No new upstream test/evidence bundle is included.
