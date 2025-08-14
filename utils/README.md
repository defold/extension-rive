
# Build and desploy steps

## For the engine

This step is needed for each platform+architecture that the runtime should support.

First, clean out any previous code that may be left since last update:

    rm -rf ./build

For each platform+arch, do:

* Build the static libraries: `DEFOLDSDK=<defold sha1> SERVER=<server> ./utils/build_pls.sh <platform>`

You can specify variables like bob, server, defoldsdk.

### Update the Rive headers

Once you've successfully built the libraries (for at least one platform), we also need to copy the headers.
It may remove or add headers in the `defold-rive/include/rive` folder.

    > ./utils/copy_rive_headers.py

## For the editor

Needed for the desktop platforms: macOS, Windows and Linux
For each platform+arch, do:

* Compile the rive-cpp library: `./utils/build_pls.sh <platform>`
* Build the editor plugin + jar file: `./utils/build_plugins.sh <platform>`

The `build_plugins.sh` can omit the platform flag if you wish to build all platforms at once.

You can also test the plugins locally:

    ./utils/test_plugin.sh path/to/scene.riv
