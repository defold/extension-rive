---
title: Rive documentation
brief: This manual describes how to show Rive animations using the Rive extension.
---

# Rive animation
[Rive](https://rive.app/) is a real-time interactive design and animation tool by Rive Inc.. Use the Rive editor to create vector based motion graphics that respond to different states and user inputs. Rive lets you create advanced timeline animations through animation mixing, interpolation and inverse-kinematics. Transition between animations using state machines.

![Rive editor](rive-editor.png)


## Requirements
In order to use Rive animations you need to run Defold 1.2.188 or higher.


## Installation
Rive animation support in Defold is provided through an official Rive extension. To use Rive animations in a Defold project, add the following URL to the list of `game.project` dependencies:

[https://github.com/defold/extension-rive/archive/main.zip](https://github.com/defold/extension-rive/archive/main.zip)

We recommend using a link to a zip file of a [specific release](https://github.com/defold/extension-rive/releases).


## Render script setup
Rive components are rendered using a method called "Stencil, then cover" (StC). This requires Rive components to render with the stencil buffer enabled in the render script:

```
render.enable_state(render.STATE_STENCIL_TEST)
render.draw(self.rive_pred)
render.disable_state(render.STATE_STENCIL_TEST)
```

For convenience, there is a modified render script included with this extension. Open your *game.project* file and modify the `Render` field in the `Bootstrap` section to use the `defold-rive/lua/rive.render` file from this extension.


## Concepts
*Rive data file*
: This file contains a Rive artboard with all of the composition and animation data of a Rive scene. You can export a Rive (.riv) file from the Rive editor.

*Rive scene*
: This file is used to create a reference to a Rive data file (.riv).

*Rive model*
: This file represents a *Rive Model* component that can be added to a game object. It references a *Rive Scene*, blend mode, material and the default state machine and animation to use.


## Creating a Rive scene
Create a Rive scene by (<kbd>right click</kbd> a location in the *Assets* browser, then select <kbd>New... ▸ Rive Scene</kbd> from the context menu). Select the Rive data file to use from the Rive File field in the *Properties* panel.

![Rive scene](rive-scene.png)

Once a Rive file has been selected a preview will be shown in the main *Editor* scene view and the bone hierarchy (see below) will be shown in the *Outline* panel.

::: important
If your Rive scene contains multiple [Artboards](https://help.rive.app/editor/fundamentals/artboards) the Rive integration in Defold will automatically select the Main Artboard from your Rive scene.
:::

## Creating Rive model components
Select a game object to hold the new component:

Either create the component in-place (<kbd>right click</kbd> the game object and select <kbd>Add Component ▸ Rive Model</kbd>)

Or create it on file first (<kbd>right click</kbd> a location in the *Assets* browser, then select <kbd>New... ▸ Rive Model</kbd> from the context menu), then add the file to the game object by <kbd>right clicking</kbd> the game object and selecting <kbd>Add Component File</kbd>).

The Rive model can now be viewed in the editor:

![Rive model component](rive-model.png)


## Rive model properties
Apart from the properties *Id*, *Position* and *Rotation* the following component specific properties exist:

*Rive Scene*
: Set this to the Rive scene file to use for this model.

*Blend Mode*
: If you want a blend mode other than the default `Alpha`, change this property.

*Material*
: If you need to render the model with a custom material, change this property.

*Default State Machine*
: Set this to the state machine you want the model to start with.

*Default Animation*
: Set this to the animation you want the model to start with.


### Blend modes
:[blend-modes](../shared/blend-modes.md)


## Runtime manipulation
*Rive Model* components can be manipulated at runtime through a number of different functions and properties (refer to the [API docs for usage](/extension-rive/rive_api/)).


### Playing animations
To play animations on a *Rive Model* component, simply call the [`rive.play_anim()`](/extension-rive/rive_api/#rive.play_anim) function:

```lua
function init(self)
	-- Play the "run" animation on the component "rivemodel"
	local options = {
		offset = 0.2, -- start 20% into the animation
		playback_rate = 1.5, -- play the animation at 150% speed
	}
	rive.play_anim("#rivemodel", "run", go.PLAYBACK_ONCE_FORWARD, options, function(self, message_id, message, sender)
		run()
	end)

end
```


### Cursor animation
In addition to using the `rive.play_anim()` to advance a Rive animation, *Rive Model* components expose a "cursor" property that can be manipulated with `go.animate()` (more about [property animations](/manuals/property-animation)):

```lua
-- Set the animation on the spine model but don't run it.
rive.play_anim("#rivemodel", "run", go.PLAYBACK_NONE)

-- Set the cursor to position 0
go.set("#rivemodel", "cursor", 0)

-- Tween the cursor slowly between 0 and 1 pingpong with in-out quad easing.
go.animate("#rivemodel", "cursor", go.PLAYBACK_LOOP_PINGPONG, 1, go.EASING_INOUTQUAD, 6)
```


### Changing properties

A *Rive Model* component also has a number of different properties that can be manipulated using `go.get()` and `go.set()`:

`animation`
: The current model animation (`hash`) (READ ONLY). You change animation using `rive.play_anim()`.

`cursor`
: The normalized animation cursor (`number`).

`material`
: The rive model material (`hash`). You can change this using a material resource property and `go.set()`. Refer to the [API reference for an example](/extension-rive/rive_api/#material).

`playback_rate`
: The animation playback rate (`number`).


### Interacting with state machines
To interact with a state machine in a *Rive Model* component it first needs to be started using [`rive.play_state_machine()`](/extension-rive/rive_api/#rive.play_state_machine). Once it has been started it can be interacted with using [`go.set()`](/ref/go#go.set):

```lua
-- Start the state machine named "State Machine 1"
rive.play_state_machine("#rivemodel", "State Machine 1")

-- Set the boolean value "Trigger 1" to true
go.set("#rivemodel", "Trigger 1", true)

-- Set the numeric value "Number 1" to 0.8
go.set("#rivemodel", "Number 1", 0.8)
```


### Bone hierarchy
The individual bones in the *Rive Scene* skeleton are represented internally as game objects. In the *Outline* view of the *Rive Scene* the full hierarchy is visible.

![Rive model hierarchy](rive-bones.png)

With the bone name at hand, it is possible to retrieve the instance id of the bone in runtime. The function [`rive.get_go()`](/extension-rive/rive_api/#rive.get_go) returns the id of the specified bone and it is, for instance, possible to child other game objects under the animated game object:

```lua
-- Attach pistol game object to the left forearm
local forearm = rive.get_go("#rivemodel", "Left forearm")
msg.post("pistol", "set_parent", { parent_id = forearm })
```

## Source code
The source code is available on [GitHub](https://github.com/defold/extension-rive)


## API reference
