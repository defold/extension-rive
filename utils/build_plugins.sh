#!/usr/bin/env bash

# Run from the project foler (containing the game.project)

set -e

PROJECT=defold-rive

if [ "" == "${BOB}" ]; then
    BOB=~/work/defold/tmp/dynamo_home/share/java/bob.jar

    # comment out when you want to use the bob version instead!
    DEFOLDSDK="--defoldsdk=eb061db73144081bd125b4a028a5ae9a180fc9b6"
fi

echo "Using BOB=${BOB}"

echo "Using DEFOLDSDK=${DEFOLDSDK}"

if [ "" == "${SERVER}" ]; then
    SERVER=https://build.defold.com
fi
#SERVER=http://localhost:9000

echo "Using SERVER=${SERVER}"

if [ "" == "${VARIANT}" ]; then
    VARIANT=release
fi

echo "Using VARIANT=${VARIANT}"

TARGET_DIR=./$PROJECT/plugins
mkdir -p $TARGET_DIR

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

    # Copy the .jar files
    for path in ./build/$platform_ne/$PROJECT/*.jar; do
        copyfile $path $TARGET_DIR/share
    done

    # Copy the files to the target folder
    for path in ./build/$platform_ne/$PROJECT/*.dylib; do
        copyfile $path $TARGET_DIR/lib/$platform_ne
    done

    for path in ./build/$platform_ne/$PROJECT/*.so; do
        copyfile $path $TARGET_DIR/lib/$platform_ne
    done

    for path in ./build/$platform_ne/$PROJECT/*.dll; do
        copyfile $path $TARGET_DIR/lib/$platform_ne
    done
}


function build_plugin() {
    local platform=$1
    local platform_ne=$2

    java -jar $BOB --platform=$platform build --build-artifacts=plugins --variant $VARIANT --build-server=$SERVER $DEFOLDSDK

    copy_results $platform $platform_ne
}


build_plugin "x86_64-darwin" "x86_64-osx"
build_plugin "x86_64-linux"  "x86_64-linux"
build_plugin "x86_64-win32"  "x86_64-win32"

tree $TARGET_DIR
