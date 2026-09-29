# IndependentMDX 0.25

## Changes in 0.25

Updates since the initial GitHub release, beginning with the software volume-LFO correction:

- Update Independent OPM source, DLL and API documents to 0.12; apply noise-level correction to the visualization core.
- Correct square MA depth/baseline and reset MP/MA on retrigger even with MD0, preserving ties.
- Add append-only error CSV, opt-in MDX collection, recursive headless validation and console folder progress; exclude missing PDX.
- Resume playback position/pause state after output changes.
- Load browser titles asynchronously and restore focus after window reactivation.
- Distinguish F1 relocation from loops for duration/Position; accept raw q bytes and 256-iteration zero repeat count; ignore invalid sync destinations with diagnostics.

Fixed premature transport wrapping caused by short auxiliary-track loops.

## Changes in 0.24

Based on 0.23 with first-definition-wins instrument handling. Later records with the same ID are skipped; distinct IDs continue to load. 66 native regression groups pass. Investigation has ended; no further dedicated handling is added.

## Changes in 0.23

MH/MP/MA switches now use bit7 to select the switch form and bit0 for OFF/ON. Unused bits1–6 do not affect the result: 0xFE is OFF and 0xFF is ON.

MP/MA settings use bits0–1 for waveform and bit2 for x256 scaling; other unused mode bits are ignored. MH PMS/AMS is masked with 0x73. Repeat-skip operand sizing also uses bit7.

Numeric parameters and file offsets retain range/bounds validation. Unsupported opcodes are not remapped or skipped.

65 native groups include all 128 switch encodings for each of MH/MP/MA, unused waveform bits and PMS/AMS. The reported input gets past the switch error but still stops at unsupported opcode 0xE3 on channel B, offset 0x378. Complete playback is not achieved. Native Windows listening is unverified.

## Changes in 0.22

MDX layout variations use common structural checks independent of identifying strings. The earliest voice/track payload boundary validates a compact nine- or sixteen-track table. Voice-first layout, nonmonotonic physical track order and shared entry offsets are supported.

Removed fixed-pattern entry reconstruction, dedicated tempo/startup waits and exceptional voice selection. Out-of-range offsets, incomplete tables and indeterminate layouts produce errors with offset, value and expected range. Invalid bytecode is not guessed or executed as host code.

Dedicated handling and its analysis records, descriptions and fixtures are removed from this distribution. Files previously relying on that handling may no longer play. These changes do not establish legal compliance.

64 native groups pass, including audio/startup equivalence across synthetic payload layouts and existing synthesis/LFO/MIDI tests. Windows GUI/CLI are rebuilt; native Windows UI/listening remains unverified.

## Changes in 0.21

Removed an extra x256 multiplier from square-wave software pitch LFO (MP). The signed stored value already uses fixed-point detune units, with WAVE+4 expansion applied separately; the square branch multiplied it again.

Amplitude 1 now represents ±1/64 semitone; amplitude -24 reverses polarity with ±24/64 semitone peaks. Peak-to-peak spans are 2/64 and 48/64 semitone respectively. Sign changes still occur every stored P ticks.

Only MP square modes (WAVE1/5) change. Other MP shapes, MA, hardware LFO and the 0.20 key-off freeze are retained. Tests decode KC/KF writes for amplitudes 1/-24, both equivalent fixed-point WAVE1 and expanded WAVE5 encodings, and the period.

64 native groups pass. Windows GUI/CLI are rebuilt; native Windows listening is unverified. No actual MDX compiled from the reported MML was supplied, so these are synthetic MDX tests.

## Changes in 0.20

Reverted the 0.16 software pitch/volume LFO release continuation. As in 0.15, MP/MA phase and the MD delay counter advance only while keyed on, and freeze after key-off, during rests and at track end. Key-off retains the current modulation value; it does not reset pitch/volume to their unmodulated values.

Hardware LFO, MP/MA ON/OFF commands and the separation of MD from hardware LFO are unchanged. The 0.17 portamento fix remains independent and continues to the end of a quantized note. The 0.19 UI and performance changes remain.

