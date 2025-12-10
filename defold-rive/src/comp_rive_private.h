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
        rive::ViewModelInstanceHandle           m_ViewModelInstance;
        rive::DrawKey                           m_DrawKey;

        dmGameObject::Playback                  m_AnimationPlayback;
        float                                   m_AnimationPlaybackRate;

        // dmArray<rive::Bone*>                    m_Bones;
        // dmArray<dmGameObject::HInstance>        m_BoneGOs;

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
    rive::Vec2D     WorldToLocal(RiveComponent* component, float x, float y);
    RiveSceneData*  CompRiveGetRiveSceneData(RiveComponent* component);
}

#endif //DM_COMP_RIVE_PRIVATE_H
