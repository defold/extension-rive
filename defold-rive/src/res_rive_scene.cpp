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

#include "res_rive_scene.h"
#include <common/atlas.h>

#include <dmsdk/dlib/log.h>
#include <dmsdk/resource/resource.h>

namespace dmRive
{
    static dmResource::Result AcquireResources(dmResource::HFactory factory, RiveSceneResource* resource, const char* filename)
    {
        // The rive file (.riv)
        dmResource::Result result = dmResource::Get(factory, resource->m_DDF->m_Scene, (void**) &resource->m_Scene);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }

        resource->m_Atlas = 0;
        resource->m_TextureSet = 0;

        if (resource->m_DDF->m_Atlas[0] != 0)
        {
            dmResource::Result result = dmResource::Get(factory, resource->m_DDF->m_Atlas, (void**) &resource->m_TextureSet); // .atlas -> .texturesetc
            resource->m_Atlas = dmRive::CreateAtlas(resource->m_TextureSet->m_TextureSet);
        }

        return dmResource::RESULT_OK;
    }

    static void ReleaseResources(dmResource::HFactory factory, RiveSceneResource* resource)
    {
        if (resource->m_Atlas)
            dmRive::DestroyAtlas(resource->m_Atlas);
        if (resource->m_TextureSet)
            dmResource::Release(factory, resource->m_TextureSet);
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
        if (resource->m_Scene != 0x0)
            dmResource::Release(factory, resource->m_Scene);
    }

    static dmResource::Result ResourceTypePreload(const dmResource::ResourcePreloadParams& params)
    {
        dmRiveDDF::RiveSceneDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRiveDDF_RiveSceneDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Scene); // the .riv file

        // Will throw error if we don't check this here
        if (ddf->m_Atlas[0] != 0)
        {
            dmResource::PreloadHint(params.m_HintInfo, ddf->m_Atlas);
        }

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResourceTypeCreate(const dmResource::ResourceCreateParams& params)
    {
        RiveSceneResource* resource = new RiveSceneResource();
        resource->m_DDF = (dmRiveDDF::RiveSceneDesc*) params.m_PreloadData;
        dmResource::Result r = AcquireResources(params.m_Factory, resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) resource;
        }
        else
        {
            ReleaseResources(params.m_Factory, resource);
            delete resource;
        }
        return r;
    }

    static dmResource::Result ResourceTypeDestroy(const dmResource::ResourceDestroyParams& params)
    {
        RiveSceneResource* resource = (RiveSceneResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, resource);
        delete resource;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResourceTypeRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRiveDDF::RiveSceneDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRiveDDF_RiveSceneDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        RiveSceneResource* resource = (RiveSceneResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, resource);
        resource->m_DDF = ddf;
        return AcquireResources(params.m_Factory, resource, params.m_Filename);
    }

    static dmResource::Result RegisterResourceType(dmResource::ResourceTypeRegisterContext& ctx)
    {
        return dmResource::RegisterType(ctx.m_Factory,
                                           ctx.m_Name,
                                           0, // context
                                           ResourceTypePreload,
                                           ResourceTypeCreate,
                                           0, // post create
                                           ResourceTypeDestroy,
                                           ResourceTypeRecreate);

    }
}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeRiveScene, "rivescenec", dmRive::RegisterResourceType, 0);

#endif // DM_RIVE_UNSUPPORTED