63 native regression groups pass, including held-note modulation and no LFO writes after quantized key-off/rest/end, plus existing portamento tests. Windows GUI/CLI are rebuilt; native Windows listening is unverified.

The history below describes older versions. Software-LFO release continuation in 0.16–0.19 is superseded by 0.20.

## Changes in 0.19

- The keyboard header now places View, Carrier pitch and Waveforms on the left, with the instruction at the right on the same row. Waveforms defaults to ON and is persisted. Narrow instruction text is ellipsized; hover to read it in full.
- Instrument export defaults to the MDX filename with a .mml extension.
- Speed, key and master sliders support absolute background clicks and dragging.
- FM delay, PCM delay and the next-playback notice share one fixed-order row. The title area reserves two text lines plus padding.
- Opening the MIDI dropdown enumerates devices again, retaining None or matching the selected name.
- Language changes suspend control repainting until captions and layout are ready; unchanged time/slider values are not repainted.
- Instrument-list updates release the shared audio mutex before Windows calls. Keyboard painting reuses stock brushes. The private FM display model caches exact repeated math results. Waveforms OFF suspends that model, retaining audio, note display and MIDI; re-enabling initializes it from current registers and held notes.

See [changes and performance investigation](docs/CHANGES_0.19.md). Native Windows rendering, real-time dropout behavior and physical MIDI hotplug remain unverified.

## Changes in 0.18

- Updated the runtime DLL, C ABI header, license and API documentation from `independent-opm-0.11b-windows-x64.zip`.
- Reorganized the central controls into Playback, Volume, Output and MIDI. Slider endpoints, numeric fields, labels and input columns are aligned.
- Control sizes account for both Japanese and English text, keeping the same geometry when switching languages. Dropdown widths fit their options; numeric fields share the dropdown height.
- Moved View and Carrier pitch to the top of the keyboard pane, aligned to its right edge. The upper file controls and lower browser retain their operations.

Native Windows layout/interaction and audio from the replacement DLL have not been tested here. See [0.18 update details](docs/LAYOUT_0.18.md).

## Changes in 0.17

- Updated the used independent-opm sources and notices to the copyright-bearing 0.11b archive, including the monitor copy and in-app license text: `Copyright (C) 2026 by I.C.KaZe`.
- Physical C86CTL voice programming temporarily gates only the affected channel's pan, programs operators with final volume-adjusted TL, then restores output. No raw louder TL intermediate, extra reset, or fixed wait is introduced. MIDI/viewer voice programming follows the same sequence.
- Pan/feedback-only writes no longer resend TL. Identical physical TL and unchanged gain settings are suppressed. Algorithm/gain changes still update affected levels.
- Portamento now spans the complete note duration after quantized key-off. It stops at rests/end and follows the next note's own settings. Software LFO release continuation is retained.

Physical C86CTL noise suppression is unverified; see docs/C86_AND_PORTAMENTO_0.17.md.

## 0.16

Updated sources and supplied DLL with iopm_state_size / iopm_save_state / iopm_load_state. The dependency contains build sources and licenses only; its DLL is in bin. Upstream evidence and tests are omitted.

Software pitch/volume LFO continues through key-off, rests and the existing final release interval until MP/MA OFF. MD delay, ties, retrigger phase handling and hardware LFO are unchanged. No new state-save UI is introduced.

MDX / PDX player for Windows 10 / 11 x64, using Independent OPM 0.12.

Run `bin/IndependentMDX.exe`. Open or drop a file. Keep its PDX beside the MDX, or use **Choose PDX**. Play, pause/resume, stop and export 48 kHz 16-bit stereo WAV. Synthetic demo files are in `samples/`.

On first launch, Japanese Windows UI language selects Japanese; all other languages select English. Use **Language** to switch at any time. The selection is saved in `IndependentMDX.ini` beside the executable and takes precedence on subsequent launches. Button and label widths and the required content area are recalculated from actual font metrics on each language switch. Audio continues uninterrupted. System DPI is captured at startup; per-monitor DPI changes are not implemented.

