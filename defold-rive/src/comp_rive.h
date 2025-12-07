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

#include <rive/command_queue.hpp> // handles

namespace dmRive
{
    /////////////////////////////////////////////////////////////////////////////////////
    // For scripting

    static const char* RIVE_MODEL_EXT = "rivemodelc";

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

    struct RiveComponent;

    // Get the game object identifier
    bool CompRiveGetBoneID(RiveComponent* component, dmhash_t bone_name, dmhash_t* id);

    bool CompRivePlayStateMachine(RiveComponent* component, dmRiveDDF::RivePlayAnimation* ddf, dmScript::LuaCallbackInfo* callback_info);
    bool CompRivePlayAnimation(RiveComponent* component, dmRiveDDF::RivePlayAnimation* ddf, dmScript::LuaCallbackInfo* callback_info);

    const char* CompRiveGetTextRun(RiveComponent* component, const char* name, const char* nested_artboard_path);
    bool        CompRiveSetTextRun(RiveComponent* component, const char* name, const char* text_run, const char* nested_artboard_path);

    float CompRiveGetDisplayScaleFactor();
    void  CompRiveDebugSetBlitMode(bool value);

    // state machine impl
    StateMachineInputData::Result CompRiveGetStateMachineInput(RiveComponent* component, const char* input_name, const char* nested_artboard_path, StateMachineInputData& data);
    StateMachineInputData::Result CompRiveSetStateMachineInput(RiveComponent* component, const char* input_name, const char* nested_artboard_path, const StateMachineInputData& data);


    // Scripting
    bool                        CompRiveSetArtboard(RiveComponent* component, const char* name);
    rive::ArtboardHandle        CompRiveGetArtboard(RiveComponent* component);
    bool                        CompRiveSetStateMachine(RiveComponent* component, const char* name);
    rive::StateMachineHandle    CompRiveGetStateMachine(RiveComponent* component);

    enum PointerAction
    {
        POINTER_MOVE,
        POINTER_DOWN,
        POINTER_UP,
        POINTER_EXIT,
    };

    void CompRivePointerAction(RiveComponent* component, PointerAction cmd, float x, float y);
}

#endif // DM_GAMESYS_COMP_RIVE_H
