@echo off
set "MSYSTEM=UCRT64"

"C:\Users\josef\dev\msys64\usr\bin\bash.exe" -lc "cd /c/Users/josef/dev/Cataclysm-AOL && echo Building in MSYSTEM=$MSYSTEM && make -j$(nproc) RELEASE=1 MSYS2=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 && ./cataclysm-tiles"
