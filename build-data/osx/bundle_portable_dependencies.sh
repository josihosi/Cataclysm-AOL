#!/usr/bin/env bash
# Bundle and verify macOS app-local dylib dependencies for release DMGs.
#
# Cataclysm.sh launches cataclysm/cataclysm-tiles from Contents/Resources, so
# dependencies rewritten by dylibbundler should resolve through @executable_path.
# Release packaging must not ship binaries that still point at local package
# manager prefixes such as /opt/local; those paths only exist on the build host.

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: build-data/osx/bundle_portable_dependencies.sh [--verify-only] APP_BUNDLE TARGET_BINARY

Bundles TARGET_BINARY dependencies into APP_BUNDLE/Contents/Resources with
install names rooted at @executable_path/, then verifies the app bundle does not
retain absolute dependencies on local package-manager prefixes.

Options:
  --verify-only   Skip dylibbundler and only run the portability preflight.
EOF
}

mode="bundle"
if [[ "${1:-}" == "--verify-only" ]]; then
    mode="verify"
    shift
fi
if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi
if [[ $# -ne 2 ]]; then
    usage >&2
    exit 2
fi

app_bundle=$1
target_binary=$2
resources_dir="$app_bundle/Contents/Resources"
macos_dir="$app_bundle/Contents/MacOS"
bundle_prefix="@executable_path/"

if [[ ! -d "$app_bundle" ]]; then
    echo "macOS app bundle not found: $app_bundle" >&2
    exit 2
fi
if [[ ! -d "$resources_dir" ]]; then
    echo "macOS app Resources directory not found: $resources_dir" >&2
    exit 2
fi
if [[ ! -f "$target_binary" ]]; then
    echo "macOS app target binary not found: $target_binary" >&2
    exit 2
fi
if ! command -v otool >/dev/null 2>&1; then
    echo "otool is required to verify macOS app bundle dependencies" >&2
    exit 2
fi

if [[ "$mode" == "bundle" ]]; then
    if ! command -v dylibbundler >/dev/null 2>&1; then
        echo "dylibbundler is required to bundle macOS app dependencies" >&2
        exit 2
    fi
    dylibbundler -of -b -x "$target_binary" -d "$resources_dir/" -p "$bundle_prefix"
fi

is_macho() {
    local path=$1
    file "$path" | grep -q 'Mach-O'
}

check_binary() {
    local binary=$1
    local bad=0
    local dep

    if ! is_macho "$binary"; then
        return 0
    fi

    while IFS= read -r dep; do
        case "$dep" in
            /opt/local/*|/opt/homebrew/*|/usr/local/*)
                printf 'Non-portable local dependency in %s: %s\n' "$binary" "$dep" >&2
                bad=1
                ;;
        esac
    done < <(otool -L "$binary" | sed '1d' | awk '{ print $1 }' | sort -u)

    return "$bad"
}

bad=0

# Check the launcher-visible binaries, bundled dylibs, and framework binaries.
# Avoid walking the full data tree: it contains many ordinary game assets and is
# not where dylibbundler places rewritten libraries.
while IFS= read -r -d '' candidate; do
    if ! check_binary "$candidate"; then
        bad=1
    fi
done < <(
    {
        find "$macos_dir" -type f -print 2>/dev/null || true
        find "$resources_dir" -maxdepth 1 -type f -print 2>/dev/null || true
        find "$resources_dir" -path '*.framework/*' -type f -print 2>/dev/null || true
    } | sort -u | tr '\n' '\0'
)

if [[ "$bad" -ne 0 ]]; then
    cat >&2 <<'EOF'
macOS app bundle is not portable: at least one Mach-O binary still links against
an absolute local package-manager path. Release packaging must bundle that dylib
and rewrite the install name before creating the DMG.
EOF
    exit 1
fi

printf 'macOS app bundle dependency preflight passed: %s\n' "$app_bundle"
