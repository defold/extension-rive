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

#if !defined(DM_RIVE_UNSUPPORTED)

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
#include <common/atlas.h>
#include <common/factory.h>

namespace dmRive
{
    static void SetupData(RiveSceneData* scene_data, rive::File* file, const char* path)
    {
        scene_data->m_File = file;

        scene_data->m_ArtboardDefault = scene_data->m_File->artboardDefault();
        rive::Artboard* artboard = scene_data->m_ArtboardDefault.get();
        if (!artboard)
            return;

        uint32_t animation_count = (uint32_t)artboard->animationCount();
        if (animation_count > 0)
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
        if (state_machine_count > 0)
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
    }

    static dmResource::Result ResourceType_RiveData_Create(const dmResource::ResourceCreateParams& params)
    {
        dmRive::DefoldFactory* rive_factory = (dmRive::DefoldFactory*)params.m_Context;
        rive::Span<uint8_t> data((uint8_t*)params.m_Buffer, params.m_BufferSize);

        // Creates DefoldRenderImage with a hashed name for each image resource
        AtlasNameResolver atlas_resolver = AtlasNameResolver();

        rive::ImportResult result;
        std::unique_ptr<rive::File> file = rive::File::import(data,
                                                        rive_factory,
                                                        &result,
                                                        &atlas_resolver);

        if (result != rive::ImportResult::success)
        {
            params.m_Resource->m_Resource = 0;
            return  dmResource::RESULT_INVALID_DATA;
        }

        RiveSceneData* scene_data = new RiveSceneData();

        SetupData(scene_data, file.release(), params.m_Filename);

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
        dmRive::DefoldFactory* rive_factory = (dmRive::DefoldFactory*)params.m_Context;
        rive::Span<uint8_t> data((uint8_t*)params.m_Buffer, params.m_BufferSize);

        AtlasNameResolver atlas_resolver = AtlasNameResolver();

        rive::ImportResult result;
        std::unique_ptr<rive::File> file = rive::File::import(data,
                                                        rive_factory,
                                                        &result,
                                                        &atlas_resolver);

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

        SetupData(scene_data, file.release(), params.m_Filename);

        params.m_Resource->m_Resource     = (void*) scene_data;
        params.m_Resource->m_ResourceSize = 0;

        return dmResource::RESULT_OK;
    }

    static dmResource::Result RegisterResourceType_RiveData(dmResource::ResourceTypeRegisterContext& ctx)
    {
        dmRive::DefoldFactory* rive_ext_context = new dmRive::DefoldFactory();
        ctx.m_Contexts->Put(ctx.m_NameHash, rive_ext_context);
        return dmResource::RegisterType(ctx.m_Factory,
                                           ctx.m_Name,
                                           rive_ext_context,
                                           0, // preload
                                           ResourceType_RiveData_Create,
                                           0, // post create
                                           ResourceType_RiveData_Destroy,
                                           ResourceType_RiveData_Recreate);

    }

    static dmResource::Result DeregisterResourceType_RiveData(dmResource::ResourceTypeRegisterContext& ctx)
    {
        dmRive::DefoldFactory** context = (dmRive::DefoldFactory**)ctx.m_Contexts->Get(ctx.m_NameHash);
        delete *context;
        return dmResource::RESULT_OK;
    }
}


DM_DECLARE_RESOURCE_TYPE(ResourceTypeRiveData, "rivc", dmRive::RegisterResourceType_RiveData, dmRive::DeregisterResourceType_RiveData);

#endif // DM_RIVE_UNSUPPORTED
