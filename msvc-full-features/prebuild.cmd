@echo off
SETLOCAL

cd ..\msvc-full-features
set PATH=%PATH%;%VSAPPIDDIR%\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd
if not "%CAOL_RELEASE_VERSION%"=="" (
set VERSION=%CAOL_RELEASE_VERSION%
) else (
for /F "tokens=*" %%i in ('git describe --tags --always --dirty --match "cdda-experimental-*"') do set VERSION=%%i
)
if "%VERSION%"=="" (
set VERSION=Please install `git` to generate VERSION
)
findstr /c:%VERSION% ..\src\version.h > NUL 2> NUL
if %ERRORLEVEL% NEQ 0 (
echo Generating "version.h"...
echo VERSION defined as "%VERSION%"
>..\src\version.h echo #define VERSION "%VERSION%"
)
