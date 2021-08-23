#!/usr/bin/env bash

set -e

#URL=https://github.com/rive-app/rive-cpp/archive/refs/heads/master.zip
#UNPACK_FOLDER="rive-cpp-master"
URL=https://github.com/rive-app/rive-cpp/archive/refs/heads/low_level_rendering.zip
UNPACK_FOLDER="rive-cpp-low_level_rendering"
CXXFLAGS="${CXXFLAGS} -DLOW_LEVEL_RENDERING -DCONTOUR_RECURSIVE"

PLATFORM=$1
if [ ! -z "${PLATFORM}" ]; then
    shift
else
    export PLATFORM=$(uname | awk '{print tolower($0)}')
fi

BUILD_DIR=./build/${PLATFORM}
SOURCE_DIR="${UNPACK_FOLDER}/src"
TARGET_INCLUDE_DIR="../../defold-rive/include/rive"
TARGET_LIBRARY_DIR="../../defold-rive/lib/${PLATFORM}"
TARGET_NAME=librivecpp
TARGET_NAME_SUFFIX=.a
OPT="-O2"


SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

function download_package
{
    local url=$1
    local basename=$(basename $url)

    if [ ! -e ${basename} ]; then
        wget $URL
    else
        echo "Found ${basename}, skipping."
    fi
}

function unpack_package
{
    local package=$1
    if [ ! -e ${package} ]; then
        echo "Cannot unpack! Package ${package} was not found!" && exit 1
    fi

    if [ -e "${UNPACK_FOLDER}" ]; then
        echo "Folder already exists ${UNPACK_FOLDER}"
    else
        unzip -q ${package}
    fi
}

function copy_headers
{
    local target_dir=$1
    echo "Copying header files to ${target_dir}"
    cp -r ${UNPACK_FOLDER}/include/rive/ ${target_dir}
}

function copy_library
{
    local library=$1
    local target_dir=$2

    if [ ! -d "${target_dir}" ]; then
        mkdir -p ${target_dir}
    fi
    echo "Copying library to ${target_dir}"
    cp -v ${library} ${target_dir}
}

function run_cmd
{
    local args=$1
    echo "${args}"
    ${args}
}

function compile_file
{
    local src=$1
    local tgt=$2

    run_cmd "${CXX} ${OPT} ${SYSROOT} ${CXXFLAGS} -c ${src} -o ${tgt}"
}

function link_library
{
    local out=$1
    local object_files=$2
    run_cmd "${AR} -rcs ${out} ${object_files}"

    if [ ! -z "${RANLIB}" ]; then
        run_cmd "${RANLIB} ${out}"
    fi
}

function build_library
{
    local platform=$1
    local library_target=$2
    local source_dir=$3
    local files=$(find ${source_dir} -iname "*.cpp")

    echo "Building library for platform ${platform}"

    if [ ! -d "${BUILD_DIR}" ]; then
        mkdir -p ${BUILD_DIR}
    fi

    object_files=""

    for f in ${files}
    do
        local tgt=${BUILD_DIR}/$(basename ${f}).o

        object_files="$object_files $tgt"

        compile_file ${f} ${tgt}
    done

    link_library ${library_target} "${object_files}"

    echo ""
    echo "Wrote ${library_target}"
}

function find_latest_sdk
{
    local pattern=$1

    ls -1 -td $DYNAMO_HOME/ext/SDKs/${pattern} | head -1
}

if [ ! -z "${DYNAMO_HOME}" ]; then
    echo "Found DYNAMO_HOME=${DYNAMO_HOME}, setting up SDK paths:"
    DARWIN_TOOLCHAIN_ROOT=$(find_latest_sdk XcodeDefault*.xctoolchain)
    OSX_SDK_ROOT=$(find_latest_sdk MacOSX*.sdk)
    IOS_SDK_ROOT=$(find_latest_sdk iPhoneOS*.sdk)
    ANDROID_NDK_ROOT=$(find_latest_sdk android-ndk-*)
    EMSCRIPTEN=$(find_latest_sdk emsdk-*)/upstream/emscripten

    echo DARWIN_TOOLCHAIN_ROOT=${DARWIN_TOOLCHAIN_ROOT}
    echo OSX_SDK_ROOT=${OSX_SDK_ROOT}
    echo IOS_SDK_ROOT=${IOS_SDK_ROOT}
    echo ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT}
    echo EMSCRIPTEN=${EMSCRIPTEN}
    echo ""
