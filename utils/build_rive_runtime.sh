#!/usr/bin/env bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SCRIPT_DIR_UTILS=${SCRIPT_DIR}/buildscripts

PLATFORM=$1
shift

RIVECPP=$1
shift

function Usage {
    echo "Usage: ./utils/build_rive_runtime.sh <platform> <rive_runtime_repo>"
    echo "platforms:"
    echo "  * arm64-android"
    echo "  * armv7-android"
    echo "  * wasm-web"
    echo "  * wasm_pthread-web"
    echo "  * js-web"
    echo "  * arm64-macos"
    echo "  * x86_64-macos"
    echo "  * arm64-ios"
    echo "  * x86_64-ios"
    echo "  * arm64-linux"
    echo "  * x86_64-linux"
    echo "  * x86_64-win32"
    echo "  * x86-win32"
    exit 1
}

if [ "" == "$RIVECPP" ]; then
    echo "You must specify a runtime path"
    Usage
fi

if [ ! -d "$RIVECPP" ]; then
    echo "Rive folder does not exist: '$RIVECPP'"
    Usage
fi

# Check platforms
case $PLATFORM in
    arm64-android|armv7-android)
        ;;
    wasm-web|js-web|wasm_pthread-web)
        ;;
    arm64-macos|x86_64-macos|arm64-ios|x86_64-ios)
        ;;
    arm64-linux|x86_64-linux)
        ;;
    x86_64-win32|x86-win32)
        ;;

    *)
        echo "Platform '${PLATFORM}' is not supported!"
        Usage
        ;;
esac


PREFIX=$(realpath ${SCRIPT_DIR}/../defold-rive)
LIBDIR=${PREFIX}/lib/${PLATFORM}

echo "Using PLATFORM:${PLATFORM}"
echo "Using RIVECPP:${RIVECPP}"
echo "Using PREFIX:${PREFIX}"

echo "Removing old headers"
rm -rf ${PREFIX}/include/rive

mkdir -p ${LIBDIR}


function CleanLibraries {
    local folder=$1

    echo "Removing old libraries from ${folder}"
    set +e
    rm ${folder}/liblibjpeg.a
    rm ${folder}/liblibpng.a
    rm ${folder}/liblibwebp.a
    rm ${folder}/libminiaudio.a
    rm ${folder}/librive.a
    rm ${folder}/librive_decoders.a
    rm ${folder}/librive_harfbuzz.a
    rm ${folder}/librive_pls_renderer.a
    rm ${folder}/librive_sheenbidi.a
    rm ${folder}/librive_yoga.a
    rm ${folder}/libzlib.a
    set -e
}

CleanLibraries ${LIBDIR}

VERSIONHEADER=${PREFIX}/include/defold/rive_version.h

# temp while developing
# cp -v ${RIVECPP}/build_version_header.sh ${SCRIPT_DIR}/
# cp -v ${RIVECPP}/build_android.sh ${SCRIPT_DIR}/
# cp -v ${RIVECPP}/build_darwin.sh ${SCRIPT_DIR}/
# cp -v ${RIVECPP}/build_emscripten.sh ${SCRIPT_DIR}/
# cp -v ${RIVECPP}/build_linux.sh ${SCRIPT_DIR}/
# cp -v ${RIVECPP}/build_headers.sh ${SCRIPT_DIR}/

echo "Writing version header ${VERSIONHEADER}"
${SCRIPT_DIR_UTILS}/build_version_header.sh ${VERSIONHEADER} ${RIVECPP}


