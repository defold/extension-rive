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
#include <dmsdk/script.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/transform.h>
#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/gamesys/render_constants.h>

#include "rive_ddf.h"

namespace rive
{
    class StateMachineInstance;
    class LinearAnimationInstance;
    class Bone;
}

namespace dmRive
{
    static const char* RIVE_MODEL_EXT = "rivemodelc";

    struct RiveModelResource;
    struct RiveBuffer;

    // Keep this private from the scripting api
    struct RiveComponent
    {
        dmGameObject::HInstance                 m_Instance;
        dmTransform::Transform                  m_Transform;
        dmVMath::Matrix4                        m_World;
        RiveModelResource*                      m_Resource;
        dmMessage::URL                          m_Listener;
        dmGameSystem::HComponentRenderConstants m_RenderConstants;
        dmRender::HMaterial                     m_Material;
        dmScript::LuaCallbackInfo*              m_Callback;
        uint32_t                                m_CallbackId;
        rive::Mat2D                             m_InverseRendererTransform;

        std::unique_ptr<rive::ArtboardInstance>         m_ArtboardInstance;
        std::unique_ptr<rive::LinearAnimationInstance>  m_AnimationInstance;
        std::unique_ptr<rive::StateMachineInstance>     m_StateMachineInstance;

        dmArray<std::unique_ptr<rive::StateMachineInstance>> m_AllSMSInstances;

        dmGameObject::Playback                  m_AnimationPlayback;
        float                                   m_AnimationPlaybackRate;

        dmArray<rive::Bone*>                    m_Bones;
        dmArray<dmGameObject::HInstance>        m_BoneGOs;
        dmArray<dmhash_t>                       m_StateMachineInputs; // A list of the hashed names for the state machine inputs. Index corresponds 1:1 to the statemachine inputs

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

    struct StateMachineInputData
    {
        union
        {
            bool  m_BoolValue;
            float m_NumberValue;
        };

        enum Result
        {
            RESULT_NOT_FOUND        = -3,
            RESULT_TYPE_MISMATCH    = -2,
            RESULT_TYPE_UNSUPPORTED = -1,
            RESULT_OK               = 0,
        };

        enum Type
        {
            TYPE_INVALID = -1,
            TYPE_BOOL    = 0,
            TYPE_NUMBER  = 1
        };

        Type m_Type;
    };

    // Get the game object identifier
    bool CompRiveGetBoneID(RiveComponent* component, dmhash_t bone_name, dmhash_t* id);

    void CompRivePointerMove(RiveComponent* component, float x, float y);
    void CompRivePointerUp(RiveComponent* component, float x, float y);
    void CompRivePointerDown(RiveComponent* component, float x, float y);

    bool CompRivePlayStateMachine(RiveComponent* component, dmRiveDDF::RivePlayAnimation* ddf, dmScript::LuaCallbackInfo* callback_info);
    bool CompRivePlayAnimation(RiveComponent* component, dmRiveDDF::RivePlayAnimation* ddf, dmScript::LuaCallbackInfo* callback_info);

    const char* CompRiveGetTextRun(RiveComponent* component, const char* name, const char* nested_artboard_path);
    bool        CompRiveSetTextRun(RiveComponent* component, const char* name, const char* text_run, const char* nested_artboard_path);

    StateMachineInputData::Result CompRiveGetStateMachineInput(RiveComponent* component, const char* input_name, const char* nested_artboard_path, StateMachineInputData& data);
    StateMachineInputData::Result CompRiveSetStateMachineInput(RiveComponent* component, const char* input_name, const char* nested_artboard_path, const StateMachineInputData& data);

    float CompRiveGetDisplayScaleFactor();
    void  CompRiveDebugSetBlitMode(bool value);

    // bool CompRiveSetIKTargetInstance(RiveComponent* component, dmhash_t constraint_id, float mix, dmhash_t instance_id);
    // bool CompRiveSetIKTargetPosition(RiveComponent* component, dmhash_t constraint_id, float mix, Vectormath::Aos::Point3 position);
    // bool CompRiveResetIKTarget(RiveComponent* component, dmhash_t constraint_id);
}

#endif // DM_GAMESYS_COMP_RIVE_H
