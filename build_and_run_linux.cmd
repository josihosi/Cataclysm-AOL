@echo off
setlocal
set "CLEAN=1"
set "INSTALL_DEPS=0"
set "WITH_SUMMARY=0"
for %%A in (%*) do (
  if /I "%%~A"=="--unclean" set "CLEAN=0"
  if /I "%%~A"=="--install-deps" set "INSTALL_DEPS=1"
  if /I "%%~A"=="--with-summary" set "WITH_SUMMARY=1"
)
set "LLM_BG_SUMMARY_PYTHON=false"
if "%WITH_SUMMARY%"=="1" set "LLM_BG_SUMMARY_PYTHON=python3"

for /f "delims=" %%I in ('wsl.exe wslpath -a "%cd%"') do set "WSL_REPO=%%I"
if "%WSL_REPO%"=="" (
  echo Failed to resolve WSL path for %cd%
  exit /b 1
)

echo Building in WSL at %WSL_REPO%
wsl.exe bash -lc "if [ \"%INSTALL_DEPS%\" = \"1\" ]; then sudo apt-get update && sudo apt-get install -y pkg-config astyle libsdl2-dev libncursesw5-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev libpulse-dev ccache gettext parallel && sudo apt-get autoremove -y; else echo Skipping Linux dependency bootstrap; fi && cd \"%WSL_REPO%\" && if [ -f \"config/options.linux.json\" ]; then echo Applying Linux LLM options snapshot; cp \"config/options.linux.json\" \"config/options.json\"; fi && rm -f pch/main-pch.hpp.gch obj/*/pch/main-pch.hpp.gch zstd.a zstd_test.a && if [ \"%CLEAN%\" = \"1\" ]; then echo Cleaning in WSL; make clean; rm -f zstd.a; else echo Skipping clean in WSL; fi && export LLM_BG_SUMMARY_PYTHON=\"%LLM_BG_SUMMARY_PYTHON%\" && make -j$(nproc) TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0"
if errorlevel 1 exit /b 1

echo Running cataclysm-tiles in WSL...
wsl.exe bash -lc "cd \"%WSL_REPO%\" && ./cataclysm-tiles"
