# 0.16 dependency build subset

The supplied `bin/independent_opm.dll` is the user-provided latest x64 build, unchanged.
SHA-256: `d86593cad3f848f66e9ba5882140db11a822122a852ff7205cd1f6fdb9cb185a`.

The dependency directory contains the three public headers, core/C API sources, envelope/state-codec internal headers, DLL export definition, licenses and a minimal distribution CMake file. Source/license files are byte-identical to the supplied updated 0.11b archive. Upstream evidence, tests, examples and CLI are omitted. Application regression tests are retained under the root tests directory.

Build the application with the root CMake project as described in VALIDATION.md. This uses the supplied DLL on Windows. To build a replacement DLL separately in Visual Studio 2022:

```bat
cmake -S independent-opm -B opm-build -A x64
cmake --build opm-build --config Release
```

This does not replace the supplied DLL automatically. For a locally rebuilt DLL, copy `opm-build/Release/independent_opm.dll` beside the application. The loader now checks the three state API exports, so older DLLs missing them produce the existing missing-symbol error. The added APIs are available in the dependency; player-level snapshot/seek serialization is not implemented in this release.
