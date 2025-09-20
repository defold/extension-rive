#!/usr/bin/env bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

PLATFORM=$1
shift

RIVECPP=$1
shift

function Usage {
    echo "Usage: ./utils/build_rive_runtime.sh <platform> <rive_runtime_repo>"
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

    *)
        echo "This script doesn't support platform ${PLATFORM}"
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
#cp -v ${RIVECPP}/build_version_header.sh ${SCRIPT_DIR}/
#cp -v ${RIVECPP}/build_android.sh ${SCRIPT_DIR}/

echo "Writing version header ${VERSIONHEADER}"
${SCRIPT_DIR}/build_version_header.sh ${VERSIONHEADER} ${RIVECPP}


RIVEPATCH=${SCRIPT_DIR}/rive.patch
echo "Applying patch ${RIVEPATCH}"
set +e
(cd ${RIVECPP} && git apply ${RIVEPATCH})
set -e


case $PLATFORM in

    arm64-android)
        # expects ANDROID_NDK to be set
        (cd ${RIVECPP} && ${SCRIPT_DIR}/build_android.sh --prefix ${PREFIX} --abis arm64 --config release)
        ;;

    armv7-android)
        # expects ANDROID_NDK to be set
        (cd ${RIVECPP} && ${SCRIPT_DIR}/build_android.sh --prefix ${PREFIX} --abis arm --config release)
        ;;

    *)
        echo "This script doesn't support platform ${PLATFORM} yet!"
        exit 1
        ;;

esac

echo "Done!"
