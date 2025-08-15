NOTES

Notes to make it as easy to rebuild/update your libraries.

Currently our rive runtime libraries/headers are pre built using sha `3f6e5319f6998d54cd756faf5f82e4e05e4f23b9`

To avoid using our whole


* Remove the Defold rive runtime from the extension-rive

E.g. for arm64-osx.

    rm defold-rive/lib/arm64-osx/librive.a
    rm defold-rive/lib/arm64-osx/librive_renderer.a

Copy your locally build runtime:

    cp -v ~/work/external/rive-runtime/renderer/out/release/librive*.a defold-rive/lib/arm64-osx/



Similar for wasm-web:

    rm defold-rive/lib/wasm-web/librive.a
    rm defold-rive/lib/wasm-web/librive_renderer.a
    rm defold-rive/lib/wasm-web/libyoga.a

Build your runtime locally:

    cd rive-runtime
    git checkout 3f6e5319f6998d54cd756faf5f82e4e05e4f23b9

    cd ./renderer
    build_rive.sh release wasm --with-webgpu --with_wagyu


Copy the libraries to the extension:

    cp -v ~/work/external/rive-runtime/renderer/out/wasm_release/librive*.a defold-rive/lib/wasm-web/


# testing the app

In all the following steps, each step is explained in the instructions (see the shared Google Drive document)

## Bundle the app

Follow the steps in the instructions (shared privately)
and export for wasm-web

It sends the build to our build server, and afterwards it'll open up the exported folder.

## Prepare the bundle for NRDP device

Follow the steps in the instructions (shared privately)
and copy the defold.js to wasm-web/extension-rive

## Run the game on device

Serve the content:

    cd extension-rive
    (cd wasm-web/extension-rive && python -m http.server)



# iterate testing

* Rebuild your local runtime
* copy the build libraries to the corresponding library folder (e.g. defold-rive/lib/wasm-web)
* Bundle the app again
* Copy defold.js to the bundle folder
* Launch the app on device











