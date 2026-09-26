# IndependentMDX 0.24 — validation

## 0.24 monitor correction

DEMO.MDX reproduced a silent channel B visualization before the correction.
Its ALG4 voice uses raw KON mask 5: register slots M1/C1 (0/2).
The display-only core incorrectly swapped KON bits 4 and 5. It now maps
bits 3..6 directly to register slots 0..3. The runtime DLL is unchanged.
The existing ALG5 monitor fixture now uses the correct raw mask 5 too.
The new DEMO regression checks channel B waveform output and verifies that
visual synthesis does not leak into external-FM audio. All 67 native groups
pass; observed channel B monitor peak was 0.00888221. Windows GUI/CLI were
cross-built; native Windows listening/UI validation remains unavailable.

The monitor remains a separate estimated-waveform implementation. Replacing
the DLL alone does not update it. The supplied C ABI has stereo output but no
per-channel output API; a future DLL API extension is needed to remove this
separate model without rendering multiple full chip instances.

## 0.24 verification

66 native groups pass, including first-definition retention for duplicate instrument IDs, continued loading of distinct IDs and identical audio to the first definition alone. Windows GUI/CLI are rebuilt with this change. The remaining unsupported-opcode limitation reported for 0.23 remains; investigation is closed. Runtime OPM DLL and x86 bridge are unchanged. Native Windows listening remains unverified.

## 0.23 verification

65 native regression groups pass. All 0x80..0xFF values of MH, MP and MA produce the same timestamped register trace as canonical 0x80/0x81 according to bit0. Low-mode unused waveform bits leave audio unchanged; reserved PMS/AMS bits are cleared. Numeric-parameter diagnostics still include offset, value and range. Previous playback, LFO, PCM, MIDI and layout cases remain included.

The reported input passes the switch check but reaches unsupported opcode 0xE3 at channel B / file offset 0x378. This is not a complete playback fix. Unsupported opcode interpretation and entry reconstruction are outside this change. No identifying strings or dedicated pattern recognizers are introduced.

Windows GUI/CLI are rebuilt. Native Windows audio and physical devices are not tested here.

## 0.22 verification

64 native application groups pass, including 1,000 malformed-input mutations. Generic layout tests cover 9/16 channels, voice-first versus track-first sample-identical output (therefore identical startup timing), reordered channel entries, shared offsets, header-overlapping/EOF-exceeding offsets and every truncated table prefix. Previous dedicated fixtures and associated analysis records are removed.

The loader uses only bounded relative offsets and compact table boundaries. It neither identifies producer strings nor reconstructs entry points from fixed bytecode patterns. Default sequencer tempo/wait and first-definition-wins duplicate-voice handling apply uniformly. Layouts that cannot be established remain errors; no guarantee of recovery for arbitrary malformed files is made.

Windows GUI/CLI cross-build successfully. No native Windows listening/UI validation is available. The OPM DLL and x86 bridge are unchanged.

## 0.21 verification

64 native application groups pass, including 1,000 malformed MDX mutations. New square-pitch cases compare every KC/KF update against base + sign * amplitude in 1/64-semitone units. Amplitudes 1 and -24 each test WAVE1 with D=256/-6144 and WAVE5 with D=1/-24. Stored P=4 must invert polarity every four ticks. Existing software-LFO gate/ON-OFF, MA, triangle, MD/tie, hardware LFO and portamento tests remain passing.

The compiler-side conversion is not tested: no MDX from the user's compiler was supplied. The MML parameter description was checked against the MXC.COM manual excerpt at https://w.atwiki.jp/mxdrv/pages/19.html (period parameter and maximum amplitude). The synthetic raw-MDX fixtures test the implemented unit conversion; they do not claim a full original-MXDRV comparison. No third-party implementation was copied.

Windows GUI/CLI are rebuilt; no native Windows listening test is available. Audio DLL and x86 bridge are unchanged.

## 0.20 verification

