-- Auto generated from utils/update_script_api.py
-- WARNING: Do not edit manually.

--[[
Rive API documentation
Functions and constants for interacting with Rive models
--]]

---@meta
---@diagnostic disable: lowercase-global
---@diagnostic disable: missing-return
---@diagnostic disable: duplicate-doc-param
---@diagnostic disable: duplicate-set-field
---@diagnostic disable: args-after-dots

---@alias ArtboardHandle integer
---@alias AudioSourceHandle integer
---@alias FileHandle integer
---@alias FontHandle integer
---@alias RenderImageHandle integer
---@alias StateMachineHandle integer
---@alias ViewModelInstanceHandle integer

--- @class rive
rive = {}

--- Lua wrapper for pointer movement.
---@param url url Component receiving the pointer move.
---@param x number Pointer x coordinate in component space.
---@param y number Pointer y coordinate in component space.
function rive.pointer_move(url, x, y) end

--- Lua wrapper for pointer up events.
---@param url url Component receiving the pointer release.
---@param x number Pointer x coordinate.
---@param y number Pointer y coordinate.
function rive.pointer_up(url, x, y) end

--- Lua wrapper for pointer down events.
---@param url url Component receiving the pointer press.
---@param x number Pointer x coordinate.
---@param y number Pointer y coordinate.
function rive.pointer_down(url, x, y) end

--- Lua wrapper for pointer exit events.
---@param url url Component receiving the pointer leave.
---@param x number Pointer x coordinate.
---@param y number Pointer y coordinate.
function rive.pointer_exit(url, x, y) end

--- Returns the projection matrix in render coordinates.
---@return vmath.matrix4 matrix Current projection matrix for the window.
function rive.get_projection_matrix() end

--- Sets or clears the global file listener callback.
---@param callback? fun(self, event, data) Callback invoked for file system events; pass nil to disable.
---@param callback_self object The calling script instance.
---@param callback_event string One of: onFileLoaded, onFileDeleted, onFileError, onArtboardsListed, onViewModelsListed, onViewModelInstanceNamesListed, onViewModelPropertiesListed, onViewModelEnumsListed
---@param callback_data table Additional fields vary by event. Common keys include:
---@param callback_data_file FileHandle File handle involved in the event.
---@param callback_data_viewModelName string View model name for the request, when applicable.
---@param callback_data_instanceNames table Array of instance name strings.
---@param callback_data_artboardNames table Array of artboard name strings.
---@param callback_data_properties table Array of property metadata tables.
---@param callback_data_enums table Array of enum definitions.
---@param callback_data_error string Error message when a failure occurs.
function rive.set_file_listener(callback) end

--- Sets or clears the artboard listener callback.
---@param callback? fun(self, event, data) Callback invoked for artboard-related events; pass nil to disable.
---@param callback_self object The calling script instance.
---@param callback_event string One of: onArtboardError, onDefaultViewModelInfoReceived, onArtboardDeleted, onStateMachinesListed
---@param callback_data table Additional data per event, typically:
---@param callback_data_artboard ArtboardHandle Artboard handle involved.
---@param callback_data_viewModelName string View model name for defaults (received event).
---@param callback_data_instanceName string Instance name for defaults.
---@param callback_data_stateMachineNames table Array of state machine name strings.
---@param callback_data_error string Error message when an error event fires.
function rive.set_artboard_listener(callback) end

--- Sets or clears the state machine listener callback.
---@param callback? fun(self, event, data) Callback invoked for state machine events; pass nil to disable.
---@param callback_self object The calling script instance.
---@param callback_event string One of: onStateMachineError, onStateMachineDeleted, onStateMachineSettled
---@param callback_data table Event-specific details:
---@param callback_data_stateMachine StateMachineHandle Active state machine handle.
---@param callback_data_error string Error message when an error occurs.
function rive.set_state_machine_listener(callback) end

