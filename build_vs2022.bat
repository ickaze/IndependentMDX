@echo off
setlocal
cd /d "%~dp0"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto configure_failed
cmake --build build --config Release
if errorlevel 1 goto failed
ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 goto test_failed
echo.
echo Built: build\Release\IndependentMDX.exe
pause
exit /b 0
:failed
echo Compilation or linking failed. See the errors above.
pause
exit /b 1
:configure_failed
echo CMake configuration failed. Check the errors above and your Visual Studio C++ / CMake installation.
pause
exit /b 1
:test_failed
echo Build succeeded, but tests failed. See build\Testing\Temporary\LastTest.log.
pause
exit /b 1
