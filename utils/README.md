
# Build and desploy steps

## For the engine

This step is needed for each platform+architecture that the runtime should support.

First, clean out any previous code that may be left since last update.
You only need to do this once for each version update.

    rm -rf ./build

### Build the rive runtime

the `build_pls.sh` script copies the files needed for each library, and sends them to our build server, in order to make sure they're built with the SDK's we need.

For each platform+arch, do:

    `./utils/build_pls.sh <platform>`

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

     `DEFOLDSDK=<defold sha1> SERVER=<server> ./utils/build_pls.sh <platform>`

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


