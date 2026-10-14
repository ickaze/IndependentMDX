# IndependentMDX 0.33.5 validation


## 0.33.5

See [RNG recurrence and executed comparisons](RANDOM_LFO_0.33.5.md): 202/202 available probe cases match target register states; native CTest 5/5; Windows cross-build and binary checks passed.

## 0.33.4

See [random LFO arithmetic and limits](RANDOM_LFO_0.33.4.md). RNG output remains approximate.

## 0.33.3

See [signed LFO validation](LFO_SIGNED_0.33.3.md): 48/58 matching captures, 5/5 native test suites, Windows cross-build and limits.

## 0.33.2

See [LFO_SIGNED_0.33.2.md](LFO_SIGNED_0.33.2.md) for signed MA trace comparison and regression/build scope.

## 0.33.1

The bookmark combobox now includes WS_VSCROLL. Windows GUI/CLI and the x86 bridge were cross-built; PE debug information and dependency checks were repeated. GUI interaction was not executed in this environment. Core playback and archive behavior are unchanged; their executed regression results remain in the 0.33 section.

## 0.33

See [RELEASE_0.33.md](RELEASE_0.33.md) for archive/PDX tests and build-only Windows limits. Historical sections below retain their original scope.

## 0.32

See [RELEASE_0.32.md](RELEASE_0.32.md) for build/test limits and [LFO_COMPARISON_0.32.md](LFO_COMPARISON_0.32.md) for the new register comparison. The unresolved software RNG is not claimed to match. Historical sections below apply to their named versions.

## 0.31

See [RELEASE_0.31.md](RELEASE_0.31.md) for the 0.31 test scope and [LFO_COMPARISON_0.31.md](LFO_COMPARISON_0.31.md) for the observed unresolved behavior. Historical results below describe their named versions.

## 0.30.1 output correction validation

Native Release CTest: **5/5 suites passed**. Windows GUI/CLI and x86 bridge cross-build succeeded.

- Native regression coverage adds eight isolated-operator cases: four KON bits through immediate and timestamped DLL writes.
- Eight supplied MRL/MRZ inputs were rendered for 20 seconds with PCM muted. Three reproduced total FM silence under the obsolete routing; five reproduced missing channels. All missing observed channel outputs returned with raw KON passthrough. See [OUTPUT_FIX_0.30.1.md](OUTPUT_FIX_0.30.1.md) for anonymized measurements and scope.
- Corrected partial-operator test masks and the project's original DEMO MDX/MRL/ZIP fixture. The log fixture remains a 588-record format example, not a byte-for-byte baseline for current sequencer output.
- The Independent OPM 0.14 Windows DLL is unchanged. Existing logs and user settings are preserved. Windows/device execution remains unverified.


## 0.30 executed validation

- Native Release CTest: **5/5 suites passed** (MRL, archive, VGZ export, VGM, MDX/core).
- Re-exported 38 representative MDX inputs at two loops as legacy FM MRL. All 38 completed. Compared against the supplied reference logs at raw native timestamps: key-on counts matched for all 38; common key-on timestamp prefixes matched exactly for 37. Voice parameter and hardware-LFO frequency/waveform/sensitivity state differences were absent in the compared ranges; pitch, volume and some PAN/AL/FL differences remain. This is a selected regression set, not a revalidation of all 8,943 pairs.
- See [MRL_VALIDATION.html](MRL_VALIDATION.html) for the full baseline dataset, method, anonymized per-input results and limitations. Random-wave behavior remains an independent approximation.
- A real approximately 1.3 GB ZIP was indexed and a 29,252,224-byte / 1,828,260-event MRL was played for one second in a native Linux probe. Peak RSS was 12,596 KiB. Title lookup did not materialize the track. This is a measurement for that workload, not a Windows/PDX memory guarantee.
- A sparse ZIP64 fixture with member data beyond 4 GiB passed index/extraction checks. ZIP32, Deflate, stored entries, nested paths, CRC and unsafe-path checks also passed.
- New MRL tests cover UTF-8 titles, embedded PDX, external lookup and override, missing/mismatched banks, eight-channel PCM mixing and masks, playback monitoring, seek reconstruction and MRZ round trips. MRL playback retains a 64 KiB event buffer instead of all records.
- Windows x64 GUI/CLI and x86 X68Sound bridge cross-build succeeded. GUI and hardware execution were not available. The Independent OPM DLL remains byte-identical to upstream 0.14.

## Earlier validation coverage

### Executed tests before 0.30

Linux Release build against an independently compiled, unmodified Independent OPM 0.14 shared library:

