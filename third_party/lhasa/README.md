# Lhasa 0.6.0 integration

Upstream: https://github.com/fragglet/lhasa/tree/v0.6.0
Source archive: https://codeload.github.com/fragglet/lhasa/tar.gz/refs/tags/v0.6.0
SHA-256: `427c47527f11d157380d90e33c86535a3a0e8eb5c7e2cf0278bd6dbfe733fcee`

The library C/header files are unmodified. `sources.cmake` and `sources.txt`
select the translation units statically linked by IndependentMDX. The main
project's CMake/Visual Studio or build_cross.sh builds them automatically.
No lhasa DLL, installed archiver or network download is required at build/run time.
The upstream CLI, build system and test corpus are not part of this integration.
README.upstream.md and NEWS.md are upstream reference documents, not build instructions
for this distribution. See COPYING.md and the per-source copyright notices.
IndependentMDX uses header parsing and stream decoding only. It does not invoke
Lhasa's filesystem extraction or restore file attributes/permissions.
