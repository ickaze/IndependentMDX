# IndependentMDX 0.12 — source map

- `opm_backend.h`: versioned C ABI loading (Windows) and identical linked API (Linux tests). Audible synthesis uses the DLL. A private visualization-only monitor_core copy provides all eight taps in one pass.
- `player.cpp`: MDX interpreter, direct-register instrument cache, timelines, replay-based deterministic seeking, retained voices/registers and optional timestamped external writes.
- `formats.cpp`: MDX/PDX input, sample rendering/seeking and WAV output.
- `pcm_pitch_shift.h`: independent 512-point phase vocoder; unchanged transposition modes.
- `midi_synth.h`: retained-voice MIDI engine with idle/paused Mono/Poly routing and bounded thread-safe input queue. Separate synthesis bypasses MDX masks. PCM follows last retained bank/format.
- `live_session.h`: pause freezes song generation while MIDI continues; seek and pause transitions request waveOut buffer reset before changing state.
- `c86_output.h`: official COM ABI subset, OPM detection, optional GIMIC 4 MHz PLL, timestamped queued writes on a dedicated sender, reset and release. Device switching joins the audio worker before mutation. Hardware MIDI claims suppress conflicting song writes on held channels.
- `advanced_ui.h`: MIDI selection/idle playback, instrument export, modeless selectable MML/register text and GDI algorithm diagram.
- `ui_extras.h`: native browser columns (title ID 9), keyboard monitor, bookmarks, license window and delayed executable-local INI preferences.
- `gui.cpp`: controls, measured multilingual layout, audio-worker lifetime and transport.

C API render at 62,500 Hz includes the upstream FIR. Existing host PCM mix/output integration and optional right DAC delay remain downstream. The single monitor mirror receives the same register stream and advances once per native frame; it does not contribute audible samples. Analysis skips sound writes and advances sequence ticks; seeking renders from zero so synthesis/filter/PCM/LFO state is reconstructed. It is cancellable but does not use checkpoints.

`algorithm_layout.h` derives columns from modulation edges and aligns carriers to a final output bus. As of 0.8 catalog selection also updates the channel and MIDI program. VoiceState holds all 256 definitions plus defined/used masks and a revision for loaded-song changes. Program Change maps raw 7-bit values with CC0 bank selection; undefined programs preserve the previous selection. LastPlayedFile/LastPlayedPdx record playback starts and restore loading only.

## 0.8 transport and adapters

Options.fastSeek suppresses chip rendering during native-clock reconstruction, retains sequencer/PCM positions, then restores register latches (including separate AMD/PMD) and held notes. Monitor publication is suppressed while seeking. Exact synthesis reconstruction remains the default core API for regression tests. A new seek request cancels an older interactive seek.

X68Bridge uses fixed-width shared-memory records and request/reply events. A job object terminates the x86 child if the parent exits. Calls are bounded by a five-second reply timeout; helper crashes become playback errors. Register writes preserve per-native-frame order in each 1024-frame batch. Separate song and MIDI helpers isolate the DLL's singleton state. Public API declarations verified against https://github.com/rururutan/X68Sound/blob/master/X68Sound/X68Sound.cpp ; implementation source is not included.

Channel waveform displays remain the Independent OPM monitor mirror, including in X68Sound/C86CTL mode; they are not audio captured from those external outputs.

## 0.9 MIDI and X68Sound

MidiSynth routes pooled notes while idle/paused and tracks source channel ownership separately from output slot. LiveSession snapshots retained voice/register/PCM state before pausing; silence and restoration precede song register synchronization on resume. VoiceState.midiNotes publishes per-slot note highlights atomically.

In 0.9 the X68Sound visualization mirror was disabled. In 0.10 it is restored. x68_render.h batches audio generation within register-stable spans while preserving fractional 44100/62500 phase across blocks. Protocol magic changed with the 1024-frame buffer; the GUI and x86 helper must be upgraded together.

## 0.10 controls and audio

Normal MH writes sensitivity immediately, bypassing the pending MD gate for that explicit setting. Subsequent retriggers retain MD. Absolute blank-click seeking reads the track channel rectangle instead of moving the thumb without repaint to query its endpoints.

Polyphonic allocation reads the input-channel program/retained voice and PCM bank/format. The destination retains its own saved instrument. Per-slot pressure combines with velocity/volume/expression; channel pressure fans out by source owner, key pressure additionally matches note. LiveSession retains the existing pause snapshot and restores it on resume. A live Mono/Poly option applies only to idle/paused audition.