63 native application groups pass, including 1,000 malformed-input mutations. The former release-continuation test is replaced with held-note MP/MA updates and absence of software-LFO pitch/TL writes after key-off in three cases: final release, a rest and the silent tail of a q1 note. Existing full-note portamento, software LFO ON/OFF, MD/tie and OPMLFO tests pass.

Windows x64 GUI and CLI are rebuilt. GUI code changes only the version caption; the x86 bridge, runtime OPM DLL and GUI layout are unchanged. No native Windows listening test is available. Prior release-continuation assertions below are historical and are superseded by this reversal.

## 0.19 verification

63 native application groups pass, including waveform OFF/ON audio identity and note retention. Private-monitor cache validation matches the old waveform hash on eight-channel playback and a 2,000-step changing-register/key/state-restore probe. See CHANGES_0.19.md for timings, scope and limitations.

Windows GUI/CLI and GUI regression executable cross-compile. GUI tests are NOT run. Native Windows redraw, physical MIDI hotplug and real-time audio remain unverified. Existing source tests are retained; the runtime DLL is unchanged from 0.18.

## 0.18 verification

Windows x64 GUI and CLI and the GUI regression executable cross-compile successfully with Zig 0.13.0. The unchanged x86 X68Sound bridge is retained from 0.17. New GUI test assertions cover identical item rectangles across English/Japanese/English, aligned slider endpoints and numeric fields, non-overlapping controls and the right-aligned keyboard toolbar. These tests are compiled only, NOT executed. No native Windows or Wine is available.

The DLL is byte-identical to the supplied Windows x64 release package and its SHA256SUMS entry. The C ABI header is byte-identical to the previously used header. Runtime loading, audio and actual DPI/font rendering are unverified. The engine is unchanged; the 62-group Linux results below belong to 0.17, not a fresh 0.18 run. Earlier DLL test results refer to their original packages and do not establish runtime validation of this replacement DLL.

Manual Windows checks: switch Japanese/English without moving controls; inspect all dropdown options, numeric fields and long MIDI device names at 100/125/150% DPI; resize/split while playing; check the right toolbar and browser at small window heights; select each output and verify playback. See LAYOUT_0.18.md for package provenance.

## Previous verification (0.17 and earlier)


0.17 adds q1/q8 full-note portamento trace equivalence (four descending octaves with ties, including quantized release), rest/end termination, and physical-output voice programming order. The platform-independent C86 gain mapper is tested with a captured register sink: final TL before output restoration, pan-only traffic, duplicate TL suppression, unchanged gain, zero-gain mute, gain restoration and carrier/modulator reassignment. These are software tests, not hardware recordings.

The latest upstream full-state test was built and run separately: core state round-trip, four LFO shapes, timers/BUSY/latch/release, failed-load atomicity, C API filter/remainder restore, and identical next 4,096 frames passed. Upstream test sources and evidence are intentionally not distributed. The application's own tests remain. The supplied Windows DLL exports all three state functions and is copied byte-for-byte; native DLL execution was not available.


New regressions cover direct y-only instrument programming without @, retained instrument data, MXDRV MML parameter round-trip, finite/looped timeline analysis, analysis cancellation, loop-position wrapping, sample-exact seek versus continuous synthesis, and MIDI note/volume/bend/all-sounds-off through retained voices. Existing PCM pitch-shift, live mask, LFO, missing-bank/voice and malformed input regressions remain included. FM live-mask comparisons now allow the C API FIR's initial settling tail (64 output frames); synthesis after settling must still match.

Windows x64 application and CLI are cross-built with Zig 0.13.0; the supplied DLL is preserved unchanged. GUI test source covers title-column defaults, output/MIDI choices, viewer creation, existing multilingual bounds, preferences and browser regressions. GUI tests are cross-compiled but NOT run. No Wine/native Windows, MIDI input device or C86CTL hardware is available here. DLL export inspection is not runtime validation. Physical output, USB timing and device disconnection must be checked on Windows.

