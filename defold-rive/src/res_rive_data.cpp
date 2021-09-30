// Copyright 2021 The Defold Foundation
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

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/resource/resource.h>

// Rive includes
#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/linear_animation.hpp>
#include <rive/animation/state_machine.hpp>

#include "res_rive_data.h"
#include <common/bones.h>

namespace dmRive
{
    static void SetupBones(RiveSceneData* scene_data, const char* path)
    {
        scene_data->m_Roots.SetSize(0);
        scene_data->m_Bones.SetSize(0);

        rive::Artboard* artboard = scene_data->m_File->artboard();
        if (!artboard) {
            return;
        }

        dmRive::BuildBoneHierarchy(artboard, &scene_data->m_Roots, &scene_data->m_Bones);

        //dmRive::DebugBoneHierarchy(&scene_data->m_Roots);

        dmRive::ValidateBoneNames(&scene_data->m_Bones);
    }

    static void SetupData(RiveSceneData* scene_data, rive::File* file, const char* path)
    {
        scene_data->m_File = file;

        rive::Artboard* artboard = file->artboard();

        uint32_t animation_count = (uint32_t)artboard->animationCount();
        if (artboard && animation_count > 0)
        {
            scene_data->m_LinearAnimations.SetCapacity(animation_count);
            scene_data->m_LinearAnimations.SetSize(animation_count);

            for (int i = 0; i < animation_count; ++i)
            {
                rive::LinearAnimation* animation = artboard->animation(i);
                assert(animation);
                scene_data->m_LinearAnimations[i] = dmHashString64(animation->name().c_str());
            }
        }

        uint32_t state_machine_count = (uint32_t)artboard->stateMachineCount();
        if (artboard && state_machine_count > 0)
        {
            scene_data->m_StateMachines.SetCapacity(state_machine_count);
            scene_data->m_StateMachines.SetSize(state_machine_count);

            for (int i = 0; i < state_machine_count; ++i)
            {
                rive::StateMachine* state_machine = artboard->stateMachine(i);
                assert(state_machine);
                scene_data->m_StateMachines[i] = dmHashString64(state_machine->name().c_str());
            }
        }

        if (artboard)
        {
            SetupBones(scene_data, path);
        }
    }

    static dmResource::Result ResourceType_RiveData_Create(const dmResource::ResourceCreateParams& params)
    {
        rive::File* file          = 0;
        rive::BinaryReader reader = rive::BinaryReader((uint8_t*) params.m_Buffer, params.m_BufferSize);
        rive::ImportResult result = rive::File::import(reader, &file);

        if (result != rive::ImportResult::success)
        {
            params.m_Resource->m_Resource = 0;
            return  dmResource::RESULT_INVALID_DATA;
        }

        RiveSceneData* scene_data = new RiveSceneData();

        SetupData(scene_data, file, params.m_Filename);

        params.m_Resource->m_Resource     = (void*) scene_data;
        params.m_Resource->m_ResourceSize = 0;

        return dmResource::RESULT_OK;
    }

    static void DeleteData(RiveSceneData* scene_data)
    {
        delete scene_data->m_File;
        delete scene_data;
    }

    static dmResource::Result ResourceType_RiveData_Destroy(const dmResource::ResourceDestroyParams& params)
    {
        RiveSceneData* scene_data = (RiveSceneData*)params.m_Resource->m_Resource;
        DeleteData(scene_data);
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResourceType_RiveData_Recreate(const dmResource::ResourceRecreateParams& params)
    {
        rive::File* file          = 0;
        rive::BinaryReader reader = rive::BinaryReader((uint8_t*) params.m_Buffer, params.m_BufferSize);
        rive::ImportResult result = rive::File::import(reader, &file);

        if (result != rive::ImportResult::success)
        {
            // If we cannot load the new file, let's keep the old one
            return dmResource::RESULT_INVALID_DATA;
        }

        if (params.m_Resource->m_Resource != 0)
        {
            RiveSceneData* data = (RiveSceneData*) params.m_Resource->m_Resource;
            params.m_Resource->m_Resource = 0;
            DeleteData(data);
        }

        RiveSceneData* scene_data = new RiveSceneData();

        SetupData(scene_data, file, params.m_Filename);

        params.m_Resource->m_Resource     = (void*) scene_data;
        params.m_Resource->m_ResourceSize = 0;

        return dmResource::RESULT_OK;
    }

    static dmResource::Result RegisterResourceType_RiveData(dmResource::ResourceTypeRegisterContext& ctx)
    {
        return dmResource::RegisterType(ctx.m_Factory,
                                           ctx.m_Name,
                                           0, // context
                                           0, // preload
                                           ResourceType_RiveData_Create,
                                           0, // post create
                                           ResourceType_RiveData_Destroy,
                                           ResourceType_RiveData_Recreate);

    }
}


DM_DECLARE_RESOURCE_TYPE(ResourceTypeRiveData, "rivc", dmRive::RegisterResourceType_RiveData, 0);
