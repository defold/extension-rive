#!/usr/bin/env bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

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
${SCRIPT_DIR}/build_version_header.sh ${VERSIONHEADER} ${RIVECPP}


RIVEPATCH=${SCRIPT_DIR}/rive.patch
echo "Applying patch ${RIVEPATCH}"
set +e
(cd ${RIVECPP} && git apply ${RIVEPATCH})
set -e

CONFIGURATION=release

case $PLATFORM in

    arm64-android|armv7-android)
        ARCH=arm64
        if [ "armv7-android" == "${PLATFORM}" ]; then
            ARCH=arm
        fi

        # expects ANDROID_NDK to be set
        (cd ${RIVECPP} && ${SCRIPT_DIR}/build_android.sh --prefix ${PREFIX} --abis ${ARCH} --config ${CONFIGURATION})
        ;;

    wasm-web|wasm_pthread-web|js-web)
        ARCH=wasm
        if [ "js-web" == "${PLATFORM}" ]; then
            ARCH=js
        fi

        if [ "" != "${EMSDK}" ]; then
            echo "Using EMSDK=${EMSDK}"
        else
            if [ "" == "${RIVE_EMSDK_VERSION}" ]; then
                export RIVE_EMSDK_VERSION="4.0.6"
            fi
            echo "Using RIVE_EMSDK_VERSION=${RIVE_EMSDK_VERSION}"
        fi
        (cd ${RIVECPP} && ${SCRIPT_DIR}/build_emscripten.sh --prefix ${PREFIX} --targets ${ARCH} --config ${CONFIGURATION})

        if [ "js-web" != "${PLATFORM}" ]; then
            echo "Building for Wagyu"
            (cd ${RIVECPP} && ${SCRIPT_DIR}/build_emscripten.sh --with-wagyu --prefix ${PREFIX} --targets ${ARCH} --config ${CONFIGURATION})
        fi

        ;;

    arm64-macos|x86_64-macos)
        ARCH=arm64
        if [ "x86_64-macos" == "${PLATFORM}" ]; then
            ARCH=x64
        fi

        (cd ${RIVECPP} && ${SCRIPT_DIR}/build_darwin.sh --prefix ${PREFIX} --targets macos --archs ${ARCH} --config ${CONFIGURATION})
        ;;

    arm64-ios|x86_64-ios)
        ARCH=arm64
        if [ "x86_64-ios" == "${PLATFORM}" ]; then
            ARCH=x64
        fi

        (cd ${RIVECPP} && ${SCRIPT_DIR}/build_darwin.sh --prefix ${PREFIX} --targets ios --archs ${ARCH} --config ${CONFIGURATION})
        ;;

    arm64-linux|x86_64-linux)
        ARCH=arm64
        if [ "x86_64-linux" == "${PLATFORM}" ]; then
            ARCH=x64
        fi

        (cd ${RIVECPP} && ${SCRIPT_DIR}/build_linux.sh --prefix ${PREFIX} --archs ${ARCH} --config ${CONFIGURATION})
        ;;

    *)
        echo "This script doesn't support platform ${PLATFORM} yet!"
        exit 1
        ;;

esac

echo "Done!"
