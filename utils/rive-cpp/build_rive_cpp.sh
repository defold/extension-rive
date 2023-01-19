#!/usr/bin/env bash

set -e

URL=https://github.com/rive-app/rive-cpp/archive/refs/heads/master.zip
UNPACK_FOLDER="rive-cpp-master"

EARCUT_URL=https://github.com/mapbox/earcut.hpp/archive/refs/heads/master.zip
EARCUT_UNPACK_FOLDER="earcut.hpp-master"

LIBTESS2_URL=https://github.com/memononen/libtess2/archive/refs/heads/master.zip
LIBTESS2_UNPACK_FOLDER="libtess2-master"

PLATFORM=$1
if [ ! -z "${PLATFORM}" ]; then
    shift
else
    echo "You must specify a target platform!"
    exit 1
fi

BUILD_DIR=./build/${PLATFORM}
SOURCE_DIR="${UNPACK_FOLDER}/src"
TESS_DIR="${UNPACK_FOLDER}/tess"
TESS2_DIR="${LIBTESS2_UNPACK_FOLDER}/Source"
TARGET_INCLUDE_DIR="../../defold-rive/include/rive"
TARGET_LIBRARY_DIR="../../defold-rive/lib/${PLATFORM}"
TARGET_NAME_RIVE=librivecpp
TARGET_NAME_TESS=librivetess
TARGET_NAME_TESS2=libtess2
TARGET_LIB_SUFFIX=.a
OPT="-Oz"
CCFLAGS=" -g -Werror=format"
CXXFLAGS="${CXXFLAGS} ${CCFLAGS}"


SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

function download_package
{
    local url=$1
    local basename=$(basename $url)

    if [ ! -e ${basename} ]; then
        wget ${url}
    else
        echo "Found ${basename}, skipping."
    fi
}

function unpack_package
{
    local package=$1
    local folder=$2
    if [ ! -e ${package} ]; then
        echo "Cannot unpack! Package ${package} was not found!" && exit 1
    fi

    if [ -e "${folder}" ]; then
        echo "Folder already exists ${folder}"
    else
        echo "Unpacking ${package}"
        unzip -q ${package}
    fi
}

function remove_package
{
    local url=$1
    local basename=$(basename $url)
    rm ${basename}
}

function copy_headers
{
    local target_dir=$1
    echo "Removing old headers from ${target_dir}"
    rm -rf ${target_dir}
    echo "Copying rive header files to ${target_dir}"
    cp -r ${UNPACK_FOLDER}/include/rive/ ${target_dir}
    echo "Copying tess header files to ${target_dir}"
    cp -r ${UNPACK_FOLDER}/tess/include/rive/ ${target_dir}
}

function copy_library
{
    local library=$1
    local target_dir=$2

    if [ ! -d "${target_dir}" ]; then
        mkdir -p ${target_dir}
    fi
    cp -v ${library} ${target_dir}
}

function run_cmd
{
    local args=$1
    echo "${args}"
    ${args}
}

function compile_cpp_file
{
    local src=$1
    local tgt=$2

    run_cmd "${CXX} ${OPT} ${SYSROOT} ${CXXFLAGS} -c ${src} -o ${tgt}"
}

function compile_c_file
{
    local src=$1
    local tgt=$2

    run_cmd "${CC} ${OPT} ${SYSROOT} ${CCFLAGS} -c ${src} -o ${tgt}"
}

function link_library
{
    local out=$1
    local object_files=$2

    if [ -e "${out}" ]; then
        rm ${out}
    fi
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
    local pattern=$4
    local files=$(find "${source_dir}" -iname "${pattern}")

    echo "Building library for platform ${platform}"

    if [ ! -d "${BUILD_DIR}" ]; then
        mkdir -p ${BUILD_DIR}
    fi

    object_files=""

    OBJ_DIR=${BUILD_DIR}/obj

    mkdir -p ${OBJ_DIR}

    for f in ${files}
    do
        local tgt=${OBJ_DIR}/$(basename ${f}).o

        object_files="$object_files $tgt"

        if [ "*.cpp" == "${pattern}" ]; then
            compile_cpp_file ${f} ${tgt}
        else
            compile_c_file ${f} ${tgt}
        fi
    done

    link_library ${library_target} "${object_files}"

    echo "Removing .o files"
    rm -rf ${OBJ_DIR}

    echo ""
    echo "Wrote ${library_target}"
}