## Changes in 0.15

- Replaces 0.14's WS_EX_COMPOSITED toggling with a drag-only bitmap overlay. Child controls print into a memory image and the complete image is presented together. Layout/frame work is coalesced to roughly 33ms; unchanged pointer positions do not relayout. Release/cancellation restores normal painting. Native Windows visual verification is still pending.
- PageUp/PageDown now selects **o0–o8**. **AWSEDFTGYHUJK** maps to 13 semitones, C through the next C (U=A#, J=B, K=next C). The current 13-note span is labeled. Notes outside the existing OPM keyboard range o0 D# through o8 D are not played at the extreme octaves.
- Carrier pitch now includes its full octave displacement; the 0.14 modulo-12 folding is removed. Out-of-display-range notes are omitted rather than clamped to another key.
- Corrects the monitor core's C1/M2 key-on mapping. The supplied @11 (ALG5, OP=3) now produces a nonzero waveform in regression tests. MIDI/viewer audition now also feeds a per-channel visualization renderer.
- **FM waveforms are simulated**, including C86CTL mode. No C86 audio input is captured or separated. A private software core receives the outgoing register writes and exposes pre-mix channel taps. It is not a measurement of hardware/analog output.
- Bundles the newly supplied DLL as `bin/independent_opm.dll` and synchronizes the same-version 0.11b source containing the key-on fix.
- Fixes measured output timing being forced ON for MIDI/viewer audition. Live toggles now reach that renderer; the MDX path already supported them. Audible timing simulation applies to **Independent OPM DLL, ALG5/7**. The control is disabled for X68Sound/C86CTL. See [timing and waveform notes](docs/TIMING_AND_WAVEFORMS_0.15.md).

Historical 0.14 compositing, folding and keyboard-range descriptions below are superseded by this release.

## Changes in 0.14

- Whole-window double buffering is enabled only while dragging the splitter. Live layout/redraw continues; release or capture cancellation restores normal painting.
- **Carrier pitch**, beside View (default OFF), displays each FM carrier's ML/DT2-adjusted pitch on both keyboards and piano roll. ALG0–3: one carrier; ALG4: two; ALG5/6: three; ALG7: four. Unison carriers overlap. Offsets greater than one octave are reduced modulo 12 semitones; exactly ±12 is retained. Fractional notes are rounded to the nearest key, out-of-range notes omitted. PCM display and audio are unchanged.
- **Playback speed 0–200%**, above Key (default 100%), changes MDX sequencing for FM and PCM without changing chip pitch, hardware LFO or envelope speed. Software LFO follows the sequence clock. Zero freezes the song while allowing audition. Timeline and seeks remain in score seconds at 100%. Standalone PDX preview changes sample read rate, so its pitch also changes. WAV export remains at 100%.
- The instrument viewer adds an audition keyboard above the instrument list. Click/drag to play, or use **AWSEDFTGYHJK** for C through B. **PageUp/PageDown** changes octave (default o4; range o1–o7). Only the current octave shows key labels. Mono/Poly controls audition allocation, up to eight FM voices in Poly. Shortcuts work while child controls have focus; Ctrl+A/C remain text operations. Deactivation, closing, octave changes and allocation-mode changes release audition keys.
- Instrument text/copy/MML export now use **OP1,OP2,OP3,OP4 (M1,C1,M2,C2)**, matching the diagram; OP2/OP3 were reversed. Register text remains in address order.
- Carrier mode and speed use the existing two-second settings autosave.

## Changes in 0.13

Splitter movement commits all control positions together, then immediately repaints the parent and child controls, including keyboards and sliders. Old window pixels are not copied during repositioning, avoiding stale images and missing thumb redraws.

## Changes in 0.12

MD now gates software pitch/volume LFO only. Hardware LFO sensitivity is no longer cleared/restored by MD, retriggers or delay expiry. EA80=OFF/EA81=ON is retained; software triangle/tie behavior is unchanged from 0.11. PCM-track MH operands are consumed without writing any OPM register, including global LFO settings and switches.

View selection moves beside loop count; Aftertouch moves beside Mono/Poly. **Alt+Enter** toggles fullscreen on the current monitor and restores the previous window placement/maximized state. Drag the blue vertical splitter to resize the keyboard area; double-click restores the default ratio. The ratio is saved. The controls retain their measured minimum width.

Piano-roll keys/lanes all have exactly equal pixel widths; unused pixels become centered margins. Numeric fields and closed dropdowns use matching heights, with smaller row gaps and margins. The browser fills the remaining height; small/high-DPI windows retain scrolling when necessary.

Historical MD/OPMLFO coupling descriptions below are superseded by 0.12.

## Changes in 0.11 (history)

- Correct software pitch/volume LFO switches: EC80=MPOF, EC81=MPON; EB80=MAOF, EB81=MAON. Previous implementation and documentation were reversed.
- Aftertouch is optional, default OFF. Releasing all keys clears pressure; late pressure packets while no keys are held are ignored. Disabling restores normal held-note volume.
- Separate saved gains for each output. Independent OPM DLL and X68Sound DLL default to FM 200%, PCM 100%, master 100%; C86CTL defaults to 100% for all three. Legacy shared gain settings are not migrated. New preferences retain two-second autosave.
- Piano-roll time freezes while stopped/paused without MIDI voices, runs during MIDI audition, and freezes again after the final voice ends. Sustained voices keep it moving.

The sections below are historical. The corrected switches and new defaults above supersede prior descriptions.

## Changes in 0.10 (history)

Immediate MH enable and absolute seek clicks are fixed. Idle/paused MIDI borrows the input channel's instrument, supports Mono/Poly (default Poly) and aftertouch. X68Sound avoids excessive internal queue polling, pipelines IPC and restores channel waveform display. Independent C86CTL FM/PCM delay, live PCM gain, a 0–400% master slider, a shared vertical piano roll and the supplied image as an embedded icon are added.

See [0.10 details](docs/CHANGES_0.10.md) for controls, behavior and limitations. Sections below are historical; 0.10 supersedes 0.9 destination-specific voices and disabled waveforms.

## Changes in 0.9 (history)

- Correct hardware LFO switches to EA80=MHOF and EA81=MHON. Earlier code reversed these and restored PMS/AMS for OFF; the 0.8 explanation and tests used the same incorrect assumption. Software EC/EB switches are unchanged.
- Clicking empty transport track now positions the thumb at the pointer, with drag support, instead of page increments.
- X68Sound disables the extra FM monitor core and all channel waveform drawing; keyboard notes remain visible. GetPcm calls are batched between register changes, with 1024-frame IPC blocks (~16.4ms FM transfer latency).
- While stopped or paused, MIDI allocates up to eight FM and eight PCM voices in separate pools. Prefer the input channel, then free slots, then steal the oldest. Each destination uses its own retained instrument or PCM bank/format. Undefined FM slots are skipped. PCM retains the note-to-PDX-sample mapping.
- Note Off, sustain, volume and bend follow the originating MIDI channel. Keyboard highlights follow allocated voices, including masked keys. On resume, silence MIDI, restore the saved song voices/registers/PCM settings, then continue playback. Ordinary song playback keeps the original one-MIDI-channel-to-one-playback-channel mapping.

Windows GUI, MIDI hardware, actual X68Sound DLL audio and C86CTL hardware are not runtime-tested here; real-device dropout resolution remains unverified.

## Changes in 0.8 (history)

- Interactive seeking reconstructs sequence/PCM state without rendering discarded FM audio or animating the keyboard. Duplicate end-track requests are ignored and waveOut is explicitly restarted. FM envelopes retrigger at the destination; this is not sample-exact continuous synthesis.
- Selecting an instrument number applies it to the selected FM channel and subsequent MIDI notes, and updates an already held MIDI voice. Later MDX instrument commands remain effective.
- Muted song channels no longer send pitch/key/instrument writes to external hardware. MIDI stays playable independently; Note Off does not unmask the song. Unmask restores current song registers.
- Viewer layout: MML above the compact algorithm diagram on the left, full-height register text on the right, instrument list below both.
- Output order: Independent OPM DLL / X68Sound DLL / C86CTL (OPM / YM2151). Put your **32-bit X68Sound.dll beside the EXEs**. The x86 helper EXE is included; X68Sound.dll itself is not. FM uses the chosen DLL, PCM remains in the application; software WAV export follows the selection.
- The X68Sound adapter calls the public C API in a separate x86 process. No X68Sound implementation is embedded. The supplied Independent OPM DLL and upstream sources remain unchanged. The bridge adds 256 native frames (~4.1ms) of FM latency and holds 44.1kHz DLL output on the 62.5kHz sequencer grid. Measured-output timing is an Independent OPM setting.
- Correct software LFO switches to $80=ON / $81=OFF. Ties preserve MD delay/phase. Hardware LFO retains MHOF and respects MD delay. Triangle modulation now swings above and below the base pitch. See docs/LFO_0.8.md for stored-period interpretation.

Windows GUI, MIDI devices, X68Sound DLL playback and C86CTL hardware have not been runtime-tested here. Windows binaries are cross-built.

## Changes in 0.7 (history)

- Remember the last file actually started for playback and its PDX override; load them at startup without autoplay. Merely browsing another file does not replace the remembered selection. Missing files are reported.
- Replace eight whole-chip DLL monitor instances with one visualization-only monitor core. Audible software FM still uses the unchanged supplied DLL. Hardware output skips DLL audio synthesis, and silent MIDI skips unused rendering.
- C86CTL writes use a timestamped queue and dedicated sender, avoiding audio-thread sleeps. Reset/stop/seek/close clears queued writes before resetting hardware.
- Program Change works immediately after loading an MDX, even before playback. MIDI channels 1–8, raw program 0–127 select @0–@127; CC0 bank 1 selects @128–@255, bank 0 restores the lower bank. Undefined programs leave the selection unchanged. The next Note On uses the selected definition. Devices displaying 1–128 show program 1 for raw value 0.
- The instrument viewer's bottom catalog lists all defined voice numbers and marks used voices with *. Click a number to inspect that definition; reselect a channel to resume live viewing. Catalog clicks do not change the MIDI program.
- Directed algorithm hierarchy with parallel operators stacked vertically, OP1–OP4 labels and an unboxed output bus. OP1/2/3/4 map to M1/C1/M2/C2. MXDRV MML row order is unchanged.

Benchmark: the same Linux Release workload (8 FM channels, 10 seconds) improved from about 5.96 to 1.39 seconds. This is not a Windows/hardware timing measurement.

## Changes in 0.6 (history)

- Song title is a normal first column, with one-time migration of old layouts; users can move/remove/resize/sort it. Overlay removed.
- Windows uses the supplied x64 `independent_opm.dll` through C ABI 1. Keep it beside the EXE. No static fallback. Upstream source is unmodified; Linux tests link its C API.
- Direct y writes retain channel instrument/TL/algorithm state, including instruments programmed without @. Normal later note/volume/instrument commands still write their registers.
- DLL/C86CTL output switch stops playback. Hardware resets on stop, natural end, failure, output change and application exit. FM uses the first identified OPM/YM2151 device; PCM stays on PC audio. WAV export always uses the DLL.
- Current/total time and seek slider. Removed the GUI 600-second playback cap so long songs can be sought in full; finite loop counts still apply. A looped song displays its first pass and wraps to the loop start; the longest first loop is the reference when channels differ. Analysis is bounded to one hour. Seeking flushes old audio and silently reconstructs state from the beginning; long forward seeks can take time. Pause allows seeking.
- MIDI input: channels 1–8 map to FM A–H, 9–16 to PCM P–W, monophonic last-note priority. Note/velocity, CC7/11/64/120/123 and ±2-semitone pitch bend supported. Program Change was added in 0.7; SysEx and MIDI clock synchronization remain unsupported. Retained voices and PCM bank/format remain playable while paused/stopped, until application exit. MIDI bypasses song masks. MIDI note 15 maps to PCM sample 0. Undefined FM voices stay silent.
- MIDI device name and output choice are saved. Refresh the device list by changing UI language or restarting after device changes.
- Export every defined instrument in the last-started MDX as MXDRV MML. The modeless viewer selects FM A–H, shows MML and y-register text side by side with an algorithm diagram. Select and Ctrl+C to copy. Focus in either text field freezes updates; return focus to channel selection/background to resume.

C86CTL requires a separate x64 `c86ctl.dll` beside the EXE: https://www.c86box.com/software.html . No hardware DLL is bundled. GIMIC PLL is set to 4 MHz when supported; other hardware must already use 4 MHz. Other chip families are rejected. Register writes are paced, but USB/Windows scheduling and PCM/audio-device latency are not sample-exact. Hardware audio is not captured to WAV. Software timing/DAC options do not alter hardware circuitry.

The supplied OPM C API lacks per-channel taps. As of 0.7 a separate visualization-only monitor supplies the channel traces; the main audible mix uses the DLL. The DLL FIR adds its own filter history, so mask changes settle over a short filter tail.

Actual Windows UI, MIDI hardware and C86CTL hardware remain untested in this Linux environment.

## Changes in 0.5 (history)

- Draw each visible song title once, left aligned after the complete native report, using warm yellow text and a translucent dark backing.
- Extend E/F and B/C white-key dividers to the top while retaining light gaps beside black keys.
- Three live **Key mode** choices: FM only; FM + PCM playback rate (default, previous behavior); FM + PCM preserving PCM duration. The selection is saved and restored, translated with the UI, and snapshotted for WAV export. CLI: `--key-mode 0|1|2`.
- Duration-preserving mode uses an independently implemented 512-point phase vocoder on decoded PCM, including standalone PDX preview. The source cursor keeps its original rate; zero semitones is an exact bypass. FM transposes in every mode.
- Pitch shifting can smear attacks or color the sound, especially for short samples and large shifts. Decoded-source lookahead avoids delaying sequencer/sample onset; live changes transition over up to approximately 512 PCM samples. This is not a claim of transparent studio-quality processing.

## Changes in 0.4 (history)

- Updated the same-named 0.11b archive, including all evidence assets, renamed recording references, bilingual LFO and ALG5/7 stereo-timing reports. Open `independent-opm/docs/evidence/index.html`.
- Translucent song titles over MDX rows (bounded to an 8 KiB title prefix). Returning to a parent restores scroll position and selects the folder just left.
- **Play next file on completion** follows the visible sorted list, skips folders, and stops at its end. It defaults OFF. Manual stop, errors and WAV export do not advance. If the current file is no longer in the displayed folder, no next file is selected.
- **Infinite loop** disables loop-count and 600-second limits at the next playback start. Finite songs without loop markers still end. Infinite WAV export continues until stopped.
- **Key (semitones)** slider: −24…+24, default 0. Updates FM pitch and PCM playback rate at audio-block boundaries without retriggering notes. PCM is resampled, not time-stretched. Export snapshots the key at start.
- Missing/unreadable PDX remains logged while FM continues. Missing FM voices key off only that channel until a valid voice is selected. Missing PDX samples are logged and silent; other tracks continue.
- Both keyboards start at o0 D-sharp. Removed C labels and separated black keys using white-colored gaps.
- Software pitch LFO accepts 0–3 and their 256x-depth forms 4–7; 128/129 are ON/OFF. WAVE2 was centered around the base pitch in 0.8. Type 3 is an independent deterministic sample-and-hold approximation, not a claim of identical MXDRV random or integer-overflow behavior.
- MDX errors include file offset, referenced value and valid range/required structural condition. Truncated references show EOF.
- Add/select/remove folder bookmarks (up to 100).
- Preferences use a UTF-16LE `IndependentMDX.ini` beside the executable, replaced atomically two seconds after the last change and flushed on close. Write errors remain in the log; there is no alternate save location. Old registry preferences are not imported or modified. First launch uses defaults.

CLI additions: `--loops 0` for infinite, `--seconds 0` for no time limit, `--key -24..24`. With `--loops 0`, no time limit is used unless `--seconds` is explicitly supplied.

## Changes in 0.3

- Re-fetched the corrected same-named Independent OPM 0.11b archive. Removed the obsolete 0.10 scope section from application notices. See `docs/LICENSE_AUDIT.md` for hashes and exact provenance.
- Dark navy colors, lavender text, cyan waveforms and MS Gothic 9pt based on the supplied reference application.
- Measured output timing defaults to ON; saved preferences take precedence.
- Licenses open in an internal, resizable window.
- Bottom native file browser and 16 keyboards on the right. Click a keyboard row to mute/unmute; muted rows are dark. The former mask checkboxes are removed. P also controls standalone PDX audition.
- Settings save two seconds after the last change, restore on launch, and flush on close.

## Browser, monitor and preferences

The initial columns are Name, Date created, Date modified, Type and Size (bytes). Drag headers to reorder, drag boundaries to resize, or right-click a header to add/remove columns. Additional columns: Date accessed, Extension, Attributes, Folder. At least one column remains visible. Click a header to sort; repeat to reverse. Default: natural name order, folders first.

MDX/PDX filters are initially checked. Only checked file types appear; folders remain visible for navigation. Up/Down selects, Enter or double-click opens a folder or plays a file. PDX uses the selected sample index and format. Enter a folder, drive or UNC path in the address field and press Enter; Back/Up navigate.

FM A–H uses all 96 KC positions, approximately D-sharp 0 through D8 at 4 MHz. KF, detune and operator multiples extend or alter the actual frequency. PCM P–W maps bank-local sample indices 0–95 onto the same D-sharp 0–D8 positions as FM; this is not a pitch detector. Waveforms are real per-channel L/R averages after channel pan/mute/gain/fade, individually amplitude-normalized, before final resampling and right-DAC delay. They are not copies of the final mix. Rendering can lead audible waveOut playback by up to about 128 ms at 48 kHz. Stop resets the monitor.

Saved preferences include infinite loop, automatic next-file playback, key, key mode, bookmarks, language, numeric playback settings, PDX index/format, timing, masks, current folder, filters, columns/order/widths, sort and window geometry. Navigation history and playback position are not saved. Small windows expose scrollbars rather than clipping inaccessible controls. System DPI is used at startup.

## Features

- 8 FM channels; MDX PCM with up to 8 mixed channels.
- Loops 1–1000 or infinite (default 2), master gain 0–200% (40), PCM gain 0–400% (100), live FM gain 0–800% (200).
- Live A–H mute masks preserve synthesis state.
- PDX audition: sample = bank × 96 + slot. F0–F4 ADPCM; F5 signed big-endian 16-bit; F6 signed 8-bit. F5/F6 use 15.625 kHz and require explicit selection.
- Relative volume, gate/tie/key delay, repeats, synchronization, detune, portamento, hardware LFO/noise, software pitch/volume LFO including OFF/ON.
- Clock/64 synthesis at 4 MHz. Optional 7.00 us FM-right delay and independent ALG5/ALG7 measured output timing switch, right delay OFF and measured timing ON by default, both adjustable live. Use `--no-measured-output-timing` to disable the latter from the CLI.
- WAV export snapshots audio options at start. Stop during export leaves a valid partial WAV.

The console executable is `independent-mdx.exe`; use `--help`. Examples:

```bat
bin\independent-mdx.exe song.mdx --pdx bank.pdx --wav output.wav
bin\independent-mdx.exe song.mdx --dac-delay --measured-output-timing --wav timing.wav
bin\independent-mdx.exe bank.pdx --sample 0 --format 4 --wav sample.wav
```

CLI messages are English. Space pauses/resumes, Esc or Ctrl+C stops playback. Existing WAV files are not overwritten by the CLI.

## Limitations and build

This is a sequencer implementation, not execution of the original music driver. Complete compatibility, exact software LFO integer behavior and analog hardware response are not claimed. Compressed PDX/LZX, direct PCM8 control and some MDX extensions are unsupported. Unsupported instructions stop with a diagnostic. Resampling uses sample hold plus interval integration; no filter selection is provided. The random hardware LFO remains provisional as documented by the included core.

Use Visual Studio 2022 Desktop development with C++ and CMake, then run `build_vs2022.bat`. No vcpkg or external sound DLL is needed. Outputs are under `build/Release/`. Linux supports tests and CLI WAV export; direct audio and GUI require Windows. See `docs/VALIDATION.md` for actual validation coverage.

Original application code is 0BSD. The supplied 0.12 core is distributed under 0BSD; its corrected provenance documentation is included verbatim. A small host monitor tap is documented in `docs/MONITOR_CORE_PATCH.diff`; core licensing files remain unchanged. Keep `THIRD_PARTY_NOTICES.txt` with distributed binaries. See the full Japanese feature reference in `README.md` and the core documentation for technical details.

Rebuilding the x86 helper with Visual Studio 2022:

```bat
cmake -S . -B build-x86 -A Win32 -DBUILD_TESTING=OFF
cmake --build build-x86 --config Release --target IndependentMDX-X68Bridge32
```
Copy the resulting helper beside the x64 application. Do not build the whole application for x86 with the supplied x64 Independent OPM DLL.

## Playback corrections (0.25)

- Removed the extra 256x multiplier from square-wave software volume LFO depth.
- GUI load/playback errors append to `IndependentMDX_errors.csv` beside the executable: UTF-8 CSV, no header, absolute filename / song title / error. Unavailable titles are empty. Nonfatal playback errors are included. The destination must be writable.
- Changing output during playback resets the old output and resumes the same file at its saved position, preserving pause state. Switching and seeking introduce a short interruption.

## Error MDX collection and MA correction

Launch with `--collect-error-mdx` to copy MDX files with data errors into `ErrorMDX` beside the executable. Originals are unchanged. Content hashes distinguish same-name files and suppress repeat copies. This opt-in flag is not persisted. Missing PDX and consequent missing-bank PCM warnings remain visible but are excluded from logging and collection.
Square MA now alternates between base attenuation and base plus depth. Window reactivation restores the previously focused child control.

## Headless validation and asynchronous titles

Run `IndependentMDX.exe --validate-folder "D:\Music\MDX"` to recursively validate MDX without opening a window or audio device. Errors append to `IndependentMDX_errors.csv`; failing MDX files are automatically collected in `ErrorMDX` beside the executable. Missing PDX is excluded. Exit codes: 0 clean, 1 data errors, 2 invalid arguments/traversal failure. Validation stops at each track's first end/loop, with a 3600-second score-time limit; reaching the limit is logged as incomplete. This does not validate sound quality or hardware.

The browser displays file metadata first and fills titles from a background reader. Navigating cancels stale results. Title arrivals do not reorder rows; click the title header to sort again. MP/MA phase now resets on every retrigger even with MD0, but remains continuous for tied notes.

## Collected-data compatibility corrections

Forward F1 offsets are accepted as bounded relocation jumps, without marking loop completion. Raw q accepts the full byte: 0..127 proportional gate, 128..255 signed subtraction. Repeat count zero wraps to 256 iterations. Bounds and nesting limits remain enforced.
`--validate-folder` attaches to the parent console and prints the containing directory when the folder of the MDX under validation changes. It does not create a window or a new console.

## Out-of-range sync destinations

An EF sync send to a nonexistent track is ignored with an error diagnostic instead of aborting playback. The value is not masked into a different channel. Logging and error-file collection remain enabled. This recovery does not guarantee the intended synchronization of malformed data.

## F1 relocation and transport timing

F1 jumps to unvisited addresses, in either direction, are relocation rather than loop completion. The first return to an executed address establishes the loop; later relocation jumps do not increment its counter. This corrects duration and Position for relocated tracks.
