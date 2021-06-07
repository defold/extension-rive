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

#include "res_rive_data.h"
#include <dmsdk/dlib/log.h>
#include <dmsdk/resource/resource.h>

namespace dmRive
{
    static dmResource::Result ResourceType_RiveData_Create(const dmResource::ResourceCreateParams& params)
    {
        // TODO: Load rive data and store it as the resource
        // You can either use a type from rive-cpp directly, or create a RiveDataResource in the header file
        // Also note that the RiveSceneResource points to this resource (with a void*), you should update that type
        // to make it usable in comp_rive.cpp

        params.m_Resource->m_Resource = (void*)params.m_Buffer;
        params.m_Resource->m_ResourceSize = params.m_BufferSize;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResourceType_RiveData_Destroy(const dmResource::ResourceDestroyParams& params)
    {
        // TODO: Destroy rive data
        void* data = params.m_Resource->m_Resource;

        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResourceType_RiveData_Recreate(const dmResource::ResourceRecreateParams& params)
    {
        // TODO: Reload the data
        void* prev_data = params.m_Resource->m_Resource;

        // Setup new data
        params.m_Resource->m_Resource = (void*)params.m_Buffer;
        params.m_Resource->m_ResourceSize = params.m_BufferSize;

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