--- Sets or clears the view model instance listener callback.
---@param callback? fun(self, event, data) Callback invoked for view model instance events; pass nil to disable.
---@param callback_self object The calling script instance.
---@param callback_event string One of: onViewModelInstanceError, onViewModelDeleted, onViewModelDataReceived, onViewModelListSizeReceived
---@param callback_data table Additional payload per event:
---@param callback_data_viewModel ViewModelInstanceHandle Handle of the affected view model instance.
---@param callback_data_error string Error description when an error fires.
---@param callback_data_path string Path being inspected when list size arrives.
---@param callback_data_size number List size value for list-size events.
function rive.set_view_model_instance_listener(callback) end

--- Sets or clears the render image listener callback.
---@param callback? fun(self, event, data) Callback invoked for render image events; pass nil to disable.
---@param callback_self object The calling script instance.
---@param callback_event string One of: onRenderImageDecoded, onRenderImageError, onRenderImageDeleted
---@param callback_data table Additional fields:
---@param callback_data_renderImage RenderImageHandle Handle of the render image.
---@param callback_data_error string Error message for the failure event.
function rive.set_render_image_listener(callback) end

--- Sets or clears the audio source listener callback.
---@param callback? fun(self, event, data) Callback invoked for audio source events; pass nil to disable.
---@param callback_self object The calling script instance.
---@param callback_event string One of: onAudioSourceDecoded, onAudioSourceError, onAudioSourceDeleted
---@param callback_data table Additional fields:
---@param callback_data_audioSource AudioSourceHandle Audio source handle for the event.
---@param callback_data_error string Error message when provided.
function rive.set_audio_source_listener(callback) end

--- Sets or clears the font listener callback.
---@param callback? fun(self, event, data) Callback invoked for font events; pass nil to disable.
---@param callback_self object The calling script instance.
---@param callback_event string One of: onFontDecoded, onFontError, onFontDeleted
---@param callback_data table Additional fields:
---@param callback_data_font FontHandle Font handle for the associated event.
---@param callback_data_error string Error message for failure events.
function rive.set_font_listener(callback) end

--- Returns the Rive file handle tied to the component.
---@param url url Component whose file handle to query.
---@return FileHandle file_handle Handle identifying the loaded Rive file.
function rive.get_file(url) end

--- Switches the active artboard for the component.
---@param url url Component using the artboard.
---@param name string Name of the artboard to activate.
---@return boolean success True if the artboard was found and activated.
function rive.set_artboard(url, name) end

--- Queries the current artboard handle for the component.
---@param url url Component whose artboard handle to return.
---@return ArtboardHandle artboard_handle Active artboard handle.
function rive.get_artboard(url) end

--- Selects a state machine by name on the component.
---@param url url Component owning the state machine.
---@param name string Name of the state machine to activate.
---@return boolean success True if the state machine was activated.
function rive.set_state_machine(url, name) end

--- Returns the active state machine handle for the component.
---@param url url Component whose active state machine to query.
---@return StateMachineHandle state_machine_handle Current state machine handle.
function rive.get_state_machine(url) end

--- Selects a view model instance by name.
---@param url url Component owning the view model instance.
---@param name string View model instance name to activate.
---@return boolean success True if the view model instance was activated.
function rive.set_view_model_instance(url, name) end

--- Returns the handle of the currently bound view model instance.
---@param url url Component whose view model instance handle to query.
---@return ViewModelInstanceHandle view_model_instance_handle Handle for the active view model instance.
function rive.get_view_model_instance(url) end

--- @class rive.cmd
rive.cmd = {}

--- Returns the artboard handle created for the named view model.
---@param file_handle FileHandle Handle to a previously loaded Rive file.
---@param viewmodel_name string Name of the view model to instantiate.
---@return ArtboardHandle artboard_handle Artboard handle created for the named view model.
function rive.cmd.instantiateArtboardNamed(file_handle, viewmodel_name) end

--- Returns the default artboard handle for the file.
---@param file_handle FileHandle Handle to a previously loaded Rive file.
---@return ArtboardHandle artboard_handle Default artboard handle for the file.
function rive.cmd.instantiateDefaultArtboard(file_handle) end

--- Returns a blank view model instance handle for the given artboard or view model.
---@param file_handle FileHandle Handle to a previously loaded Rive file.
---@param artboard_or_viewmodel ArtboardHandle|string Artboard handle or view model name that identifies where to instantiate.
---@return ViewModelInstanceHandle view_model_instance_handle Blank view model instance handle.
function rive.cmd.instantiateBlankViewModelInstance(file_handle, artboard_or_viewmodel) end

