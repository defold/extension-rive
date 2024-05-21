#! /usr/bin/env bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

OUTPUT_LIB_DIR=${SCRIPT_DIR}/../defold-rive/lib

PLATFORMS=$1
# web isn't yet supported: js-web wasm-web
if [ "" == "${PLATFORMS}" ]; then
    PLATFORMS="x86_64-macos arm64-macos x86_64-linux x86_64-win32 x86-win32 arm64-ios x86_64-ios arm64-android armv7-android"
fi

DEFAULT_SERVER_NAME=build-stage.defold.com
if [ "" == "${DM_EXTENDER_USERNAME}" ] && [ "" == "${DM_EXTENDER_PASSWORD}" ]; then
    DEFAULT_SERVER=https://${DEFAULT_SERVER_NAME}
else
    DEFAULT_SERVER=https://${DM_EXTENDER_USERNAME}:${DM_EXTENDER_PASSWORD}@${DEFAULT_SERVER_NAME}
fi

if [ "" == "${BOB}" ]; then
    BOB=${DYNAMO_HOME}/share/java/bob.jar
fi

echo "Using BOB=${BOB}"

if [ "" == "${DEFOLDSDK}" ]; then
    DEFOLDSDK=$(java -jar $BOB --version | awk '{print $5}')
fi

echo "Using DEFOLDSDK=${DEFOLDSDK}"

if [ "" == "${SERVER}" ]; then
    SERVER=${DEFAULT_SERVER}
fi

echo "Using SERVER=${SERVER}"

if [ "" == "${VARIANT}" ]; then
    VARIANT=headless
fi

echo "Using VARIANT=${VARIANT}"

function copyfile() {
    local path=$1
    local folder=$2
    if [ -f "$path" ]; then
        if [ ! -d "$folder" ]; then
            mkdir -v -p $folder
        fi
        cp -v $path $folder
    fi
}