function build_cpp_library
{
    build_library $1 $2 $3 "*.cpp"
}

function build_c_library
{
    build_library $1 $2 $3 "*.c"
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
    WIN32_MSVC_INCLUDE_DIR=$(find_latest_sdk Win32/MicrosoftVisualStudio14.0/VC/Tools/MSVC/14.*)/include
    WIN32_SDK_INCLUDE_DIR=$(find_latest_sdk Win32/WindowsKits/10/Include/10.0.*)/ucrt

    echo DARWIN_TOOLCHAIN_ROOT=${DARWIN_TOOLCHAIN_ROOT}
    echo OSX_SDK_ROOT=${OSX_SDK_ROOT}
    echo IOS_SDK_ROOT=${IOS_SDK_ROOT}
    echo ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT}
    echo EMSCRIPTEN=${EMSCRIPTEN}
    echo ""
fi

CCFLAGS="${CCFLAGS} -I./${UNPACK_FOLDER}/include -I./${UNPACK_FOLDER}/tess/include/ -I./${EARCUT_UNPACK_FOLDER}/include/mapbox -I./${LIBTESS2_UNPACK_FOLDER}/Include"
CXXFLAGS="${CXXFLAGS} ${CCFLAGS} -std=c++17 -fno-exceptions -fno-rtti"

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
        #export CXX=$DARWIN_TOOLCHAIN_ROOT/usr/bin/clang++
        #export AR=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ar
        #export RANLIB=$DARWIN_TOOLCHAIN_ROOT/usr/bin/ranlib

        export CXX=/usr/bin/clang++
        export AR=/usr/bin/ar
        export RANLIB=/usr/bin/ranlib

        # self.env.append_value(f, ['-target', '%s-apple-darwin19' % arch])
        export CXXFLAGS="${CXXFLAGS} -stdlib=libc++ -target x86_64-apple-darwin21.2.0"

        #if [ -z "${OSX_MIN_SDK_VERSION}" ]; then
            OSX_MIN_SDK_VERSION="10.7"
        #fi

        #if [ ! -z "${OSX_MIN_SDK_VERSION}" ]; then
            export MACOSX_DEPLOYMENT_TARGET=${OSX_MIN_SDK_VERSION}
            export CXXFLAGS="${CXXFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION} "
            export LDFLAGS="${LDFLAGS} -mmacosx-version-min=${OSX_MIN_SDK_VERSION}"
        #fi

        ;;

    x86_64-linux)
        export CXXFLAGS="${CXXFLAGS} -fPIC"
        export CCFLAGS="${CCFLAGS} -fPIC"
        if [ -z "${CXX}" ]; then
            export CXX=clang++
        fi
        if [ -z "${AR}" ]; then
            export AR=ar
        fi
        if [ -z "${RANLIB}" ]; then
            export RANLIB=ranlib
        fi
        ;;

    x86_64-win32)
        [ ! -e "${WIN32_MSVC_INCLUDE_DIR}" ] && echo "No SDK found at WIN32_MSVC_INCLUDE_DIR=${WIN32_MSVC_INCLUDE_DIR}" && exit 1
        [ ! -e "${WIN32_SDK_INCLUDE_DIR}" ] && echo "No SDK found at WIN32_SDK_INCLUDE_DIR=${WIN32_SDK_INCLUDE_DIR}" && exit 1

        TARGET_LIB_SUFFIX=.lib

        export host_platform=`uname | awk '{print tolower($0)}'`
        if [ "darwin" == "${host_platform}" ] || [ "linux" == "${host_platform}" ]; then
            export CXXFLAGS="${CXXFLAGS} -target x86_64-pc-windows-msvc -m64 -D_CRT_SECURE_NO_WARNINGS -D__STDC_LIMIT_MACROS -DWINVER=0x0600 -DNOMINMAX -gcodeview"
            export CXXFLAGS="${CXXFLAGS} -nostdinc++ -isystem ${WIN32_MSVC_INCLUDE_DIR} -isystem ${WIN32_SDK_INCLUDE_DIR}"
        fi
        ;;

    x86-win32)
        [ ! -e "${WIN32_MSVC_INCLUDE_DIR}" ] && echo "No SDK found at WIN32_MSVC_INCLUDE_DIR=${WIN32_MSVC_INCLUDE_DIR}" && exit 1
        [ ! -e "${WIN32_SDK_INCLUDE_DIR}" ] && echo "No SDK found at WIN32_SDK_INCLUDE_DIR=${WIN32_SDK_INCLUDE_DIR}" && exit 1

        TARGET_LIB_SUFFIX=.lib

        export host_platform=`uname | awk '{print tolower($0)}'`
        if [ "darwin" == "${host_platform}" ] || [ "linux" == "${host_platform}" ]; then
            export CXXFLAGS="${CXXFLAGS} -target i386-pc-win32-msvc -m32 -D_CRT_SECURE_NO_WARNINGS -D__STDC_LIMIT_MACROS -DWINVER=0x0600 -DNOMINMAX -gcodeview"
            export CXXFLAGS="${CXXFLAGS} -nostdinc++ -isystem ${WIN32_MSVC_INCLUDE_DIR} -isystem ${WIN32_SDK_INCLUDE_DIR}"
        fi
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
        echo "Unknown platform: ${PLATFORM}, using host clang++, ar and ranlib. Prefix with CROSS_TOOLS_PREFIX to use specific tools."
        ;;
