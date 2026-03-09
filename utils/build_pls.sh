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
        local filename=$(basename "$path")
        case "$filename" in
            RiveExt_*.lib)
                echo "Skipping ${filename}"
                continue
                ;;
        esac
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

    if [ ! -s "${zip}" ]; then
        echo "Downloading ${url} to ${zip}"
        mkdir -p $(dirname ${zip})
        rm -f "${zip}"
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
echo "Downloading libjpeg files"

LIBJPEG_VERSION=9f
LIBJPEG_ZIP=${DOWNLOAD_DIR}/libjpeg-${LIBJPEG_VERSION}.zip
LIBJPEG_URL=https://github.com/rive-app/libjpeg/archive/refs/tags/v${LIBJPEG_VERSION}.zip
LIBJPEG_ORIGINAL_DIR=${DOWNLOAD_DIR}/libjpeg/libjpeg-${LIBJPEG_VERSION}
LIBJPEG_SOURCE_DIR=${SOURCE_DIR}/libjpeg/src

download_zip ${LIBJPEG_ZIP} ${DOWNLOAD_DIR}/libjpeg ${LIBJPEG_URL}

if [ ! -d "${LIBJPEG_ORIGINAL_DIR}" ]; then
    LIBJPEG_ORIGINAL_DIR=$(find ${DOWNLOAD_DIR}/libjpeg -maxdepth 1 -type d -name "libjpeg-*" | sort | tail -n 1)
fi

if [ ! -d "${LIBJPEG_ORIGINAL_DIR}" ]; then
    echo "ERROR: Unable to locate extracted libjpeg source directory in ${DOWNLOAD_DIR}/libjpeg"
    exit 1
fi

echo "*************************************************"
echo "Downloading zlib files"

ZLIB_VERSION=1.3.1
ZLIB_ZIP=${DOWNLOAD_DIR}/zlib-${ZLIB_VERSION}.zip
ZLIB_URL=https://github.com/madler/zlib/archive/refs/tags/v${ZLIB_VERSION}.zip
ZLIB_ORIGINAL_DIR=${DOWNLOAD_DIR}/zlib/zlib-${ZLIB_VERSION}
ZLIB_SOURCE_DIR=${SOURCE_DIR}/zlib/src

download_zip ${ZLIB_ZIP} ${DOWNLOAD_DIR}/zlib ${ZLIB_URL}

if [ ! -d "${ZLIB_ORIGINAL_DIR}" ]; then
    ZLIB_ORIGINAL_DIR=$(find ${DOWNLOAD_DIR}/zlib -maxdepth 1 -type d -name "zlib-*" | sort | tail -n 1)
fi

if [ ! -d "${ZLIB_ORIGINAL_DIR}" ]; then
    echo "ERROR: Unable to locate extracted zlib source directory in ${DOWNLOAD_DIR}/zlib"
    exit 1
fi

echo "*************************************************"
echo "Downloading libpng files"

LIBPNG_VERSION=libpng16
LIBPNG_ZIP=${DOWNLOAD_DIR}/libpng-${LIBPNG_VERSION}.zip
LIBPNG_URL=https://github.com/pnggroup/libpng/archive/refs/heads/${LIBPNG_VERSION}.zip
LIBPNG_ORIGINAL_DIR=${DOWNLOAD_DIR}/libpng/libpng-${LIBPNG_VERSION}
LIBPNG_SOURCE_DIR=${SOURCE_DIR}/libpng/src

download_zip ${LIBPNG_ZIP} ${DOWNLOAD_DIR}/libpng ${LIBPNG_URL}

if [ ! -d "${LIBPNG_ORIGINAL_DIR}" ]; then
    LIBPNG_ORIGINAL_DIR=$(find ${DOWNLOAD_DIR}/libpng -maxdepth 1 -type d -name "libpng-*" | sort | tail -n 1)
fi

if [ ! -d "${LIBPNG_ORIGINAL_DIR}" ]; then
    echo "ERROR: Unable to locate extracted libpng source directory in ${DOWNLOAD_DIR}/libpng"
    exit 1