function copy_results() {
    local platform=$1
    local platform_ne=$2
    local target_dir=$3

    for path in ./build/$platform_ne/*.a; do
        copyfile $path $target_dir
    done
    for path in ./build/$platform_ne/*.lib; do
        copyfile $path $target_dir
    done
}

function clear_build_options() {
    unset CXXFLAGS
    unset DEFINES
    unset INCLUDES
    echo "Cleared build options"
}

function build_library() {
    local name=$1
    local platform=$2
    local platform_ne=$3
    local source_dir=$4
    local target_dir=$5

    echo "Using CXXFLAGS=${CXXFLAGS}"
    echo "Using DEFINES=${DEFINES}"
    echo "Using INCLUDES=${INCLUDES}"

    # make sure it doesn't pick up the project's app manifest
    echo "[native_extension]" > ${target_dir}/ext.settings
    echo "app_manifest =" >> ${target_dir}/ext.settings
    java -jar $BOB --platform=$platform --architectures=$platform --settings=${target_dir}/ext.settings build --build-artifacts=library --variant $VARIANT --build-server=$SERVER --defoldsdk=$DEFOLDSDK --debug-ne-upload true --ne-output-name=${name} --ne-build-dir ${source_dir}

    copy_results $platform $platform_ne $target_dir

    clear_build_options
}

function download_zip() {
    local zip=$1
    local dir=$2
    local url=$3

    if [ ! -e "${zip}" ]; then
        echo "Downloading ${url} to ${zip}"
        mkdir -p $(dirname ${zip})
        wget -O ${zip} ${url}
    else
        echo "${zip} already downloaded!"
    fi

    if [ ! -d "${dir}" ]; then
        echo "Unpacking ${zip} to ${dir}"
        mkdir -p $(dirname ${dir})
        unzip ${zip} -d ${dir}
    else
        echo "Skipping unpack of ${zip} as ${dir} already exists."
    fi
}

set -e

DOWNLOAD_DIR=./build/pls/deps

# setup source

SOURCE_DIR=./build/pls

echo "*************************************************"
echo "Downloading harfbuzz files"

HARFBUZZ_VERSION=8.3.0
HARFBUZZ_ZIP=${DOWNLOAD_DIR}/harfbuzz-${HARFBUZZ_VERSION}.zip
HARFBUZZ_URL=https://github.com/harfbuzz/harfbuzz/archive/refs/tags/${HARFBUZZ_VERSION}.zip
HARFBUZZ_ORIGINAL_DIR=${DOWNLOAD_DIR}/harfbuzz/harfbuzz-${HARFBUZZ_VERSION}/src
HARFBUZZ_SOURCE_DIR=${SOURCE_DIR}/harfbuzz/src

download_zip ${HARFBUZZ_ZIP} ${DOWNLOAD_DIR}/harfbuzz ${HARFBUZZ_URL}

echo "*************************************************"
echo "Copying harfbuzz files"
${SCRIPT_DIR}/copy_harfbuzz.sh ${HARFBUZZ_ORIGINAL_DIR} ${HARFBUZZ_SOURCE_DIR}

echo "*************************************************"

echo "*************************************************"
echo "Downloading sheenbidi files"


SHEENBIDI_VERSION=2.6
SHEENBIDI_VERSION=2.6
SHEENBIDI_ZIP=${DOWNLOAD_DIR}/sheenbidi-${SHEENBIDI_VERSION}.zip
SHEENBIDI_ZIP=${DOWNLOAD_DIR}/sheenbidi-${SHEENBIDI_VERSION}.zip
SHEENBIDI_URL=https://github.com/Tehreer/SheenBidi/archive/refs/tags/v${SHEENBIDI_VERSION}.zip
SHEENBIDI_URL=https://github.com/Tehreer/SheenBidi/archive/refs/tags/v${SHEENBIDI_VERSION}.zip
SHEENBIDI_ORIGINAL_DIR=${DOWNLOAD_DIR}/sheenbidi/SheenBidi-${SHEENBIDI_VERSION}
SHEENBIDI_ORIGINAL_DIR=${DOWNLOAD_DIR}/sheenbidi/SheenBidi-${SHEENBIDI_VERSION}
SHEENBIDI_SOURCE_DIR=${SOURCE_DIR}/sheenbidi/src


download_zip ${SHEENBIDI_ZIP} ${DOWNLOAD_DIR}/sheenbidi ${SHEENBIDI_URL}


mkdir -p ${SHEENBIDI_SOURCE_DIR}
mkdir -p ${SHEENBIDI_SOURCE_DIR}
cp -r -v ${SHEENBIDI_ORIGINAL_DIR}/Headers ${SHEENBIDI_SOURCE_DIR}
cp -r -v ${SHEENBIDI_ORIGINAL_DIR}/Headers ${SHEENBIDI_SOURCE_DIR}
cp -r -v ${SHEENBIDI_ORIGINAL_DIR}/Source ${SHEENBIDI_SOURCE_DIR}

echo "*************************************************"
echo "Downloading libtess2 files"

LIBTESS2_VERSION=1.0.2
LIBTESS2_ZIP=${DOWNLOAD_DIR}/libtess2-${LIBTESS2_VERSION}.zip
LIBTESS2_URL=https://github.com/memononen/libtess2/archive/refs/tags/v${LIBTESS2_VERSION}.zip
LIBTESS2_ORIGINAL_DIR=${DOWNLOAD_DIR}/libtess2/libtess2-${LIBTESS2_VERSION}
LIBTESS2_SOURCE_DIR=${SOURCE_DIR}/libtess2/src

download_zip ${LIBTESS2_ZIP} ${DOWNLOAD_DIR}/libtess2 ${LIBTESS2_URL}

mkdir -p ${LIBTESS2_SOURCE_DIR}
cp -r -v ${LIBTESS2_ORIGINAL_DIR}/Include ${LIBTESS2_SOURCE_DIR}
cp -r -v ${LIBTESS2_ORIGINAL_DIR}/Source ${LIBTESS2_SOURCE_DIR}

echo "*************************************************"
echo "Downloading earcut files"

EARCUT_VERSION=master
EARCUT_ZIP=${DOWNLOAD_DIR}/earcut-${EARCUT_VERSION}.zip
EARCUT_URL="https://github.com/mapbox/earcut.hpp/archive/refs/heads/${EARCUT_VERSION}.zip"
EARCUT_ORIGINAL_DIR=${DOWNLOAD_DIR}/earcut/earcut.hpp-${EARCUT_VERSION}
EARCUT_SOURCE_DIR=${SOURCE_DIR}/earcut/src

download_zip ${EARCUT_ZIP} ${DOWNLOAD_DIR}/earcut ${EARCUT_URL}

echo "*************************************************"
echo "Downloading rive-cpp files"

RIVECPP_VERSION=master
RIVECPP_ZIP=${DOWNLOAD_DIR}/rivecpp-${RIVECPP_VERSION}.zip
RIVECPP_URL="https://github.com/rive-app/rive-cpp/archive/refs/heads/${RIVECPP_VERSION}.zip"

RIVECPP_ORIGINAL_DIR=${DOWNLOAD_DIR}/rivecpp/rive-cpp-${RIVECPP_VERSION}
RIVECPP_SOURCE_DIR=${SOURCE_DIR}/rivecpp/src
RIVECPP_TESS_SOURCE_DIR=${SOURCE_DIR}/rivecpp-tess/src
RIVECPP_HARFBUZZ_INCLUDE_DIR=${RIVECPP_SOURCE_DIR}/src/text

download_zip ${RIVECPP_ZIP} ${DOWNLOAD_DIR}/rivecpp ${RIVECPP_URL}

mkdir -p ${RIVECPP_SOURCE_DIR}
mkdir -p ${RIVECPP_TESS_SOURCE_DIR}
mkdir -p ${RIVECPP_TESS_SOURCE_DIR}/math
mkdir -p ${RIVECPP_HARFBUZZ_INCLUDE_DIR}
RIVECPP_HARFBUZZ_INCLUDE_DIR=$(realpath $RIVECPP_HARFBUZZ_INCLUDE_DIR)

echo "COPY TESS"
# tess tenderer
cp -v ${RIVECPP_ORIGINAL_DIR}/tess/src/*.cpp ${RIVECPP_TESS_SOURCE_DIR}
cp -v ${RIVECPP_ORIGINAL_DIR}/tess/src/math/*.cpp ${RIVECPP_TESS_SOURCE_DIR}/math
cp -r -v ${RIVECPP_ORIGINAL_DIR}/tess/include/rive ${RIVECPP_TESS_SOURCE_DIR}
# copy some extra includes
cp -r -v ${RIVECPP_ORIGINAL_DIR}/include/rive ${RIVECPP_TESS_SOURCE_DIR}
cp -r -v ${RIVECPP_ORIGINAL_DIR}/include/utils ${RIVECPP_TESS_SOURCE_DIR}
# copy earcut.hpp
cp -v ${EARCUT_ORIGINAL_DIR}/include/mapbox/earcut.hpp ${RIVECPP_TESS_SOURCE_DIR}
# copy tesselator.h
cp -v ${LIBTESS2_ORIGINAL_DIR}/Include/tesselator.h ${RIVECPP_TESS_SOURCE_DIR}

echo "COPY RIVECPP"

cp -r -v ${RIVECPP_ORIGINAL_DIR}/src ${RIVECPP_SOURCE_DIR}
cp -r -v ${RIVECPP_ORIGINAL_DIR}/include/rive ${RIVECPP_SOURCE_DIR}
cp -r -v ${RIVECPP_ORIGINAL_DIR}/include/utils ${RIVECPP_SOURCE_DIR}

# HACK for C++ vs Objective-C
mv -v ${RIVECPP_SOURCE_DIR}/src/audio/audio_engine.m ${RIVECPP_SOURCE_DIR}/src/audio/audio_engine.mm

# Copy text related headers
(cd ${HARFBUZZ_ORIGINAL_DIR} && cp -v *.h ${RIVECPP_HARFBUZZ_INCLUDE_DIR})
(cd ${SHEENBIDI_ORIGINAL_DIR}/Headers && cp -v *.h ${RIVECPP_HARFBUZZ_INCLUDE_DIR})

echo "*************************************************"
echo "Downloading rive-pls files"

RIVEPLS_VERSION=main
RIVEPLS_ZIP=${DOWNLOAD_DIR}/rivepls-${RIVEPLS_VERSION}.zip
RIVEPLS_URL="https://github.com/rive-app/rive-pls/archive/refs/heads/${RIVEPLS_VERSION}.zip"

RIVEPLS_ORIGINAL_DIR=${DOWNLOAD_DIR}/rivepls/rive-renderer-${RIVEPLS_VERSION}
RIVEPLS_TOP_DIR=${SOURCE_DIR}/rivepls/
RIVEPLS_SOURCE_DIR=${RIVEPLS_TOP_DIR}/src
RIVEPLS_GENERATED_DIR=${RIVEPLS_TOP_DIR}/shaders/out/generated

download_zip ${RIVEPLS_ZIP} ${DOWNLOAD_DIR}/rivepls ${RIVEPLS_URL}

RIVEPLS_MINIFY=${RIVEPLS_ORIGINAL_DIR}/renderer/shaders/minify.py

echo "COPY RIVE PLS"

mkdir -p ${RIVEPLS_GENERATED_DIR}
mkdir -p ${RIVEPLS_SOURCE_DIR}

cp -v ${RIVEPLS_ORIGINAL_DIR}/renderer/*.{cpp,hpp} ${RIVEPLS_SOURCE_DIR}
cp -r -v ${RIVEPLS_ORIGINAL_DIR}/include/rive ${RIVEPLS_SOURCE_DIR}
mkdir -p ${RIVEPLS_SOURCE_DIR}/shaders
cp -v ${RIVEPLS_ORIGINAL_DIR}/renderer/shaders/constants.glsl ${RIVEPLS_SOURCE_DIR}/shaders
# copy some extra includes from rive-cpp
cp -r -v ${RIVECPP_ORIGINAL_DIR}/include/rive ${RIVEPLS_SOURCE_DIR}
cp -r -v ${RIVECPP_ORIGINAL_DIR}/include/utils ${RIVEPLS_SOURCE_DIR}

python ${RIVEPLS_MINIFY} -o ${RIVEPLS_GENERATED_DIR} ${RIVEPLS_ORIGINAL_DIR}/renderer/shaders/draw_path.glsl

echo "*************************************************"
echo "Setup shader source variables"

source ${SCRIPT_DIR}/gen_embedded_shaders.sh

DEFOLDSHADERS_INPUT_DIR=${SCRIPT_DIR}/../defold-rive/assets/pls-shaders
DEFOLDSHADERS_INCLUDE_DIR=${SCRIPT_DIR}/../defold-rive/include/private/shaders
DEFOLDSHADERS_SOURCE_DIR=${SOURCE_DIR}/defoldshaders/src

mkdir -p ${DEFOLDSHADERS_INCLUDE_DIR}
mkdir -p ${DEFOLDSHADERS_SOURCE_DIR}

echo "*************************************************"
echo "Setup private renderer source variables"

DEFOLDRENDERER_ORIGINAL_DIR=${SCRIPT_DIR}/../defold-rive/src/private
DEFOLDRENDERER_INCLUDE_DIR=${SCRIPT_DIR}/../defold-rive/include
DEFOLDRENDERER_SOURCE_DIR=${SOURCE_DIR}/rivedefold/src

mkdir -p ${DEFOLDRENDERER_SOURCE_DIR}

cp -v -r ${DEFOLDRENDERER_ORIGINAL_DIR} ${DEFOLDRENDERER_SOURCE_DIR}
cp -v -r ${DEFOLDRENDERER_INCLUDE_DIR} ${DEFOLDRENDERER_SOURCE_DIR}

echo "*************************************************"


for platform in $PLATFORMS; do

    echo "Building platform ${platform}"

    platform_ne=$platform
    build_dir=./build/pls/libs/${PLATFORM}


    case ${platform} in
        x86_64-macos)
            platform_ne="x86_64-osx";;
        arm64-macos)
            platform_ne="arm64-osx";;
    esac

    BUILD=${OUTPUT_LIB_DIR}/${platform_ne}
    mkdir -p ${BUILD}
    BUILD=$(realpath ${BUILD})

    echo "************************************************************"
    echo "HARFBUZZ ${platform}"
    echo "************************************************************"
    # This is copied from the rive-cpp/dependencies/premake5_harfbuzz.lua
    export CXXFLAGS="-std=c++17 -fno-rtti -fno-exceptions"
    export DEFINES="HAVE_CONFIG_H"
    unset INCLUDES
    build_library harfbuzz $platform $platform_ne ${HARFBUZZ_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "SHEENBIDI ${platform}"
    echo "************************************************************"
    export INCLUDES="upload/Headers" # TODO: Make includes work with relative paths
    export CXXFLAGS="-x c"
    unset DEFINES
    build_library sheenbidi $platform $platform_ne ${SHEENBIDI_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "TESS2 ${platform}"
    echo "************************************************************"
    export INCLUDES="upload/Include" # TODO: Make includes work with relative paths
    export CXXFLAGS="-x c"
    unset DEFINES
    build_library tess2 $platform $platform_ne ${LIBTESS2_SOURCE_DIR} ${BUILD}

    # Rive CPP
    echo "************************************************************"
    echo "RIVETESS ${platform}"
    echo "************************************************************"
    unset DEFINES
    unset INCLUDES
    export CXXFLAGS="-std=c++17 -fno-rtti -fno-exceptions"
    build_library rivetess $platform $platform_ne ${RIVECPP_TESS_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "RIVE CPP ${platform}"
    echo "************************************************************"
    export CXXFLAGS="-std=c++17 -fno-rtti -fno-exceptions"
    export DEFINES="WITH_RIVE_TEXT"
    unset INCLUDES
    build_library rive $platform $platform_ne ${RIVECPP_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "RIVE PLS ${platform}"
    echo "************************************************************"
    # Rive PLS
    export CXXFLAGS="-std=c++17 -fno-rtti -fno-exceptions"
    export DEFINES="_CRT_USE_BUILTIN_OFFSETOF"
    export INCLUDES="upload/src"
    build_library rive_pls_renderer $platform $platform_ne ${RIVEPLS_TOP_DIR} ${BUILD}

    echo "************************************************************"
    echo "RIVE SHADERS ${platform}"
    echo "************************************************************"
    # shaders
    unset DEFINES
    unset INCLUDES
    export CXXFLAGS="-x c"
    generate_cpp_sources ${platform} ${DEFOLDSHADERS_INPUT_DIR} ${DEFOLDSHADERS_SOURCE_DIR}
    build_library riveshaders $platform $platform_ne ${DEFOLDSHADERS_SOURCE_DIR} ${BUILD}

    mkdir -p ${DEFOLDSHADERS_INCLUDE_DIR}
    rm -v ${DEFOLDSHADERS_INCLUDE_DIR}/*.gen.h
    cp -v ${DEFOLDSHADERS_SOURCE_DIR}/*.gen.h ${DEFOLDSHADERS_INCLUDE_DIR}
done


echo "************************************************************"
echo "DONE"
echo "************************************************************"
