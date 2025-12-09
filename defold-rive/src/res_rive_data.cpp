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
#include <dmsdk/extension/extension.h>
#include <dmsdk/resource/resource.h>

// Extension includes
#include "res_rive_data.h"
#include "script_rive.h"
#include "defold/renderer.h"
#include <common/atlas.h>
#include <common/font.h>
#include <common/factory.h>
#include <common/commands.h>

#include <stdint.h>
#include <vector>

namespace dmRive
{
    static rive::FileHandle LoadFile(rive::rcp<rive::CommandQueue> queue, const char* path, const void* data, uint32_t data_size, RiveSceneData* scene_data)
    {
        // RIVE: Currently their api doesn't support passing the bytes directly, but require you to make a copy of it.
        const uint8_t* _data = (const uint8_t*)data;
        std::vector<uint8_t> rive_data(_data, _data + data_size);

        rive::FileHandle file = queue->loadFile(rive_data, 0, (uint64_t)(uintptr_t)scene_data);
        return file;
    }

    static void SetupData(RiveSceneData* scene_data, rive::FileHandle file, const char* path, HRenderContext rive_render_context)
    {
        scene_data->m_PathHash = dmHashString64(path);
        scene_data->m_File = file;
        scene_data->m_RiveRenderContext = rive_render_context;
    }

    static dmResource::Result ResourceType_RiveData_Create(const dmResource::ResourceCreateParams* params)
    {
        HRenderContext render_context_res = (HRenderContext) params->m_Context;

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        rive::Factory* rive_factory = dmRiveCommands::GetFactory();
        assert(rive_factory);

        RiveSceneData* scene_data = new RiveSceneData();
        scene_data->m_File = LoadFile(queue, params->m_Filename, params->m_Buffer, params->m_BufferSize, scene_data);
        if (!scene_data->m_File)
        {
            dmLogError("Failed to load '%s'", params->m_Filename);
            delete scene_data;
            return dmResource::RESULT_INVALID_DATA;
        }

        SetupData(scene_data, scene_data->m_File, params->m_Filename, render_context_res);

        dmResource::SetResource(params->m_Resource, scene_data);
        dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize);

        return dmResource::RESULT_OK;
    }

    static void DeleteData(RiveSceneData* scene_data)
    {
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        queue->deleteFile(scene_data->m_File);
        delete scene_data;
    }

    static dmResource::Result ResourceType_RiveData_Destroy(const dmResource::ResourceDestroyParams* params)
    {
        RiveSceneData* scene_data = (RiveSceneData*)dmResource::GetResource(params->m_Resource);
        DeleteData(scene_data);
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResourceType_RiveData_Recreate(const dmResource::ResourceRecreateParams* params)
    {
        HRenderContext render_context_res = (HRenderContext) params->m_Context;
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        RiveSceneData* scene_data = new RiveSceneData();
        scene_data->m_File = LoadFile(queue, params->m_Filename, params->m_Buffer, params->m_BufferSize, scene_data);
        if (!scene_data->m_File)
        {
            dmLogError("Failed to load '%s'", params->m_Filename);
            delete scene_data;
            return dmResource::RESULT_INVALID_DATA;
        }

        RiveSceneData* old_data = (RiveSceneData*)dmResource::GetResource(params->m_Resource);
        assert(old_data == 0);

        // We don't want to delete the old resource, as the pointer may be "live"
        rive::FileHandle tmp_handle = scene_data->m_File;
        scene_data->m_File = old_data->m_File;
        old_data->m_File = tmp_handle;

        DeleteData(scene_data);

        SetupData(old_data, scene_data->m_File, params->m_Filename, render_context_res);

        dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize);

        return dmResource::RESULT_OK;
    }

    static ResourceResult RegisterResourceType_RiveData(HResourceTypeContext ctx, HResourceType type)
    {
        HRenderContext rive_render_context = dmRiveCommands::GetDefoldRenderContext();
        assert(rive_render_context != 0);

        return (ResourceResult)dmResource::SetupType(ctx,
                                                     type,
                                                     rive_render_context,
                                                     0, // preload
                                                     ResourceType_RiveData_Create,
                                                     0, // post create
                                                     ResourceType_RiveData_Destroy,
                                                     ResourceType_RiveData_Recreate);

    }

    static ResourceResult DeregisterResourceType_RiveData(HResourceTypeContext ctx, HResourceType type)
    {
        return RESOURCE_RESULT_OK;
    }
}


DM_DECLARE_RESOURCE_TYPE(ResourceTypeRiveData, "rivc", dmRive::RegisterResourceType_RiveData, dmRive::DeregisterResourceType_RiveData);

#endif // DM_RIVE_UNSUPPORTED
