@echo off
setlocal
set "MSYSTEM=UCRT64"
set "CLEAN=1"
set "WITH_SUMMARY=0"
set "ISOLATE_USERDIR=1"
set "RESET_USERDIR=0"
set "FAST_DEBUG=0"
for %%A in (%*) do (
  if /I "%%~A"=="--unclean" set "CLEAN=0"
  if /I "%%~A"=="--with-summary" set "WITH_SUMMARY=1"
  if /I "%%~A"=="--shared-userdir" set "ISOLATE_USERDIR=0"
  if /I "%%~A"=="--reset-userdir" set "RESET_USERDIR=1"
  if /I "%%~A"=="--fast" set "FAST_DEBUG=1"
  if /I "%%~A"=="-f" set "FAST_DEBUG=1"
  if /I "%%~A"=="--fast-debug" set "FAST_DEBUG=1"
)
if "%FAST_DEBUG%"=="1" set "CLEAN=0"
set "LLM_BG_SUMMARY_PYTHON=false"
if "%WITH_SUMMARY%"=="1" set "LLM_BG_SUMMARY_PYTHON=C:/Users/josef/openvino_models/openvino_env/Scripts/python.exe"
set "GIT_BRANCH=detached"
for /f "delims=" %%I in ('git rev-parse --abbrev-ref HEAD 2^>nul') do set "GIT_BRANCH=%%I"
if not defined GIT_BRANCH set "GIT_BRANCH=detached"
set "PROFILE_NAME=%GIT_BRANCH:/=_%"
set "PROFILE_NAME=%PROFILE_NAME:\=_%"
if "%PROFILE_NAME%"=="" set "PROFILE_NAME=default"
set "USERDIR_REL="
if "%ISOLATE_USERDIR%"=="1" set "USERDIR_REL=.userdata/%PROFILE_NAME%/"
if exist config\options.windows.json (
  echo Applying Windows LLM options snapshot...
  copy /Y config\options.windows.json config\options.json >nul
)
if not "%USERDIR_REL%"=="" (
  echo Runtime userdir profile: %USERDIR_REL%
)
set "MAKE_RELEASE=1"
set "MAKE_SOUND=1"
set "MAKE_LOCALIZE=1"
set "MAKE_LANG_FLAGS= LANGUAGES=all"
set "MAKE_EXTRA="
if "%FAST_DEBUG%"=="1" (
  set "MAKE_RELEASE=0"
  set "MAKE_SOUND=0"
  set "MAKE_LOCALIZE=0"
  set "MAKE_LANG_FLAGS="
  set "MAKE_EXTRA= DEBUG_SYMBOLS=1 BACKTRACE=0"
  echo Using fast build profile ^(--fast / -f^): tiles playable, lower compile cost, auto-unclean.
)

"C:\Users\josef\dev\msys64\usr\bin\bash.exe" -lc "cd /c/Users/josef/dev/Cataclysm-AOL && if [ \"%RESET_USERDIR%\" = \"1\" ] && [ -n \"%USERDIR_REL%\" ]; then echo Resetting userdir profile %USERDIR_REL%; rm -rf \"%USERDIR_REL%\"; fi && export LLM_BG_SUMMARY_PYTHON=\"%LLM_BG_SUMMARY_PYTHON%\" && rm -f pch/main-pch.hpp.gch obj*/pch/main-pch.hpp.gch objwin/*/pch/main-pch.hpp.gch zstd.a zstd_test.a && if [ \"%CLEAN%\" = \"1\" ]; then echo Cleaning in MSYSTEM=$MSYSTEM; make clean; rm -f zstd.a; else echo Skipping clean in MSYSTEM=$MSYSTEM; fi && echo Building in MSYSTEM=$MSYSTEM && make -j$(nproc) RELEASE=%MAKE_RELEASE% MSYS2=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=%MAKE_SOUND% LOCALIZE=%MAKE_LOCALIZE%%MAKE_LANG_FLAGS% LINTJSON=0 ASTYLE=0 TESTS=0%MAKE_EXTRA% && if [ -n \"%USERDIR_REL%\" ]; then ./cataclysm-tiles.exe --userdir \"%USERDIR_REL%\"; else ./cataclysm-tiles.exe; fi"
