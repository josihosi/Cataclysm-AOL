@echo off
setlocal
set "CLEAN=1"
set "INSTALL_DEPS=0"
set "WITH_SUMMARY=0"
set "ISOLATE_USERDIR=1"
set "RESET_USERDIR=0"
set "RESET_WORLDS=0"
set "FAST_DEBUG=0"
for %%A in (%*) do (
  if /I "%%~A"=="--unclean" set "CLEAN=0"
  if /I "%%~A"=="--install-deps" set "INSTALL_DEPS=1"
  if /I "%%~A"=="--with-summary" set "WITH_SUMMARY=1"
  if /I "%%~A"=="--shared-userdir" set "ISOLATE_USERDIR=0"
  if /I "%%~A"=="--reset-userdir" set "RESET_USERDIR=1"
  if /I "%%~A"=="--reset-worlds" set "RESET_WORLDS=1"
  if /I "%%~A"=="--resetworlds" set "RESET_WORLDS=1"
  if /I "%%~A"=="--reset-world" set "RESET_WORLDS=1"
  if /I "%%~A"=="--reset" set "RESET_WORLDS=1"
  if /I "%%~A"=="--fast" set "FAST_DEBUG=1"
  if /I "%%~A"=="-f" set "FAST_DEBUG=1"
  if /I "%%~A"=="--fast-debug" set "FAST_DEBUG=1"
)
if "%FAST_DEBUG%"=="1" set "CLEAN=0"
set "LLM_BG_SUMMARY_PYTHON=false"
if "%WITH_SUMMARY%"=="1" set "LLM_BG_SUMMARY_PYTHON=python3"
set "GIT_BRANCH=detached"
for /f "delims=" %%I in ('git rev-parse --abbrev-ref HEAD 2^>nul') do set "GIT_BRANCH=%%I"
if not defined GIT_BRANCH set "GIT_BRANCH=detached"
set "RUN_EXE=cataclysm-tiles"
echo %GIT_BRANCH% | findstr /I "ctlg" >nul && set "RUN_EXE=cataclysm-tlg-tiles"
set "PROFILE_NAME=%GIT_BRANCH:/=_%"
set "PROFILE_NAME=%PROFILE_NAME:\=_%"
if "%PROFILE_NAME%"=="" set "PROFILE_NAME=default"
set "USERDIR_REL="
if "%ISOLATE_USERDIR%"=="1" set "USERDIR_REL=.userdata/%PROFILE_NAME%/"

for /f "delims=" %%I in ('wsl.exe wslpath -a "%cd%"') do set "WSL_REPO=%%I"
if "%WSL_REPO%"=="" (
  echo Failed to resolve WSL path for %cd%
  exit /b 1
)
if not "%USERDIR_REL%"=="" (
  echo Runtime userdir profile: %USERDIR_REL%
)
echo Runtime executable: %RUN_EXE%
set "MAKE_RELEASE=1"
set "MAKE_SOUND=1"
set "MAKE_LOCALIZE=1"
set "MAKE_LANGUAGES=all"
set "MAKE_EXTRA="
if "%FAST_DEBUG%"=="1" (
  set "MAKE_RELEASE=0"
  set "MAKE_SOUND=0"
  set "MAKE_LOCALIZE=0"
  set "MAKE_LANGUAGES=none"
  set "MAKE_EXTRA= DEBUG_SYMBOLS=1 BACKTRACE=0"
  echo Using fast build profile ^(--fast / -f^): tiles playable, lower compile cost, auto-unclean.
)

echo Building in WSL at %WSL_REPO%
wsl.exe bash -lc "if [ \"%INSTALL_DEPS%\" = \"1\" ]; then sudo apt-get update && sudo apt-get install -y pkg-config astyle libsdl2-dev libncursesw5-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev libpulse-dev ccache gettext parallel && sudo apt-get autoremove -y; else echo Skipping Linux dependency bootstrap; fi && cd \"%WSL_REPO%\" && if [ \"%RESET_USERDIR%\" = \"1\" ] && [ -n \"%USERDIR_REL%\" ]; then echo Resetting userdir profile %USERDIR_REL%; rm -rf \"%USERDIR_REL%\"; fi && if [ \"%RESET_WORLDS%\" = \"1\" ] && [ -n \"%USERDIR_REL%\" ]; then echo Resetting worlds in profile %USERDIR_REL%; rm -rf \"%USERDIR_REL%save\"; rm -f \"%USERDIR_REL%config/lastworld.json\"; fi && if [ -n \"%USERDIR_REL%\" ]; then mkdir -p \"%USERDIR_REL%config\"; cp -f config/fonts.json \"%USERDIR_REL%config/fonts.json\"; fi && if [ -f \"config/options.linux.json\" ]; then echo Applying Linux LLM options snapshot; cp \"config/options.linux.json\" \"config/options.json\"; fi && rm -f pch/main-pch.hpp.gch obj/*/pch/main-pch.hpp.gch zstd.a zstd_test.a cataclysm-tiles cataclysm-tlg-tiles && if [ \"%CLEAN%\" = \"1\" ]; then echo Cleaning in WSL; make clean; rm -f zstd.a; else echo Skipping clean in WSL; fi && export LLM_BG_SUMMARY_PYTHON=\"%LLM_BG_SUMMARY_PYTHON%\" && make -j$(nproc) TILES=1 SOUND=%MAKE_SOUND% RELEASE=%MAKE_RELEASE% LOCALIZE=%MAKE_LOCALIZE% LANGUAGES=%MAKE_LANGUAGES% LINTJSON=0 ASTYLE=0%MAKE_EXTRA%"
if errorlevel 1 exit /b 1

echo Running cataclysm-tiles in WSL...
wsl.exe bash -lc "cd \"%WSL_REPO%\" && if [ -n \"%USERDIR_REL%\" ]; then ./%RUN_EXE% --userdir \"%USERDIR_REL%\"; else ./%RUN_EXE%; fi"
