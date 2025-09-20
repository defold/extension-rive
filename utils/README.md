
# Build and desploy steps

*UPDATE 2025-09-20*

For platforms `arm64-android|armv7-android` we now use the build system of the rive runtime.

The build scripts do several things:
* Remove old headers
* Copy new headers
* Generate `rive_version.h`
* Remove old libraries
* Build and copy new libraries

**NOTE** The build steps currently apply a small patch to the rive-runtiome in order to successfully build for all targets! See [rive.patch](./utils/rive.patch) for the details.

## Clone the rive runtime repo

* Clone [rive-runtime](https://github.com/rive-app/rive-runtime) to a local folder `path/to/rive`

### Android

* Make sure you have ANDROID_NDK version `25.2.9519653` installed
* Make sure the ANDROID_NDK environment is set: `ANDROID_NDK=~/Library/Android/sdk/ndk/25.2.9519653`

Build for arm64-android/armv7-android:

> ANDROID_NDK=~/Library/Android/sdk/ndk/25.2.9519653 ./utils/build_rive_runtime.sh arm64-android ~/work/external/rive-runtime


## For the engine (old build path)

This step is needed for each platform+architecture that the runtime should support.

First, clean out any previous code that may be left since last update.
You only need to do this once for each version update.

    rm -rf ./build

### Build the rive runtime

the `build_pls.sh` script copies the files needed for each library, and sends them to our build server, in order to make sure they're built with the SDK's we need.

For each platform+arch, do:

    ./utils/build_pls.sh <platform>

This updates the folder `./defold-rive/lib<platform>`

The currently supported platforms:

* arm64-ios
* x86_64-ios
* arm64-macos
* x86_64-macos
* arm64-android
* arm64-linux
* x86_64-linux
* x86_64-win32
* x86-win32
* js-web
* wasm-web
* wasm_pthread-web

For the more advanced use cases, there are variables that can be modified:

     DEFOLDSDK=<defold sha1> SERVER=<server> ./utils/build_pls.sh <platform>

You can specify variables: BOB, SERVER, DEFOLDSDK.

### Update the Rive headers

Once you've successfully built the libraries (for at least one platform), we also need to copy the headers.
Note that it may remove or add headers in the `defold-rive/include/rive` folder. This often happens in their api.

    > ./utils/copy_rive_headers.py


## For the editor

This step is only needed if you've altered some particular extension code itself, namely the pluginsrc/ and commonsrc/ folders.

For a regular rive-runtime update, this step isn't needed.

Needed for the desktop platforms: macOS, Windows and Linux
For each platform+arch, do:

* Compile the rive-cpp library: `./utils/build_pls.sh <platform>`
* Build the editor plugin + jar file: `./utils/build_plugins.sh <platform>`

The `build_plugins.sh` can omit the platform flag if you wish to build all platforms at once.

You can also test the plugins locally:

    ./utils/test_plugin.sh path/to/scene.riv



