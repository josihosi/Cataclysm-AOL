@echo off
set "MSYSTEM=UCRT64"
set "CLEAN=1"
set "WITH_SUMMARY=0"
for %%A in (%*) do (
  if /I "%%~A"=="--unclean" set "CLEAN=0"
  if /I "%%~A"=="--with-summary" set "WITH_SUMMARY=1"
)
set "LLM_BG_SUMMARY_PYTHON=false"
if "%WITH_SUMMARY%"=="1" set "LLM_BG_SUMMARY_PYTHON=C:/Users/josef/openvino_models/openvino_env/Scripts/python.exe"
if exist config\options.windows.json (
  echo Applying Windows LLM options snapshot...
  copy /Y config\options.windows.json config\options.json >nul
)

"C:\Users\josef\dev\msys64\usr\bin\bash.exe" -lc "cd /c/Users/josef/dev/Cataclysm-AOL && export LLM_BG_SUMMARY_PYTHON=\"%LLM_BG_SUMMARY_PYTHON%\" && rm -f pch/main-pch.hpp.gch obj*/pch/main-pch.hpp.gch objwin/*/pch/main-pch.hpp.gch && if [ \"%CLEAN%\" = \"1\" ]; then echo Cleaning in MSYSTEM=$MSYSTEM; make clean; rm -f zstd.a; else echo Skipping clean in MSYSTEM=$MSYSTEM; fi && echo Building in MSYSTEM=$MSYSTEM && make -j$(nproc) RELEASE=1 MSYS2=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 && ./cataclysm-tiles"