--- Returns a default-populated view model instance handle for the given artboard or view model.
---@param file_handle FileHandle Handle to a previously loaded Rive file.
---@param artboard_or_viewmodel ArtboardHandle|string Artboard handle or view model name the instance should derive from.
---@return ViewModelInstanceHandle view_model_instance_handle Default view model instance handle.
function rive.cmd.instantiateDefaultViewModelInstance(file_handle, artboard_or_viewmodel) end

--- Creates a named view model instance and returns its handle.
---@param file_handle FileHandle Handle to a previously loaded Rive file.
---@param artboard_or_viewmodel ArtboardHandle|string Artboard handle or view model name that will host the instance.
---@param instance_name string Name to assign to the new view model instance.
---@return ViewModelInstanceHandle view_model_instance_handle Named view model instance handle.
function rive.cmd.instantiateViewModelInstanceNamed(file_handle, artboard_or_viewmodel, instance_name) end

--- Returns a handle to the nested view model at the given path.
---@param view_model_handle ViewModelInstanceHandle Parent view model instance handle.
---@param path string Dot-delimited path to the nested view model.
---@return ViewModelInstanceHandle view_model_handle Handle to the nested view model.
function rive.cmd.referenceNestedViewModelInstance(view_model_handle, path) end

--- Returns the handle for the list entry at the specified path and index.
---@param view_model_handle ViewModelInstanceHandle Parent view model instance handle.
---@param path string Dot-delimited path to the list view model.
---@param index number Index within the list entry to reference.
---@return ViewModelInstanceHandle view_model_handle Handle to the referenced list entry.
function rive.cmd.referenceListViewModelInstance(view_model_handle, path, index) end

--- Replaces the nested view model at the given path with the supplied handle.
---@param view_model_handle ViewModelInstanceHandle View model instance whose nested child is updated.
---@param path string Path to the nested view model.
---@param nested_handle ViewModelInstanceHandle Handle of the nested view model to attach.
function rive.cmd.setViewModelInstanceNestedViewModel(view_model_handle, path, nested_handle) end

--- Inserts a nested view model into the list at the given index.
---@param view_model_handle ViewModelInstanceHandle View model instance owning the list.
---@param path string Path to the target list.
---@param nested_handle ViewModelInstanceHandle Handle of the view model to insert.
---@param index number Destination index for the insertion.
function rive.cmd.insertViewModelInstanceListViewModel(view_model_handle, path, nested_handle, index) end

--- Appends a nested view model to the list at the specified path.
---@param view_model_handle ViewModelInstanceHandle View model instance owning the list.
---@param path string Path to the target list.
---@param nested_handle ViewModelInstanceHandle Handle of the view model to append.
function rive.cmd.appendViewModelInstanceListViewModel(view_model_handle, path, nested_handle) end

--- Swaps two entries in the nested list.
---@param view_model_handle ViewModelInstanceHandle View model instance that owns the list.
---@param path string Path to the nested list.
---@param indexa number First index to swap.
---@param indexb number Second index to swap.
function rive.cmd.swapViewModelInstanceListValues(view_model_handle, path, indexa, indexb) end

--- Removes the entry at the supplied index from the nested list.
---@param view_model_handle ViewModelInstanceHandle View model instance owning the list.
---@param path string Path to the target list.
---@param index number Index of the entry to remove.
function rive.cmd.removeViewModelInstanceListViewModelIndex(view_model_handle, path, index) end

--- Removes the specified nested view model from the list at the path.
---@param view_model_handle ViewModelInstanceHandle View model instance owning the list.
---@param path string Path to the target list.
---@param nested_handle ViewModelInstanceHandle Handle of the view model to remove.
function rive.cmd.removeViewModelInstanceListViewModel(view_model_handle, path, nested_handle) end

--- Binds the state machine to the provided view model instance.
---@param state_machine_handle StateMachineHandle State machine handle.
---@param view_model_handle ViewModelInstanceHandle View model instance handle.
function rive.cmd.bindViewModelInstance(state_machine_handle, view_model_handle) end

