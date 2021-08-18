#!/usr/bin/env bash

# Run from the project foler (containing the game.project)

set -e

PROJECT=defold-rive

BOB=~/work/defold/tmp/dynamo_home/share/java/bob.jar
SERVER=https://build.defold.com
SERVER=http://localhost:9000
VARIANT=debug

TARGET_DIR=./$PROJECT/plugins
mkdir -p $TARGET_DIR

# comment out when you want to use the bob version instead!
DEFOLDSDK="--defoldsdk=afa49790f992cb8cdd8de64be0c1cb53f06a9a1a"

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