## Windows build and verification

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Manual checks: title/name columns visible independently; seek while playing/paused and at loop boundaries; MIDI notes before/after stop and while masked; MML copy while parameters change; C86CTL switch while playing, stop and application close (silence/reset); missing/incompatible DLL; OPM device disconnect. Hardware resets report failures when the API exposes a failure status; destruction also attempts reset before release.

## 0.7 verification

- Program Change before MDX playback, bank-1 mapping and ignoring undefined programs are covered in the core suite.
- Every algorithm edge points to a strictly later layer; carriers occupy the final layer. Algorithm 0 is a serial chain and algorithm 7 is four parallel nodes.
- Existing monitor-on/off bit-exact audible comparison and channel-isolation tests still pass after replacing the monitor instances.
- Windows GUI tests additionally cover remembered file/PDX persistence, catalog population, definition selection and return to live channel mode. They are cross-compiled, not run.
- Native Release benchmark: same 8-channel, 10-second synthetic MDX with monitoring enabled, 0.6 5.963 seconds versus 0.7 1.391 seconds. Single-run local measurements; not Windows DLL/C86CTL hardware results. Rendering durations remain 10 seconds.
- C86CTL scheduled writes no longer sleep in the renderer. The sender and reset serialize on the same mutex; reset clears all pending events. Physical hardware is unavailable for timing validation.

## 0.8 verification

New groups cover fast forward/backward seek with audible resumption and advancing clock; masked song writes after MIDI Note On/Off; high-number catalog selection; MHOF/MHON across MD; tie continuity; and per-tick WAVE2 values. The previous exact-seek core path remains tested separately; GUI uses the faster state reconstruction. Windows GUI tests additionally check the three output choices and voice-selection request. Windows x64 GUI/CLI and x86 helper are cross-built. X68Sound DLL is not supplied and its runtime output/IPC have not been exercised here.

Fast-seek benchmark (Linux Release, 8 FM tracks): reconstructing position 600 seconds took 0.747 seconds; the next block contained nonzero audio and advanced the clock to 600.043 seconds. This is a local single-run measurement, not Windows hardware timing.

## 0.9 verification

Additional core cases cover eight FM voices, oldest-voice stealing, stale Note Off, sustain routing, destination-specific instruments, eight PCM voices and isolated Note Off, pause/resume instrument restoration and advancing song clock. The X68 batch renderer is compared sample-by-sample with the former single-sample scheduling model over four blocks and register changes, including retained fractional phase. A three-event block requires three GetPcm calls. The hardware-LFO test now explicitly uses EA80=OFF and EA81=ON. GUI tests cover an absolute click at 75% of the transport. Windows GUI tests are cross-compiled, not executed.

## 0.10 verification

Seven Linux Release CTest suites pass, including 49 application groups. New cases cover explicit MH after pending MD/MHOF, borrowing an input instrument into undefined destinations without overwriting retained voices, polyphonic note pressure versus channel pressure, switching to monophonic playback, sample-exact 10ms PCM delay including its drained tail, and live zero/400% master and PCM gain. Existing pause/resume restoration and X68 rational block-render timing tests remain included.

Windows GUI/CLI x64 and X68 helper x86 are cross-built. GUI regression code additionally checks the pointer position and thumb center, new controls and persisted preferences; it is compiled, NOT executed. The GUI PE contains the seven icon resolutions. No actual Windows GUI, X68Sound DLL, MIDI device or C86CTL device was exercised. The OpmWait diagnosis is source-based and the IPC pipeline's audible performance needs Windows verification; no measured dropout-resolution claim is made.

Manual Windows checks: MH immediately after MD; blank clicks near both ends and middle, followed by dragging; multiple MIDI channels with distinct tones and resume; both pressure types; Mono/Poly switching while holding notes; C86 FM/PCM offsets and zero master; roll overlap/mutes and both languages; X68 continuous playback with restored waves and frequent register changes.

## 0.11 verification

Core suite: 51 groups. Corrected pitch-LFO regression verifies EC80 OFF and EC81 ON; added volume-register regression verifies EB80 OFF and EB81 ON. Mono and Poly MIDI tests check aftertouch default OFF, explicit enabling, final key release, late zero-pressure packets, fresh-note volume and disabling during a held note.

