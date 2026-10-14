# IndependentMDX 0.33.5

Adds verified LFO/sequencing corrections, MRL 1.2 titles and PCM8 events with optional embedded PDX, MRZ ZIP output, and low-memory large ZIP/MRL access.

Windows 10/11 x64 MDX/PDX and single-YM2151 VGM/VGZ/MRL/MRZ player with ZIP/LZH/LHA browsing. Audio and per-channel waveforms use the unmodified Independent OPM 0.14 DLL. No OPM implementation source or private monitor core is bundled.

[日本語](README.md) · [Validation](docs/VALIDATION.md)

## Changes in 0.33.5

- Implement the software-LFO RNG recurrence independently inferred from register captures, replacing the approximate mixer. No recorded sequence table is embedded.
- Correct signed 16-bit pitch overflow before clipping to the chip range.
- Match target register states in all 202 available cases: 76 original LFO, 58 signed and 68 random-depth probes. Sixteen inputs rejected by the reference tool are excluded.
- See [recurrence, results and scope](docs/RANDOM_LFO_0.33.5.md). Settings are preserved; re-export existing MRL/MRZ from MDX to apply the changes.

## Changes in 0.33.4

- Correct random MP depth to signed low-byte interpretation and random MA fixed-point extraction, including negative-product rounding. Period no longer scales random amplitude.
- Hold random values between updates: MP starts on the first advancing tick, MA after one period. Draw values from a song-wide sequence.
- **The RNG remains approximate. This release does not reproduce the reference random sequence or claim audio equivalence.** See [validation and limits](docs/RANDOM_LFO_0.33.4.md).
- Preserve settings. Re-export MRL/MRZ from MDX to record the changes.

## Changes in 0.33.3

- Correct negative MA saw/square start attenuation, continuous MA phase across MD0 note retriggers, and unmodulated attenuation during MD waits. MD1 waits zero ticks; MD4 waits three.
- Signed probes: 48 of 58 available reference cases match; 10 random-wave cases remain different. See [scope](docs/LFO_SIGNED_0.33.3.md).
- Add 68 original inputs in `samples/LFO_Random_Depth_Probe` to separate random depth arithmetic from RNG output.
- Preserve settings. Re-export MRL/MRZ from the original MDX to record the correction.

## Changes in 0.33.2

- Correct negative-depth MA triangle attenuation, including its initial level during MD delay.
- Compare the affected 192-tick interval across three carrier TL registers; see [validation scope](docs/LFO_SIGNED_0.33.2.md).
- Add 70 original signed-LFO MDX probes in `samples/LFO_Signed_Probe` for further reference capture. Negative MA saw/square and random-wave compatibility remain unresolved.
- Re-export existing MRL/MRZ from MDX to apply this sequencing correction. Settings and the sound DLL are unchanged.

## Changes in 0.33.1

Enable vertical scrolling in the bookmark dropdown so entries beyond the visible list remain accessible. Existing bookmarks and settings are preserved.

## Changes in 0.33

- Fix case-insensitive PDX cache lookup on Windows. Prefer the MDX directory, then use a unique matching PDX elsewhere in the same archive; reject ambiguous matches.
- Browse LZH/LHA like ZIP folders, including bookmarks, text sidecars and Explorer copy. The decoder is statically linked and extracts only requested members.
- See [supported methods, limits and validation](docs/RELEASE_0.33.md). Existing settings and the OPM DLL are retained.

## Changes in 0.32

- Correct MP square startup/retrigger timing, MP triangle centering/turning points and raw MH PMD/AMD bank bits from black-box register captures.
- Match all 31 non-random/hardware fixture states in the defined comparison window. The 45 software-random cases remain different; four reference MA extension cases failed to export. See [scope and findings](docs/LFO_COMPARISON_0.32.md).
- Show `Position (seeking 50%)` inline. Add HED/PDL plain text support, DOC/TXT/HED/MUS/PDL sidecar priority and extension icons, and right-click sidecar selection.
- Add 17 original random-state probes in `samples/LFO_Random_Probe`. Existing settings and logs remain usable; re-export MDX logs to include sequencing corrections.

