> Historical release note. These details describe the named earlier version; see README.md and docs/VALIDATION.md for the current distribution.

# Windows CTest correction (0.24)

The reported core-tests exit code 0xC00000FD is STATUS_STACK_OVERFLOW.
The regression suite contains many local synthesis fixtures. Its Windows
stack reserve is now explicitly 8 MiB (MSVC /STACK:8388608; MinGW equivalent),
instead of relying on the toolchain default. This setting applies only to
core-tests; the player executables and audio behavior are unchanged.

build_vs2022.bat now distinguishes configuration, compilation/linking, and
test failures. A test failure no longer suggests reinstalling Visual Studio.

Validation: Linux CTest passed (66 core regression groups). Windows/MSVC
execution is not available in the repair environment; rerun build_vs2022.bat
to regenerate, relink, and execute the Windows tests. If a failure remains,
provide build/Testing/Temporary/LastTest.log and the executable exit code.