Windows GUI regressions additionally cover per-output defaults/isolation/persistence, the aftertouch switch and virtual piano-roll clock freezing/resuming. GUI tests are cross-compiled only. Seven Linux CTest suites and Windows x64 GUI/CLI plus x86 bridge builds are validated; physical Windows audio/MIDI/C86CTL remain untested.

## 0.12 verification

53 application groups: the new OPMLFO case compares complete audio and timestamped register writes with/without MD across retriggers and MHOF/MHON. Both must be identical. Another case places full MH and MHOF/MHON on PCM track P and verifies identical FM audio and register traces to a file without those commands. Existing software LFO delay/tie and switch tests remain passing. Seven Linux CTest suites pass.

Windows GUI regression source checks swapped control rows, selector bounds at the default window size, increased keyboard width after splitting, fullscreen style/placement restoration, and persisted split ratio. GUI tests are cross-compiled, not executed. Actual Windows rendering, DPI changes and physical audio devices remain unverified here.

## 0.13 verification

Windows x64 GUI/CLI and x86 bridge builds succeed. GUI regression code checks immediate keyboard/seek painting after repeated splitter layouts without a message pump and checks that no invalid region remains. It is cross-compiled, NOT executed. Windows rendering and drag smoothness require native verification. Audio sources are unchanged; the seven-suite/53-group Linux results above belong to 0.12 and were not rerun for this GUI-only update.

## 0.14 verification

Seven Linux Release CTest suites pass, with 56 application groups. New checks cover ALG4/7 register-slot carrier mapping, octave folding, ML=0, PCM bypass, live 200%/50% score position, doubled note scheduling, speed-independent seek, zero-speed freeze/resume, and viewer Poly/Mono routing independent of external MIDI's song-playback policy. MML round-trip now uses distinct middle-operator levels and the corrected logical operator order. Existing LFO, pause restoration and MIDI voice borrowing checks pass.

Windows x64 GUI/CLI and x86 bridge are cross-built. GUI regression source checks drag-only composite style and immediate redraw, speed wiring, viewer keyboard creation, octave/note mapping, key-repeat suppression and deactivation release. GUI tests are compiled, NOT executed. Native Windows visual/keyboard/audio testing, X68Sound and C86CTL devices remain unavailable here.

Manual Windows checks: drag splitter while playing and with both keyboard views; interrupt capture/Alt-Tab; test 0/50/100/200% and seeks/loops; verify distinct carrier pitches in ALG4–7; click and glide on the audition keyboard, hold chords in both allocation modes, change octave and focus; select/copy instrument text with Ctrl+A/C; repeat in Japanese/English and high DPI.

## 0.15 verification

Eight Linux Release suites pass; 58 application groups plus seven upstream OPM suites. New/updated cases cover full carrier octaves, the user's @11 waveform in external-FM and MIDI modes, no monitor signal leaking into hardware audio buffers, live MIDI timing toggles and the corrected C1-based MDX timing test. Upstream key-on tests cover 8 channels x 16 masks and the three supplied regression voices.

Windows x64 GUI/CLI and x86 helper builds succeed. GUI test code checks the drag bitmap overlay without WS_EX_COMPOSITED, immediate control printing into the overlay, cleanup, 13-key mapping including U/upper C, and octave limits 0/8. GUI tests are compiled only. Native Windows visual behavior remains unverified. The attached replacement DLL is byte-identical to bin/independent_opm.dll; exports were checked but it was not executed. See TIMING_AND_WAVEFORMS_0.15.md for scope and limits.


## Copyright refresh in 0.17

The final source subset was refreshed from the copyright-bearing 0.11b archive. Comparing each used upstream file against the prior archive shows only the added copyright comment/notice; executable code is unchanged. The private monitor retains the same changes and updated notices. Root/bin third-party notices contain the complete updated core license. The supplied DLL remains byte-identical to independent_opm(2).dll. No additional rebuild or test run was needed for comment-only source edits and external license text updates.