--- Deletes the view model instance handle.
---@param view_model_handle ViewModelInstanceHandle View model instance to delete.
function rive.cmd.deleteViewModelInstance(view_model_handle) end

--- Fires a trigger on the view model instance.
---@param view_model_handle ViewModelInstanceHandle View model instance handle.
---@param trigger_path string Trigger path to activate.
function rive.cmd.fireViewModelTrigger(view_model_handle, trigger_path) end

--- Updates the boolean property at the path.
---@param view_model_handle ViewModelInstanceHandle View model instance handle.
---@param path string Path to the boolean property.
---@param value boolean New boolean value.
function rive.cmd.setViewModelInstanceBool(view_model_handle, path, value) end

--- Updates the numeric property at the path.
---@param view_model_handle ViewModelInstanceHandle View model instance handle.
---@param path string Path to the numeric property.
---@param value number New numeric value.
function rive.cmd.setViewModelInstanceNumber(view_model_handle, path, value) end

--- Updates the color property using the supplied vector.
---@param view_model_handle ViewModelInstanceHandle View model instance handle.
---@param path string Path to the color property.
---@param vector4_color vector4 Color encoded as a Defold Vector4 (WXYZ).
function rive.cmd.setViewModelInstanceColor(view_model_handle, path, vector4_color) end

--- Updates the enum property at the path.
---@param view_model_handle ViewModelInstanceHandle View model instance handle.
---@param path string Path to the enum property.
---@param enum_string string Enum name to select.
function rive.cmd.setViewModelInstanceEnum(view_model_handle, path, enum_string) end

--- Updates the string property at the path.
---@param view_model_handle ViewModelInstanceHandle View model instance handle.
---@param path string Path to the string property.
---@param string_value string New string value.
function rive.cmd.setViewModelInstanceString(view_model_handle, path, string_value) end

--- Updates the image property at the path.
---@param view_model_handle ViewModelInstanceHandle View model instance handle.
---@param path string Path to the image property.
---@param render_image_handle RenderImageHandle Render image handle to assign.
function rive.cmd.setViewModelInstanceImage(view_model_handle, path, render_image_handle) end

--- Updates the artboard reference at the path.
---@param view_model_handle ViewModelInstanceHandle View model instance handle.
---@param path string Path to the artboard reference.
---@param artboard_handle ArtboardHandle Artboard handle to assign.
function rive.cmd.setViewModelInstanceArtboard(view_model_handle, path, artboard_handle) end

--- Subscribes for updates to the named property.
---@param view_model_handle ViewModelInstanceHandle View model instance handle.
---@param path string Path to the property to subscribe.
---@param data_type enum Data type identifier from rive::DataType.
function rive.cmd.subscribeToViewModelProperty(view_model_handle, path, data_type) end

--- Cancels a previous subscription.
---@param view_model_handle ViewModelInstanceHandle View model instance handle.
---@param path string Path to the property.
---@param data_type enum Data type identifier previously subscribed.
function rive.cmd.unsubscribeToViewModelProperty(view_model_handle, path, data_type) end

--- Loads Rive bytes and returns a file handle.
---@param riv_bytes string Raw Rive file data.
---@return FileHandle file_handle Loaded Rive file handle.
function rive.cmd.loadFile(riv_bytes) end

--- Deletes the runtime file handle.
---@param file_handle FileHandle File handle to delete.
function rive.cmd.deleteFile(file_handle) end

--- Decodes image bytes and returns a render image handle.
---@param image_bytes string Encoded image data.
---@return RenderImageHandle render_image_handle Decoded render image handle.
function rive.cmd.decodeImage(image_bytes) end

--- Deletes the render image handle.
---@param image_handle RenderImageHandle Render image handle to delete.
function rive.cmd.deleteImage(image_handle) end

--- Decodes audio bytes and returns an audio source handle.
---@param audio_bytes string Encoded audio data.
---@return AudioSourceHandle audio_handle Decoded audio source handle.
function rive.cmd.decodeAudio(audio_bytes) end

--- Deletes the audio source handle.
---@param audio_handle AudioSourceHandle Audio source handle to delete.
function rive.cmd.deleteAudio(audio_handle) end

