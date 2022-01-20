# How to build the libraries

## Editor extension

The Editor and Bob pipelines use shared libraries and a .jar file.
To build these, use the `./utils/build_plugins.sh`.    (TODO: make BOB/BUILDSERVER more generic )

### Testing the plugin

To test the plugin, use the `./utils/test_plugin.sh <.riv file>`
This will run the Rive.java main function, using the C++ plugins as implementation.

## rive-cpp

To avoid rebuilding the library all the time, we prebuild static libraries.

Generally, any platform supporting `clang++` (10+) will work.

We have the script `./utils/rive-cpp/build_rive_cpp.sh` to help with this.

In this script we setup the system root for the compiler for each platform.
If you have `$DYNAMO_HOME` set, it will use that folder as the base path to search for SDK's for each platform.

Each build will automatically copy the library to the correct place within the `./defold-rive/lib` folder.

### General

> ./utils/rive-cpp/build_rive_cpp.sh <platform>

Where platform is the 2-tuple "arch-os":
* js-web, wasm-web
* x86-win32, x86_64-win32
* x86_64-osx
* x86_64-linux
* armv7-android, arm64-android
* arm64-ios

### Linux

We use the linux (Ubuntu 20) docker container in Defold repo (you have to build it first ofc, using build.sh).

Start a linux command prompt (with clang++ available)

> <defold>/scripts/docker/run.sh

Build rive-cpp
> ./utils/rive-cpp/build_rive_cpp.sh x86_64-linux


### Win32

It's relatively easy to cross compile for windows.
You need to have a clang++ (10´) installed. Generally it doesn't work well with Apple Clang.

> PATH=$(brew --prefix llvm)/bin:${PATH} ./utils/rive-cpp/build_rive_cpp.sh x86-win32
> PATH=$(brew --prefix llvm)/bin:${PATH} ./utils/rive-cpp/build_rive_cpp.sh x86_64-win32

NOTE: Currently, the resulting library may not work 100% with the pipeline.
You may need to build this using the instructions found in the rive-cpp repository.

It's possible to generate a Visual Studio solution by using `premake5.exe vs2019`.
Note however that it only contains a Win32 build, so first thing needed is to duplicate that config and modify the copy to produce x64 content.
Make sure the configs have "Multi-threaded (/MT)" set under project properties: "C/C++ -> Code Generation -> Runtime Library: Multi-threaded (/MT)"