## Changes in 0.31 (history)

- MRL 1.2 loop metadata supports repeated playback while retaining the requested number of recorded loops. WAV export ignores infinite playback and uses a finite loop count followed by fade-out.
- Display MRL/MRZ seek progress; export external-bank logs when an embedded PDX cannot be found.
- Show ZIP filenames in the title column, an All files filter, DOC sidecar icons and a modeless text viewer (Space for MDX sidecars, Enter for text files).
- Drag browser items to Explorer to copy them, including extracted ZIP members.
- Include 80 original LFO comparison inputs and a documented random-wave investigation. Random behavior and missing-voice handling are not guessed from insufficient evidence.

## Changes in 0.30.1

Remove the obsolete key-on bit 4/5 swap before Independent OPM output. Partial-operator voices now activate the intended operators. Existing MRL/MRZ logs and the DLL do not need replacement. The shared correction also applies to MDX/VGM/VGZ and MIDI output.

## Changes in 0.30

- Correct deterministic MP/MA waveforms, pitch triangle initial displacement, first square reversal, and modulation during key-off/rests. Random-wave fidelity and universal compatibility remain unverified.
- Correct tempo transition timing, delayed key-on preparation, deferred voice changes, y/MH state separation, and PMD/AMD bank selection.
- Browse large ZIP32/ZIP64 archives with on-demand streaming extraction; stream MRL events through a 64 KiB buffer.
- Add MRL 1.1 title/PCM8/PDX metadata. Embed PDX by default, or use `--no-embed-pdx`; use `--mrz` for ZIP output and `--mrl-v1` for legacy FM comparison logs.
- [Export options](docs/REGISTER_LOG_EXPORT.md), [format contract](docs/REGISTER_LOG_FORMAT.md), [validation report](docs/MRL_VALIDATION.html). Existing user settings are retained.

## Changes in 0.29

- Add MDXRLOG 1.0 playback and seek.
- Browse ZIP files as folders, including PDX lookup, bookmarks and last-file restoration.
- See [ZIP and MRL support](docs/ARCHIVE_AND_MRL.md) for formats and limits.

## Changes in 0.28

- Add `--reglog-folder` for recursive timestamped YM2151 register logs with mirrored directories.
- Publish the [MDXRLOG 1.0 binary contract](docs/REGISTER_LOG_FORMAT.md) and [export usage](docs/REGISTER_LOG_EXPORT.md). No synthesis, PDX or GUI is required.

## Changes in 0.27.1

- Add `--no-pcm` for FM-only batch VGZ exports.
- Skip unused FM synthesis during export; use table-based gzip CRC. FM-only export also skips PDX loading, decoding and mixing.
- Write the MDX title into both GD3 title fields, `Independent MDX` as converter/ripper, and `Exported from Independent MDX` as notes/comment.

## Changes in 0.27

- Headless recursive MDX-to-VGZ conversion: `--vgz-folder DIRECTORY --loops 2`.
- Finite loop rendering, mirrored folders below the working directory, no overwrite, and per-file error logging.
- PCM is recorded as stereo 12-bit direct PWM; generated files can also be played here. See [batch export](docs/VGZ_EXPORT.md).

## Changes in 0.26.1

- Changed MML parameters and edited instrument numbers in the list are red; Reset restores their normal color.
- Minimum height is calculated from the actual font to fit all nine MML lines. Initial width and height use the minimum size.
- Visibility, normal bounds and maximized/minimized state persist in the INI and restore at startup. Off-screen bounds are adjusted to an available monitor.
- The Instruments button toggles visibility. Closing the window hides it while retaining edits.
- Text formatting uses the Windows system Msftedit.dll, loaded dynamically.

## Changes in 0.26

- Updated the DLL, public C header and API documentation to Independent OPM 0.14.
- Instrument window follows NOTE-MDX-Workbench 2.8.21: MML and 47 knobs on the left, registers and algorithm on the right, audition keyboard and instrument list below. No Save button.
- All MDX instrument definitions are captured at playback start. Edits update every channel using that instrument, including future notes. Reset restores the selected original definition. Changed knobs turn red and a faint pointer marks the original value.

