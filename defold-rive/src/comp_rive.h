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

#ifndef DM_GAMESYS_COMP_RIVE_H
#define DM_GAMESYS_COMP_RIVE_H

#include <stdint.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/transform.h>
#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/gamesys/render_constants.h>

namespace dmRive
{
    struct RiveModelResource;
    struct RiveBuffer;

    struct RiveComponent
    {
        dmGameObject::HInstance                 m_Instance;
        dmTransform::Transform                  m_Transform;
        dmVMath::Matrix4                        m_World;
        RiveModelResource*                      m_Resource;
        dmMessage::URL                          m_Listener;
        dmGameSystem::HComponentRenderConstants m_RenderConstants;
        dmRender::HMaterial                     m_Material;

        rive::LinearAnimationInstance*          m_AnimationInstance;
        dmGameObject::Playback                  m_AnimationPlayback;
        float                                   m_AnimationPlaybackRate;
        int                                     m_AnimationCallbackRef;

        //dmArray<dmGameObject::HInstance>        m_NodeInstances; // Node instances corresponding to the bones
        uint32_t                                m_VertexCount;
        uint32_t                                m_IndexCount;
        uint32_t                                m_MixedHash;
        uint16_t                                m_ComponentIndex;
        uint8_t                                 m_AnimationIndex;
        uint8_t                                 m_Enabled : 1;
        uint8_t                                 m_DoRender : 1;
        uint8_t                                 m_AddedToUpdate : 1;
        uint8_t                                 m_ReHash : 1;
    };

    // For scripting
    // bool CompRiveSetIKTargetInstance(RiveComponent* component, dmhash_t constraint_id, float mix, dmhash_t instance_id);
    // bool CompRiveSetIKTargetPosition(RiveComponent* component, dmhash_t constraint_id, float mix, Vectormath::Aos::Point3 position);
    // bool CompRiveResetIKTarget(RiveComponent* component, dmhash_t constraint_id);

    // bool CompRiveSetSkin(RiveComponent* component, dmhash_t skin_id);
    // bool CompRiveSetSkinSlot(RiveComponent* component, dmhash_t skin_id, dmhash_t slot_id);
}

#endif // DM_GAMESYS_COMP_RIVE_H
