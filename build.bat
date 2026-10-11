@echo off
setlocal EnableDelayedExpansion

REM ===========================================================================
REM  bklv Windows build launcher
REM
REM  This file only LOCATES the toolchain. All build logic lives in the
REM  top-level CMakeLists.txt, so there is exactly one description of how the
REM  project is built, on every platform.
REM
REM  Usage:
REM    build.bat                    GL backend       -> dist\*.exe
REM    build.bat --vk               Vulkan backend   -> dist\vk\*.exe
REM    build.bat --gl --only=demo   only build the main program, skip demo_01~07
REM    build.bat --gui              no console window (you lose BKLV_LOG_*)
REM    build.bat --sdl-force        force a full reconfigure of SDL3
REM
REM  An existing build directory is rebuilt incrementally, so re-running after
REM  editing one file only recompiles that file.
REM ===========================================================================

set "ROOT_DIR=%~dp0."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"
cd /d "%ROOT_DIR%"

REM ---------------------------------------------------------------------------
REM  Defaults
REM ---------------------------------------------------------------------------
set "GRAPHICS=gl"
set "ONLY="
set "GUI=0"
set "SDL_FORCE=0"
set "BUILD_ARGS="

REM ---------------------------------------------------------------------------
REM  Parse arguments
REM ---------------------------------------------------------------------------
:parse
if "%~1"=="" goto parsed
set "ARG=%~1"

if /I "%ARG%"=="--vk"       ( set "GRAPHICS=vk" & shift & goto parse )
if /I "%ARG%"=="--vulkan"   ( set "GRAPHICS=vk" & shift & goto parse )
if /I "%ARG%"=="--gl"       ( set "GRAPHICS=gl" & shift & goto parse )
if /I "%ARG%"=="--opengl"   ( set "GRAPHICS=gl" & shift & goto parse )
if /I "%ARG%"=="--gui"      ( set "GUI=1" & shift & goto parse )
if /I "%ARG%"=="--sdl-force" ( set "SDL_FORCE=1" & shift & goto parse )

if /I "%ARG%"=="--only" (
    set "ONLY=%~2"
    shift
    shift
    goto parse
)
REM Leading check on the prefix --only=, without writing a ')' inside a REM line
set "PREFIX=!ARG:~0,7!"
if /I "!PREFIX!"=="--only=" (
    set "ONLY=!ARG:~7!"
    shift
    goto parse
)

REM Everything else is forwarded verbatim to cmake (for example -DBKLV_TARGETS=).
set "BUILD_ARGS=%BUILD_ARGS% %ARG%"
shift
goto parse
:parsed

if defined ONLY set "BUILD_ARGS=%BUILD_ARGS% -DBKLV_TARGETS=%ONLY%"
REM Always pass this explicitly: it is a CMake cache entry, so leaving it out
REM would keep whatever --gui set the last time and silently stick.
if "%GUI%"=="1" (
    set "BUILD_ARGS=%BUILD_ARGS% -DBKLV_WINDOWS_GUI=ON"
) else (
    set "BUILD_ARGS=%BUILD_ARGS% -DBKLV_WINDOWS_GUI=OFF"
)

REM ---------------------------------------------------------------------------
REM  Locate MSYS2
REM ---------------------------------------------------------------------------
set "MSYS_ROOT="
if defined MSYS2_ROOT if exist "%MSYS2_ROOT%\usr\bin\bash.exe" set "MSYS_ROOT=%MSYS2_ROOT%"

if not defined MSYS_ROOT if exist "E:\bin\msys64\usr\bin\bash.exe" set "MSYS_ROOT=E:\bin\msys64"
if not defined MSYS_ROOT if exist "C:\msys64\usr\bin\bash.exe"     set "MSYS_ROOT=C:\msys64"
if not defined MSYS_ROOT if exist "D:\msys64\usr\bin\bash.exe"     set "MSYS_ROOT=D:\msys64"
if not defined MSYS_ROOT if exist "%LOCALAPPDATA%\msys64\usr\bin\bash.exe" set "MSYS_ROOT=%LOCALAPPDATA%\msys64"
if not defined MSYS_ROOT if exist "%ProgramFiles%\msys64\usr\bin\bash.exe" set "MSYS_ROOT=%ProgramFiles%\msys64"
if not defined MSYS_ROOT if exist "%SystemDrive%\msys64\usr\bin\bash.exe"  set "MSYS_ROOT=%SystemDrive%\msys64"

if not defined MSYS_ROOT (
    for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MSYS2 64bit" /v InstallLocation 2^>nul ^| findstr InstallLocation') do (
        if exist "%%B\usr\bin\bash.exe" set "MSYS_ROOT=%%B"
    )
)

