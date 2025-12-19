#!/usr/bin/env bash

# build_android.sh
#
# Build Rive C++ static libraries for Android and install into a PREFIX dir.
#
# Usage:
#   ./build_android.sh --prefix /abs/path/to/prefix [--abis arm64,arm,x64,x86] [--config release|debug]
#
# Notes:
# - Requires ANDROID_NDK (or NDK_PATH) to be set and point to NDK r27c (27.2.12479018).
# - Uses the repo's build/build_rive.sh (premake+ninja) to produce librive.a per ABI.
# - Installs headers to "$PREFIX/include" and libs to "$PREFIX/lib/<abi>-android".

set -euo pipefail
shopt -s nullglob

# Treat the current working directory as the repo root (do not depend on script location).
ROOT_DIR="$(pwd -P)"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

PREFIX=""
ABIS="arm64"
CONFIG="release"

print_help() {
    cat <<EOF
Build Android static libraries and install to a PREFIX directory.

Options:
  -p, --prefix PATH     Install prefix directory (required)
  -a, --abis LIST       Comma- or space-separated ABIs (default: arm64)
                        Supported: arm64, arm, x64, x86
  -c, --config NAME     Build config: release|debug (default: release)
  -h, --help            Show this help

Env:
  ANDROID_NDK or NDK_PATH must be set to a valid NDK (r27c).

Examples:
  ANDROID_NDK=~/Library/Android/sdk/ndk/27.2.12479018 \
    ./build_android.sh --prefix /tmp/rive-install --abis arm64,arm,x64 --config release
EOF
}

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--prefix)
            PREFIX="${2:-}"
            shift 2
            ;;
        -a|--abis)
            ABIS="${2:-}"
            shift 2
            ;;
        -c|--config)
            CONFIG="${2:-}"
            shift 2
            ;;
        -h|--help)
            print_help
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            echo >&2
            print_help >&2
            exit 2
            ;;
    esac
done

if [[ -z "$PREFIX" ]]; then
    echo "error: --prefix is required" >&2
    echo >&2
    print_help >&2
    exit 2
fi

# Ensure PREFIX is an absolute, existing path
mkdir -p "$PREFIX"
PREFIX="$(cd "$PREFIX" && pwd -P)"

if [[ "${ANDROID_NDK:-}" == "" && "${NDK_PATH:-}" == "" ]]; then
    echo "error: ANDROID_NDK (or NDK_PATH) is not set" >&2
    exit 2
fi

if [[ "$CONFIG" != "release" && "$CONFIG" != "debug" ]]; then
    echo "error: --config must be 'release' or 'debug'" >&2
    exit 2
fi

BUILD_SCRIPT="$ROOT_DIR/build/build_rive.sh"
if [[ ! -x "$BUILD_SCRIPT" ]]; then
    if [[ -f "$BUILD_SCRIPT" ]]; then
        chmod +x "$BUILD_SCRIPT"
    else
        echo "error: build script not found: $BUILD_SCRIPT" >&2
        exit 2
    fi
fi

# Normalize ABIs: split on comma and/or whitespace
IFS="," read -r -a ABI_LIST_RAW <<< "$ABIS"
ABI_LIST=()
for entry in "${ABI_LIST_RAW[@]}"; do
    # split again on spaces in case the user passed space-separated values
    for abi in $entry; do
        if [[ -n "$abi" ]]; then
            ABI_LIST+=("$abi")
        fi
    done
done

# Ensure prefix subdirs (headers handled by build_headers.sh)
INCLUDE_DST="$PREFIX/include"
LIB_ROOT="$PREFIX/lib"
mkdir -p "$INCLUDE_DST" "$LIB_ROOT"

# Install headers via shared helper
"${SCRIPT_DIR}/build_headers.sh" --prefix "$PREFIX" --root "$ROOT_DIR"

# Change into renderer directory before building so premake picks up renderer/premake5.lua
BUILD_DIR="$ROOT_DIR/renderer"
echo "Changing directory to: $BUILD_DIR"
cd "$BUILD_DIR"

# Build and install for each ABI
for ABI in "${ABI_LIST[@]}"; do
    case "$ABI" in
        arm64|arm|x64|x86) ;;
        *)
            echo "error: unsupported ABI: $ABI (supported: arm64, arm, x64, x86)" >&2
            exit 2
            ;;
    esac

    echo
    echo "==> Building: android $ABI ($CONFIG)"
    "$BUILD_SCRIPT" ninja android "$ABI" "$CONFIG" --with-libs-only

    OUT_DIR="$BUILD_DIR/out/android_${ABI}_${CONFIG}"

    # Map ABI to install dir name (special-case arm => armv7-android)
    if [[ "$ABI" == "arm" ]]; then
        ABI_DIR="armv7-android"
    else
        ABI_DIR="${ABI}-android"
    fi
    ABI_LIB_DST="$LIB_ROOT/$ABI_DIR"
    mkdir -p "$ABI_LIB_DST"

    libs=( "$OUT_DIR"/lib*.a "$OUT_DIR"/*.so )
    if (( ${#libs[@]} == 0 )); then
        echo "error: no libraries found in $OUT_DIR (expected at least lib*.a)" >&2
        exit 2
    fi

    echo "Installing libraries to $ABI_LIB_DST/:"
    for lib in "${libs[@]}"; do
        echo "  - $(basename "$lib")"
        cp -f "$lib" "$ABI_LIB_DST/"
    done
done

echo
echo "Done. Installed to: $PREFIX"
