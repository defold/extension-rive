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

#include <stdio.h>

#include "comp_rive.h"
#include "comp_rive_private.h"
#include "res_rive_data.h"
#include "res_rive_scene.h"
#include "res_rive_model.h"

#include <rive/animation/state_machine_instance.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/runtime/viewmodel_instance_runtime.hpp>
#include <rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp>
#include <rive/viewmodel/runtime/viewmodel_instance_number_runtime.hpp>

//debug
#include <rive/viewmodel/runtime/viewmodel_runtime.hpp>
#include <rive/viewmodel/viewmodel.hpp>

namespace dmRive
{

static rive::ViewModelRuntime* FindViewModelRuntimeByHash(RiveComponent* component, dmhash_t name_hash)
{
    dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;
    rive::File* file = data->m_File;

    size_t n = file->viewModelCount();
    for (size_t i = 0; i < n; ++i)
    {
        rive::ViewModelRuntime* vmr = file->viewModelByIndex(i);
        dmhash_t hash = dmHashString64(vmr->name().c_str());
        if (hash == name_hash)
            return vmr;
    }
    return 0;
}

static rive::ViewModelInstanceRuntime* CreateViewModelInstanceRuntimeByHash(RiveComponent* component, dmhash_t name_hash)
{
    dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;
    rive::File* file = data->m_File;

    rive::ViewModelRuntime* vmr = name_hash != 0 ? FindViewModelRuntimeByHash(component, name_hash) : file->viewModelByIndex(0);

    printf("CreateViewModelInstanceRuntime: selected ViewModelRuntime '%s'\n", vmr->name().c_str());
    return vmr->createInstance();
}

void SetViewModelInstanceRuntime(RiveComponent* component, rive::ViewModelInstanceRuntime* vmir)
{
    if (component->m_StateMachineInstance)
    {
        printf("SetViewModelInstance:  m_StateMachineInstance\n");
        component->m_StateMachineInstance->bindViewModelInstance(vmir->instance());
    }
    else
    {
        printf("SetViewModelInstance: m_ArtboardInstance\n");
        component->m_ArtboardInstance->bindViewModelInstance(vmir->instance());
    }
}

void DebugModelViews(RiveComponent* component)
{
    dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;

    size_t num_viewmodels = data->m_File->viewModelCount();
    for (size_t i = 0; i < num_viewmodels; ++i)
    {
        rive::ViewModelRuntime* vmr = data->m_File->viewModelByIndex(i);

        printf("NAME: '%s'\n", vmr->name().c_str());

        size_t num_properties = vmr->propertyCount();
        std::vector<rive::PropertyData> pdatas = vmr->properties();
        for (size_t j = 0; j < num_properties; ++j)
        {
            rive::PropertyData& property = pdatas[j];
            printf("  DATA: %d '%s'\n", property.type, property.name.c_str());
        }

        size_t num_instances = vmr->instanceCount();
        printf("  #instances: %u\n", (uint32_t)num_instances);
        std::vector<std::string> names = vmr->instanceNames();
        for (size_t j = 0; j < names.size(); ++j)
        {
            printf("  INST: '%s'\n", names[j].c_str());
        }
    }
}

// Scripting api + helpers

static rive::ViewModelInstanceRuntime* FromHandle(RiveComponent* component, uint32_t handle)
{
    rive::ViewModelInstanceRuntime** pvmir = component->m_ViewModelInstanceRuntimes.Get(handle);
    return pvmir ? *pvmir : 0;
}

bool CompRiveSetViewModelInstanceRuntime(RiveComponent* component, uint32_t handle)
{
    dmLogInfo("Setting default ViewModelInstance");
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    if (!vmir)
    {
        return false;
    }

    dmRive::SetViewModelInstanceRuntime(component, vmir);
    component->m_CurrentViewModelInstanceRuntime = handle;
    return true;
}

uint32_t CompRiveGetViewModelInstanceRuntime(RiveComponent* component)
{
    return component->m_CurrentViewModelInstanceRuntime;
}

uint32_t CompRiveCreateViewModelInstanceRuntime(RiveComponent* component, dmhash_t name_hash)
{
    dmLogInfo("%s", __FUNCTION__);

    uint32_t handle = dmRive::INVALID_HANDLE;
    rive::ViewModelInstanceRuntime* vmir = dmRive::CreateViewModelInstanceRuntimeByHash(component, name_hash);
    if (vmir)
    {
        handle = component->m_HandleCounter++;

        if (component->m_ViewModelInstanceRuntimes.Full())
            component->m_ViewModelInstanceRuntimes.OffsetCapacity(4);
        component->m_ViewModelInstanceRuntimes.Put(handle, vmir);

        // test
        //dmLogInfo("Setting number ViewModelInstance");
        //dmRive::SetViewModelPropertyNumber(component, vmir, "1st_Num", 10);
        // dmRive::SetViewModelPropertyNumber(component, vmir, "min", -30);
        // dmRive::SetViewModelPropertyNumber(component, vmir, "max", 150);
        // dmRive::SetViewModelPropertyNumber(component, vmir, "current", 75);
    }
    else
    {
        dmLogError("Failed to create ViewModelInstance of name '%s'", dmHashReverseSafe64(name_hash));
    }

    //dmRive::DebugModelViews(component);
    return handle;
}


bool CompRiveRuntimePropertyBool(RiveComponent* component, uint32_t handle, const char* path, bool value)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    if (!vmir)
    {
        return false;
    }

    rive::ViewModelInstanceBooleanRuntime* prop = vmir->propertyBoolean(path);
    if (!prop)
    {
        dmLogError("No property of type number, with path '%s'", path);
        return false;
    }

    prop->value(value);

    dmLogInfo("%s:%d: Setting boolean: %s = %d", __FUNCTION__, __LINE__, path, (int)value);
    return true;
}

bool CompRiveRuntimePropertyF32(RiveComponent* component, uint32_t handle, const char* path, float value)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    if (!vmir)
    {
        return false;
    }

    rive::ViewModelInstanceNumberRuntime* prop = vmir->propertyNumber(path);
    if (!prop)
    {
        dmLogError("No property of type number, with path '%s'", path);
        return false;
    }

    prop->value(value);

    dmLogInfo("%s:%d: Setting number: %s = %f", __FUNCTION__, __LINE__, path, value);
    return true;
}

bool CompRiveRuntimePropertyColor(RiveComponent* component, uint32_t handle, const char* path, dmVMath::Vector4* color)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    if (!vmir)
    {
        return false;
    }

    rive::ViewModelInstanceColorRuntime* prop = vmir->propertyColor(path);
    if (!prop)
    {
        dmLogError("No property of type number, with path '%s'", path);
        return false;
    }

    prop->argb(255 * color->getW(), 255 * color->getX(), 255 * color->getY(), 255 * color->getZ());

    dmLogInfo("%s:%d: Setting color: %s = %d %d %d %d", __FUNCTION__, __LINE__, path,
        (int)(255 * color->getW()),
        (int)(255 * color->getX()),
        (int)(255 * color->getY()),
        (int)(255 * color->getZ()));
    return true;
}

} // namespace

#endif // DM_RIVE_UNSUPPORTED