if not defined MSYS_ROOT (
    echo [ERROR] MSYS2 not found.
    echo         Set MSYS2_ROOT, or install it from https://www.msys2.org/
    echo         Default search paths: E:\bin\msys64, C:\msys64, D:\msys64,
    echo         %%LOCALAPPDATA%%\msys64, %%ProgramFiles%%\msys64
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM  Locate the MinGW-w64 toolchain
REM ---------------------------------------------------------------------------
set "TOOLCHAIN="
if defined MSYS2_ENV if exist "%MSYS_ROOT%\%MSYS2_ENV%\bin\g++.exe" set "TOOLCHAIN=%MSYS_ROOT%\%MSYS2_ENV%"
if not defined TOOLCHAIN if exist "%MSYS_ROOT%\ucrt64\bin\g++.exe" set "TOOLCHAIN=%MSYS_ROOT%\ucrt64"
if not defined TOOLCHAIN if exist "%MSYS_ROOT%\mingw64\bin\g++.exe" set "TOOLCHAIN=%MSYS_ROOT%\mingw64"

if not defined TOOLCHAIN (
    echo [ERROR] No MinGW-w64 toolchain found under %MSYS_ROOT%
    echo         Install it with:
    echo             pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
    exit /b 1
)

for %%I in ("%TOOLCHAIN%\bin\g++.exe")     do set "GXX=%%~fI"
for %%I in ("%TOOLCHAIN%\bin\cmake.exe")   do set "CMAKEEXE=%%~fI"
for %%I in ("%TOOLCHAIN%\bin\ninja.exe")   do set "NINJAEXE=%%~fI"

REM ---------------------------------------------------------------------------
REM  MSYS2 installs cmake and ninja under usr\bin as well. Take those when the
REM  toolchain directory does not carry its own copy.
REM ---------------------------------------------------------------------------
if not exist "%CMAKEEXE%" if exist "%MSYS_ROOT%\usr\bin\cmake.exe"   set "CMAKEEXE=%MSYS_ROOT%\usr\bin\cmake.exe"
if not exist "%NINJAEXE%" if exist "%MSYS_ROOT%\usr\bin\ninja.exe"  set "NINJAEXE=%MSYS_ROOT%\usr\bin\ninja.exe"

if not exist "%CMAKEEXE%" (
    echo [ERROR] cmake not found.
    echo         Install it with:  pacman -S mingw-w64-ucrt-x86_64-cmake ninja
    exit /b 1
)
if not exist "%NINJAEXE%" (
    echo [ERROR] ninja not found.
    echo         Install it with:  pacman -S mingw-w64-ucrt-x86_64-ninja
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM  Make the toolchain reachable. cmake looks up the compiler in PATH, so this
REM  is what actually selects mingw-w64 rather than some other gcc on the system.
REM ---------------------------------------------------------------------------
set "PATH=%TOOLCHAIN%\bin;%MSYS_ROOT%\usr\bin;%PATH%"

REM ---------------------------------------------------------------------------
REM  Build directory per backend, so GL and Vulkan never overwrite each other
REM  and each can be rebuilt incrementally.
REM ---------------------------------------------------------------------------
if /I "%GRAPHICS%"=="vk"   set "BUILD_DIR=build\vk"
if /I "%GRAPHICS%"=="gl"   set "BUILD_DIR=build\gl"

if "%SDL_FORCE%"=="1" (
    echo [INFO] removing %BUILD_DIR% for a full reconfigure
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

REM ---------------------------------------------------------------------------
REM  Configure
REM ---------------------------------------------------------------------------
set "CMAKE_ARGS=-S . -B %BUILD_DIR% -G Ninja -DCMAKE_MAKE_PROGRAM=%NINJAEXE%"
set "CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_C_COMPILER=%TOOLCHAIN%\bin\gcc.exe"
set "CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_CXX_COMPILER=%GXX%"
set "CMAKE_ARGS=%CMAKE_ARGS% -DBKLV_GRAPHICS=%GRAPHICS%"
set "CMAKE_ARGS=%CMAKE_ARGS% %BUILD_ARGS%"

echo [INFO] MSYS2   : %MSYS_ROOT%
echo [INFO] toolchain: %TOOLCHAIN%
echo [INFO] backend : %GRAPHICS%
echo [INFO] build dir: %BUILD_DIR%

"%CMAKEEXE%" %CMAKE_ARGS%
if errorlevel 1 (
    echo [ERROR] configure failed
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM  Build
REM ---------------------------------------------------------------------------
"%CMAKEEXE%" --build "%BUILD_DIR%"
if errorlevel 1 (
    echo [ERROR] build failed
    exit /b 1
)

if /I "%GRAPHICS%"=="vk" (
    echo [OK] backend %GRAPHICS%, output in %ROOT_DIR%\dist\vk
) else (
    echo [OK] backend %GRAPHICS%, output in %ROOT_DIR%\dist
)

REM Pause only when double-clicked from Explorer, so a failure is readable.
set "CMDLINE_NOC=!CMDCMDLINE:/c=!"
if not "!CMDLINE_NOC!"=="!CMDCMDLINE!" pause
exit /b 0
