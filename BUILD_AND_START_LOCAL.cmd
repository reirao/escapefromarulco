@echo off
setlocal EnableExtensions

title Escape from Arulco - Build and Start
color 0C

rem Self-contained local developer launcher. Optional overrides:
rem   set EFA_BUILD_DIR=D:\build\escape-from-arulco
rem   set EFA_MSYS2_ROOT=C:\msys64
rem   set EFA_BUILD_JOBS=4
set "EFA_SOURCE_DIR=%~dp0"
if "%EFA_SOURCE_DIR:~-1%"=="\" set "EFA_SOURCE_DIR=%EFA_SOURCE_DIR:~0,-1%"
if not defined EFA_BUILD_DIR set "EFA_BUILD_DIR=C:\tmp\ja2-sandbox-build"
if not defined EFA_MSYS2_ROOT set "EFA_MSYS2_ROOT=C:\msys64"
if not defined EFA_BUILD_JOBS set "EFA_BUILD_JOBS=2"

set "EFA_MINGW_BIN=%EFA_MSYS2_ROOT%\mingw64\bin"
set "EFA_USR_BIN=%EFA_MSYS2_ROOT%\usr\bin"
set "EFA_CMAKE=%EFA_MINGW_BIN%\cmake.exe"
set "EFA_NINJA=%EFA_MINGW_BIN%\ninja.exe"
set "PATH=%EFA_MINGW_BIN%;%EFA_USR_BIN%;%PATH%"

echo.
echo ============================================================
echo   ESCAPE FROM ARULCO - CONFIGURE, BUILD, START
echo ============================================================
echo   Source: %EFA_SOURCE_DIR%
echo   Build : %EFA_BUILD_DIR%
echo.

if not exist "%EFA_CMAKE%" goto :missing_tools
if not exist "%EFA_NINJA%" goto :missing_tools

if not exist "%EFA_BUILD_DIR%" mkdir "%EFA_BUILD_DIR%"
if errorlevel 1 goto :failed

echo [1/4] Configuring CMake...
"%EFA_CMAKE%" -S "%EFA_SOURCE_DIR%" -B "%EFA_BUILD_DIR%" -G Ninja -DWITH_UNITTESTS=OFF
if errorlevel 1 goto :failed

echo.
echo [2/4] Building ja2.exe...
"%EFA_CMAKE%" --build "%EFA_BUILD_DIR%" --target ja2 --parallel %EFA_BUILD_JOBS%
if errorlevel 1 goto :failed

echo.
echo [3/4] Installing MinGW runtime DLLs...
for %%F in (SDL2.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
	if not exist "%EFA_MINGW_BIN%\%%F" goto :missing_runtime
	"%EFA_CMAKE%" -E copy_if_different "%EFA_MINGW_BIN%\%%F" "%EFA_BUILD_DIR%\%%F"
	if errorlevel 1 goto :failed
)

if not exist "%EFA_BUILD_DIR%\ja2.exe" goto :failed

echo.
echo [4/4] Starting %EFA_BUILD_DIR%\ja2.exe...
if "%~1"=="" (
	start "Escape from Arulco" /D "%EFA_BUILD_DIR%" "%EFA_BUILD_DIR%\ja2.exe" -window
) else (
	start "Escape from Arulco" /D "%EFA_BUILD_DIR%" "%EFA_BUILD_DIR%\ja2.exe" %*
)
if errorlevel 1 goto :failed

echo Done. Future runs compile only changed files.
"%SystemRoot%\System32\timeout.exe" /t 2 /nobreak >nul
exit /b 0

:missing_tools
echo.
echo ERROR: CMake or Ninja was not found below:
echo   %EFA_MINGW_BIN%
echo Install the MSYS2 MinGW64 toolchain or set EFA_MSYS2_ROOT.
goto :stop

:missing_runtime
echo.
echo ERROR: A required runtime DLL was not found in:
echo   %EFA_MINGW_BIN%
goto :stop

:failed
echo.
echo ERROR: Configure, build, DLL copy, or launch failed.
echo Read the last error above. The game was not started.

:stop
echo.
pause
exit /b 1