- CTest: 4/4 suites passed.
- VGZ export: PCM omission preserving all FM events/timestamps and duration, Japanese GD3 title round trip, converter/comment fields, GD3 size/offset, and finite loop durations, FM register output, PDX/PWM playback, no overwrite, timeout without partial files, mirrored subfolders, invalid-file continuation, error logging, argument validation and gzip round trips at block boundaries.
- MDX/core: 76 groups, including 1,000 malformed-input mutations.
- VGM: 16 groups, including event timing, high-density streams, seek and channel output.
- New instrument coverage checks all-definition snapshots including unused instruments, MML conversion, live updates to two channels sharing one voice, no key-off during an edit, unchanged initial definitions, edited voices after seek, reset, fresh-session restoration and editing a held MIDI note.

The native test library is built from the separate upstream 0.14 source release; it is not included in this package. Passing native tests does not prove the supplied Windows DLL's execution behavior.

Parameter-span checks cover all 47 values, skipping instrument IDs and comments, including signed hexadecimal values. These are executed native tests.

## Build and static checks

Windows x64 GUI/CLI and the x86 X68Sound bridge are cross-built with Zig 0.13.0. Windows GUI tests are cross-compiled, not executed. The GUI test source also checks RichEdit parameter color, nine-line text space, visibility toggling, INI fields and window bounds restoration. These assertions are compiled only; they require Windows to execute.

The bundled DLL, C header and license are compared byte-for-byte with the Independent OPM 0.14 Windows release. DLL exports/imports and application dependencies are inspected statically. Release executables are stripped of debug directories and were checked for CodeView/PDB records and workspace/toolchain paths. The upstream DLL retains only its COFF group debug-directory entry; no CodeView path is present. The supplied DLL remains byte-identical. A generated PCM VGZ also passed Python gzip decompression and CRC verification. SHA256SUMS.txt covers the packaged files.

## Not verified

No native Windows/Wine runtime, MIDI device, X68Sound runtime or C86CTL hardware is available in the test environment. Visual layout, DPI scaling, knob interaction, device output and real-time editing latency require Windows verification. GUI compilation is not a visual test.

The previously reported periodic audible artifact in dense VGM playback remains unconfirmed by a Windows recording comparison. Updating to OPM 0.14 and passing offline regression tests must not be interpreted as confirmation that this symptom is fixed.

## Reproduce

Windows: run build_vs2022.bat with Visual Studio 2022 C++ desktop tools and CMake. This runs all CTest targets, including GUI tests.

Non-Windows core tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DINDEPENDENT_OPM_LIBRARY=/absolute/path/to/libindependent_opm.so
cmake --build build
ctest --test-dir build --output-on-failure
```

The library must implement the 0.14 C ABI used by the bundled header. A test-only 8 MiB Windows stack reserve is configured for core-tests to avoid STATUS_STACK_OVERFLOW from local fixtures. Test executables are not included in the binary folder.

## 0.27.1 export performance

Linux Release CLI, bundled original FM+PCM sample, `--loops 2`, three sequential conversions per mode to fresh output directories. Median wall time, including process startup and disk output:

| Mode | Seconds |
| --- | ---: |
| 0.27, PCM included | 0.915 |
| 0.27.1, PCM included | 0.140 |
| 0.27.1, --no-pcm | 0.086 |

PCM-inclusive conversion was approximately 6.5 times faster on this fixture. This is not a Windows benchmark or a guarantee for other tracks. Decompressed VGM command bytes, including wait commands and PWM data, were identical before and after the optimization; the new header and GD3 metadata were excluded from that comparison. Each generated gzip stream passed independent Python decompression/CRC verification.

## 0.28 register-log validation

The native export suite checks the MDXRLOG magic/version/layout, record count, complete-file length, clock and quantum, all event values/order/timestamps against normal playback with FM synthesis enabled, no-overwrite behavior, timeout cleanup, recursive layout, error continuation and incompatible-option rejection. A separate Python reader checks the documented struct layouts and verifies the source CRC with zlib. This validates the writer and interchange format, not MXDRV compatibility. No third-party MXDRV source was consulted for this register-log feature.

## 0.29 archive/MRL validation

Native tests cover Deflate and Stored ZIP extraction, Japanese names, nested ZIP paths, PDX lookup, MRL rendering and seek/resume, fractional timestamps, invalid MRL headers/chips/truncation and ZIP traversal rejection. Windows GUI tests add ZIP navigation, bookmark paths/restoration and parent selection checks; these are compiled only, not executed. ZIP browsing layout, live device output and shutdown behavior require Windows runtime verification. No external archive DLL was introduced.

## 0.29.1 export diagnostic regression

- Native tests export generated MDX fixtures containing both MP and MA to MRL and VGZ.
- A missing-voice error after the LFO warning still aborts both formats and publishes no partial output.
- Three supplied MDX inputs completed native two-loop MRL export with no error CSV entries. Output is byte-identical to the same engine's earlier capture with only the LFO diagnostic tolerated.
- This verifies warning handling, not MXDRV compatibility. LFO arithmetic, gate behavior and event timing are unchanged. The entire previously rejected collection has not been re-exported in this release validation.
- Windows binaries are cross-built; runtime execution on Windows is not verified here.
