#!/usr/bin/env bash
set -euo pipefail

CLEAN=1
INSTALL_DEPS=0
WITH_SUMMARY=0
ISOLATE_USERDIR=1
RESET_USERDIR=0
RESET_WORLDS=0
FAST_DEBUG=0

for arg in "$@"; do
  case "$arg" in
    --unclean) CLEAN=0 ;;
    --install-deps) INSTALL_DEPS=1 ;;
    --with-summary) WITH_SUMMARY=1 ;;
    --shared-userdir) ISOLATE_USERDIR=0 ;;
    --reset-userdir) RESET_USERDIR=1 ;;
    --reset-worlds|--resetworlds|--reset-world|--reset) RESET_WORLDS=1 ;;
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

echo "Runtime executable: auto-detect from built artifact"

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
if [[ "$RESET_WORLDS" == "1" && -n "$USERDIR_REL" ]]; then
  echo "Resetting worlds in profile $USERDIR_REL"
  rm -rf "${USERDIR_REL}save"
  rm -f "${USERDIR_REL}config/lastworld.json"
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

args=()
[[ "$CLEAN" == "0" ]] && args+=(--unclean)
[[ "$INSTALL_DEPS" == "1" ]] && args+=(--install-deps)
[[ "$WITH_SUMMARY" == "1" ]] && args+=(--with-summary)
[[ "$ISOLATE_USERDIR" == "0" ]] && args+=(--shared-userdir)
[[ "$RESET_USERDIR" == "1" ]] && args+=(--reset-userdir)
[[ "$FAST_DEBUG" == "1" ]] && args+=(--fast)

if (( ${#args[@]} )); then
  "$(dirname "$0")/just_build_macos.sh" "${args[@]}"
else
  "$(dirname "$0")/just_build_macos.sh"
fi

RUN_EXE=cataclysm-tiles
if [[ -x ./cataclysm-tlg-tiles ]]; then
  RUN_EXE=cataclysm-tlg-tiles
fi
if [[ -n "$USERDIR_REL" ]]; then
  ./$RUN_EXE --userdir "$USERDIR_REL"
else
  ./$RUN_EXE
fi
