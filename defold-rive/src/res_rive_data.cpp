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

// Rive includes
#include <rive/artboard.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/assets/font_asset.hpp>
#include <rive/file.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/linear_animation.hpp>
#include <rive/animation/state_machine.hpp>

// Extension includes
#include "res_rive_data.h"
#include "script_rive.h"
#include "defold/renderer.h"
#include <common/atlas.h>
#include <common/font.h>
#include <common/factory.h>
#include <common/commands.h>

#include <string>

namespace dmRive
{
    static rive::FileHandle LoadFile(rive::rcp<rive::CommandQueue> queue, const char* path, const void* data, uint32_t data_size, RiveSceneData* scene_data)
    {
        rive::CommandQueue::FileListener* listener = ScriptGetFileListener();

        // RIVE: Currently their api doesn't support passing the bytes directly, but require you to make a copy of it.
        const uint8_t* _data = (const uint8_t*)data;
        std::vector<uint8_t> rive_data(_data, _data + data_size); // FULL COPY!!!

        rive::FileHandle file = queue->loadFile(rive_data, listener, (uint64_t)(uintptr_t)scene_data);
        return file;
    }

    static void SetupData(RiveSceneData* scene_data, rive::FileHandle file, const char* path, HRenderContext rive_render_context)
    {
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        // rive::Factory* rive_factory = dmRiveCommands::GetFactory();

        scene_data->m_PathHash = dmHashString64(path);
        scene_data->m_File = file;
        scene_data->m_RiveRenderContext = rive_render_context;

        //scene_data->m_ArtboardDefault = queue->

        // scene_data->m_ArtboardDefault = scene_data->m_File->artboardDefault();
        // if (!scene_data->m_ArtboardDefault)
        // {
        //     return;
        // }

        // size_t artboard_count = scene_data->m_File->artboardCount();

        // scene_data->m_ArtboardIdLists.SetCapacity(artboard_count);
        // scene_data->m_ArtboardIdLists.SetSize(artboard_count);

        // for (int i = 0; i < artboard_count; ++i)
        // {
        //     rive::Artboard* artboard = scene_data->m_File->artboard(i);

        //     RiveArtboardIdList* id_list = new RiveArtboardIdList();
        //     scene_data->m_ArtboardIdLists[i] = id_list;

        //     id_list->m_ArtboardNameHash = dmHashString64(artboard->name().c_str());

        //     // Setup state machine ID lists
        //     uint32_t state_machine_count = (uint32_t)artboard->stateMachineCount();
        //     if (state_machine_count)
        //     {
        //         id_list->m_StateMachines.SetCapacity(state_machine_count);
        //         id_list->m_StateMachines.SetSize(state_machine_count);

        //         for (int j = 0; j < state_machine_count; ++j)
        //         {
        //             rive::StateMachine* state_machine = artboard->stateMachine(j);
        //             id_list->m_StateMachines[j] = dmHashString64(state_machine->name().c_str());
        //         }
        //     }

        //     // Setup animation ID lists
        //     uint32_t animation_count = (uint32_t)artboard->animationCount();
        //     if (animation_count)
        //     {
        //         id_list->m_LinearAnimations.SetCapacity(animation_count);
        //         id_list->m_LinearAnimations.SetSize(animation_count);

        //         for (int j = 0; j < animation_count; ++j)
        //         {
        //             rive::LinearAnimation* animation = artboard->animation(j);
        //             id_list->m_LinearAnimations[j] = dmHashString64(animation->name().c_str());
        //         }
        //     }
        // }
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

        // // Creates DefoldRenderImage with a hashed name for each image resource
        // rive::rcp<AtlasNameResolver> atlas_resolver(new AtlasNameResolver(params->m_Factory, render_context_res));

        // rive::ImportResult result;
        // rive::rcp<rive::File> file = rive::File::import(data,
        //                                                 rive_factory,
        //                                                 &result,
        //                                                 atlas_resolver);

        // if (result != rive::ImportResult::success)
        // {
        //     dmResource::SetResource(params->m_Resource, 0);
        //     return dmResource::RESULT_INVALID_DATA;
        // }

        //scene_data->m_FileAssets.Swap(atlas_resolver->GetAssets());

        SetupData(scene_data, scene_data->m_File, params->m_Filename, render_context_res);

        dmResource::SetResource(params->m_Resource, scene_data);
        dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize);

