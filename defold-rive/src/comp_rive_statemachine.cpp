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

#if !defined(DM_RIVE_UNSUPPORTED)

#include "comp_rive.h"
#include "comp_rive_private.h"
#include "res_rive_data.h"

#include <common/commands.h>

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/gameobject/component.h>
#include <dmsdk/gameobject/gameobject.h>

#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/animation/state_machine_bool.hpp>
#include <rive/animation/state_machine_number.hpp>
#include <rive/animation/state_machine_trigger.hpp>

namespace dmRive
{
    rive::StateMachine* FindStateMachine(rive::Artboard* artboard, dmRive::RiveSceneData* data, int* state_machine_index, dmhash_t anim_id)
    {
        dmLogOnceError("MAWE Missing implementation; %s", __FUNCTION__);
        return 0;

        // RiveArtboardIdList* id_list = FindArtboardIdList(artboard, data);
        // if (!id_list)
        // {
        //     return 0;
        // }

        // int index = FindAnimationIndex(id_list->m_StateMachines.Begin(), id_list->m_StateMachines.Size(), anim_id);
        // if (index == -1) {
        //     return 0;
        // }
        // *state_machine_index = index;
        // return artboard->stateMachine(index);
    }

    int FindStateMachineInputIndex(RiveComponent* component, dmhash_t property_name)
    {
        dmLogOnceError("MAWE Missing implementation; %s", __FUNCTION__);
        return -1;
        // uint32_t count = component->m_StateMachineInputs.Size();
        // for (uint32_t i = 0; i < count; ++i)
        // {
        //     if (component->m_StateMachineInputs[i] == property_name)
        //     {
        //         return (int)i;
        //     }
        // }
        // return -1;
    }

    void GetStateMachineInputNames(rive::StateMachineInstance* smi, dmArray<dmhash_t>& names)
    {
        dmLogOnceError("MAWE Missing implementation; %s", __FUNCTION__);
        return;

        // uint32_t count = smi->inputCount();
        // if (count > names.Capacity())
        // {
        //     names.SetCapacity(count);
        // }
        // names.SetSize(count);

        // for (uint32_t i = 0; i < count; ++i)
        // {
        //     const rive::SMIInput* input = smi->input(i);
        //     names[i] = dmHashString64(input->name().c_str());
        // }
    }

    dmGameObject::PropertyResult SetStateMachineInput(RiveComponent* component, int index, const dmGameObject::ComponentSetPropertyParams& params)
    {
        dmLogOnceError("MAWE Missing implementation; %s", __FUNCTION__);
        return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

        // const rive::StateMachine* state_machine = component->m_StateMachineInstance->stateMachine();
        // const rive::StateMachineInput* input = state_machine->input(index);
        // rive::SMIInput* input_instance = component->m_StateMachineInstance->input(index);

        // if (input->is<rive::StateMachineTrigger>())
        // {
        //     if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_BOOLEAN)
        //     {
        //         dmLogError("Found property %s of type trigger, but didn't receive a boolean", input->name().c_str());
        //         return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        //     }

        //     // The trigger can only respond to the value "true"
        //     if (!params.m_Value.m_Bool)
        //     {
        //         dmLogError("Found property %s of type trigger, but didn't receive a boolean of true", input->name().c_str());
        //         return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        //     }

        //     rive::SMITrigger* trigger = (rive::SMITrigger*)input_instance;
        //     trigger->fire();
        // }
        // else if (input->is<rive::StateMachineBool>())
        // {
        //     if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_BOOLEAN)
        //     {
        //         dmLogError("Found property %s of type bool, but didn't receive a boolean", input->name().c_str());
        //         return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        //     }

        //     rive::SMIBool* v = (rive::SMIBool*)input_instance;
        //     v->value(params.m_Value.m_Bool);
        // }
        // else if (input->is<rive::StateMachineNumber>())
        // {
        //     if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
        //     {
        //         dmLogError("Found property %s of type number, but didn't receive a number", input->name().c_str());
        //         return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        //     }

        //     rive::SMINumber* v = (rive::SMINumber*)input_instance;
        //     v->value(params.m_Value.m_Number);
        // }

        // return dmGameObject::PROPERTY_RESULT_OK;
    }

    dmGameObject::PropertyResult GetStateMachineInput(RiveComponent* component, int index,
            const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        dmLogOnceError("MAWE Missing implementation; %s", __FUNCTION__);
        return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

        // const rive::StateMachine* state_machine = component->m_StateMachineInstance->stateMachine();
        // const rive::StateMachineInput* input = state_machine->input(index);
        // rive::SMIInput* input_instance = component->m_StateMachineInstance->input(index);

        // if (input->is<rive::StateMachineTrigger>())
        // {
        //     dmLogError("Cannot get value of input type trigger ( %s )", input->name().c_str());
        //     return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        // }
        // else if (input->is<rive::StateMachineBool>())
        // {
        //     rive::SMIBool* v = (rive::SMIBool*)input_instance;
        //     out_value.m_Variant = dmGameObject::PropertyVar(v->value());
        // }
        // else if (input->is<rive::StateMachineNumber>())
        // {
        //     rive::SMINumber* v = (rive::SMINumber*)input_instance;
        //     out_value.m_Variant = dmGameObject::PropertyVar(v->value());
        // }

        // return dmGameObject::PROPERTY_RESULT_OK;
    }


