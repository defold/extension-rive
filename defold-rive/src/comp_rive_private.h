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

#ifndef DM_COMP_RIVE_PRIVATE_H
#define DM_COMP_RIVE_PRIVATE_H

#include <stdint.h>

#include <dmsdk/script.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/transform.h>
#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/gamesys/render_constants.h>

#include "rive_ddf.h"

#include <rive/math/mat2d.hpp>
#include <rive/refcnt.hpp>
#include <rive/command_queue.hpp>

namespace rive
{
    class Artboard;
    class ArtboardInstance;
    class Bone;
    class LinearAnimationInstance;
    class StateMachine;
    class StateMachineInstance;
    class ViewModelInstance;
    class ViewModelInstanceRuntime;
    class RenderImage;
    enum class DataType : unsigned int;
}

namespace dmGameObject
{
    struct ComponentSetPropertyParams;
    struct ComponentGetPropertyParams;
    struct PropertyDesc;
}

namespace dmGameSystem
{
    struct MaterialResource;
}

namespace dmRive
{
    struct RiveModelResource;
    struct RiveSceneData;
    struct RiveArtboardIdList;

    // Keep this private from the scripting api
    struct RiveComponent
    {
        dmGameObject::HInstance                 m_Instance;
        dmTransform::Transform                  m_Transform;
        dmVMath::Matrix4                        m_World;
        RiveModelResource*                      m_Resource;
        dmMessage::URL                          m_Listener;
        dmGameSystem::HComponentRenderConstants m_RenderConstants;
        dmGameSystem::MaterialResource*         m_Material;
        dmScript::LuaCallbackInfo*              m_Callback;
        uint32_t                                m_CallbackId;
        rive::Mat2D                             m_InverseRendererTransform;

        rive::ArtboardHandle                    m_Artboard;
        rive::StateMachineHandle                m_StateMachine;
        rive::DrawKey                           m_DrawKey;

        //std::unique_ptr<rive::ArtboardInstance>             m_ArtboardInstance;
        //std::unique_ptr<rive::LinearAnimationInstance>      m_AnimationInstance;
        //std::unique_ptr<rive::StateMachineInstance>         m_StateMachineInstance;
        dmHashTable32<rive::ViewModelInstanceRuntime*>      m_ViewModelInstanceRuntimes;

        dmArray<std::unique_ptr<rive::StateMachineInstance>> m_AllSMSInstances;

        dmGameObject::Playback                  m_AnimationPlayback;
        float                                   m_AnimationPlaybackRate;

        dmArray<rive::Bone*>                    m_Bones;
        dmArray<dmGameObject::HInstance>        m_BoneGOs;
        dmArray<dmhash_t>                       m_StateMachineInputs; // A list of the hashed names for the state machine inputs. Index corresponds 1:1 to the statemachine inputs

        rive::Fit                               m_Fit;
        rive::Alignment                         m_Alignment;

        uint32_t                                m_VertexCount;
        uint32_t                                m_IndexCount;
        uint32_t                                m_MixedHash;
        uint32_t                                m_CurrentViewModelInstanceRuntime;
        uint16_t                                m_HandleCounter;
        uint16_t                                m_ComponentIndex; // The component type index
        uint8_t                                 m_AnimationIndex;
        uint8_t                                 m_Enabled : 1;
        uint8_t                                 m_DoRender : 1;
        uint8_t                                 m_AddedToUpdate : 1;
        uint8_t                                 m_ReHash : 1;
    };

    static const uint32_t INVALID_HANDLE = 0xFFFFFFFF;

    // Math
    rive::Vec2D         WorldToLocal(RiveComponent* component, float x, float y);

    // animations
    RiveArtboardIdList* FindArtboardIdList(rive::Artboard* artboard, dmRive::RiveSceneData* data);
    int                 FindAnimationIndex(dmhash_t* entries, uint32_t num_entries, dmhash_t anim_id);

    // State machine
    rive::StateMachine* FindStateMachine(rive::Artboard* artboard, dmRive::RiveSceneData* data, int* state_machine_index, dmhash_t anim_id);
    int                 FindStateMachineInputIndex(RiveComponent* component, dmhash_t property_name);
    void                GetStateMachineInputNames(rive::StateMachineInstance* smi, dmArray<dmhash_t>& names);

    dmGameObject::PropertyResult SetStateMachineInput(RiveComponent* component, int index, const dmGameObject::ComponentSetPropertyParams& params);
    dmGameObject::PropertyResult GetStateMachineInput(RiveComponent* component, int index,
                                                      const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    // Data bindings
    void      DebugModelViews(RiveComponent* component);

    // script api
    uint32_t    CompRiveCreateViewModelInstanceRuntime(RiveComponent* component, dmhash_t name_hash);
    bool        CompRiveDestroyViewModelInstanceRuntime(RiveComponent* component, uint32_t handle);
    bool        CompRiveSetViewModelInstanceRuntime(RiveComponent* component, uint32_t handle);
    uint32_t    CompRiveGetViewModelInstanceRuntime(RiveComponent* component);

    bool        GetPropertyDataType(RiveComponent* component, uint32_t handle, const char* path, rive::DataType* type);

    bool        CompRiveRuntimeSetPropertyBool(RiveComponent* component, uint32_t handle, const char* path, bool value);
    bool        CompRiveRuntimeSetPropertyF32(RiveComponent* component, uint32_t handle, const char* path, float value);
    bool        CompRiveRuntimeSetPropertyColor(RiveComponent* component, uint32_t handle, const char* path, dmVMath::Vector4* color);
    bool        CompRiveRuntimeSetPropertyString(RiveComponent* component, uint32_t handle, const char* path, const char* value);
    bool        CompRiveRuntimeSetPropertyEnum(RiveComponent* component, uint32_t handle, const char* path, const char* value);
    bool        CompRiveRuntimeSetPropertyTrigger(RiveComponent* component, uint32_t handle, const char* path);
    bool        CompRiveRuntimeSetPropertyImage(RiveComponent* component, uint32_t handle, const char* path, rive::RenderImage* image);

    bool        CompRiveRuntimeGetPropertyBool(RiveComponent* component, uint32_t handle, const char* path, bool* value);
    bool        CompRiveRuntimeGetPropertyF32(RiveComponent* component, uint32_t handle, const char* path, float* value);
    bool        CompRiveRuntimeGetPropertyColor(RiveComponent* component, uint32_t handle, const char* path, dmVMath::Vector4* color);
    bool        CompRiveRuntimeGetPropertyString(RiveComponent* component, uint32_t handle, const char* path, const char** value);
    bool        CompRiveRuntimeGetPropertyEnum(RiveComponent* component, uint32_t handle, const char* path, const char** value);

    bool        CompRiveRuntimeListAddInstance(RiveComponent* component, uint32_t handle, const char* path, uint32_t instance);
    bool        CompRiveRuntimeListRemoveInstance(RiveComponent* component, uint32_t handle, const char* path, uint32_t instance);

    RiveSceneData* CompRiveGetRiveSceneData(RiveComponent* component);
}

#endif //DM_COMP_RIVE_PRIVATE_H
