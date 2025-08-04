#! /usr/bin/env bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

DOWNLOAD_DIR=./build/

# https://github.com/rive-app/rive-runtime/commits/main/
RIVECPP_VERSION=18dd7545b8289983a64b2bfaed14d5933ce03d88
RIVECPP_ZIP=${DOWNLOAD_DIR}/rivecpp-${RIVECPP_VERSION}.zip
RIVECPP_URL="https://github.com/rive-app/rive-runtime/archive/${RIVECPP_VERSION}.zip"

YOGA_VERSION=2.0.1
YOGA_ZIP=${DOWNLOAD_DIR}/yoga-${YOGA_VERSION}.zip
YOGA_URL=https://github.com/rive-app/yoga/archive/refs/heads/rive_changes_v2_0_1.zip
YOGA_DOWNLOAD_DIR=${DOWNLOAD_DIR}/yoga/yoga-rive_changes_v2_0_1

YOGA_SOURCE_DIR=./defold-rive/commonsrc/yoga

RIVE_DOWNLOAD_DIR=./build/rivecpp/rive-runtime-${RIVECPP_VERSION}

RIVE_SOURCE_DIR=./defold-rive/commonsrc/rive
RIVE_INCLUDE_DIR=./defold-rive/include/rive

PLUGIN_SOURCE_DIR=./defold-rive/pluginsrc/rive

RIVE_SOURCE_DIR=./defold-rive/commonsrc/rive
RIVE_INCLUDE_DIR=./defold-rive/include/rive


if [ "" == "${BOB}" ]; then
    echo "You must specify a BOB variable"
    exit 1
fi

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

download_zip ${RIVECPP_ZIP} ${DOWNLOAD_DIR}/rivecpp ${RIVECPP_URL}
download_zip ${YOGA_ZIP} ${DOWNLOAD_DIR}/yoga ${YOGA_URL}


if [ -e ${RIVE_SOURCE_DIR} ]; then
    echo "Removing old source folder"
    rm -rf ${RIVE_SOURCE_DIR}
fi

if [ -e ${RIVE_INCLUDE_DIR} ]; then
    echo "Removing old include folder"
    rm -rf ${RIVE_INCLUDE_DIR}
    rm -rf ${RIVE_INCLUDE_DIR}/../utils
fi

##
##
## YOGA
##
##

echo "Copying YOGA source to ${YOGA_SOURCE_DIR}"
if [ -e ./defold-rive/include/yoga ]; then
    rm -rf ./defold-rive/include/yoga
fi
mkdir -p ./defold-rive/include/yoga
cp -r ${YOGA_DOWNLOAD_DIR}/yoga ./defold-rive/commonsrc/
cp -r ${YOGA_DOWNLOAD_DIR}/yoga/*.h ./defold-rive/include/yoga

##
##
## RIVE
##
##
echo "Copying Rive source to ${RIVE_SOURCE_DIR}"
cp -r ${RIVE_DOWNLOAD_DIR}/src/ ${RIVE_SOURCE_DIR}

echo "Coyping Riveheaders to ${RIVE_INCLUDE_DIR}"
mkdir -p ${RIVE_INCLUDE_DIR}/renderer/gl
mkdir -p ${RIVE_INCLUDE_DIR}/tess
mkdir -p ${RIVE_INCLUDE_DIR}/math
mkdir -p ${RIVE_INCLUDE_DIR}/../utils
cp -r ${RIVE_DOWNLOAD_DIR}/include/rive/ ${RIVE_INCLUDE_DIR}
cp -r ${RIVE_DOWNLOAD_DIR}/include/utils/ ${RIVE_INCLUDE_DIR}/../utils
cp -r ${RIVE_DOWNLOAD_DIR}/renderer/include/rive/ ${RIVE_INCLUDE_DIR}
cp -r ${RIVE_DOWNLOAD_DIR}/renderer/glad/ ${RIVE_INCLUDE_DIR}/renderer/gl
#cp -r ${RIVE_DOWNLOAD_DIR}/renderer/glad ${RIVE_INCLUDE_DIR}/tess

echo "Coyping tess source to ${PLUGIN_SOURCE_DIR}"
mkdir -p ${PLUGIN_SOURCE_DIR}/math
cp -v ${RIVE_DOWNLOAD_DIR}/tess/src/*.cpp ${PLUGIN_SOURCE_DIR}
cp -v ${RIVE_DOWNLOAD_DIR}/tess/src/math/*.cpp ${PLUGIN_SOURCE_DIR}/math


## cleanup
find ${RIVE_INCLUDE_DIR} -iname "*.c" | xargs rm

#copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/include/rive", TARGET_DIR)
#copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/renderer/include/rive", TARGET_DIR)
#copy_folder(f"./build/pls/deps/rivecpp/rive-runtime-{RIVE_RUNTIME_VERSION}/renderer/glad", os.path.join(TARGET_DIR, "renderer/gl"))
#copy_folder(f"./build/pls/rivecpp-tess/src/rive/tess", os.path.join(TARGET_DIR, "tess"))
#copy_folder(f"./build/pls/rivecpp-tess/src/rive/math", os.path.join(TARGET_DIR, "math"))

# Generate the shader .c file
#source ${SCRIPT_DIR}/gen_embedded_shaders.sh
