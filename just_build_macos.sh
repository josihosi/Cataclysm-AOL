#!/usr/bin/env bash
set -euo pipefail

CLEAN=1
INSTALL_DEPS=0
WITH_SUMMARY=0
ISOLATE_USERDIR=1
RESET_USERDIR=0
FAST_DEBUG=0

for arg in "$@"; do
  case "$arg" in
    --unclean) CLEAN=0 ;;
    --install-deps) INSTALL_DEPS=1 ;;
    --with-summary) WITH_SUMMARY=1 ;;
    --shared-userdir) ISOLATE_USERDIR=0 ;;
    --reset-userdir) RESET_USERDIR=1 ;;
    --fast|-f|--fast-debug) FAST_DEBUG=1 ;;
  esac
done

if [[ "$FAST_DEBUG" == "1" ]]; then
  CLEAN=0
fi

LLM_BG_SUMMARY_PYTHON=false
if [[ "$WITH_SUMMARY" == "1" ]]; then
  LLM_BG_SUMMARY_PYTHON=python3
fi

GIT_BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo detached)"
PROFILE_NAME="${GIT_BRANCH//\//_}"
PROFILE_NAME="${PROFILE_NAME//\\/_}"
[[ -z "$PROFILE_NAME" ]] && PROFILE_NAME=default
USERDIR_REL=""
if [[ "$ISOLATE_USERDIR" == "1" ]]; then
  USERDIR_REL=".userdata/${PROFILE_NAME}/"
fi

if [[ -n "$USERDIR_REL" ]]; then
  echo "Runtime userdir profile: $USERDIR_REL"
fi

MAKE_RELEASE=1
MAKE_SOUND=1
MAKE_LOCALIZE=1
MAKE_LANGUAGES=all
MAKE_EXTRA=()
if [[ "$FAST_DEBUG" == "1" ]]; then
  MAKE_RELEASE=0
  MAKE_SOUND=0
  MAKE_LOCALIZE=0
  MAKE_EXTRA+=(DEBUG_SYMBOLS=1 BACKTRACE=0)
  echo "Using fast build profile (--fast / -f): lower compile cost, auto-unclean."
fi

if [[ "$INSTALL_DEPS" == "1" ]]; then
  echo "Installing/updating macOS build dependencies via Homebrew..."
  HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1 brew install gettext ccache dylibbundler sdl2 sdl2_image sdl2_ttf sdl2_mixer pkg-config
fi

if [[ -f "config/options.macos.json" ]]; then
  echo "Applying macOS LLM options snapshot..."
  cp "config/options.macos.json" "config/options.json"
fi

if [[ "$RESET_USERDIR" == "1" && -n "$USERDIR_REL" ]]; then
  echo "Resetting userdir profile $USERDIR_REL"
  rm -rf "$USERDIR_REL"
fi

FONT_SEED=data/fontdata.json
if [[ ! -f "$FONT_SEED" && -f config/fonts.json ]]; then
  FONT_SEED=config/fonts.json
fi
if [[ -n "$USERDIR_REL" ]]; then
  mkdir -p "${USERDIR_REL}config"
  if [[ -f "$FONT_SEED" ]]; then
    cp -f "$FONT_SEED" "${USERDIR_REL}config/fonts.json"
  fi
fi

rm -f pch/main-pch.hpp.gch pch/main-pch.hpp.d pch/tests-pch.hpp.gch pch/tests-pch.hpp.d obj/*/pch/main-pch.hpp.gch zstd.a zstd_test.a cataclysm-tiles cataclysm-tlg-tiles || true
if [[ "$CLEAN" == "1" ]]; then
  echo "Cleaning native macOS build..."
  if ! make clean; then
    echo "make clean failed, continuing with artifact cleanup"
  fi
  rm -f zstd.a zstd_test.a cataclysm.a pch/tests-pch.hpp.gch pch/tests-pch.hpp.d || true
else
  echo "Skipping clean in native macOS build"
fi

export LLM_BG_SUMMARY_PYTHON
make_cmd=(
  make -j"$(sysctl -n hw.ncpu)"
  TILES=1 SOUND="$MAKE_SOUND" RELEASE="$MAKE_RELEASE" LOCALIZE="$MAKE_LOCALIZE"
  LINTJSON=0 ASTYLE=0 TESTS=0 USE_HOME_DIR=1 DYNAMIC_LINKING=1
  PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$(brew --prefix)/opt/gettext/lib/pkgconfig"
)
if [[ "$MAKE_LOCALIZE" == "1" ]]; then
  make_cmd+=(LANGUAGES="$MAKE_LANGUAGES")
fi
if (( ${#MAKE_EXTRA[@]} )); then
  make_cmd+=("${MAKE_EXTRA[@]}")
fi
"${make_cmd[@]}"
