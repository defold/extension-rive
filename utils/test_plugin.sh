#!/usr/bin/env bash

set -e

LIBNAME=RiveExt
CLASS_NAME=com.dynamo.bob.pipeline.Rive
JAR=./defold-rive/plugins/share/plugin${LIBNAME}.jar

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
pushd $SCRIPT_DIR/..

set -e

PLUGIN_PLATFORM_DIR=$(realpath ./defold-rive/plugins/lib/x86_64-linux)
if [ "Darwin" == "$(uname)" ]; then
    PLUGIN_PLATFORM_DIR=$(realpath ./defold-rive/plugins/lib/x86_64-osx)
fi

if [ -z "${JAR}" ]; then
    echo "Couldn't find the jar file!"
fi

if [ "" == "${BOB}" ]; then
    BOB=~/work/defold/tmp/dynamo_home/share/java/bob.jar
fi

echo "BOB=${BOB}"
echo "JAR=${JAR}"
echo "CLASS_NAME=${CLASS_NAME}"
echo "LIBNAME=${LIBNAME}"
#echo "PLUGIN_PLATFORM_DIR=${PLUGIN_PLATFORM_DIR}"

JNI_DEBUG_FLAGS="-Xcheck:jni"

export DM_RIVE_LOG_LEVEL=DEBUG

java ${JNI_DEBUG_FLAGS} -Djava.library.path=${PLUGIN_PLATFORM_DIR} -Djni.library.path=${PLUGIN_PLATFORM_DIR} -cp ${JAR} ${CLASS_NAME} $*
