#!/usr/bin/env bash

# Run from the project folder (containing the game.project)

set -e

PROJECT=defold-rive
#BOB=bob.jar
DEFAULT_SERVER=https://build-stage.defold.com

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
#SERVER=http://localhost:9000

echo "Using SERVER=${SERVER}"

if [ "" == "${VARIANT}" ]; then
    VARIANT=headless
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

    java -jar $BOB --platform=$platform resolve build --build-artifacts=plugins --variant $VARIANT --build-server=$SERVER --defoldsdk=$DEFOLDSDK

    copy_results $platform $platform_ne
}


function delete_file() {
    local file=$1
    if [ -e "$file" ]; then
        echo "Cleaning $file"
        rm $file
    fi
}

function delete_dir() {
    local directory=$1
    if [ -d "$directory" ]; then
        echo "Cleaning $directory"
        rm -rf $directory
    fi
}

function clean_plugin() {
    local platform=$1
    local platform_ne=$2

    delete_dir ./build/plugins/${PROJECT}/plugins/share
    delete_dir ./build/plugins/${PROJECT}/plugins/lib/${platform_ne}

    for path in $TARGET_DIR/share/*.jar; do
        delete_file $path
    done

    for path in $TARGET_DIR/lib/$platform_ne/*.dylib; do
        delete_file $path
    done

    for path in $TARGET_DIR/lib/$platform_ne/*.so; do
        delete_file $path
    done

    for path in $TARGET_DIR/lib/$platform_ne/*.dll; do
        delete_file $path
    done
}

PLATFORMS=$1
if [ "" == "${PLATFORMS}" ]; then
    PLATFORMS="x86_64-macos arm64-macos x86_64-linux x86_64-win32"
fi

echo "Building ${PLATFORMS}"

if [[ $# -gt 0 ]] ; then
    PLATFORMS="$*"
fi

for platform in $PLATFORMS; do

    platform_ne=$platform

    if [ "$platform" == "x86_64-macos" ]; then
        platform_ne="x86_64-osx"
    fi

    if [ "$platform" == "arm64-macos" ]; then
        platform_ne="arm64-osx"
    fi

    clean_plugin $platform $platform_ne
    build_plugin $platform $platform_ne
done

if command -v tree >/dev/null 2>&1; then
    # The tree command is available. Use it.
    tree $TARGET_DIR
else
    # We don't have tree. Approximate its output using find and sed.
    find $TARGET_DIR -print | sed -e "s;[^/]*/;|-- ;g;s;-- |;   |;g"
fi
