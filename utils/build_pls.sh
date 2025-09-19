#! /usr/bin/env bash
set -x

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

OUTPUT_LIB_DIR=${SCRIPT_DIR}/../defold-rive/lib

RIVE_DIR=
PLATFORMS=

while [ "$#" -ge "1" ]; do
    arg="$1"
    if [ "$arg" = "--rive" ]; then
        shift
        RIVE_DIR="$1"
    else
        PLATFORMS="$PLATFORMS $1"
    fi
    shift
done


if [ "" == "${PLATFORMS}" ]; then
    PLATFORMS="x86_64-macos arm64-macos arm64-linux x86_64-linux x86_64-win32 x86-win32 arm64-ios x86_64-ios arm64-android js-web wasm-web wasm_pthread-web"
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

function stripfile() {
    local platform=$1
    local path=$2

    local STRIP=
    case ${platform} in
        x86_64-macos|arm64-macos|arm64-ios|x86_64-ios)
            STRIP=$(which strip)
            ;;
        # arm64-android)
        #     STRIP=$(find ~/Library/android/sdk -iname "*llvm-strip" | sort -r | head -n 1)
        #     ;;
    esac

    if [ "" != "${STRIP}" ]; then
        echo "${STRIP} ${path}"
        ${STRIP} ${path}
    else
        echo "No strip exe found, Skipping"
    fi
}

