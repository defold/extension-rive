#!/usr/bin/env bash

# build_darwin.sh
#
# Build Rive C++ static libraries for Apple platforms (macOS and iOS) and
# install into a PREFIX directory. Mirrors the patterns used by
# build_android.sh and build_emscripten.sh.
#
# Usage:
#   ./build_darwin.sh --prefix /abs/prefix \
#       [--targets macos,ios] [--archs arm64,x64] [--config release|debug]
#
# Notes:
# - Uses the repo's build/build_rive.sh (premake+ninja) to build libraries
#   under renderer/ with --with-libs-only.
# - Installs headers via ./build_headers.sh into "$PREFIX/include".
# - Installs libraries to "$PREFIX/lib/<platform>[/<arch>]" where
#   platform in {macos, ios} and arch is optional for macOS (host default)
#   and typically 'arm64' for iOS.

set -euo pipefail
shopt -s nullglob

# Treat the current working directory as the repo root (do not depend on script location).
ROOT_DIR="$(pwd -P)"

PREFIX=""
TARGETS="macos"
ARCHS=""
CONFIG="release"

print_help() {
    cat <<EOF
Build macOS and/or iOS libraries and install to a PREFIX directory.

Options:
  -p, --prefix PATH      Install prefix directory (required)
  -t, --targets LIST     Comma/space-separated: macos, ios (default: macos)
  -a, --archs LIST       Comma/space-separated architectures (e.g. arm64,x64)
                         - macos: if omitted, uses host default
                         - ios: defaults to arm64 if omitted
  -c, --config NAME      Build config: release|debug (default: release)
  -h, --help             Show this help

Examples:
  ./build_darwin.sh --prefix /tmp/rive-darwin --targets macos,ios --archs arm64,x64 --config release
EOF
}

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--prefix)
            PREFIX="${2:-}"
            shift 2
            ;;
        -t|--targets)
            TARGETS="${2:-}"
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

# Normalize targets: split on comma and/or whitespace
IFS="," read -r -a TARGET_LIST_RAW <<< "$TARGETS"
TARGET_LIST=()
for entry in "${TARGET_LIST_RAW[@]}"; do
    for t in $entry; do
        if [[ -n "$t" ]]; then
            case "$t" in
                macos|ios) TARGET_LIST+=("$t") ;;
                *) echo "error: unsupported target '$t' (supported: macos, ios)" >&2; exit 2 ;;
            esac
        fi
    done
done

# Normalize archs if provided
ARCH_LIST=()
if [[ -n "$ARCHS" ]]; then
    IFS="," read -r -a ARCH_LIST_RAW <<< "$ARCHS"
    for entry in "${ARCH_LIST_RAW[@]}"; do
        for a in $entry; do
            if [[ -n "$a" ]]; then
                case "$a" in
                    arm64|x64|universal) ARCH_LIST+=("$a") ;;
                    *) echo "error: unsupported arch '$a' (supported: arm64, x64, universal)" >&2; exit 2 ;;
                esac
            fi
        done
    done
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

build_for_target_arch() {
    local target="$1"   # macos or ios
    local arch="$2"      # may be empty for macos (host default)

    local rive_os
    local install_dir
    local out_dir

    case "$target" in
        macos)
            rive_os="macosx"
            # If no arch provided, detect host architecture and use it.
            if [[ -z "$arch" ]]; then
                host_machine="$(uname -m)"
                case "$host_machine" in
                    arm64) arch="arm64" ;;
                    x86_64) arch="x64" ;;
                    *) echo "error: unsupported host arch '$host_machine'" >&2; exit 2 ;;
                esac
            fi
            out_dir_rel="out/${rive_os}_${arch}_${CONFIG}"
            out_dir="$BUILD_DIR/$out_dir_rel"
            install_arch="$arch"
            if [[ "$install_arch" == "x64" ]]; then install_arch="x86_64"; fi
            install_dir="$LIB_ROOT/${install_arch}-osx"
            ;;
        ios)
            rive_os="ios"
            # Default iOS arch to arm64 if not provided
            local ios_arch="$arch"
            if [[ -z "$ios_arch" ]]; then
                ios_arch="arm64"
            fi
            local rive_token="$rive_os"
            # If building for simulator (x64), use variant emulator (iossim)
            if [[ "$ios_arch" == "x64" ]]; then
                rive_token="iossim"
                out_dir_rel="out/${rive_token}_${ios_arch}_${CONFIG}"
            else
                out_dir_rel="out/${rive_os}_${ios_arch}_${CONFIG}"
            fi
            out_dir="$BUILD_DIR/$out_dir_rel"
            install_arch="$ios_arch"
            if [[ "$install_arch" == "x64" ]]; then install_arch="x86_64"; fi
            install_dir="$LIB_ROOT/${install_arch}-ios"
            arch="$ios_arch"
            ;;
    esac

    echo
    echo "==> Building: $target ${arch:+($arch) }($CONFIG)"

    # Force the out directory naming to include target and architecture consistently.
    if [[ "$target" == "macos" ]]; then
        RIVE_OUT="$out_dir_rel" "$BUILD_SCRIPT" ninja "$rive_os" "$arch" "$CONFIG" --with-libs-only
    else
        # For iOS, use iossim when arch is x64 (variant emulator), otherwise ios.
        if [[ "$arch" == "x64" ]]; then
            RIVE_OUT="$out_dir_rel" "$BUILD_SCRIPT" ninja iossim "$arch" "$CONFIG" --with-libs-only
        else
            RIVE_OUT="$out_dir_rel" "$BUILD_SCRIPT" ninja "$rive_os" "$arch" "$CONFIG" --with-libs-only
        fi
    fi

    # Collect and install libraries
    mkdir -p "$install_dir"
    libs=( "$out_dir"/lib*.a "$out_dir"/*.dylib )
    if (( ${#libs[@]} == 0 )); then
        echo "error: no libraries found in $out_dir (expected lib*.a or *.dylib)" >&2
        exit 2
    fi
    echo "Installing libraries to $install_dir/:"
    for lib in "${libs[@]}"; do
        echo "  - $(basename "$lib")"
        cp -f "$lib" "$install_dir/"
    done
}

# Drive builds per target/arch
for T in "${TARGET_LIST[@]}"; do
    if [[ ${#ARCH_LIST[@]} -gt 0 ]]; then
        for A in "${ARCH_LIST[@]}"; do
            build_for_target_arch "$T" "$A"
        done
    else
        build_for_target_arch "$T" ""
    fi
done

echo
echo "Done. Installed to: $PREFIX"
