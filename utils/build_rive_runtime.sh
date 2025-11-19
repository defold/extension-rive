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
    echo "Skipping patch for windows."

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
        echo "Patch apply failed. Attempting fallback edits..." >&2
        # Fallback transforms for build/rive_build_config.lua
        RBC="${RIVECPP}/build/rive_build_config.lua"
        # macOS min 11.0 -> 10.15
        sed -i "s/-mmacosx-version-min=11.0/-mmacosx-version-min=10.15/" "$RBC" || true
        # iOS mins/targets 13.0 -> 11.0 (device + simulator)
        sed -i "s/--target=arm64-apple-ios13.0.0/--target=arm64-apple-ios11.0.0/" "$RBC" || true
        sed -i "s/-mios-version-min=13.0.0/-mios-version-min=11.0.0/" "$RBC" || true
        sed -i "s/--target=arm64-apple-ios13.0.0-simulator/--target=arm64-apple-ios11.0.0-simulator/" "$RBC" || true
        # Android NDK string 27.2... -> 25.2...
        sed -i "s/NDK_LONG_VERSION_STRING = \"27.2.12479018\"/NDK_LONG_VERSION_STRING = \"25.2.9519653\"/" "$RBC" || true
        # Insert sysroot/with_pthread newoptions before first 'location(RIVE_BUILD_OUT)'
        awk '1; /^location\(RIVE_BUILD_OUT\)/ && !done {print "\n-- Optional sysroot for cross builds (primarily Linux)\nnewoption({\n    trigger = '\''sysroot'\'',\n    value = '\''PATH'\'',\n    description = '\''Path to a sysroot for cross-compiling (adds --sysroot=PATH to compile and link options)'\',\n})\n\n-- Optional pthread enabling (used e.g. for Emscripten wagyu builds)\nnewoption({\n    trigger = '\''with_pthread'\'',\n    description = '\''Enable pthread compile/link flags (-pthread)'\',\n})\n"; done=1}' "$RBC" > "$RBC.tmp" && mv "$RBC.tmp" "$RBC"
        # Insert Linux cross-compilation helpers before first filter('system:macosx')
        awk 'BEGIN{ins=0} {if($0 ~ /^filter\('\''system:macosx'\''\)/ && !ins){
          print "\n-- Cross-compilation helpers for Linux.\nif os.target() == '\''linux'\'' then";
          print "    local host_arch = os.outputof('\''uname -m'\'') or '\'''\'''";
          print "    if _OPTIONS['\''arch'\''] == '\''x64'\'' and host_arch ~= '\''x86_64'\'' then";
          print "        buildoptions({ '\''--target=x86_64-linux-gnu'\'' })";
          print "        linkoptions({ '\''--target=x86_64-linux-gnu'\'' })";
          print "    elseif _OPTIONS['\''arch'\''] == '\''arm64'\'' and host_arch ~= '\''aarch64'\'' then";
          print "        buildoptions({ '\''--target=aarch64-linux-gnu'\'' })";
          print "        linkoptions({ '\''--target=aarch64-linux-gnu'\'' })";
          print "    elseif _OPTIONS['\''arch'\''] == '\''arm'\'' and host_arch !~ /arm/ then";
          print "        buildoptions({ '\''--target=arm-linux-gnueabihf'\'' })";
          print "        linkoptions({ '\''--target=arm-linux-gnueabihf'\'' })";
          print "    end";
          print "    if _OPTIONS['\''sysroot'\''] ~= nil then";
          print "        buildoptions({ '\''--sysroot='\'' .. _OPTIONS['\''sysroot'\''] })";
          print "        linkoptions({ '\''--sysroot='\'' .. _OPTIONS['\''sysroot'\''] })";
          print "    end\nend\n"; ins=1}
          print }' "$RBC" > "$RBC.tmp" && mv "$RBC.tmp" "$RBC"
        # with-libs-only option and guards in renderer files
        RPL="${RIVECPP}/renderer/premake5.lua"
        RPLS="${RIVECPP}/renderer/premake5_pls_renderer.lua"
        sed -i "s/^if _OPTIONS\['with-skia'\] then/if _OPTIONS['with-skia'] and not _OPTIONS['with-libs-only'] then/" "$RPL" || true
        sed -i "s/^if not _OPTIONS\['with-webgpu'\] then/if not _OPTIONS['with-webgpu'] and not _OPTIONS['with-libs-only'] then/" "$RPL" || true
        sed -i "s/^if (_OPTIONS\['with-webgpu'\] or _OPTIONS\['with-dawn'\]) then/if (_OPTIONS['with-webgpu'] or _OPTIONS['with-dawn']) and not _OPTIONS['with-libs-only'] then/" "$RPL" || true
        if ! grep -q "with-libs-only" "$RPLS" 2>/dev/null; then
          awk '1;/^filter\(\{\}\)/ && !done {print "\nnewoption({\n    trigger = '\''with-libs-only'\'',\n    description = '\''only builds the libraries'\'',\n})\n"; done=1}' "$RPLS" > "$RPLS.tmp" && mv "$RPLS.tmp" "$RPLS"
        fi
        echo "Fallback edits applied."
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
