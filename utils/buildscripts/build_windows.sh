#!/usr/bin/env bash

# build_windows.sh
#
# Build Rive C++ static libraries for Windows and install into a PREFIX dir.
# Intended for CI use on a Windows runner (Git Bash), with MSVC toolchain
# available (e.g., Visual Studio 2022 on windows-latest).
#
# Usage:
#   ./build_windows.sh --prefix /abs/path/to/prefix \
#       [--archs x64,x86] [--config release|debug]
#
# Notes:
# - Uses the repo's build/build_rive.sh (premake+ninja) to produce libraries.
# - Installs headers to "$PREFIX/include" and libs to
#   "$PREFIX/lib/<arch>-win32" where <arch> is x86 or x86_64.

set -euo pipefail
shopt -s nullglob

# Treat the current working directory as the repo root (do not depend on script location).
ROOT_DIR="$(pwd -P)"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

PREFIX=""
ARCHS="x64,x86"
CONFIG="release"

print_help() {
    cat <<EOF
Build Windows static libraries and install to a PREFIX directory.

Options:
  -p, --prefix PATH    Install prefix directory (required)
  -a, --archs LIST     Comma/space-separated: x64, x86 (default: x64,x86)
  -c, --config NAME    Build config: release|debug (default: release)
  -h, --help           Show this help

Examples:
  ./build_windows.sh --prefix /tmp/rive-win --archs x64,x86 --config release
EOF
}

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--prefix)
            PREFIX="${2:-}"
            shift 2
            ;;
        -a|--archs)
            ARCHS="${2:-}"
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

# Normalize arch list
IFS="," read -r -a ARCH_LIST_RAW <<< "$ARCHS"
ARCH_LIST=()
for entry in "${ARCH_LIST_RAW[@]}"; do
    for a in $entry; do
        if [[ -n "$a" ]]; then
            case "$a" in
                x64|x86) ARCH_LIST+=("$a") ;;
                *) echo "error: unsupported arch '$a' (supported: x64, x86)" >&2; exit 2 ;;
            esac
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

for ARCH in "${ARCH_LIST[@]}"; do
    echo
    echo "==> Building: windows $ARCH ($CONFIG)"

    out_dir_rel="out/windows_${ARCH}_${CONFIG}"
    out_dir="$BUILD_DIR/$out_dir_rel"

    # Drive build (use windows token and arch)
    RIVE_OUT="$out_dir_rel" "$BUILD_SCRIPT" ninja windows "$ARCH" "$CONFIG" --with-libs-only

    # Collect and install libraries
    install_arch="$ARCH"
    if [[ "$install_arch" == "x64" ]]; then install_arch="x86_64"; fi
    if [[ "$install_arch" == "x86" ]]; then install_arch="x86"; fi
    install_dir="$LIB_ROOT/${install_arch}-win32"
    mkdir -p "$install_dir"

    libs=( "$out_dir"/lib*.a "$out_dir"/*.lib "$out_dir"/*.dll )
    if (( ${#libs[@]} == 0 )); then
        echo "error: no libraries found in $out_dir (expected *.lib or lib*.a)" >&2
        exit 2
    fi

    echo "Installing libraries to $install_dir/:"
    for lib in "${libs[@]}"; do
        echo "  - $(basename "$lib")"
        cp -f "$lib" "$install_dir/"
    done
done

echo
echo "Done. Installed to: $PREFIX"