esac

if [ ! -z "${SDKROOT}" ]; then
    export SYSROOT="-isysroot $SDKROOT"
fi

if [ -z "${CXX}" ]; then
    export CXX=${CROSS_TOOLS_PREFIX}clang++
fi
if [ -z "${CC}" ]; then
    export CC=${CROSS_TOOLS_PREFIX}clang
fi
if [ -z "${AR}" ]; then
    export AR=${CROSS_TOOLS_PREFIX}ar
fi
if [ -z "${RANLIB}" ]; then
    export RANLIB=${CROSS_TOOLS_PREFIX}ranlib
fi

# *************************************************

pushd $SCRIPT_DIR

if [ ! -d "${BUILD_DIR}" ]; then
    mkdir -p ${BUILD_DIR}
fi

download_package ${URL}
unpack_package $(basename ${URL}) ${UNPACK_FOLDER}
remove_package ${URL}
rm -rf ${TESS_DIR}/test
rm -rf ${TESS_DIR}/src/sokol

download_package ${EARCUT_URL}
unpack_package $(basename ${EARCUT_URL}) ${EARCUT_UNPACK_FOLDER}
remove_package ${EARCUT_URL}

download_package ${LIBTESS2_URL}
unpack_package $(basename ${LIBTESS2_URL}) ${LIBTESS2_UNPACK_FOLDER}
remove_package ${LIBTESS2_URL}

copy_headers ${TARGET_INCLUDE_DIR}

TARGET_LIBRARY_RIVE=${BUILD_DIR}/${TARGET_NAME_RIVE}${TARGET_LIB_SUFFIX}
build_cpp_library ${PLATFORM} ${TARGET_LIBRARY_RIVE} ${SOURCE_DIR}

TARGET_LIBRARY_TESS=${BUILD_DIR}/${TARGET_NAME_TESS}${TARGET_LIB_SUFFIX}
build_cpp_library ${PLATFORM} ${TARGET_LIBRARY_TESS} ${TESS_DIR}

TARGET_LIBRARY_TESS2=${BUILD_DIR}/${TARGET_NAME_TESS2}${TARGET_LIB_SUFFIX}
build_c_library ${PLATFORM} ${TARGET_LIBRARY_TESS2} ${TESS2_DIR}

copy_library ${TARGET_LIBRARY_RIVE} ${TARGET_LIBRARY_DIR}
copy_library ${TARGET_LIBRARY_TESS} ${TARGET_LIBRARY_DIR}
copy_library ${TARGET_LIBRARY_TESS2} ${TARGET_LIBRARY_DIR}

rm -rf ${UNPACK_FOLDER}
rm -rf ${EARCUT_UNPACK_FOLDER}
rm -rf ${LIBTESS2_UNPACK_FOLDER}

popd
