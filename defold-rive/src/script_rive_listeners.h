// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_GAMESYS_SCRIPT_RIVE_LISTENERS_H
#define DM_GAMESYS_SCRIPT_RIVE_LISTENERS_H

#include <stdint.h>

#include <dmsdk/script/script.h>

#include <string>
#include <rive/command_queue.hpp>

namespace dmRive
{

class FileListener : public rive::CommandQueue::FileListener
{
public:
    virtual void onArtboardsListed(const rive::FileHandle fileHandle, uint64_t requestId, std::vector<std::string> artboardNames) override;
    virtual void onFileError(const rive::FileHandle, uint64_t requestId, std::string error) override;
    virtual void onFileDeleted(const rive::FileHandle, uint64_t requestId) override;
    virtual void onFileLoaded(const rive::FileHandle, uint64_t requestId) override;
    virtual void onViewModelsListed(const rive::FileHandle, uint64_t requestId, std::vector<std::string> viewModelNames) override;
    virtual void onViewModelInstanceNamesListed(const rive::FileHandle, uint64_t requestId, std::string viewModelName, std::vector<std::string> instanceNames) override;
    virtual void onViewModelPropertiesListed( const rive::FileHandle, uint64_t requestId, std::string viewModelName, std::vector<ViewModelPropertyData> properties) override;
    virtual void onViewModelEnumsListed(const rive::FileHandle, uint64_t requestId, std::vector<rive::ViewModelEnum> enums) override;

    dmScript::LuaCallbackInfo* m_Callback;
};

class RenderImageListener : public rive::CommandQueue::RenderImageListener
{
public:
    virtual void onRenderImageDecoded(const rive::RenderImageHandle, uint64_t requestId) override;
    virtual void onRenderImageError(const rive::RenderImageHandle, uint64_t requestId, std::string error) override;
    virtual void onRenderImageDeleted(const rive::RenderImageHandle, uint64_t requestId) override;
    dmScript::LuaCallbackInfo* m_Callback;
};

class AudioSourceListener : public rive::CommandQueue::AudioSourceListener
{
public:
    virtual void onAudioSourceDecoded(const rive::AudioSourceHandle, uint64_t requestId) override;
    virtual void onAudioSourceError(const rive::AudioSourceHandle, uint64_t requestId, std::string error) override;
    virtual void onAudioSourceDeleted(const rive::AudioSourceHandle, uint64_t requestId) override;
    dmScript::LuaCallbackInfo* m_Callback;
};

class FontListener : public rive::CommandQueue::FontListener
{
public:
    virtual void onFontDecoded(const rive::FontHandle, uint64_t requestId) override;
    virtual void onFontError(const rive::FontHandle, uint64_t requestId, std::string error) override;
    virtual void onFontDeleted(const rive::FontHandle, uint64_t requestId) override;
    dmScript::LuaCallbackInfo* m_Callback;
};

class ArtboardListener : public rive::CommandQueue::ArtboardListener
{
public:
    virtual void onArtboardError(const rive::ArtboardHandle, uint64_t requestId, std::string error) override;
    virtual void onDefaultViewModelInfoReceived(const rive::ArtboardHandle, uint64_t requestId, std::string viewModelName, std::string instanceName) override;
    virtual void onArtboardDeleted(const rive::ArtboardHandle, uint64_t requestId) override;
    virtual void onStateMachinesListed(const rive::ArtboardHandle, uint64_t requestId, std::vector<std::string> stateMachineNames) override;
    dmScript::LuaCallbackInfo* m_Callback;
};

class ViewModelInstanceListener : public rive::CommandQueue::ViewModelInstanceListener
{
public:
    virtual void onViewModelInstanceError(const rive::ViewModelInstanceHandle, uint64_t requestId, std::string error) override;
    virtual void onViewModelDeleted(const rive::ViewModelInstanceHandle, uint64_t requestId) override;
    virtual void onViewModelDataReceived(const rive::ViewModelInstanceHandle, uint64_t requestId, rive::CommandQueue::ViewModelInstanceData) override;
    virtual void onViewModelListSizeReceived(const rive::ViewModelInstanceHandle, uint64_t requestId, std::string path, size_t size) override;
    dmScript::LuaCallbackInfo* m_Callback;
};

class StateMachineListener : public rive::CommandQueue::StateMachineListener
{
public:
    virtual void onStateMachineError(const rive::StateMachineHandle, uint64_t requestId, std::string error) override;
    virtual void onStateMachineDeleted(const rive::StateMachineHandle, uint64_t requestId) override;
    virtual void onStateMachineSettled(const rive::StateMachineHandle, uint64_t requestId) override;
    dmScript::LuaCallbackInfo* m_Callback;
};

} // namespace

#endif // DM_GAMESYS_SCRIPT_RIVE_LISTENERS_H