    StateMachineInputData::Result CompRiveSetStateMachineInput(RiveComponent* component, const char* input_name, const char* nested_artboard_path, const StateMachineInputData& value)
    {
        dmLogOnceError("MAWE Missing implementation; %s", __FUNCTION__);
        return StateMachineInputData::RESULT_NOT_FOUND;

        // rive::ArtboardInstance* artboard = component->m_ArtboardInstance.get();
        // rive::SMIInput* input_instance = 0x0;

        // if (nested_artboard_path)
        // {
        //     input_instance = artboard->input(input_name, nested_artboard_path);
        // }
        // else
        // {
        //     dmhash_t input_hash = dmHashString64(input_name);
        //     int index = FindStateMachineInputIndex(component, input_hash);
        //     if (index >= 0)
        //     {
        //         input_instance = component->m_StateMachineInstance->input(index);
        //     }
        // }

        // if (input_instance)
        // {
        //     const rive::StateMachineInput* input = input_instance->input();

        //     if (input->is<rive::StateMachineTrigger>())
        //     {
        //         if (value.m_Type != StateMachineInputData::TYPE_BOOL)
        //         {
        //             return StateMachineInputData::RESULT_TYPE_MISMATCH;
        //         }
        //         rive::SMITrigger* trigger = (rive::SMITrigger*)input_instance;
        //         trigger->fire();
        //         return StateMachineInputData::RESULT_OK;
        //     }
        //     else if (input->is<rive::StateMachineBool>())
        //     {
        //         if (value.m_Type != StateMachineInputData::TYPE_BOOL)
        //         {
        //             return StateMachineInputData::RESULT_TYPE_MISMATCH;
        //         }
        //         rive::SMIBool* v = (rive::SMIBool*)input_instance;
        //         v->value(value.m_BoolValue);
        //         return StateMachineInputData::RESULT_OK;
        //     }
        //     else if (input->is<rive::StateMachineNumber>())
        //     {
        //         if (value.m_Type != StateMachineInputData::TYPE_NUMBER)
        //         {
        //             return StateMachineInputData::RESULT_TYPE_MISMATCH;
        //         }
        //         rive::SMINumber* v = (rive::SMINumber*)input_instance;
        //         v->value(value.m_NumberValue);
        //         return StateMachineInputData::RESULT_OK;
        //     }
        // }

        // return StateMachineInputData::RESULT_NOT_FOUND;
    }

    StateMachineInputData::Result CompRiveGetStateMachineInput(RiveComponent* component, const char* input_name, const char* nested_artboard_path, StateMachineInputData& out_value)
    {
        dmLogOnceError("MAWE Missing implementation; %s", __FUNCTION__);
        return StateMachineInputData::RESULT_NOT_FOUND;

        // rive::ArtboardInstance* artboard = component->m_ArtboardInstance.get();
        // rive::SMIInput* input_instance = 0x0;

        // if (nested_artboard_path)
        // {
        //     input_instance = artboard->input(input_name, nested_artboard_path);
        // }
        // else
        // {
        //     dmhash_t input_hash = dmHashString64(input_name);
        //     int index = FindStateMachineInputIndex(component, input_hash);
        //     if (index >= 0)
        //     {
        //         input_instance = component->m_StateMachineInstance->input(index);
        //     }
        // }

        // out_value.m_Type = StateMachineInputData::TYPE_INVALID;

        // if (input_instance)
        // {
        //     const rive::StateMachineInput* input = input_instance->input();

        //     if (input->is<rive::StateMachineTrigger>())
        //     {
        //         return StateMachineInputData::RESULT_TYPE_UNSUPPORTED;
        //     }
        //     else if (input->is<rive::StateMachineBool>())
        //     {
        //         rive::SMIBool* v = (rive::SMIBool*)input_instance;
        //         out_value.m_Type = StateMachineInputData::TYPE_BOOL;
        //         out_value.m_BoolValue = v->value();
        //         return StateMachineInputData::RESULT_OK;
        //     }
        //     else if (input->is<rive::StateMachineNumber>())
        //     {
        //         rive::SMINumber* v = (rive::SMINumber*)input_instance;
        //         out_value.m_Type = StateMachineInputData::TYPE_NUMBER;
        //         out_value.m_NumberValue = v->value();
        //         return StateMachineInputData::RESULT_OK;
        //     }
        // }

        // return StateMachineInputData::RESULT_NOT_FOUND;
    }

    static void FillPointerEvent(RiveComponent* component, rive::CommandQueue::PointerEvent& event, rive::Vec2D& pos)
    {
        event.fit = rive::Fit::layout; // TODO:
        event.alignment = rive::Alignment::center; // TODO:
        event.screenBounds.x = 1024;
        event.screenBounds.y = 768;
        event.position = pos;
        event.scaleFactor = CompRiveGetDisplayScaleFactor();
    }
    void CompRivePointerMove(RiveComponent* component, float x, float y)
    {
        if (!component->m_StateMachine)
            return;

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        rive::Vec2D p = WorldToLocal(component, x, y);
        rive::CommandQueue::PointerEvent event;
        FillPointerEvent(component, event, p);
        queue->pointerMove(component->m_StateMachine, event);
    }

    void CompRivePointerUp(RiveComponent* component, float x, float y)
    {
        if (!component->m_StateMachine)
            return;

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        rive::Vec2D p = WorldToLocal(component, x, y);
        rive::CommandQueue::PointerEvent event;
        FillPointerEvent(component, event, p);
        queue->pointerUp(component->m_StateMachine, event);
    }

    void CompRivePointerDown(RiveComponent* component, float x, float y)
    {
        if (!component->m_StateMachine)
            return;

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        rive::Vec2D p = WorldToLocal(component, x, y);
        rive::CommandQueue::PointerEvent event;
        FillPointerEvent(component, event, p);
        queue->pointerDown(component->m_StateMachine, event);
    }
}

#endif
