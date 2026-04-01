# Editor plugin

## Build

* Set DYNAMO_HOME to the Defold SDK folder
* Build the editor plugin with (replace the platform accordingly):

```
$ cd path-to-extension-rive
extension-rive$ PROTOC=${DYNAMO_HOME}/ext/bin/arm64-macos/protoc BOB=${DYNAMO_HOME}/share/java/bob.jar ./utils/plugin/build.sh arm64-macos
```

## Test offline

To test offline, you can use:

```bash
./utils/plugin/test.sh ./main/grimley/4951-10031-grimley.riv
```

## Test in editor

Assuming you've updated the plugin in the plugins folder, the editor will find them and load them upon next editor start:

* ./defold-rive/plugins/share/pluginRiveExt.jar
* ./defold-rive/plugins/lib/<platform>/libRiveExt.*