        return dmResource::RESULT_OK;
    }

    static void DeleteData(RiveSceneData* scene_data)
    {
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        dmHashTable64<const char*>::Iterator iter = scene_data->m_ArtboardNames.GetIterator();
        while(iter.Next())
        {
            free((void*)iter.GetValue()); // Free the name
        }
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
        // HRenderContext render_context_res = (HRenderContext) params->m_Context;
        // rive::Span<uint8_t> data((uint8_t*)params->m_Buffer, params->m_BufferSize);

        // rive::Factory* rive_factory = dmRiveCommands::GetFactory();
        // assert(rive_factory != 0);

        // rive::rcp<AtlasNameResolver> atlas_resolver(new AtlasNameResolver(params->m_Factory, render_context_res));

        // rive::ImportResult result;
        // rive::rcp<rive::File> file = rive::File::import(data,
        //                                                 rive_factory,
        //                                                 &result,
        //                                                 atlas_resolver);

        // if (result != rive::ImportResult::success)
        // {
        //     // If we cannot load the new file, let's keep the old one
        //     return dmResource::RESULT_INVALID_DATA;
        // }

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
        old_data->m_ArtboardNames.Swap(scene_data->m_ArtboardNames);
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

    // Scripting functions

    static rive::FileAsset* FindAsset(RiveSceneData* resource, const char* asset_name)
    {
        dmLogError("MAWE TODO: Reimplement somehow!");
        // for (uint32_t i = 0; i < resource->m_FileAssets.Size(); ++i)
        // {
        //     rive::FileAsset* _asset = resource->m_FileAssets[i];
        //     const std::string& name = _asset->name();
        //     const char* name_str = name.c_str();
        //     if (strcmp(asset_name, name_str) == 0)
        //         return _asset;
        // }
        return 0;
    }

    dmResource::Result ResRiveDataSetAssetFromMemory(RiveSceneData* resource, const char* asset_name, void* payload, uint32_t payload_size)
    {
        rive::FileAsset* _asset = FindAsset(resource, asset_name);
        if (!_asset)
        {
            dmLogError("Rive scene doesn't have asset named '%s'", asset_name);
            return dmResource::RESULT_INVALID_DATA;
        }

        if (_asset->is<rive::ImageAsset>())
        {
            rive::ImageAsset* asset = _asset->as<rive::ImageAsset>();
            rive::rcp<rive::RenderImage> image = dmRive::LoadImageFromMemory(resource->m_RiveRenderContext, payload, payload_size);
            if (!image)
            {
                dmLogError("Failed to load asset '%s' from payload.", asset_name);
                return dmResource::RESULT_INVALID_DATA;
            }

            asset->renderImage(image);
            return dmResource::RESULT_OK;
        }
        else if (_asset->is<rive::FontAsset>())
        {
            rive::FontAsset* asset = _asset->as<rive::FontAsset>();
            rive::rcp<rive::Font> font = dmRive::LoadFontFromMemory(resource->m_RiveRenderContext, payload, payload_size);
            if (!font)
            {
                dmLogError("Failed to load font asset '%s' from payload.", asset_name);
                return dmResource::RESULT_INVALID_DATA;
            }

            asset->font(font);
            return dmResource::RESULT_OK;
        }

        dmLogError("We currently don't support swapping the asset type of '%s'", asset_name);
        return dmResource::RESULT_NOT_SUPPORTED;
    }

    dmResource::Result ResRiveDataSetAsset(dmResource::HFactory factory, RiveSceneData* resource, const char* asset_name, const char* path)
    {
        rive::FileAsset* _asset = FindAsset(resource, asset_name);
        if (!_asset)
        {
            dmLogError("Rive scene doesn't have asset named '%s'", asset_name);
            return dmResource::RESULT_INVALID_DATA;
        }

        if (_asset->is<rive::ImageAsset>())
        {
            rive::ImageAsset* asset = _asset->as<rive::ImageAsset>();
            rive::rcp<rive::RenderImage> image = dmRive::LoadImageFromFactory(factory, resource->m_RiveRenderContext, path);
            if (!image)
            {
                dmLogError("Failed to load asset '%s' with path '%s'", asset_name, path);
                return dmResource::RESULT_INVALID_DATA;
            }

            asset->renderImage(image);
            return dmResource::RESULT_OK;
        }
        else if (_asset->is<rive::FontAsset>())
        {
            rive::FontAsset* asset = _asset->as<rive::FontAsset>();
            rive::rcp<rive::Font> font = dmRive::LoadFontFromFactory(factory, resource->m_RiveRenderContext, path);
            if (!font)
            {
                dmLogError("Failed to load font asset '%s' with path '%s'", asset_name, path);
                return dmResource::RESULT_INVALID_DATA;
            }

            asset->font(font);
            return dmResource::RESULT_OK;
        }

        dmLogError("We currently don't support swapping the asset type of '%s'", asset_name);
        return dmResource::RESULT_NOT_SUPPORTED;
    }

    rive::RenderImage* ResRiveDataCreateRenderImage(dmResource::HFactory factory, RiveSceneData* resource, uint8_t* data, uint32_t data_length)
    {
        rive::rcp<rive::RenderImage> image = dmRive::LoadImageFromMemory(resource->m_RiveRenderContext, data, data_length);
        return image.release();
    }

    const char* ResRiveDataFindArtboardName(RiveSceneData* resource, dmhash_t name_hash)
    {
        const char** name = resource->m_ArtboardNames.Get(name_hash);
        return name != 0 ? *name : 0;
    }
}


DM_DECLARE_RESOURCE_TYPE(ResourceTypeRiveData, "rivc", dmRive::RegisterResourceType_RiveData, dmRive::DeregisterResourceType_RiveData);

#endif // DM_RIVE_UNSUPPORTED