The x86 bridge collects register events in a private array while the helper renders the preceding shared block. Only after completion does the parent reuse shared memory. Two blocks (2048 native frames) are buffered before audible FM; host PCM is delayed equally. Closing waits for in-flight work before sending shutdown, or terminates the helper via its job. OpmWait(1) avoids the classic implementation's OpmWait(0) empty-loop overhead. The visualization mirror is not a measurement of X68Sound output.

C86 scheduled register timestamps gain an independent fixed FM delay; song PCM uses a native-rate ring. Delays are fixed per playback to preserve event ordering; finite-song release tails include the maximum delay. Software gain/PCM gain and MIDI mode use atomic live values. Hardware FM gain adjusts carrier TL from unmodified cached register values and closes pan at zero.

`piano_roll.h` stores eight seconds of displayed note history, including MIDI slot notes, and paints one shared pitch keyboard with per-channel colors. No additional audio synthesis is introduced by changing visualization mode.

## 0.11 state changes

Software LFO toggles share an 80=OFF / 81=ON decoder. MIDI tracks physical key-down state per input channel, independently from pooled slots and sustain; full release resets channel pressure and released-phrase pressure packets are ignored. Aftertouch enable is a live atomic option, default false. Output0/1/2FmGain, PcmGain and MasterGain are persisted independently; old shared keys are not imported. The piano roll uses a virtual monotonic time accumulated only while song playback or MIDI voices are active.

## 0.12 transport/UI

Hardware LFO enable depends only on its explicit enabled state; MD/retrigger/tick paths do not touch PMS/AMS. PCM-track MH is parsed but ignored for both global and channel registers. Software-LFO delay and tied-note phase behavior remain unchanged.

Layout uses measured closed-combo height for compact rows and a saved split fraction constrained by control minimum width and keyboard minimum width. The splitter captures the mouse while dragging. Fullscreen stores placement/style, removes window chrome and covers the current monitor; preference saves use the stored windowed placement while fullscreen. Piano-roll lanes have a common integer width and centered leftover margins.

## Splitter repaint (0.13)

Layout batches sibling positions through DeferWindowPos with SWP_NOREDRAW and SWP_NOCOPYBITS, then synchronously invalidates, erases and redraws the parent and all children. This prevents low-priority WM_PAINT starvation under captured mouse movement and prevents copying stale keyboard/trackbar pixels. A failed deferred batch falls back to individual SetWindowPos calls before the same complete redraw.

## Interactive changes (0.14)

During splitter capture, WS_EX_COMPOSITED buffers the full parent/child hierarchy; the previous extended-style bit is restored on capture loss. Deferred placement and synchronous redraw still update live geometry. Keyboard controls retain their own memory-DC buffers. Win32 background painting is included in the compositor rather than suppressed, so uncovered regions are still cleared.

The sequencer advances fractional score samples by the live speed multiplier while synthesis and C86 scheduling use physical native samples. Timeline analysis and seek reconstruction remain at 100%; the interactive session treats zero speed like a transport pause, leaving MIDI audition running. Standalone PDX uses variable-rate preview. Export does not take the live-speed pointer.

Carrier display uses raw register masks 8,8,8,8,12,14,14,15 and the multiplier 0=0.5. MML/diagram operator order maps to register slots 0,2,1,3. Carrier offset is 12*log2(ML)+DT2 semitones, folded only when its magnitude exceeds 12; the existing KF is added before key rounding. It is a nominal carrier-pitch visualization, not spectral analysis of FM sidebands.

Viewer audition shares the MIDI synth and voice catalog. Private message bits mark viewer events and their Mono/Poly policy, scoped to each routed event, so live external MIDI retains its playback policy. Note-off stores the original channel, pitch and policy even if the viewer selection later changes. Keyboard autorepeat is ignored; deactivation/close releases all viewer-owned notes.

## 0.15 superseding behavior

WS_EX_COMPOSITED toggling is removed. During splitter capture a topmost child overlay displays a memory-DC image assembled through child WM_PRINT messages. Custom keyboard/splitter paint handlers accept print DCs. A 33ms timer coalesces pointer changes; unchanged ratios do not relayout, and pending geometry is committed on release. Capture loss destroys the overlay and frees its bitmap/DC. layoutBusy remains set throughout painting to reject re-entry.

Carrier offsets retain full octave displacement. Viewer key map has 13 entries AWSEDFTGYHUJK and selectable octaves 0..8; out-of-OPM-range input is ignored. The monitor core now shares the updated upstream key-on mapping. MIDI rendering optionally supplies channel waveforms, including hardware-output sessions, without mixing monitor audio into output. LiveSession forwards measured timing into both song and audition rendering; hardware and X68 output selectors disable the Independent-OPM-specific switch.
