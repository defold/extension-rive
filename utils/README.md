
# Build and desploy steps

## For the engine

This step is needed for each platform+architecture that the runtime should support.

For each platform+arch, do:

* Build the static libraries: `./utils/build_pls.sh <platform>`

You can specify variables like bob, server, sdk
Once the libraries are built, you need to also update the public repo:


## For the editor

Needed for the desktop platforms: macOS, Windows and Linux
For each platform+arch, do:

* Compile the rive-cpp library: `./utils/build_pls.sh <platform>`
* Build the editor plugin + jar file: `./utils/build_plugins.sh <platform>`

The `build_plugins.sh` can omit the platform flag if you wish to build all platforms at once.