fi

echo "*************************************************"
echo "Downloading libwebp files"

LIBWEBP_VERSION=1.4.0
LIBWEBP_ZIP=${DOWNLOAD_DIR}/libwebp-${LIBWEBP_VERSION}.zip
LIBWEBP_URL=https://github.com/webmproject/libwebp/archive/refs/tags/v${LIBWEBP_VERSION}.zip
LIBWEBP_ORIGINAL_DIR=${DOWNLOAD_DIR}/libwebp/libwebp-${LIBWEBP_VERSION}
LIBWEBP_SOURCE_DIR=${SOURCE_DIR}/libwebp/src

download_zip ${LIBWEBP_ZIP} ${DOWNLOAD_DIR}/libwebp ${LIBWEBP_URL}

if [ ! -d "${LIBWEBP_ORIGINAL_DIR}" ]; then
    LIBWEBP_ORIGINAL_DIR=$(find ${DOWNLOAD_DIR}/libwebp -maxdepth 1 -type d -name "libwebp-*" | sort | tail -n 1)
fi

if [ ! -d "${LIBWEBP_ORIGINAL_DIR}" ]; then
    echo "ERROR: Unable to locate extracted libwebp source directory in ${DOWNLOAD_DIR}/libwebp"
    exit 1
fi

echo "*************************************************"
echo "Downloading miniaudio files"

MINIAUDIO_VERSION=rive_changes_5
MINIAUDIO_ZIP=${DOWNLOAD_DIR}/miniaudio-${MINIAUDIO_VERSION}.zip
MINIAUDIO_URL=https://github.com/rive-app/miniaudio/archive/refs/heads/${MINIAUDIO_VERSION}.zip
MINIAUDIO_ORIGINAL_DIR=${DOWNLOAD_DIR}/miniaudio/miniaudio-${MINIAUDIO_VERSION}
MINIAUDIO_SOURCE_DIR=${SOURCE_DIR}/miniaudio/src
RIVE_DECODERS_SOURCE_DIR=${SOURCE_DIR}/rive-decoders/src

download_zip ${MINIAUDIO_ZIP} ${DOWNLOAD_DIR}/miniaudio ${MINIAUDIO_URL}

if [ ! -d "${MINIAUDIO_ORIGINAL_DIR}" ]; then
    MINIAUDIO_ORIGINAL_DIR=$(find ${DOWNLOAD_DIR}/miniaudio -maxdepth 1 -type d -name "miniaudio-*" | sort | tail -n 1)
fi

if [ ! -d "${MINIAUDIO_ORIGINAL_DIR}" ]; then
    echo "ERROR: Unable to locate extracted miniaudio source directory in ${DOWNLOAD_DIR}/miniaudio"
    exit 1
fi

if [ -z "$RIVE_DIR" ]; then
    echo "*************************************************"
    echo "Downloading rive-cpp files"

    # https://github.com/rive-app/rive-runtime/commit/<sha>

    if [ "" == "${RIVECPP_VERSION}" ]; then
        RIVECPP_VERSION=a228887fa6032efd0e0e23af70455913dee4ac1f
    fi
    RIVECPP_ZIP=${DOWNLOAD_DIR}/rivecpp-${RIVECPP_VERSION}.zip
    RIVECPP_URL="https://github.com/rive-app/rive-runtime/archive/${RIVECPP_VERSION}.zip"

    download_zip ${RIVECPP_ZIP} ${DOWNLOAD_DIR}/rivecpp ${RIVECPP_URL}
    RIVECPP_ORIGINAL_DIR=${DOWNLOAD_DIR}/rivecpp/rive-runtime-${RIVECPP_VERSION}
else
    RIVECPP_ORIGINAL_DIR=${RIVE_DIR}
fi

echo "*************************************************"
echo "Copying libjpeg files"

