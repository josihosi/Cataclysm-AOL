#!/usr/bin/env bash
set -euo pipefail

CLEAN=1
INSTALL_DEPS=0
WITH_SUMMARY=0
ISOLATE_USERDIR=1
RESET_USERDIR=0
FAST_DEBUG=0
SUMMARY_BACKEND="${LLM_SUMMARY_BACKEND:-}"
SUMMARY_MODEL_DIR="${LLM_SUMMARY_MODEL_DIR:-}"
SUMMARY_DEVICE="${LLM_SUMMARY_DEVICE:-}"
SUMMARY_OLLAMA_URL="${LLM_SUMMARY_OLLAMA_URL:-}"
SUMMARY_OLLAMA_MODEL="${LLM_SUMMARY_OLLAMA_MODEL:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --unclean) CLEAN=0 ;;
    --install-deps) INSTALL_DEPS=1 ;;
    --with-summary) WITH_SUMMARY=1 ;;
    --summary-backend)
      shift
      SUMMARY_BACKEND="${1:-}"
      ;;
    --summary-backend=*)
      SUMMARY_BACKEND="${1#*=}"
      ;;
    --summary-model-dir)
      shift
      SUMMARY_MODEL_DIR="${1:-}"
      ;;
    --summary-model-dir=*)
      SUMMARY_MODEL_DIR="${1#*=}"
      ;;
    --summary-device)
      shift
      SUMMARY_DEVICE="${1:-}"
      ;;
    --summary-device=*)
      SUMMARY_DEVICE="${1#*=}"
      ;;
    --summary-ollama-url)
      shift
      SUMMARY_OLLAMA_URL="${1:-}"
      ;;
    --summary-ollama-url=*)
      SUMMARY_OLLAMA_URL="${1#*=}"
      ;;
    --summary-ollama-model)
      shift
      SUMMARY_OLLAMA_MODEL="${1:-}"
      ;;
    --summary-ollama-model=*)
      SUMMARY_OLLAMA_MODEL="${1#*=}"
      ;;
    --shared-userdir) ISOLATE_USERDIR=0 ;;
    --reset-userdir) RESET_USERDIR=1 ;;
    --fast|-f|--fast-debug) FAST_DEBUG=1 ;;
  esac
  shift
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

load_option_value() {
  local options_path="$1"
  local option_name="$2"
  python3 - "$options_path" "$option_name" <<'PY'
import json, sys
path, key = sys.argv[1], sys.argv[2]
try:
    with open(path, 'r', encoding='utf-8') as handle:
        data = json.load(handle)
except Exception:
    raise SystemExit(0)
if isinstance(data, list):
    for entry in data:
        if isinstance(entry, dict) and entry.get('name') == key:
            value = entry.get('value')
            if isinstance(value, str):
                print(value)
            raise SystemExit(0)
PY
}

if [[ "$WITH_SUMMARY" == "1" ]]; then
  SUMMARY_OPTIONS_PATH=""
  if [[ -n "$USERDIR_REL" && -f "${USERDIR_REL}config/options.json" ]]; then
    SUMMARY_OPTIONS_PATH="${USERDIR_REL}config/options.json"
  elif [[ -f "config/options.json" ]]; then
    SUMMARY_OPTIONS_PATH="config/options.json"
  fi

  if [[ -n "$SUMMARY_OPTIONS_PATH" ]]; then
    [[ -z "$SUMMARY_BACKEND" ]] && SUMMARY_BACKEND="$(load_option_value "$SUMMARY_OPTIONS_PATH" "LLM_SUMMARY_BACKEND")"
    [[ -z "$SUMMARY_MODEL_DIR" ]] && SUMMARY_MODEL_DIR="$(load_option_value "$SUMMARY_OPTIONS_PATH" "LLM_SUMMARY_MODEL_DIR")"
    [[ -z "$SUMMARY_OLLAMA_URL" ]] && SUMMARY_OLLAMA_URL="$(load_option_value "$SUMMARY_OPTIONS_PATH" "LLM_SUMMARY_OLLAMA_URL")"
    [[ -z "$SUMMARY_OLLAMA_MODEL" ]] && SUMMARY_OLLAMA_MODEL="$(load_option_value "$SUMMARY_OPTIONS_PATH" "LLM_SUMMARY_OLLAMA_MODEL")"
    [[ -z "$SUMMARY_OLLAMA_URL" ]] && SUMMARY_OLLAMA_URL="$(load_option_value "$SUMMARY_OPTIONS_PATH" "LLM_INTENT_OLLAMA_URL")"
    [[ -z "$SUMMARY_OLLAMA_MODEL" ]] && SUMMARY_OLLAMA_MODEL="$(load_option_value "$SUMMARY_OPTIONS_PATH" "LLM_INTENT_OLLAMA_MODEL")"
  fi

  if [[ -z "$SUMMARY_BACKEND" ]]; then
    SUMMARY_BACKEND="ollama"
  fi

  case "$SUMMARY_BACKEND" in
    ollama)
      [[ -z "$SUMMARY_OLLAMA_URL" ]] && SUMMARY_OLLAMA_URL="http://127.0.0.1:11434"
      if [[ -z "$SUMMARY_OLLAMA_MODEL" ]]; then
        echo "--with-summary with backend=ollama requires a local Ollama model name."
        echo "Set LLM_SUMMARY_OLLAMA_MODEL, pass --summary-ollama-model, or configure it in the LLM options menu."
        exit 2
      fi
      export LLM_SUMMARY_BACKEND="ollama"
      export LLM_SUMMARY_OLLAMA_URL="$SUMMARY_OLLAMA_URL"
      export LLM_SUMMARY_OLLAMA_MODEL="$SUMMARY_OLLAMA_MODEL"
      echo "Background summaries enabled with local Ollama model: $SUMMARY_OLLAMA_MODEL @ $SUMMARY_OLLAMA_URL"
      ;;
    openvino)
      if [[ -z "$SUMMARY_MODEL_DIR" ]]; then
        echo "--with-summary with backend=openvino requires a local summary model dir."
        echo "Set LLM_SUMMARY_MODEL_DIR, pass --summary-model-dir /path/to/local/model, or configure it in the LLM options menu."
        exit 2
      fi
      if [[ ! -d "$SUMMARY_MODEL_DIR" ]]; then
        echo "Local summary model dir not found: $SUMMARY_MODEL_DIR"
        exit 2
      fi
      export LLM_SUMMARY_BACKEND="openvino"
      export LLM_SUMMARY_MODEL_DIR="$SUMMARY_MODEL_DIR"
      if [[ -n "$SUMMARY_DEVICE" ]]; then
        export LLM_SUMMARY_DEVICE="$SUMMARY_DEVICE"
      fi
      echo "Background summaries enabled with local OpenVINO model: $SUMMARY_MODEL_DIR"
      ;;
    *)
      echo "Unsupported summary backend: $SUMMARY_BACKEND"
      echo "Use 'ollama' or 'openvino'."
      exit 2
      ;;
  esac
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

rm -f pch/main-pch.hpp.gch pch/main-pch.hpp.pch pch/main-pch.hpp.d \
      pch/tests-pch.hpp.gch pch/tests-pch.hpp.pch pch/tests-pch.hpp.d \
      obj/*/pch/main-pch.hpp.gch obj/*/pch/main-pch.hpp.pch \
      zstd.a zstd_test.a cataclysm-tiles cataclysm-tlg-tiles || true
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
