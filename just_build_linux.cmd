@echo off
setlocal
set "CLEAN=1"
for %%A in (%*) do (
  if /I "%%~A"=="--unclean" set "CLEAN=0"
)

for /f "delims=" %%I in ('wsl.exe wslpath -a "%cd%"') do set "WSL_REPO=%%I"
if "%WSL_REPO%"=="" (
  echo Failed to resolve WSL path for %cd%
  exit /b 1
)

echo Building in WSL at %WSL_REPO%
wsl.exe bash -lc "sudo apt-get update && sudo apt-get install -y pkg-config astyle libsdl2-dev libncursesw5-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev libpulse-dev ccache gettext parallel && sudo apt-get autoremove -y && cd \"%WSL_REPO%\" && if [ -f \"config/options.linux.json\" ]; then echo Applying Linux LLM options snapshot; cp \"config/options.linux.json\" \"config/options.json\"; fi && if [ \"%CLEAN%\" = \"1\" ]; then echo Cleaning in WSL; make clean; rm -f zstd.a; else echo Skipping clean in WSL; fi && export LLM_BG_SUMMARY_PYTHON=python3 && make -j$(nproc) TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0"