# Apply patch set. On Windows, the full patch may fail due to upstream changes; for
# Windows we apply a minimal patch enabling --with-libs-only to avoid building apps.
RIVEPATCH=${SCRIPT_DIR}/rive.patch
if [[ "$PLATFORM" == x86_64-win32 || "$PLATFORM" == x86-win32 ]]; then
    echo "Applying Windows-specific patch utils/rive-windows.patch"
    set +e
    (
        cd ${RIVECPP}
        # Dry-run with LF EOL to catch issues and produce helpful diagnostics
        git -c core.autocrlf=false -c core.eol=lf apply --check -v "${SCRIPT_DIR}/rive-windows.patch"
    )
    APPLY_RC=$?
    set -e
    if [ ${APPLY_RC} -ne 0 ]; then
        echo "Patch pre-check failed. Attempting fallback edit based on patch contents..." >&2
        patchfile="${SCRIPT_DIR}/rive-windows.patch"
        lua_file="${RIVECPP}/renderer/premake5.lua"
        pls_file="${RIVECPP}/renderer/premake5_pls_renderer.lua"

        # Extract -/+ replacement pairs for premake5.lua and apply them
        mapfile -t repls < <(awk '
            $0 ~ /^diff --git a\/renderer\/premake5\.lua b\/renderer\/premake5\.lua$/ { inblock=1; next }
            inblock && $0 ~ /^diff --git / { inblock=0 }
            inblock {
              if ($0 ~ /^---/ || $0 ~ /^\+\+\+/ || $0 ~ /^@@/) next;
              if ($0 ~ /^-/) { minus = substr($0,2); next }
              if ($0 ~ /^\+/ && length(minus)>0) { plus=substr($0,2); print minus "\t" plus; minus="" }
            }
        ' "$patchfile")

        repl_failed=0
        for pair in "${repls[@]}"; do
            orig=${pair%%$'\t'*}
            new=${pair#*$'\t'}
            tmpfile=$(mktemp)
            awk -v orig="$orig" -v new="$new" 'BEGIN{r=0} {if($0==orig){print new; r=1} else print} END{exit r?0:1}' "$lua_file" > "$tmpfile" || repl_failed=1
            mv "$tmpfile" "$lua_file"
        done

        # Extract added lines for premake5_pls_renderer.lua and insert after anchor 'filter({})'
        tmpblock=$(mktemp)
        awk '
            $0 ~ /^diff --git a\/renderer\/premake5_pls_renderer\.lua b\/renderer\/premake5_pls_renderer\.lua$/ { inblock=1; next }
            inblock && $0 ~ /^diff --git / { inblock=0 }
            inblock {
              if ($0 ~ /^\+\+\+/ || $0 ~ /^---/ || $0 ~ /^@@/) next;
              if ($0 ~ /^\+/) { print substr($0,2) }
            }
        ' "$patchfile" > "$tmpblock"

        if grep -q "with-libs-only" "$tmpblock"; then
            anchor=$(grep -n "^filter({})$" "$pls_file" | head -1 | cut -d: -f1 || true)
            if [ -n "$anchor" ]; then
                tmpout=$(mktemp)
                sed "${anchor}r $tmpblock" "$pls_file" > "$tmpout"
                mv "$tmpout" "$pls_file"
            else
                repl_failed=1
            fi
        else
            repl_failed=1
        fi
        rm -f "$tmpblock" 2>/dev/null || true

        # Validate fallback edits
        if ! grep -q "with-libs-only" "$pls_file" 2>/dev/null; then repl_failed=1; fi
        if ! grep -q "with-libs-only" "$lua_file" 2>/dev/null; then repl_failed=1; fi

        if [ $repl_failed -ne 0 ]; then
            echo "Fallback edit failed. Generating diagnostics..." >&2
            (
                cd ${RIVECPP}
                git -c core.autocrlf=false -c core.eol=lf apply --reject --whitespace=nowarn "${SCRIPT_DIR}/rive-windows.patch" || true
                echo "--- Git EOL configuration ---" >&2
                git config --get core.autocrlf || true
                git config --get core.eol || true
                echo "--- File(1) output ---" >&2
                file renderer/premake5.lua renderer/premake5_pls_renderer.lua || true
                echo "--- CRLF check (\r at EOL) ---" >&2
                grep -n $'\r$' renderer/premake5.lua || true
                grep -n $'\r$' renderer/premake5_pls_renderer.lua || true
                echo "--- Rejects (if any) ---" >&2
                for r in renderer/*.rej; do echo "# $r"; sed -n '1,120p' "$r"; done 2>/dev/null || true
                echo "--- Current context (premake5.lua top) ---" >&2
                sed -n '1,40p' renderer/premake5.lua || true
                echo "--- Current context (premake5_pls_renderer.lua around filter) ---" >&2
                awk 'NR<=120{print}' renderer/premake5_pls_renderer.lua || true
            )
            echo "Error: Failed to apply utils/rive-windows.patch (and fallback) to Rive runtime at '${RIVECPP}'." >&2
            echo "- Ensure 'rive_sha' matches the patch or update utils/rive-windows.patch." >&2
            exit 1
        else
            echo "Fallback edit succeeded based on patch contents."
        fi
    else
        # Actual apply (with LF EOL guard)
        (cd ${RIVECPP} && git -c core.autocrlf=false -c core.eol=lf apply "${SCRIPT_DIR}/rive-windows.patch")
    fi
else
    echo "Applying patch ${RIVEPATCH}"
    set +e
    (cd ${RIVECPP} && git apply ${RIVEPATCH})
    APPLY_RC=$?
    set -e
    if [ ${APPLY_RC} -ne 0 ]; then
        echo "Simple apply failed; attempting 3-way with history..."
        set +e
        (
            cd ${RIVECPP}
            # If shallow, unshallow to make 3-way possible; ignore if already full
            git rev-parse --is-shallow-repository >/dev/null 2>&1 && git fetch --unshallow || true
            git apply -3 ${RIVEPATCH}
        )
        APPLY_RC=$?
        set -e
        if [ ${APPLY_RC} -ne 0 ]; then
            echo "Patch apply failed. Generating diagnostics..." >&2
            (
                cd ${RIVECPP}
                git apply --reject --whitespace=nowarn ${RIVEPATCH} || true
                echo "--- Git EOL configuration ---" >&2
                git config --get core.autocrlf || true
                git config --get core.eol || true
                echo "--- File(1) output ---" >&2
                file build/rive_build_config.lua renderer/premake5.lua renderer/premake5_pls_renderer.lua || true
                echo "--- CRLF check (\r at EOL) ---" >&2
                grep -n $'\r$' build/rive_build_config.lua || true
                grep -n $'\r$' renderer/premake5.lua || true
                grep -n $'\r$' renderer/premake5_pls_renderer.lua || true
                echo "--- Rejects (if any) ---" >&2
                for r in build/*.rej renderer/*.rej; do echo "# $r"; sed -n '1,120p' "$r"; done 2>/dev/null || true
            )
            echo "Error: Failed to apply utils/rive.patch to Rive runtime at '${RIVECPP}'." >&2
            echo "- Ensure 'rive_sha' points to a revision compatible with this patch." >&2
            echo "- Alternatively, update utils/rive.patch for the selected rive commit." >&2
            exit 1
        fi
    fi
fi

CONFIGURATION=release

case $PLATFORM in

    arm64-android|armv7-android)
        ARCH=arm64
        if [ "armv7-android" == "${PLATFORM}" ]; then
            ARCH=arm
        fi

        # expects ANDROID_NDK to be set
        (cd ${RIVECPP} && ${SCRIPT_DIR_UTILS}/build_android.sh --prefix ${PREFIX} --abis ${ARCH} --config ${CONFIGURATION})
        ;;

    wasm-web|wasm_pthread-web|js-web)
        case $PLATFORM in
            wasm-web)
                ARCH=wasm
                ;;
            wasm_pthread-web)
                ARCH=wasm_pthread
                ;;
            js-web)
                ARCH=js
                ;;
        esac

        if [ "" != "${EMSDK}" ]; then
            echo "Using EMSDK=${EMSDK}"
        else
            if [ "" == "${RIVE_EMSDK_VERSION}" ]; then
                export RIVE_EMSDK_VERSION="4.0.6"
            fi
            echo "Using RIVE_EMSDK_VERSION=${RIVE_EMSDK_VERSION}"
        fi
        (cd ${RIVECPP} && ${SCRIPT_DIR_UTILS}/build_emscripten.sh --prefix ${PREFIX} --targets ${ARCH} --config ${CONFIGURATION})

        if [ "js-web" != "${PLATFORM}" ]; then
            echo "Building for Wagyu"
            (cd ${RIVECPP} && ${SCRIPT_DIR_UTILS}/build_emscripten.sh --with-wagyu --prefix ${PREFIX} --targets ${ARCH} --config ${CONFIGURATION})
        fi

        ;;

    arm64-macos|x86_64-macos)
        ARCH=arm64
        if [ "x86_64-macos" == "${PLATFORM}" ]; then
            ARCH=x64
        fi

        (cd ${RIVECPP} && ${SCRIPT_DIR_UTILS}/build_darwin.sh --prefix ${PREFIX} --targets macos --archs ${ARCH} --config ${CONFIGURATION})
        ;;

    arm64-ios|x86_64-ios)
        ARCH=arm64
        if [ "x86_64-ios" == "${PLATFORM}" ]; then
            ARCH=x64
        fi

        (cd ${RIVECPP} && ${SCRIPT_DIR_UTILS}/build_darwin.sh --prefix ${PREFIX} --targets ios --archs ${ARCH} --config ${CONFIGURATION})
        ;;

    arm64-linux|x86_64-linux)
        ARCH=arm64
        if [ "x86_64-linux" == "${PLATFORM}" ]; then
            ARCH=x64
        fi

        (cd ${RIVECPP} && ${SCRIPT_DIR_UTILS}/build_linux.sh --prefix ${PREFIX} --archs ${ARCH} --config ${CONFIGURATION})
        ;;

    x86_64-win32|x86-win32)
        ARCH=x64
        if [ "x86-win32" == "${PLATFORM}" ]; then
            ARCH=x86
        fi

        (cd ${RIVECPP} && ${SCRIPT_DIR_UTILS}/build_windows.sh --prefix ${PREFIX} --archs ${ARCH} --config ${CONFIGURATION})
        ;;

    *)
        echo "This script doesn't support platform ${PLATFORM} yet!"
        exit 1
        ;;

esac

echo "Removing unwanted files:"
UNWANTED=$(find ${SCRIPT_DIR}/../defold-rive/lib -iname "*glfw3.*")
if [ "" != "${UNWANTED}" ]; then
    find ${SCRIPT_DIR}/../defold-rive/lib -iname "*glfw3.*" | xargs rm -v $1
fi

echo "Done!"
