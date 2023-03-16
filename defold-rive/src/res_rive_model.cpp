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

#include "res_rive_model.h"

#include <dmsdk/dlib/log.h>
#include <dmsdk/resource/resource.h>

namespace dmRive
{
    static dmResource::Result AcquireResources(dmResource::HFactory factory, RiveModelResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_DDF->m_Scene, (void**) &resource->m_Scene);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }

        result = dmResource::Get(factory, resource->m_DDF->m_Material, (void**) &resource->m_Material);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }
        resource->m_CreateGoBones = resource->m_DDF->m_CreateGoBones;
        return dmResource::RESULT_OK;
    }

    static void ReleaseResources(dmResource::HFactory factory, RiveModelResource* resource)
    {
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
        if (resource->m_Scene != 0x0)
            dmResource::Release(factory, resource->m_Scene);
        if (resource->m_Material != 0x0)
            dmResource::Release(factory, resource->m_Material);
    }

    static dmResource::Result ResourceType_RiveModel_Preload(const dmResource::ResourcePreloadParams& params)
    {
        dmRiveDDF::RiveModelDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRiveDDF_RiveModelDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Scene);       // the .rivescenec file
        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResourceType_RiveModel_Create(const dmResource::ResourceCreateParams& params)
    {
        RiveModelResource* resource = new RiveModelResource();
        resource->m_DDF = (dmRiveDDF::RiveModelDesc*) params.m_PreloadData;
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

    static dmResource::Result ResourceType_RiveModel_Destroy(const dmResource::ResourceDestroyParams& params)
    {
        RiveModelResource* resource = (RiveModelResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, resource);
        delete resource;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResourceType_RiveModel_Recreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRiveDDF::RiveModelDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRiveDDF_RiveModelDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        RiveModelResource* resource = (RiveModelResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, resource);
        resource->m_DDF = ddf;
        return AcquireResources(params.m_Factory, resource, params.m_Filename);
    }

    static dmResource::Result RegisterResourceType_RiveModel(dmResource::ResourceTypeRegisterContext& ctx)
    {
        return dmResource::RegisterType(ctx.m_Factory,
                                           ctx.m_Name,
                                           0, // context
                                           ResourceType_RiveModel_Preload,
                                           ResourceType_RiveModel_Create,
                                           0, // post create
                                           ResourceType_RiveModel_Destroy,
                                           ResourceType_RiveModel_Recreate);

    }
}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeRiveModel, "rivemodelc", dmRive::RegisterResourceType_RiveModel, 0);

#endif // DM_RIVE_UNSUPPORTED
