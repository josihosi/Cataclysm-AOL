#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$root_dir"

version_override=""
for arg in "$@"; do
  case "$arg" in
    --version=*)
      version_override="${arg#--version=}"
      ;;
    --version)
      echo "Missing value for --version" >&2
      exit 1
      ;;
    --[0-9]*)
      version_override="${arg#--}"
      ;;
    -v)
      echo "Missing value for -v (use: -v 0.1.0)" >&2
      exit 1
      ;;
    *)
      ;;
  esac
done
if [ -n "$version_override" ]; then
  export VERSION="$version_override"
fi

if ! command -v zip >/dev/null 2>&1; then
  echo "zip is required. Install it via: pacman -S --needed zip" >&2
  exit 1
fi
if ! command -v msgfmt >/dev/null 2>&1; then
  echo "msgfmt is required. Install it via: pacman -S --needed gettext-devel" >&2
  exit 1
fi

make -j$(( $(nproc) + 0 )) \
  CCACHE=1 RELEASE=1 MSYS2=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 \
  LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0

make -j$(( $(nproc) + 0 )) \
  CCACHE=1 RELEASE=1 MSYS2=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 \
  LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 localization

rm -rf *bindist *cataclysmdda-*.zip

make -j1 -o distclean \
  CCACHE=1 RELEASE=1 MSYS2=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 \
  LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 bindist

deps="$(ldd cataclysm-tiles.exe | awk '{print $3}' | grep '^/ucrt64/bin/' | sort -u)"
if [ -z "$deps" ]; then
  echo "No runtime DLLs found via ldd; check MSYS2 UCRT64 environment." >&2
  exit 1
fi

cp -u $deps bindist/

cd bindist
if [ -n "$version_override" ]; then
  zip -r "../caol_${version_override//./_}.zip" *
else
  zip -r ../caol_0_1_0.zip *
fi