--- Decodes font bytes and returns a font handle.
---@param font_bytes string Encoded font data.
---@return FontHandle font_handle Decoded font handle.
function rive.cmd.decodeFont(font_bytes) end

--- Deletes the font handle.
---@param font_handle FontHandle Font handle to delete.
function rive.cmd.deleteFont(font_handle) end

--- Registers a global image asset.
---@param asset_name string Name used to reference the global asset.
---@param render_image_handle RenderImageHandle Render image handle to register.
function rive.cmd.addGlobalImageAsset(asset_name, render_image_handle) end

--- Unregisters the named global image asset.
---@param asset_name string Name of the asset to remove.
function rive.cmd.removeGlobalImageAsset(asset_name) end

--- Registers a global audio asset.
---@param asset_name string Name used to reference the global audio.
---@param audio_handle AudioSourceHandle Audio source handle to register.
function rive.cmd.addGlobalAudioAsset(asset_name, audio_handle) end

--- Unregisters the named global audio asset.
---@param asset_name string Name of the audio asset to remove.
function rive.cmd.removeGlobalAudioAsset(asset_name) end

--- Registers a global font asset.
---@param asset_name string Name used to reference the global font.
---@param font_handle FontHandle Font handle to register.
function rive.cmd.addGlobalFontAsset(asset_name, font_handle) end

--- Unregisters the named global font asset.
---@param asset_name string Name of the font asset to remove.
function rive.cmd.removeGlobalFontAsset(asset_name) end

--- Requests view model names for the file.
---@param file_handle FileHandle File handle whose view models to query.
function rive.cmd.requestViewModelNames(file_handle) end

--- Requests artboard names for the file.
---@param file_handle FileHandle File handle whose artboards to query.
function rive.cmd.requestArtboardNames(file_handle) end

--- Requests enum definitions for the file.
---@param file_handle FileHandle File handle whose enums to query.
function rive.cmd.requestViewModelEnums(file_handle) end

--- Requests property definitions for the view model.
---@param file_handle FileHandle File handle whose view models to query.
---@param viewmodel_name string View model name whose property metadata to request.
function rive.cmd.requestViewModelPropertyDefinitions(file_handle, viewmodel_name) end

--- Requests instance names for the view model.
---@param file_handle FileHandle File handle whose view models to query.
---@param viewmodel_name string View model name to inspect.
function rive.cmd.requestViewModelInstanceNames(file_handle, viewmodel_name) end

--- Requests the boolean value for the property.
---@param instance_handle ViewModelInstanceHandle View model instance handle.
---@param property_name string Name of the bool property.
function rive.cmd.requestViewModelInstanceBool(instance_handle, property_name) end

--- Requests the numeric value for the property.
---@param instance_handle ViewModelInstanceHandle View model instance handle.
---@param property_name string Name of the numeric property.
function rive.cmd.requestViewModelInstanceNumber(instance_handle, property_name) end

--- Requests the color value for the property.
---@param instance_handle ViewModelInstanceHandle View model instance handle.
---@param property_name string Name of the color property.
function rive.cmd.requestViewModelInstanceColor(instance_handle, property_name) end

--- Requests the enum value for the property.
---@param instance_handle ViewModelInstanceHandle View model instance handle.
---@param property_name string Name of the enum property.
function rive.cmd.requestViewModelInstanceEnum(instance_handle, property_name) end

--- Requests the string value for the property.
---@param instance_handle ViewModelInstanceHandle View model instance handle.
---@param property_name string Name of the string property.
function rive.cmd.requestViewModelInstanceString(instance_handle, property_name) end

--- Requests the list size for the property.
---@param instance_handle ViewModelInstanceHandle View model instance handle.
---@param property_name string Name of the list property.
function rive.cmd.requestViewModelInstanceListSize(instance_handle, property_name) end

--- Requests state machine names for the artboard.
---@param artboard_handle ArtboardHandle Artboard handle to query.
function rive.cmd.requestStateMachineNames(artboard_handle) end

--- Requests metadata about the default view model or artboard.
---@param artboard_handle ArtboardHandle Artboard handle to query.
---@param file_handle FileHandle File handle providing metadata.
function rive.cmd.requestDefaultViewModelInfo(artboard_handle, file_handle) end
