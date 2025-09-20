#!/usr/bin/env bash

# build_emscripten.sh
#
# Build Rive C++ static libraries for Emscripten (wasm/js) and install into a
# PREFIX directory. Mirrors build_android.sh behavior and header layout.
#
# Usage:
#   ./build_emscripten.sh --prefix /abs/prefix [--targets wasm,js] [--config release|debug]
#
# Notes:
# - If EMSDK is set, this script sources "$EMSDK/emsdk_env.sh" and forces
#   RIVE_EMSDK_VERSION=none so the repo does not auto-install a different SDK.
# - Otherwise, the repo build scripts may auto-install a pinned emsdk version.
# - Installs headers to "$PREFIX/include" (selected subtrees) and libs to
#   "$PREFIX/lib/<target>-web" (e.g. wasm-web, js-web).

set -euo pipefail
shopt -s nullglob

# Treat the current working directory as the repo root (do not depend on script location).
ROOT_DIR="$(pwd -P)"

PREFIX=""
TARGETS="wasm"
CONFIG="release"
WITH_WAGYU=false

print_help() {
    cat <<EOF
Build Emscripten libraries and install to a PREFIX directory.

Options:
  -p, --prefix PATH        Install prefix directory (required)
  -t, --targets LIST       Comma/space-separated: wasm, js (default: wasm)
  -c, --config NAME        Build config: release|debug (default: release)
  --with-wagyu             Enable wagyu option when building wasm target
  -h, --help               Show this help

Env:
  EMSDK                    If set, sources \$EMSDK/emsdk_env.sh and uses that SDK.

Examples:
  EMSDK=~/dev/emsdk ./build_emscripten.sh --prefix /tmp/rive-emsdk --targets wasm,js --config release
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
        -c|--config)
            CONFIG="${2:-}"
            shift 2
            ;;
        --with-wagyu)
            WITH_WAGYU=true
            shift 1
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

# If EMSDK is provided, source it and instruct the rive build not to manage emsdk.
if [[ -n "${EMSDK:-}" ]]; then
    if [[ -f "$EMSDK/emsdk_env.sh" ]]; then
        # shellcheck disable=SC1090
        source "$EMSDK/emsdk_env.sh"
        export RIVE_EMSDK_VERSION=none
    else
        echo "warning: EMSDK is set but \"$EMSDK/emsdk_env.sh\" not found; proceeding without sourcing" >&2
    fi
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
            TARGET_LIST+=("$t")
        fi
    done
done

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

# Build and install for each target
for TARGET in "${TARGET_LIST[@]}"; do
    case "$TARGET" in
        wasm|js) ;;
        *)
            echo "error: unsupported target: $TARGET (supported: wasm, js)" >&2
            exit 2
            ;;
    esac

    echo
    echo "==> Building: $TARGET ($CONFIG)"
    EXTRA_PREMAKE=""
    OUT_DIR_REL="out/${TARGET}_${CONFIG}"
    OUT_DIR="$BUILD_DIR/$OUT_DIR_REL"
    if [[ "$TARGET" == "wasm" && "$WITH_WAGYU" == true ]]; then
        EXTRA_PREMAKE="--with_wagyu --with-webgpu --webgpu-version=2"
        OUT_DIR_REL="out/wasm_wagyu_${CONFIG}"
        OUT_DIR="$BUILD_DIR/$OUT_DIR_REL"
    fi
    # Expand EXTRA_PREMAKE only if non-empty (safe with set -u). When wagyu is used,
    # direct the build into our custom OUT directory by setting RIVE_OUT.
    if [[ -n "$EXTRA_PREMAKE" ]]; then
        # Clean and rebuild into custom OUT directory when using wagyu.
        # Pass RIVE_OUT as a relative path so premake doesn't prepend _WORKING_DIR twice.
        RIVE_OUT="$OUT_DIR_REL" "$BUILD_SCRIPT" clean "$TARGET" "$CONFIG" "$EXTRA_PREMAKE" --with-libs-only || true
        RIVE_OUT="$OUT_DIR_REL" "$BUILD_SCRIPT" ninja "$TARGET" "$CONFIG" "$EXTRA_PREMAKE" --with-libs-only
    else
        "$BUILD_SCRIPT" ninja "$TARGET" "$CONFIG" --with-libs-only
    fi

    # Sanity check: ensure build.ninja exists where we expect
    if [[ ! -f "$OUT_DIR/build.ninja" ]]; then
        echo "error: expected $OUT_DIR/build.ninja was not generated" >&2
        echo "       Check premake output and ensure premake-ninja is available." >&2
        echo "       OUT_DIR contents:" >&2
        ls -la "$OUT_DIR" >&2 || true
        exit 2
    fi

    # Install directory per target (e.g. wasm-web or js-web)
    TGT_LIB_DST="$LIB_ROOT/${TARGET}-web"
    mkdir -p "$TGT_LIB_DST"

    if [[ "$TARGET" == "wasm" && "$WITH_WAGYU" == true ]]; then
        # Special case: only install the PLS renderer lib, and rename it
        # to librive_renderer_wagyu.a
        SRC_WAGYU_LIB="$OUT_DIR/librive_pls_renderer.a"
        DST_WAGYU_LIB="$TGT_LIB_DST/librive_renderer_wagyu.a"
        if [[ ! -f "$SRC_WAGYU_LIB" ]]; then
            echo "error: expected $SRC_WAGYU_LIB for wagyu build not found" >&2
            exit 2
        fi
        echo "Installing wagyu library to $DST_WAGYU_LIB"
        cp -f "$SRC_WAGYU_LIB" "$DST_WAGYU_LIB"
    else
        libs=( "$OUT_DIR"/lib*.a "$OUT_DIR"/lib*.bc )
        if (( ${#libs[@]} == 0 )); then
            echo "error: no libraries found in $OUT_DIR (expected lib*.a or lib*.bc)" >&2
            exit 2
        fi

        echo "Installing libraries to $TGT_LIB_DST/:"
        for lib in "${libs[@]}"; do
            echo "  - $(basename "$lib")"
            cp -f "$lib" "$TGT_LIB_DST/"
        done
    fi
done

echo
echo "Done. Installed to: $PREFIX"