rm -rf ${LIBJPEG_SOURCE_DIR}
mkdir -p ${LIBJPEG_SOURCE_DIR}
cp -v ${LIBJPEG_ORIGINAL_DIR}/*.h ${LIBJPEG_SOURCE_DIR}
cp -v ${RIVECPP_ORIGINAL_DIR}/dependencies/jconfig.h ${LIBJPEG_SOURCE_DIR}
cp -v ${RIVECPP_ORIGINAL_DIR}/dependencies/rive_libjpeg_renames.h ${LIBJPEG_SOURCE_DIR}

LIBJPEG_BUILD_SOURCES=(
    jaricom.c
    jcapimin.c
    jcapistd.c
    jcarith.c
    jccoefct.c
    jccolor.c
    jcdctmgr.c
    jchuff.c
    jcinit.c
    jcmainct.c
    jcmarker.c
    jcmaster.c
    jcomapi.c
    jcparam.c
    jcprepct.c
    jcsample.c
    jctrans.c
    jdapimin.c
    jdapistd.c
    jdarith.c
    jdatadst.c
    jdatasrc.c
    jdcoefct.c
    jdcolor.c
    jddctmgr.c
    jdhuff.c
    jdinput.c
    jdmainct.c
    jdmarker.c
    jdmaster.c
    jdmerge.c
    jdpostct.c
    jdsample.c
    jdtrans.c
    jerror.c
    jfdctflt.c
    jfdctfst.c
    jfdctint.c
    jidctflt.c
    jidctfst.c
    jidctint.c
    jquant1.c
    jquant2.c
    jutils.c
    jmemmgr.c
    jmemansi.c
)

for file in "${LIBJPEG_BUILD_SOURCES[@]}"; do
    cp -v ${LIBJPEG_ORIGINAL_DIR}/${file} ${LIBJPEG_SOURCE_DIR}

    # Defold extender doesn't accept '-include' in ext.manifest flags.
    # Inject the rename header directly into each translation unit instead.
    tmp_file="${LIBJPEG_SOURCE_DIR}/${file}.tmp"
    echo "#include \"rive_libjpeg_renames.h\"" > "${tmp_file}"
    cat "${LIBJPEG_SOURCE_DIR}/${file}" >> "${tmp_file}"
    mv "${tmp_file}" "${LIBJPEG_SOURCE_DIR}/${file}"
done

echo "*************************************************"
echo "Copying zlib files"

rm -rf ${ZLIB_SOURCE_DIR}
mkdir -p ${ZLIB_SOURCE_DIR}
cp -v ${ZLIB_ORIGINAL_DIR}/*.h ${ZLIB_SOURCE_DIR}

ZLIB_BUILD_SOURCES=(
    adler32.c
    compress.c
    crc32.c
    deflate.c
    gzclose.c
    gzlib.c
    gzread.c
    gzwrite.c
    infback.c
    inffast.c
    inftrees.c
    trees.c
    uncompr.c
    zutil.c
    inflate.c
)

for file in "${ZLIB_BUILD_SOURCES[@]}"; do
    cp -v ${ZLIB_ORIGINAL_DIR}/${file} ${ZLIB_SOURCE_DIR}
done

echo "*************************************************"
echo "Copying libpng files"

rm -rf ${LIBPNG_SOURCE_DIR}
mkdir -p ${LIBPNG_SOURCE_DIR}/arm
cp -v ${LIBPNG_ORIGINAL_DIR}/*.h ${LIBPNG_SOURCE_DIR}
cp -v ${LIBPNG_ORIGINAL_DIR}/scripts/pnglibconf.h.prebuilt ${LIBPNG_SOURCE_DIR}/pnglibconf.h
cp -v ${ZLIB_SOURCE_DIR}/zlib.h ${LIBPNG_SOURCE_DIR}
cp -v ${ZLIB_SOURCE_DIR}/zconf.h ${LIBPNG_SOURCE_DIR}
cp -v ${RIVECPP_ORIGINAL_DIR}/dependencies/rive_png_renames.h ${LIBPNG_SOURCE_DIR}

LIBPNG_BUILD_SOURCES=(
    png.c
    pngerror.c
    pngget.c
    pngmem.c
    pngpread.c
    pngread.c
    pngrio.c
    pngrtran.c
    pngrutil.c
    pngset.c
    pngtrans.c
    pngwio.c
    pngwrite.c
    pngwtran.c
    pngwutil.c
    arm/arm_init.c
    arm/filter_neon_intrinsics.c
    arm/palette_neon_intrinsics.c
)

for file in "${LIBPNG_BUILD_SOURCES[@]}"; do
    mkdir -p ${LIBPNG_SOURCE_DIR}/$(dirname ${file})
    cp -v ${LIBPNG_ORIGINAL_DIR}/${file} ${LIBPNG_SOURCE_DIR}/${file}

    # Defold extender doesn't accept '-include' in ext.manifest flags.
    # Inject the rename header directly into each translation unit instead.
    tmp_file="${LIBPNG_SOURCE_DIR}/${file}.tmp"
    echo "#include \"rive_png_renames.h\"" > "${tmp_file}"
    cat "${LIBPNG_SOURCE_DIR}/${file}" >> "${tmp_file}"
    mv "${tmp_file}" "${LIBPNG_SOURCE_DIR}/${file}"
done

echo "*************************************************"
echo "Copying libwebp files"

rm -rf ${LIBWEBP_SOURCE_DIR}
mkdir -p ${LIBWEBP_SOURCE_DIR}
cp -r -v ${LIBWEBP_ORIGINAL_DIR}/src ${LIBWEBP_SOURCE_DIR}
find ${LIBWEBP_SOURCE_DIR}/src -type f -name "*.c" -delete

# Use a config header to keep SIMD disabled for Defold extender builds where
# we cannot pass per-file target-feature flags (e.g. -mssse3/-msse4.1).
cat > ${LIBWEBP_SOURCE_DIR}/src/webp/config.h << 'EOF'
#ifndef WEBP_WEBP_CONFIG_H_
#define WEBP_WEBP_CONFIG_H_
#endif
EOF

LIBWEBP_BUILD_SOURCES=(
    src/dsp/alpha_processing.c
    src/dsp/cpu.c
    src/dsp/dec.c
    src/dsp/dec_clip_tables.c
    src/dsp/filters.c
    src/dsp/lossless.c
    src/dsp/rescaler.c
    src/dsp/upsampling.c
    src/dsp/yuv.c
    src/dsp/cost.c
    src/dsp/enc.c
    src/dsp/lossless_enc.c
    src/dsp/ssim.c
    src/dec/alpha_dec.c
    src/dec/buffer_dec.c
    src/dec/frame_dec.c
    src/dec/idec_dec.c
    src/dec/io_dec.c
    src/dec/quant_dec.c
    src/dec/tree_dec.c
    src/dec/vp8_dec.c
    src/dec/vp8l_dec.c
    src/dec/webp_dec.c
    src/dsp/alpha_processing_sse41.c
    src/dsp/dec_sse41.c
    src/dsp/lossless_sse41.c
    src/dsp/upsampling_sse41.c
    src/dsp/yuv_sse41.c
    src/dsp/alpha_processing_sse2.c
    src/dsp/dec_sse2.c
    src/dsp/filters_sse2.c
    src/dsp/lossless_sse2.c
    src/dsp/rescaler_sse2.c
    src/dsp/upsampling_sse2.c
    src/dsp/yuv_sse2.c
    src/dsp/alpha_processing_neon.c
    src/dsp/dec_neon.c
    src/dsp/filters_neon.c
    src/dsp/lossless_neon.c
    src/dsp/rescaler_neon.c
    src/dsp/upsampling_neon.c
    src/dsp/yuv_neon.c
    src/dsp/dec_msa.c
    src/dsp/filters_msa.c
    src/dsp/lossless_msa.c
    src/dsp/rescaler_msa.c
    src/dsp/upsampling_msa.c
    src/dsp/dec_mips32.c
    src/dsp/rescaler_mips32.c
    src/dsp/yuv_mips32.c
    src/dsp/alpha_processing_mips_dsp_r2.c
    src/dsp/dec_mips_dsp_r2.c
    src/dsp/filters_mips_dsp_r2.c
    src/dsp/lossless_mips_dsp_r2.c
    src/dsp/rescaler_mips_dsp_r2.c
    src/dsp/upsampling_mips_dsp_r2.c
    src/dsp/yuv_mips_dsp_r2.c
    src/dsp/cost_sse2.c
    src/dsp/enc_sse2.c
    src/dsp/lossless_enc_sse2.c
    src/dsp/ssim_sse2.c
    src/dsp/enc_sse41.c
    src/dsp/lossless_enc_sse41.c
    src/dsp/cost_neon.c
    src/dsp/enc_neon.c
    src/dsp/lossless_enc_neon.c
    src/dsp/enc_msa.c
    src/dsp/lossless_enc_msa.c
    src/dsp/cost_mips32.c
    src/dsp/enc_mips32.c
    src/dsp/lossless_enc_mips32.c
    src/dsp/cost_mips_dsp_r2.c
    src/dsp/enc_mips_dsp_r2.c
    src/dsp/lossless_enc_mips_dsp_r2.c
    src/utils/bit_reader_utils.c
    src/utils/color_cache_utils.c
    src/utils/filters_utils.c
    src/utils/huffman_utils.c
    src/utils/palette.c
    src/utils/quant_levels_dec_utils.c
    src/utils/rescaler_utils.c
    src/utils/random_utils.c
    src/utils/thread_utils.c
    src/utils/utils.c
    src/utils/bit_writer_utils.c
    src/utils/huffman_encode_utils.c
    src/utils/quant_levels_utils.c
    src/demux/anim_decode.c
    src/demux/demux.c
)

for file in "${LIBWEBP_BUILD_SOURCES[@]}"; do
    cp -v ${LIBWEBP_ORIGINAL_DIR}/${file} ${LIBWEBP_SOURCE_DIR}/${file}
done

echo "*************************************************"
echo "Copying miniaudio files"

rm -rf ${MINIAUDIO_SOURCE_DIR}
mkdir -p ${MINIAUDIO_SOURCE_DIR}
cp -v ${MINIAUDIO_ORIGINAL_DIR}/miniaudio.h ${MINIAUDIO_SOURCE_DIR}
cp -v ${MINIAUDIO_ORIGINAL_DIR}/miniaudio.c ${MINIAUDIO_SOURCE_DIR}

echo "*************************************************"
echo "Copying rive_decoders files"

rm -rf ${RIVE_DECODERS_SOURCE_DIR}
mkdir -p ${RIVE_DECODERS_SOURCE_DIR}/include/rive
mkdir -p ${RIVE_DECODERS_SOURCE_DIR}/include/utils
mkdir -p ${RIVE_DECODERS_SOURCE_DIR}/src
mkdir -p ${RIVE_DECODERS_SOURCE_DIR}/webp

cp -r -v ${RIVECPP_ORIGINAL_DIR}/include/rive/* ${RIVE_DECODERS_SOURCE_DIR}/include/rive
cp -r -v ${RIVECPP_ORIGINAL_DIR}/include/utils/* ${RIVE_DECODERS_SOURCE_DIR}/include/utils
cp -r -v ${RIVECPP_ORIGINAL_DIR}/decoders/include/rive/decoders ${RIVE_DECODERS_SOURCE_DIR}/include/rive/
cp -v ${RIVECPP_ORIGINAL_DIR}/decoders/src/*.cpp ${RIVE_DECODERS_SOURCE_DIR}/src

# Pull third-party decoder headers into this package so the native extension
# build has everything it needs locally.
cp -v ${LIBJPEG_SOURCE_DIR}/*.h ${RIVE_DECODERS_SOURCE_DIR}
cp -v ${LIBPNG_SOURCE_DIR}/*.h ${RIVE_DECODERS_SOURCE_DIR}
cp -v ${LIBWEBP_SOURCE_DIR}/src/webp/*.h ${RIVE_DECODERS_SOURCE_DIR}/webp

# Keep symbol names consistent with our renamed libjpeg/libpng builds.
for file in decode_jpeg.cpp decode_png.cpp; do
    tmp_file="${RIVE_DECODERS_SOURCE_DIR}/src/${file}.tmp"
    echo "#include \"rive_png_renames.h\"" > "${tmp_file}"
    echo "#include \"rive_libjpeg_renames.h\"" >> "${tmp_file}"
    cat "${RIVE_DECODERS_SOURCE_DIR}/src/${file}" >> "${tmp_file}"
    mv "${tmp_file}" "${RIVE_DECODERS_SOURCE_DIR}/src/${file}"
done

RIVECPP_SOURCE_DIR=${SOURCE_DIR}/rivecpp/src
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
mkdir -p ${RIVECPP_HARFBUZZ_INCLUDE_DIR}
RIVECPP_HARFBUZZ_INCLUDE_DIR=$(realpath $RIVECPP_HARFBUZZ_INCLUDE_DIR)

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
cp -r -v ${RIVECPP_ORIGINAL_DIR}/decoders/include/rive/decoders ${RIVECPP_RENDERER_SOURCE_DIR}/include/rive/
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
LIBPLY_SOURCE_DIR=${SOURCE_DIR}/libply/src

download_zip ${LIBPLY_ZIP} ${DOWNLOAD_DIR}/libply ${LIBPLY_URL}

LIBPLY_PATH=${DOWNLOAD_DIR}/libply/ply-${LIBPLY_VERSION}/src
if [ ! -d "${LIBPLY_PATH}" ]; then
    # Some archives may extract with a different top-level directory name.
    LIBPLY_PATH=${DOWNLOAD_DIR}/libply/libply-${LIBPLY_VERSION}/src
fi

if [ ! -d "${LIBPLY_PATH}" ]; then
    echo "Error: Unable to locate extracted PLY source directory."
    echo "Expected one of:"
    echo "  ${DOWNLOAD_DIR}/libply/ply-${LIBPLY_VERSION}/src"
    echo "  ${DOWNLOAD_DIR}/libply/libply-${LIBPLY_VERSION}/src"
    exit 1
fi

LIBPLY_PATH=$(realpath "${LIBPLY_PATH}")

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
    echo "LIBJPEG ${platform}"
    echo "************************************************************"

    export INCLUDES="upload/src"
    export CXXFLAGS="-x c"
    unset DEFINES
    build_library libjpeg $platform $platform_ne ${LIBJPEG_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "ZLIB ${platform}"
    echo "************************************************************"

    export INCLUDES="upload/src"
    export CXXFLAGS="-x c"
    export DEFINES="ZLIB_IMPLEMENTATION"
    case ${platform} in
        x86_64-win32|x86-win32)
            ;;
        *)
            export DEFINES="${DEFINES} HAVE_UNISTD_H"
            ;;
    esac
    build_library zlib $platform $platform_ne ${ZLIB_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "LIBPNG ${platform}"
    echo "************************************************************"

    export INCLUDES="upload/src upload/src/arm"
    export CXXFLAGS="-x c"
    unset DEFINES
    build_library libpng $platform $platform_ne ${LIBPNG_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "LIBWEBP ${platform}"
    echo "************************************************************"

    export INCLUDES="upload/src"
    export CXXFLAGS="-x c"
    export DEFINES="HAVE_CONFIG_H"
    build_library libwebp $platform $platform_ne ${LIBWEBP_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "MINIAUDIO ${platform}"
    echo "************************************************************"

    export INCLUDES="upload/src"
    unset DEFINES
    case ${platform} in
        x86_64-ios|arm64-ios)
            echo "Skipping miniaudio for ${platform} (Objective-C source path not wired in build_pls.sh yet)"
            clear_build_options
            ;;
        *)
            export CXXFLAGS="-x c"
            build_library miniaudio $platform $platform_ne ${MINIAUDIO_SOURCE_DIR} ${BUILD}
            ;;
    esac

    echo "************************************************************"
    echo "RIVE CPP ${platform}"
    echo "************************************************************"

    export CXXFLAGS="-std=c++17 -fno-rtti -fno-exceptions"
    export DEFINES="WITH_RIVE_TEXT WITH_RIVE_LAYOUT _RIVE_INTERNAL_ YOGA_EXPORT="
    unset INCLUDES
    build_library rive $platform $platform_ne ${RIVECPP_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "RIVE_DECODERS ${platform}"
    echo "************************************************************"

    export INCLUDES="upload/src/include upload/src"
    export CXXFLAGS="-std=c++17 -fno-rtti -fno-exceptions"
    export DEFINES="RIVE_PNG RIVE_JPEG RIVE_WEBP"
    build_library rive_decoders $platform $platform_ne ${RIVE_DECODERS_SOURCE_DIR} ${BUILD}

    echo "************************************************************"
    echo "RIVE Renderer ${platform}"
    echo "************************************************************"

    RIVE_RENDERER_DEFINES="RIVE_DECODERS"
    RIVE_RENDERER_CXXFLAGS=
    RIVE_RENDERER_INCLUDES="upload/src"

    # Due to a self include bug in rive_render_path.hpp, it references itself
    # We workaround it by adding a copy in the relative path it asks for "../renderer/src/rive_render_path.hpp"
    mkdir -p ${RIVECPP_RENDERER_SOURCE_DIR}/renderer/src/
    echo "// intentionally left empty due to the self include issue" > ${RIVECPP_RENDERER_SOURCE_DIR}/renderer/src/rive_render_path.hpp

    case ${platform} in
        armv7-android|arm64-android)
            RIVE_RENDERER_DEFINES="${RIVE_RENDERER_DEFINES} RIVE_ANDROID"

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

            cp -v -r ${RIVECPP_ORIGINAL_DIR}/renderer/glad/include/                      ${RIVECPP_RENDERER_SOURCE_DIR}/include
            ;;
        x86_64-win32|x86-win32)
            RIVE_RENDERER_DEFINES="${RIVE_RENDERER_DEFINES} RIVE_DESKTOP_GL RIVE_WINDOWS"
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
            cp -v -r ${RIVECPP_ORIGINAL_DIR}/renderer/glad/include/                      ${RIVECPP_RENDERER_SOURCE_DIR}
            ;;

        arm64-linux|x86_64-linux)
            RIVE_RENDERER_DEFINES="${RIVE_RENDERER_DEFINES} RIVE_DESKTOP_GL RIVE_LINUX"
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
            RIVE_RENDERER_DEFINES="${RIVE_RENDERER_DEFINES} RIVE_DESKTOP_GL RIVE_MACOSX"
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

            RIVE_RENDERER_DEFINES="${RIVE_RENDERER_DEFINES} RIVE_IOS"
            if [ "${platform}" == "x86_64-ios" ]; then
                RIVE_RENDERER_DEFINES="${RIVE_RENDERER_DEFINES} RIVE_IOS_SIMULATOR"
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
            RIVE_RENDERER_DEFINES="${RIVE_RENDERER_DEFINES} RIVE_WEBGL RIVE_WEBGPU=1"

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

            cp -v -r ${RIVECPP_ORIGINAL_DIR}/renderer/glad/include/                  ${RIVECPP_RENDERER_SOURCE_DIR}/include

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
            RIVE_RENDERER_DEFINES="${RIVE_RENDERER_DEFINES} RIVE_WEBGL RIVE_WEBGPU=2 RIVE_WAGYU"
            export INCLUDES="upload/src ${RIVE_RENDERER_INCLUDES}"

            if [ "${RIVE_RENDERER_DEFINES}" != "" ]; then
                export DEFINES="${RIVE_RENDERER_DEFINES}"
            fi

            rm ${RIVECPP_RENDERER_SOURCE_DIR}/src/webgpu/webgpu_compat.h
            cp -v ${RIVECPP_ORIGINAL_DIR}/renderer/src/webgpu/wagyu-port/new/include/webgpu/*.* ${RIVECPP_RENDERER_SOURCE_DIR}/include/webgpu/
            build_library rive_renderer_wagyu $platform $platform_ne ${RIVECPP_RENDERER_SOURCE_DIR} ${BUILD}
    esac

done


echo "************************************************************"
echo "DONE"
echo "************************************************************"