function copy_results() {
    local platform=$1
    local platform_ne=$2
    local target_dir=$3

    for path in ./build/$platform_ne/*.a; do
        stripfile ${platform} ${path}
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

    java -jar $BOB --platform=$platform --architectures=$platform --settings=${target_dir}/ext.settings resolve build --build-artifacts=library --variant $VARIANT --build-server=$SERVER --debug-ne-upload true --ne-output-name=${name} --ne-build-dir ${source_dir} --defoldsdk=${DEFOLDSDK} # remove --defoldsdk for building WebGL

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
        echo "Unpack $1 done."
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

#NOTE: If you update this, make sure to update the copy_harfbuzz.sh as well
HARFBUZZ_VERSION=11.2.1
HARFBUZZ_ZIP=${DOWNLOAD_DIR}/harfbuzz-${HARFBUZZ_VERSION}.zip
HARFBUZZ_URL=https://github.com/harfbuzz/harfbuzz/archive/refs/tags/${HARFBUZZ_VERSION}.zip
HARFBUZZ_ORIGINAL_DIR=${DOWNLOAD_DIR}/harfbuzz/harfbuzz-${HARFBUZZ_VERSION}/src
HARFBUZZ_SOURCE_DIR=${SOURCE_DIR}/harfbuzz/src

download_zip ${HARFBUZZ_ZIP} ${DOWNLOAD_DIR}/harfbuzz ${HARFBUZZ_URL}

echo "*************************************************"
echo "Copying harfbuzz files"
${SCRIPT_DIR}/copy_harfbuzz.sh ${HARFBUZZ_ORIGINAL_DIR} ${HARFBUZZ_SOURCE_DIR}

echo "*************************************************"
echo "Downloading yoga files"

RIVE_YOGA_VERSION=2.0.1
RIVE_YOGA_ZIP=${DOWNLOAD_DIR}/yoga-${RIVE_YOGA_VERSION}.zip
RIVE_YOGA_URL=https://github.com/rive-app/yoga/archive/refs/heads/rive_changes_v2_0_1.zip
RIVE_YOGA_ORIGINAL_DIR=${DOWNLOAD_DIR}/yoga/yoga-rive_changes_v2_0_1
RIVE_YOGA_SOURCE_DIR=${SOURCE_DIR}/yoga/src

download_zip ${RIVE_YOGA_ZIP} ${DOWNLOAD_DIR}/yoga ${RIVE_YOGA_URL}

mkdir -p ${RIVE_YOGA_SOURCE_DIR}/yoga
mkdir -p ${RIVE_YOGA_SOURCE_DIR}/yoga/event

cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/*.h ${RIVE_YOGA_SOURCE_DIR}/yoga
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/event/*.h ${RIVE_YOGA_SOURCE_DIR}/yoga/event

cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/Utils.cpp ${RIVE_YOGA_SOURCE_DIR}/yoga
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/YGConfig.cpp ${RIVE_YOGA_SOURCE_DIR}/yoga
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/YGLayout.cpp ${RIVE_YOGA_SOURCE_DIR}/yoga
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/YGEnums.cpp ${RIVE_YOGA_SOURCE_DIR}/yoga
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/YGNodePrint.cpp ${RIVE_YOGA_SOURCE_DIR}/yoga
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/YGNode.cpp ${RIVE_YOGA_SOURCE_DIR}/yoga
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/YGValue.cpp ${RIVE_YOGA_SOURCE_DIR}/yoga
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/YGStyle.cpp ${RIVE_YOGA_SOURCE_DIR}/yoga
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/Yoga.cpp ${RIVE_YOGA_SOURCE_DIR}/yoga
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/event/event.cpp ${RIVE_YOGA_SOURCE_DIR}/yoga/event
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/log.cpp ${RIVE_YOGA_SOURCE_DIR}/yoga

echo "*************************************************"
echo "Downloading sheenbidi files"


SHEENBIDI_VERSION=2.6
SHEENBIDI_ZIP=${DOWNLOAD_DIR}/sheenbidi-${SHEENBIDI_VERSION}.zip
SHEENBIDI_URL=https://github.com/Tehreer/SheenBidi/archive/refs/tags/v${SHEENBIDI_VERSION}.zip
SHEENBIDI_ORIGINAL_DIR=${DOWNLOAD_DIR}/sheenbidi/SheenBidi-${SHEENBIDI_VERSION}
SHEENBIDI_SOURCE_DIR=${SOURCE_DIR}/sheenbidi/src

download_zip ${SHEENBIDI_ZIP} ${DOWNLOAD_DIR}/sheenbidi ${SHEENBIDI_URL}

mkdir -p ${SHEENBIDI_SOURCE_DIR}
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

if [ -z "$RIVE_DIR" ]; then
    echo "*************************************************"
    echo "Downloading rive-cpp files"

    # https://github.com/rive-app/rive-runtime/commit/<sha>

    RIVECPP_VERSION=e54883d9099f87ed6d87c678793fd619b5594e2c
    RIVECPP_ZIP=${DOWNLOAD_DIR}/rivecpp-${RIVECPP_VERSION}.zip
    RIVECPP_URL="https://github.com/rive-app/rive-runtime/archive/${RIVECPP_VERSION}.zip"

    download_zip ${RIVECPP_ZIP} ${DOWNLOAD_DIR}/rivecpp ${RIVECPP_URL}
    RIVECPP_ORIGINAL_DIR=${DOWNLOAD_DIR}/rivecpp/rive-runtime-${RIVECPP_VERSION}
else
    RIVECPP_ORIGINAL_DIR=${RIVE_DIR}
fi

RIVECPP_SOURCE_DIR=${SOURCE_DIR}/rivecpp/src
RIVECPP_TESS_SOURCE_DIR=${SOURCE_DIR}/rivecpp-tess/src
RIVECPP_RENDERER_SOURCE_DIR=${SOURCE_DIR}/rivecpp-renderer/src
RIVECPP_HARFBUZZ_INCLUDE_DIR=${RIVECPP_SOURCE_DIR}/src/text
RIVECPP_RENDERER_SHADER_DIR=${RIVECPP_RENDERER_SOURCE_DIR}/src/


# Platforms can include different files in the renderer, so to make sure we don't build the wrong thing we remove all before building
rm -rf ${RIVECPP_RENDERER_SOURCE_DIR}

mkdir -p ${RIVECPP_SOURCE_DIR}
mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}
mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include
mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src
mkdir -p ${RIVECPP_RENDERER_SHADER_DIR}
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

echo "COPY YOGA TO RIVECPP"

mkdir -p ${RIVECPP_SOURCE_DIR}/yoga
mkdir -p ${RIVECPP_SOURCE_DIR}/yoga/event

# TODO: We have to modify the yoga headers manually because we can't set empty defines..
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/*.h       ${RIVECPP_SOURCE_DIR}/yoga
cp -r -v ${RIVE_YOGA_ORIGINAL_DIR}/yoga/event/*.h ${RIVECPP_SOURCE_DIR}/yoga/event

# Copy text related headers
(cd ${HARFBUZZ_ORIGINAL_DIR} && cp -v *.h ${RIVECPP_HARFBUZZ_INCLUDE_DIR})
(cd ${SHEENBIDI_ORIGINAL_DIR}/Headers && cp -v *.h ${RIVECPP_HARFBUZZ_INCLUDE_DIR})

echo "COPY RIVE-RENDERER"
cp -r -v ${RIVECPP_ORIGINAL_DIR}/include/rive ${RIVECPP_RENDERER_SOURCE_DIR}/include/rive
cp -r -v ${RIVECPP_ORIGINAL_DIR}/include/utils ${RIVECPP_RENDERER_SOURCE_DIR}/include
cp -r -v ${RIVECPP_ORIGINAL_DIR}/renderer/include ${RIVECPP_RENDERER_SOURCE_DIR}
cp -r -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/*.cpp ${RIVECPP_RENDERER_SOURCE_DIR}/src
cp -r -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/*.hpp ${RIVECPP_RENDERER_SOURCE_DIR}/src
cp -r -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/shaders ${RIVECPP_RENDERER_SHADER_DIR}
cp -r -v ${RIVECPP_ORIGINAL_DIR}/renderer/glad/src/*.c ${RIVECPP_RENDERER_SOURCE_DIR}/src

echo "*************************************************"
echo "Downloading python PLY files"

LIBPLY_VERSION=5c4dc94d4c6d059ec127ee1493c735963a5d2645
LIBPLY_ZIP=${DOWNLOAD_DIR}/libply-${LIBPLY_VERSION}.zip
LIBPLY_URL=https://github.com/dabeaz/ply/archive/${LIBPLY_VERSION}.zip
LIBPLY_ORIGINAL_DIR=${DOWNLOAD_DIR}/libply/libply-${LIBPLY_VERSION}
LIBPLY_SOURCE_DIR=${SOURCE_DIR}/libply/src
LIBPLY_PATH=${DOWNLOAD_DIR}/libply/ply-${LIBPLY_VERSION}/src

download_zip ${LIBPLY_ZIP} ${DOWNLOAD_DIR}/libply ${LIBPLY_URL}

echo "*************************************************"
echo "Setup shader source variables"

source ${SCRIPT_DIR}/gen_embedded_shaders.sh

DEFOLDSHADERS_INPUT_DIR=${SCRIPT_DIR}/../defold-rive/assets/shader-library
DEFOLDSHADERS_INCLUDE_DIR=${SCRIPT_DIR}/../defold-rive/include/defold/shaders
DEFOLDSHADERS_SOURCE_DIR=${SOURCE_DIR}/defoldshaders/src

mkdir -p ${DEFOLDSHADERS_INCLUDE_DIR}
mkdir -p ${DEFOLDSHADERS_SOURCE_DIR}

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
    echo "YOGA ${platform}"
    echo "************************************************************"

    export DEFINES="YOGA_EXPORT="
    export INCLUDES="upload/src"
    export CXXFLAGS="-std=c++17 -fno-rtti -fno-exceptions"
    build_library yoga $platform $platform_ne ${RIVE_YOGA_SOURCE_DIR} ${BUILD}

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

    echo "************************************************************"
    echo "RIVE CPP ${platform}"
    echo "************************************************************"

    export CXXFLAGS="-std=c++17 -fno-rtti -fno-exceptions"
    export DEFINES="WITH_RIVE_TEXT WITH_RIVE_LAYOUT _RIVE_INTERNAL_ YOGA_EXPORT="
    unset INCLUDES
    build_library rive $platform $platform_ne ${RIVECPP_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "RIVECPP TESS ${platform}"
    echo "************************************************************"
    unset DEFINES
    unset INCLUDES
    export CXXFLAGS="-std=c++17 -fno-rtti -fno-exceptions"
    build_library rivetess $platform $platform_ne ${RIVECPP_TESS_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "RIVE Renderer ${platform}"
    echo "************************************************************"

    RIVE_RENDERER_DEFINES=
    RIVE_RENDERER_CXXFLAGS=
    RIVE_RENDERER_INCLUDES="upload/src"

    # Due to a self include bug in rive_render_path.hpp, it references itself
    # We workaround it by adding a copy in the relative path it asks for "../renderer/src/rive_render_path.hpp"
    mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/renderer/src/
    echo "// intentionally left empty due to the self include issue" > ${RIVECPP_RENDERER_SOURCE_DIR}/renderer/src/rive_render_path.hpp

    case ${platform} in
        armv7-android|arm64-android)
            RIVE_RENDERER_DEFINES="RIVE_ANDROID"

            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders

            # remove any previously generated shaders
            (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && rm -rf ./out)
            (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && pwd && make FLAGS="-p ${LIBPLY_PATH}")

            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/out/generated/*.*           ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders/
            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/*.glsl                      ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders/

            # Common
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/gl_state.cpp                   ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/gl_utils.cpp                   ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/load_store_actions_ext.cpp     ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_buffer_gl_impl.cpp      ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_context_gl_impl.cpp     ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_target_gl.cpp           ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/

            # Android specific
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/load_gles_extensions.cpp       ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/pls_impl_ext_native.cpp        ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            #cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/pls_impl_framebuffer_fetch.cpp ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            ;;
        x86_64-win32|x86-win32)
            RIVE_RENDERER_DEFINES="RIVE_DESKTOP_GL RIVE_WINDOWS"
            RIVE_RENDERER_INCLUDES="upload/src/glad ${RIVE_RENDERER_INCLUDES}"

            # remove any previously generated shaders
            (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && rm -rf ./out)
            (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && pwd && make FLAGS="-p ${LIBPLY_PATH}")

            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/glad

            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/out/generated/*.*           ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders/
            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/*.glsl                      ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/gl_state.cpp                   ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/gl_utils.cpp                   ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/load_store_actions_ext.cpp     ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_buffer_gl_impl.cpp      ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_context_gl_impl.cpp     ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_target_gl.cpp           ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/pls_impl_webgl.cpp             ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/pls_impl_rw_texture.cpp        ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/glad/*.*                              ${RIVECPP_RENDERER_SOURCE_DIR}/src/glad
            cp -v -r ${RIVECPP_ORIGINAL_DIR}/renderer/glad/include/                      ${RIVECPP_RENDERER_SOURCE_DIR}/include
            ;;

        arm64-linux|x86_64-linux)
            RIVE_RENDERER_DEFINES="RIVE_DESKTOP_GL RIVE_LINUX"
            RIVE_RENDERER_INCLUDES="upload/src/glad ${RIVE_RENDERER_INCLUDES}"

            # remove any previously generated shaders
            (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && rm -rf ./out)
            (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && pwd && make FLAGS="-p ${LIBPLY_PATH}")

            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/glad

            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/out/generated/*.*           ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders/
            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/*.glsl                      ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders/

            # Common
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/gl_state.cpp                   ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/gl_utils.cpp                   ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/load_store_actions_ext.cpp     ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_buffer_gl_impl.cpp      ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_context_gl_impl.cpp     ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_target_gl.cpp           ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/pls_impl_webgl.cpp             ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/pls_impl_rw_texture.cpp        ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/glad/*.*                              ${RIVECPP_RENDERER_SOURCE_DIR}/src/glad
            cp -v -r ${RIVECPP_ORIGINAL_DIR}/renderer/glad/include/                      ${RIVECPP_RENDERER_SOURCE_DIR}/include
            ;;

        x86_64-macos|arm64-macos)
            RIVE_RENDERER_DEFINES="RIVE_DESKTOP_GL RIVE_MACOSX"
            RIVE_RENDERER_CXXFLAGS="-fobjc-arc"
            RIVE_RENDERER_INCLUDES="upload/src/glad ${RIVE_RENDERER_INCLUDES}"

            # remove any previously generated shaders
            (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && rm -rf ./out)
            (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && pwd && make rive_pls_macosx_metallib FLAGS="-p ${LIBPLY_PATH}")

            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/metal
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/glad
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders

            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/out/generated/*.*       ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders/
            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/*.glsl                  ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/metal/*.*                     ${RIVECPP_RENDERER_SOURCE_DIR}/src/metal/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/gl_state.cpp               ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/gl_utils.cpp               ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/load_store_actions_ext.cpp ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_buffer_gl_impl.cpp  ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_context_gl_impl.cpp ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_target_gl.cpp       ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/pls_impl_webgl.cpp         ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/pls_impl_rw_texture.cpp    ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/glad/*.*                          ${RIVECPP_RENDERER_SOURCE_DIR}/src/glad
            cp -v -r ${RIVECPP_ORIGINAL_DIR}/renderer/glad/include/                  ${RIVECPP_RENDERER_SOURCE_DIR}/include
            ;;
      x86_64-ios|arm64-ios)

            RIVE_RENDERER_DEFINES="RIVE_IOS"
            if [ "${platform}" == "x86_64-ios" ]; then
                RIVE_RENDERER_DEFINES="RIVE_IOS_SIMULATOR"
            fi
            RIVE_RENDERER_CXXFLAGS="-fobjc-arc"
            RIVE_RENDERER_INCLUDES="upload/src/glad ${RIVE_RENDERER_INCLUDES}"

            # remove any previously generated shaders
            (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && rm -rf ./out)
            if [ "${platform}" == "x86_64-ios" ]; then
                (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && pwd && make rive_pls_ios_simulator_metallib FLAGS="-p ${LIBPLY_PATH}")
            else
                (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && pwd && make rive_pls_ios_metallib FLAGS="-p ${LIBPLY_PATH}")
            fi

            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/metal
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/glad
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders

            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/out/generated/*.*       ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders/
            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/*.glsl                  ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/metal/*.*                     ${RIVECPP_RENDERER_SOURCE_DIR}/src/metal/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/glad/*.*                          ${RIVECPP_RENDERER_SOURCE_DIR}/src/glad
            cp -v -r ${RIVECPP_ORIGINAL_DIR}/renderer/glad/include/                  ${RIVECPP_RENDERER_SOURCE_DIR}/include
            ;;

        wasm-web|wasm_pthread-web|js-web)
            RIVE_RENDERER_DEFINES="RIVE_WEBGL RIVE_WEBGPU=1"

            # NOTE: To build WebGL, you have to do the following manual steps:
            #   * Don't pass a DEFOLDSDK to bob when building, you need to use a local dynamo home SDK
            #   * In build_input.yml on the local SDK, remove the GL_ES_VERSION_2_0 define completely

            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/src/webgpu
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders/spirv
            mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/include/webgpu

            # remove any previously generated shaders
            (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && rm -rf ./out)
            (cd ${RIVECPP_RENDERER_SHADER_DIR}/shaders && pwd && make spirv FLAGS="-p ${LIBPLY_PATH}") # Remove spirv if not building for webgpu

            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/out/generated/**.*       ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders/
            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/out/generated/spirv/**.* ${RIVECPP_RENDERER_SOURCE_DIR}/include/generated/shaders/spirv
            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/*.glsl                   ${RIVECPP_RENDERER_SOURCE_DIR}/include/shaders/
            cp -v ${RIVECPP_RENDERER_SOURCE_DIR}/src/shaders/*.glsl                   ${RIVECPP_RENDERER_SOURCE_DIR}/include/webgpu/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/gl_state.cpp               ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/gl_utils.cpp               ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/load_store_actions_ext.cpp ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_buffer_gl_impl.cpp  ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_context_gl_impl.cpp ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/render_target_gl.cpp       ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/

            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/gl/pls_impl_webgl.cpp         ${RIVECPP_RENDERER_SOURCE_DIR}/src/gl/

            cp -v -r ${RIVECPP_ORIGINAL_DIR}/renderer/glad/include/                   ${RIVECPP_RENDERER_SOURCE_DIR}/include

            # WebGPU
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/webgpu/**.*                   ${RIVECPP_RENDERER_SOURCE_DIR}/src/webgpu/
    esac

    export CXXFLAGS="-std=c++17 -fno-rtti -fno-exceptions ${RIVE_RENDERER_CXXFLAGS}"
    export INCLUDES="upload/src/include/rive upload/src/include upload/src/src ${RIVE_RENDERER_INCLUDES}"
    if [ "${RIVE_RENDERER_INCLUDES}" != "" ]; then
        export INCLUDES="${INCLUDES} ${RIVE_RENDERER_INCLUDES}"
    fi
    if [ "${RIVE_RENDERER_DEFINES}" != "" ]; then
        export DEFINES="${RIVE_RENDERER_DEFINES}"
    fi

    build_library rive_renderer $platform $platform_ne ${RIVECPP_RENDERER_SOURCE_DIR} ${BUILD}


    case ${platform} in
        wasm-web|wasm_pthread-web)

            # # Wagyu
            # cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/webgpu/wagyu-port/old/include/webgpu/*.* ${RIVECPP_RENDERER_SOURCE_DIR}/include/webgpu/

            # ##########################################################################
            # echo "Building RIVE_WEBGPU=1"

            # RIVE_RENDERER_DEFINES="RIVE_WEBGL RIVE_WEBGPU=1 RIVE_WAGYU"
            # export INCLUDES="upload/src ${RIVE_RENDERER_INCLUDES}"

            # # We temporarily remove the WebGL support until they've fixed their includes

            # if [ "${RIVE_RENDERER_DEFINES}" != "" ]; then
            #     export DEFINES="${RIVE_RENDERER_DEFINES}"
            # fi

            # build_library rive_renderer_wagyu_gl $platform $platform_ne ${RIVECPP_RENDERER_SOURCE_DIR} ${BUILD}

            ##########################################################################
            echo "Building RIVE_WEBGPU=2"
            RIVE_RENDERER_DEFINES="RIVE_WEBGL RIVE_WEBGPU=2 RIVE_WAGYU"
            export INCLUDES="upload/src ${RIVE_RENDERER_INCLUDES}"

            if [ "${RIVE_RENDERER_DEFINES}" != "" ]; then
                export DEFINES="${RIVE_RENDERER_DEFINES}"
            fi

            rm ${RIVECPP_RENDERER_SOURCE_DIR}/src/webgpu/webgpu_compat.h
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/webgpu/wagyu-port/new/include/webgpu/*.* ${RIVECPP_RENDERER_SOURCE_DIR}/include/webgpu/
            build_library rive_renderer_wagyu $platform $platform_ne ${RIVECPP_RENDERER_SOURCE_DIR} ${BUILD}
    esac

    echo "************************************************************"
    echo "RIVE SHADERS ${platform}"
    echo "************************************************************"
    unset DEFINES
    unset INCLUDES
    export CXXFLAGS="-x c"
    generate_cpp_sources ${platform} ${DEFOLDSHADERS_INPUT_DIR}/rivemodel_blit.vp ${DEFOLDSHADERS_INPUT_DIR}/rivemodel_blit.fp ${DEFOLDSHADERS_SOURCE_DIR}/rivemodel_blit.spc
    build_library riveshaders $platform $platform_ne ${DEFOLDSHADERS_SOURCE_DIR} ${BUILD}

    # # TODO: Fix this (paths are wrong)
    # mkdir -p ${DEFOLDSHADERS_INCLUDE_DIR}
    # rm -v ${DEFOLDSHADERS_INCLUDE_DIR}/*.gen.h
    # cp -v ${DEFOLDSHADERS_SOURCE_DIR}/*.gen.h ${DEFOLDSHADERS_INCLUDE_DIR}
done


echo "************************************************************"
echo "DONE"
echo "************************************************************"