## Run

Extract the archive and run `bin/IndependentMDX.exe`. Keep `independent_opm.dll` beside it. No additional .NET or VC++ redistributable is required. Windows system DLLs are used. The optional X68Sound and C86CTL DLLs are not included.

Open/drop a file, use the browser or pass its path on the command line. An existing instance plays the requested file without restoring a minimized window. Japanese Windows defaults to Japanese; other languages default to English. Language can be switched manually.

Settings are saved about two seconds after changes to `IndependentMDX.ini` beside the executable. Preserve your existing INI when upgrading; none is included in this archive.

## Instrument editing

Select a channel or instrument number. Selecting a number also applies it to the current channel. Drag knobs horizontally or use the wheel. The 4×11 operator fields are AR, DR, SR, RR, SL, OL, KS, ML, DT1, DT2, AME; global fields are CON, FL, OP. MML text is editable and selectable. Incomplete/invalid text, out-of-range values and instrument renumbering are not applied; the last valid sound remains.

Edits apply at audio block boundaries, subject to queued audio latency. Seek, output changes and pause/resume retain edits. Starting playback anew reloads original definitions. Reset affects only the displayed instrument. VGM instruments remain read-only register views.

Click the keyboard to audition. Outside the MML text field, AWSEDFTGYHUJK plays 13 chromatic notes starting at C. PageUp/PageDown selects octaves 0–8, initially 4. Mono/Poly follows the main setting. The existing whole-file MML export exports original MDX definitions; copy edited MML from this window to retain it. Editing never modifies the MDX file.

## Playback and browser

Playback/pause/stop, seek, finite/infinite loops, automatic next track, WAV export, speed 0–200%, key −24…+24, output-specific gains, MIDI input/program changes, optional aftertouch, channel mute, keyboard/piano-roll views and carrier pitch display are supported. Alt+Enter toggles fullscreen.

Default columns are title, name, creation time, modification time, type, size. Sort initially uses name; click headings to sort, drag to resize/reorder, right-click to choose columns. Titles load progressively after the file list. Use arrows and Enter to navigate/play. MDX, PDX and VGM/VGZ filters default on. Bookmarks and parent-folder selection restoration are available.

Independent OPM / X68Sound default to FM 200%, PCM 100%, master 100%; C86CTL defaults to 100% each. Switching output preserves position. Delay changes apply to the next playback. External FM waveforms are DLL-generated estimates, not separated hardware recordings.

Single-YM2151 VGM and the direct-PWM profile produced by batch export are supported; other chips and DAC streams are rejected. External outputs require 4 MHz; the OPM DLL follows the file clock. See [VGM support](docs/VGM_SUPPORT.md). The reported periodic artifact in dense VGM playback has not been confirmed resolved by Windows recording tests.

## CLI, build and license

```bat
bin\IndependentMDX.exe "C:\Music\song.mdx"
bin\independent-mdx.exe song.mdx --wav output.wav --loops 2
bin\IndependentMDX.exe --validate-folder "C:\Music"
```

`--validate-folder` validates MDX recursively without opening the GUI, logs errors and collects failing MDX files. Console progress reports each changed folder. `--collect-error-mdx` enables collection during normal playback. Missing PDX is excluded from logging/collection. Use `independent-mdx.exe --help` for CLI options.

Build with Visual Studio 2022 C++ and CMake using `build_vs2022.bat`, or Zig 0.13.0 using `build_cross.sh`. Non-Windows core tests require a separately built OPM shared library via `INDEPENDENT_OPM_LIBRARY`.

See [LICENSE.txt](LICENSE.txt), [third-party notices](THIRD_PARTY_NOTICES.txt), [distribution audit](docs/LICENSE_AUDIT.md) and [validation](docs/VALIDATION.md). Instrument text/knob code adapted from Workbench retains its 0BSD copyright notice.

Lhasa 0.6.0 (ISC) is built from bundled sources and statically linked for LZH decoding. No external archiver is required.
