@echo off
setlocal enabledelayedexpansion

set "CONFIG_PATH=config\config.json"
set "DEBUG_EXE=build\debug\Debug\FrostMonitor.exe"
set "RELEASE_EXE=build\release\Release\FrostMonitor.exe"

if /i "%~1"=="build"   goto :build
if /i "%~1"=="run"     goto :run
if /i "%~1"=="test"    goto :test
if /i "%~1"=="lint"    goto :lint
if /i "%~1"=="clean"   goto :clean
if /i "%~1"=="help"    goto :help
goto :default

:default
echo [dev] building release then running...
call :build release
if errorlevel 1 exit /b 1
call :run release
exit /b 0

:build
set "PRESET=%~2"
if not defined PRESET set "PRESET=release"
echo [dev] configuring %PRESET%...
cmake --preset %PRESET% || exit /b 1
echo [dev] building %PRESET%...
cmake --build --preset %PRESET% || exit /b 1
echo [dev] build ok
exit /b 0

:run
set "PRESET=%~2"
if not defined PRESET set "PRESET=release"
if /i "%PRESET%"=="debug" set "EXE=%DEBUG_EXE%"
if /i "%PRESET%"=="release" set "EXE=%RELEASE_EXE%"
if not exist "%EXE%" (
    echo [dev] "%EXE%" not found - run "%~nx0 build %PRESET%" first
    exit /b 1
)
echo [dev] running %EXE% with %CONFIG_PATH% (Ctrl+C to stop)...
"%EXE%" "%CONFIG_PATH%"
echo [dev] exit code: %errorlevel%
exit /b 0

:test
set "PRESET=%~2"
if not defined PRESET set "PRESET=debug"
if not exist "build\%PRESET%\CMakeCache.txt" (
    echo [dev] configuring %PRESET% first...
    call :build %PRESET%
)
echo [dev] running tests (%PRESET%)...
ctest --preset %PRESET% || exit /b 1
exit /b 0

:lint
if not defined CLANG_TIDY set "CLANG_TIDY=C:\Program Files\LLVM\bin\clang-tidy.exe"
if not exist "%CLANG_TIDY%" set "CLANG_TIDY=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\bin\clang-tidy.exe"
if not exist "%CLANG_TIDY%" (
    echo [dev] clang-tidy not found - install LLVM or set CLANG_TIDY to its path
    exit /b 1
)
echo [dev] configuring lint compile database (Ninja)...
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cmake --preset lint || exit /b 1
echo [dev] running clang-tidy...
"%CLANG_TIDY%" --warnings-as-errors=* -p build\lint src\main.cpp src\config.cpp src\cpu.cpp src\gpu.cpp src\format.cpp src\gamesense.cpp tests\test_config.cpp tests\test_format.cpp tests\test_version.cpp tests\test_gamesense.cpp || exit /b 1
echo [dev] lint ok
exit /b 0

:clean
echo [dev] removing build directories...
if exist build rmdir /s /q build
echo [dev] clean
exit /b 0

:help
echo Usage: %~nx0 [command] [debug^|release]
echo.
echo   build [debug^|release]  configure + compile (default: release)
echo   run   [debug^|release]  launch the exe with config\config.json
echo   test  [debug^|release]  run the unit tests via ctest (default: debug)
echo   lint                   run clang-tidy on all sources (CLANG_TIDY env overrides path)
echo   clean                  delete the build\ folder
echo   (no args)              build release, then run it
exit /b 0