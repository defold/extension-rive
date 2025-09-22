#!/usr/bin/env bash

# build_linux.sh
#
# Build Rive C++ static libraries for Linux and install into a PREFIX directory.
# Mirrors the patterns used by build_android.sh/build_emscripten.sh/build_darwin.sh.
#
# Usage:
#   ./build_linux.sh --prefix /abs/prefix [--archs x64,arm64,arm] [--config release|debug]
#
# Notes:
# - Uses the repo's build/build_rive.sh (premake+ninja) with --with-libs-only.
# - Installs headers via ./build_headers.sh into "$PREFIX/include".
# - Installs libraries to "$PREFIX/lib/<arch>-linux" (with x64 mapped to x86_64).

set -euo pipefail
shopt -s nullglob

# Treat the current working directory as the repo root (do not depend on script location).
ROOT_DIR="$(pwd -P)"

PREFIX=""
ARCHS=""
CONFIG="release"
SYSROOT=""

print_help() {
    cat <<EOF
Build Linux libraries and install to a PREFIX directory.

Options:
  -p, --prefix PATH    Install prefix directory (required)
  -a, --archs LIST     Comma/space-separated: x64, arm64, arm (default: host arch)
  -c, --config NAME    Build config: release|debug (default: release)
  -s, --sysroot PATH   Optional sysroot for cross builds (passed to premake: --sysroot=PATH)
  -h, --help           Show this help

Examples:
  ./build_linux.sh --prefix /tmp/rive-linux --archs x64,arm64 --config release
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
        -s|--sysroot)
            SYSROOT="${2:-}"
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

# Normalize archs; if none provided, detect host arch and use it.
ARCH_LIST=()
if [[ -n "$ARCHS" ]]; then
    IFS="," read -r -a ARCH_LIST_RAW <<< "$ARCHS"
    for entry in "${ARCH_LIST_RAW[@]}"; do
        for a in $entry; do
            if [[ -n "$a" ]]; then
                case "$a" in
                    x64|arm64|arm) ARCH_LIST+=("$a") ;;
                    *) echo "error: unsupported arch '$a' (supported: x64, arm64, arm)" >&2; exit 2 ;;
                esac
            fi
        done
    done
else
    case "$(uname -m)" in
        x86_64) ARCH_LIST+=("x64") ;;
        aarch64) ARCH_LIST+=("arm64") ;;
        armv7l|armv6l) ARCH_LIST+=("arm") ;;
        *) echo "error: unsupported host arch '$(uname -m)'; specify --archs" >&2; exit 2 ;;
    esac
fi

# Ensure prefix subdirs (headers handled by build_headers.sh)
INCLUDE_DST="$PREFIX/include"
LIB_ROOT="$PREFIX/lib"
mkdir -p "$INCLUDE_DST" "$LIB_ROOT"

# Install headers via shared helper
"$ROOT_DIR/build_headers.sh" --prefix "$PREFIX" --root "$ROOT_DIR"

# Change into renderer directory before building so premake picks up renderer/premake5.lua
BUILD_DIR="$ROOT_DIR/renderer"
echo "Changing directory to: $BUILD_DIR"
cd "$BUILD_DIR"

for ARCH in "${ARCH_LIST[@]}"; do
    echo
    echo "==> Building: linux $ARCH ($CONFIG)"

    out_dir_rel="out/linux_${ARCH}_${CONFIG}"
    out_dir="$BUILD_DIR/$out_dir_rel"

    # Force out dir naming and build only the libraries
    # Build args
    EXTRA_PREMAKE=()
    if [[ -n "$SYSROOT" ]]; then
        EXTRA_PREMAKE+=("--sysroot=$SYSROOT")
    fi
    RIVE_OUT="$out_dir_rel" "$BUILD_SCRIPT" ninja "$ARCH" "$CONFIG" --no-lto "${EXTRA_PREMAKE[@]}" --with-libs-only

    # Collect and install libraries
    install_arch="$ARCH"
    if [[ "$install_arch" == "x64" ]]; then install_arch="x86_64"; fi
    install_dir="$LIB_ROOT/${install_arch}-linux"
    mkdir -p "$install_dir"

    libs=( "$out_dir"/lib*.a "$out_dir"/*.so )
    if (( ${#libs[@]} == 0 )); then
        echo "error: no libraries found in $out_dir (expected lib*.a or *.so)" >&2
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
