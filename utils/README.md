# Utilities

This folder is split into a few categories:

## Rive Runtime

Tools and docs for building and updating the prebuilt Rive runtime libraries used by the extension.

- Build wrapper: `./utils/rive-runtime/build.sh`
- Patch and helper scripts: `./utils/rive-runtime/`
- Documentation: [utils/rive-runtime/README.md](/Users/mathiaswesterdahl/work/projects/users/defold/extension-rive/utils/rive-runtime/README.md)

## Plugin

Tools and sources for the editor plugin build.

- Build wrapper: `./utils/plugin/build.sh <platform>`
- Test helper: `./utils/plugin/test.sh <path-to-scene.riv>`
- Sources and CMake config: `./utils/plugin/`
- Documentation: [utils/plugin/README.md](/Users/mathiaswesterdahl/work/projects/users/defold/extension-rive/utils/plugin/README.md)

## Viewer

Standalone viewer app for testing runtime rendering and backend integration.

- Build wrapper: `./utils/viewer/build.sh <platform>`
- Sources and CMake config: `./utils/viewer/`
- Documentation: [utils/viewer/README.md](/Users/mathiaswesterdahl/work/projects/users/defold/extension-rive/utils/viewer/README.md)

## Script API Update

Utility for regenerating script API related data:

- `./utils/update_script_api.py`