fi


CXXFLAGS="${CXXFLAGS} -std=c++17 -I./${UNPACK_FOLDER}/include"

case $PLATFORM in
    arm64-ios)
        [ ! -e "${DARWIN_TOOLCHAIN_ROOT}" ] && echo "No SDK found at DARWIN_TOOLCHAIN_ROOT=${DARWIN_TOOLCHAIN_ROOT}" && exit 1
        [ ! -e "${IOS_SDK_ROOT}" ] && echo "No SDK found at IOS_SDK_ROOT=${IOS_SDK_ROOT}" && exit 1
        export SDKROOT="${IOS_SDK_ROOT}"

        export PATH=$DARWIN_TOOLCHAIN_ROOT/usr/bin:$PATH
        export CXXFLAGS="${CXXFLAGS} -stdlib=libc++ -arch arm64 "

        if [ -z "${IOS_MIN_SDK_VERSION}" ]; then
            IOS_MIN_SDK_VERSION="8.0"
        fi

        if [ ! -z "${IOS_MIN_SDK_VERSION}" ]; then
            export CXXFLAGS="${CXXFLAGS} -miphoneos-version-min=${IOS_MIN_SDK_VERSION} "
            export LDFLAGS="${LDFLAGS} -miphoneos-version-min=${IOS_MIN_SDK_VERSION}"
        fi

        export CXX=$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang++
        export AR=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ar
        export RANLIB=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ranlib

        ;;


    x86_64-osx)
        [ ! -e "${DARWIN_TOOLCHAIN_ROOT}" ] && echo "No SDK found at DARWIN_TOOLCHAIN_ROOT=${DARWIN_TOOLCHAIN_ROOT}" && exit 1
        [ ! -e "${OSX_SDK_ROOT}" ] && echo "No SDK found at OSX_SDK_ROOT=${OSX_SDK_ROOT}" && exit 1
        export SDKROOT="${OSX_SDK_ROOT}"
        export CXX=$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang++
        export AR=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ar
        export RANLIB=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ranlib

        export CXXFLAGS="${CXXFLAGS} -stdlib=libc++ "

        if [ -z "${OSX_MIN_SDK_VERSION}" ]; then
            OSX_MIN_SDK_VERSION="10.7"
        fi

        if [ ! -z "${OSX_MIN_SDK_VERSION}" ]; then
            export MACOSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION}
            export CXXFLAGS="${CXXFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION} "
            export LDFLAGS="${LDFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION}"
        fi

        ;;

    x86_64-linux)
        export CPPFLAGS="${CPPFLAGS} -fPIC"
        ;;
    x86_64-win32)
        #export CPPFLAGS="${CPPFLAGS} -fPIC"
        TARGET_NAME_SUFFIX=.lib
        ;;
    x86-win32)
        #export CPPFLAGS="${CPPFLAGS} -fPIC"
        TARGET_NAME_SUFFIX=.lib
        ;;

    armv7-android)
        [ ! -e "${ANDROID_NDK_ROOT}" ] && echo "No SDK found at ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT}" && exit 1
        export host_platform=`uname | awk '{print tolower($0)}'`
        export llvm="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${host_platform}-x86_64/bin"
        export SDKROOT="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${host_platform}-x86_64/sysroot"

        if [ -z "${OPT}" ]; then
            export OPT="-Os"
        fi
        export CXXFLAGS="${CXXFLAGS} -fpic -ffunction-sections -funwind-tables -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__ -march=armv7-a -mfloat-abi=softfp -mfpu=vfp -mthumb -fomit-frame-pointer -fno-strict-aliasing -DANDROID -Wno-c++11-narrowing"

        if [ -z "${ANDROID_VERSION}" ]; then
            ANDROID_VERSION=16
        fi

        export CXX="${llvm}/armv7a-linux-androideabi${ANDROID_VERSION}-clang++"
        export AR=${llvm}/arm-linux-androideabi-ar
        export RANLIB=${llvm}/arm-linux-androideabi-ranlib
        ;;

    arm64-android)
        [ ! -e "${ANDROID_NDK_ROOT}" ] && echo "No SDK found at ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT}" && exit 1
        export host_platform=`uname | awk '{print tolower($0)}'`
        export llvm="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${host_platform}-x86_64/bin"
        export SDKROOT="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${host_platform}-x86_64/sysroot"

        if [ -z "${OPT}" ]; then
            export OPT="-Os"
        fi
        export CXXFLAGS="${CXXFLAGS} -fpic -ffunction-sections -funwind-tables -D__aarch64__  -march=armv8-a -fomit-frame-pointer -fno-strict-aliasing -DANDROID -Wno-c++11-narrowing"

        if [ -z "${ANDROID_VERSION}" ]; then
            ANDROID_VERSION=21 # Android 5.0
        fi

        export CXX="${llvm}/aarch64-linux-android${ANDROID_VERSION}-clang++"
        export AR=${llvm}/aarch64-linux-android-ar
        export RANLIB=${llvm}/aarch64-linux-android-ranlib
        ;;

    js-web)
        [ ! -e "${EMSCRIPTEN}" ] && echo "No SDK found at EMSCRIPTEN=${EMSCRIPTEN}" && exit 1
        export CXX=${EMSCRIPTEN}/em++
        export AR=${EMSCRIPTEN}/emar
        export RANLIB=${EMSCRIPTEN}/emranlib
        export CXXFLAGS="${CXXFLAGS} -fPIC -fno-exceptions"

        CXXFLAGS="${CXXFLAGS} -s PRECISE_F32=2 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s DISABLE_EXCEPTION_CATCHING=1"
        CXXFLAGS="${CXXFLAGS} -s WASM=0 -s LEGACY_VM_SUPPORT=1"
        ;;

    wasm-web)
        [ ! -e "${EMSCRIPTEN}" ] && echo "No SDK found at EMSCRIPTEN=${EMSCRIPTEN}" && exit 1
        export CXX=${EMSCRIPTEN}/em++
        export AR=${EMSCRIPTEN}/emar
        export RANLIB=${EMSCRIPTEN}/emranlib
        export CXXFLAGS="${CXXFLAGS} -fPIC -fno-exceptions -fno-rtti"

        CXXFLAGS="${CXXFLAGS} -s PRECISE_F32=2 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s DISABLE_EXCEPTION_CATCHING=1"
        CXXFLAGS="${CXXFLAGS} -s WASM=1 -s IMPORTED_MEMORY=1 -s ALLOW_MEMORY_GROWTH=1"

        ;;

    *)
        echo "Unknown platform: ${PLATFORM}, using CROSS_TOOLS_PREFIX to prefix clang++, ar and ranlib, unless CXX, AR and RANLIB are specified"
        if [ -z "${CXX}" ]; then
            export CXX=${CROSS_TOOLS_PREFIX}clang++
        fi
        if [ -z "${AR}" ]; then
            export AR=${CROSS_TOOLS_PREFIX}ar
        fi
        if [ -z "${RANLIB}" ]; then
            export RANLIB=${CROSS_TOOLS_PREFIX}ranlib
        fi
        ;;
esac

if [ ! -z "${SDKROOT}" ]; then
    export SYSROOT="-isysroot $SDKROOT"
fi

# *************************************************

pushd $SCRIPT_DIR

if [ ! -d "${BUILD_DIR}" ]; then
    mkdir -p ${BUILD_DIR}
fi

download_package $URL
unpack_package $(basename $URL)

copy_headers ${TARGET_INCLUDE_DIR}

TARGET_LIBRARY=${BUILD_DIR}/${TARGET_NAME}${TARGET_NAME_SUFFIX}
build_library ${PLATFORM} ${TARGET_LIBRARY} ${SOURCE_DIR}

copy_library ${TARGET_LIBRARY} ${TARGET_LIBRARY_DIR}

popd