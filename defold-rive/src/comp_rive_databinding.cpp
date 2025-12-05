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
#include <rive/data_bind/data_values/data_type.hpp>

//debug
#include <rive/viewmodel/runtime/viewmodel_runtime.hpp>
#include <rive/viewmodel/viewmodel.hpp>

namespace dmRive
{

#define CHECK_VMIR(VMIR, HANDLE) \
    if (!(VMIR)) { \
        dmLogError("%s:%d: No viewmodel runtime instance with handle '%u'", __FUNCTION__, __LINE__, (HANDLE)); \
        return false; \
    }

#define CHECK_PROP_RESULT(PROP, TYPE, PATH) \
    if (!(PROP)) { \
        dmLogError("%s:%d: No property of type '%s', with path '%s'", __FUNCTION__, __LINE__, (TYPE), (PATH)); \
        return false; \
    }

static rive::ViewModelInstanceRuntime* FromHandle(RiveComponent* component, uint32_t handle)
{
    rive::ViewModelInstanceRuntime** pvmir = component->m_ViewModelInstanceRuntimes.Get(handle);
    return pvmir ? *pvmir : 0;
}

static rive::ViewModelRuntime* FindViewModelRuntimeByHash(RiveComponent* component, dmhash_t name_hash)
{
    assert(0 && "TODO: UNIMPLEMENTED");
    return 0;
    // dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;
    // rive::File* file = data->m_File;

    // size_t n = file->viewModelCount();
    // for (size_t i = 0; i < n; ++i)
    // {
    //     rive::ViewModelRuntime* vmr = file->viewModelByIndex(i);
    //     dmhash_t hash = dmHashString64(vmr->name().c_str());
    //     if (hash == name_hash)
    //         return vmr;
    // }
    // return 0;
}

static rive::ViewModelInstanceRuntime* CreateViewModelInstanceRuntimeByHash(RiveComponent* component, dmhash_t name_hash)
{
    assert(0 && "TODO: UNIMPLEMENTED");
    return 0;
    // dmRive::RiveSceneData* data = component->m_Resource->m_Scene->m_Scene;
    // rive::File* file = data->m_File;

    // if (name_hash != 0)
    // {
    //     rive::ViewModelRuntime* vmr = FindViewModelRuntimeByHash(component, name_hash);
    //     return vmr ? vmr->createInstance() : 0;
    // }

    // // Create a default view model instance
    // if (!component->m_ArtboardInstance)
    //     return 0;

    // rive::ViewModelRuntime* vmr = file->defaultArtboardViewModel(component->m_ArtboardInstance.get());
    // if (vmr)
    // {
    //     return vmr->createDefaultInstance();
    // }
    // return 0;
}

void SetViewModelInstanceRuntime(RiveComponent* component, rive::ViewModelInstanceRuntime* vmir)
{
    // if (component->m_ArtboardInstance)
    // {
    //     component->m_ArtboardInstance->bindViewModelInstance(vmir->instance());
    // }

    // if (component->m_StateMachineInstance)
    // {
    //     component->m_StateMachineInstance->bindViewModelInstance(vmir->instance());
    // }
}

void DebugModelViews(RiveComponent* component)
{
    assert(0 && "TODO: UNIMPLEMENTED");
    return;
    // dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;

    // size_t num_viewmodels = data->m_File->viewModelCount();
    // for (size_t i = 0; i < num_viewmodels; ++i)
    // {
    //     rive::ViewModelRuntime* vmr = data->m_File->viewModelByIndex(i);

    //     dmLogInfo("NAME: '%s'\n", vmr->name().c_str());

    //     size_t num_properties = vmr->propertyCount();
    //     std::vector<rive::PropertyData> pdatas = vmr->properties();
    //     for (size_t j = 0; j < num_properties; ++j)
    //     {
    //         rive::PropertyData& property = pdatas[j];
    //         dmLogInfo("  DATA: %d '%s'\n", (int)property.type, property.name.c_str());
    //     }

    //     size_t num_instances = vmr->instanceCount();
    //     dmLogInfo("  #instances: %u\n", (uint32_t)num_instances);
    //     std::vector<std::string> names = vmr->instanceNames();
    //     for (size_t j = 0; j < names.size(); ++j)
    //     {
    //         dmLogInfo("  INST: '%s'\n", names[j].c_str());
    //     }
    // }
}

void DebugVMIR(rive::ViewModelInstanceRuntime* vmir)
{
    printf("NAME: '%s'\n", vmir->name().c_str());

    size_t num_properties = vmir->propertyCount();
    std::vector<rive::PropertyData> pdatas = vmir->properties();
    for (size_t j = 0; j < num_properties; ++j)
    {
        rive::PropertyData& property = pdatas[j];
        printf("  DATA: %d '%s'\n", (int)property.type, property.name.c_str());
    }

    printf("  instance: '%s' %p\n", vmir->instance()->name().c_str(), vmir->instance().get());
}

// Scripting api + helpers

bool CompRiveSetViewModelInstanceRuntime(RiveComponent* component, uint32_t handle)
{
    dmLogInfo("Setting ViewModelInstanceRuntime");
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

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
    uint32_t handle = dmRive::INVALID_HANDLE;
    rive::ViewModelInstanceRuntime* vmir = dmRive::CreateViewModelInstanceRuntimeByHash(component, name_hash);
    if (vmir)
    {
        handle = component->m_HandleCounter++;

        if (component->m_ViewModelInstanceRuntimes.Full())
            component->m_ViewModelInstanceRuntimes.OffsetCapacity(4);
        component->m_ViewModelInstanceRuntimes.Put(handle, vmir);
    }
    else
    {
        dmLogError("Failed to create ViewModelInstance of name '%s'", dmHashReverseSafe64(name_hash));
    }

    //dmRive::DebugModelViews(component);
    return handle;
}

bool CompRiveDestroyViewModelInstanceRuntime(RiveComponent* component, uint32_t handle)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    // According to the Rive team, there is no need to free the pointer, as it belongs to runtime
    component->m_ViewModelInstanceRuntimes.Erase(handle);
    return true;
}

// **************************************************************************************************************
// PROPERTIES

static rive::DataType GetDataType(rive::ViewModelInstanceRuntime* vmir, const char* path)
{
    rive::ViewModelInstanceValueRuntime* prop = vmir->property(path);
    if (prop)
        return prop->dataType();
    return rive::DataType::none;
};

bool GetPropertyDataType(RiveComponent* component, uint32_t handle, const char* path, rive::DataType* type)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    if (!vmir)
    {
        return false;
    }

    *type = GetDataType(vmir, path);
    return *type != rive::DataType::none;
}

bool CompRiveRuntimeSetPropertyBool(RiveComponent* component, uint32_t handle, const char* path, bool value)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceBooleanRuntime* prop = vmir->propertyBoolean(path);
    CHECK_PROP_RESULT(prop, "boolean", path);
    prop->value(value);
    return true;
}

bool CompRiveRuntimeSetPropertyF32(RiveComponent* component, uint32_t handle, const char* path, float value)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceNumberRuntime* prop = vmir->propertyNumber(path);
    CHECK_PROP_RESULT(prop, "number", path);
    if (!prop)
    {
        dmLogError("No property of type number, with path '%s'", path);
        return false;
    }

    prop->value(value);
    return true;
}

bool CompRiveRuntimeSetPropertyColor(RiveComponent* component, uint32_t handle, const char* path, dmVMath::Vector4* color)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceColorRuntime* prop = vmir->propertyColor(path);
    CHECK_PROP_RESULT(prop, "color", path);

    prop->argb(255 * color->getW(), 255 * color->getX(), 255 * color->getY(), 255 * color->getZ());
    return true;
}

bool CompRiveRuntimeSetPropertyString(RiveComponent* component, uint32_t handle, const char* path, const char* value)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceStringRuntime* prop = vmir->propertyString(path);
    CHECK_PROP_RESULT(prop, "string", path);

    prop->value(value);
    return true;
}

bool CompRiveRuntimeSetPropertyEnum(RiveComponent* component, uint32_t handle, const char* path, const char* value)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceEnumRuntime* prop = vmir->propertyEnum(path);
    CHECK_PROP_RESULT(prop, "enum", path);

    prop->value(value);
    return true;
}

bool CompRiveRuntimeSetPropertyTrigger(RiveComponent* component, uint32_t handle, const char* path)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceTriggerRuntime* prop = vmir->propertyTrigger(path);
    CHECK_PROP_RESULT(prop, "trigger", path);

    prop->trigger();
    return true;
}

bool CompRiveRuntimeSetPropertyImage(RiveComponent* component, uint32_t handle, const char* path, rive::RenderImage* image)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceAssetImageRuntime* prop = vmir->propertyImage(path);
    CHECK_PROP_RESULT(prop, "image", path);

    prop->value(image);
    return true;
}

bool CompRiveRuntimeGetPropertyBool(RiveComponent* component, uint32_t handle, const char* path, bool* value)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceBooleanRuntime* prop = vmir->propertyBoolean(path);
    CHECK_PROP_RESULT(prop, "boolean", path);
    *value = prop->value();
    return true;
}

bool CompRiveRuntimeGetPropertyF32(RiveComponent* component, uint32_t handle, const char* path, float* value)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceNumberRuntime* prop = vmir->propertyNumber(path);
    CHECK_PROP_RESULT(prop, "number", path);
    *value = prop->value();
    return true;
}

bool CompRiveRuntimeGetPropertyColor(RiveComponent* component, uint32_t handle, const char* path, dmVMath::Vector4* color)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceColorRuntime* prop = vmir->propertyColor(path);
    CHECK_PROP_RESULT(prop, "color", path);

    uint32_t argb = (uint32_t)prop->value();
    float a = ((argb >> 24) & 0xFF) / 255.0f;
    float r = ((argb >> 16) & 0xFF) / 255.0f;
    float g = ((argb >>  8) & 0xFF) / 255.0f;
    float b = ((argb >>  0) & 0xFF) / 255.0f;
    *color = dmVMath::Vector4(r, g, b, a);
    return true;
}

bool CompRiveRuntimeGetPropertyString(RiveComponent* component, uint32_t handle, const char* path, const char** value)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceStringRuntime* prop = vmir->propertyString(path);
    CHECK_PROP_RESULT(prop, "string", path);

    *value = prop->value().c_str(); // the temp string is not for storage
    return true;
}

bool CompRiveRuntimeGetPropertyEnum(RiveComponent* component, uint32_t handle, const char* path, const char** value)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceEnumRuntime* prop = vmir->propertyEnum(path);
    CHECK_PROP_RESULT(prop, "enum", path);

    *value = prop->value().c_str(); // the temp string is not for storage
    return true;
}

// LISTS

bool CompRiveRuntimeListAddInstance(RiveComponent* component, uint32_t handle, const char* path, uint32_t instance)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceListRuntime* prop = vmir->propertyList(path);
    CHECK_PROP_RESULT(prop, "list", path);

    rive::ViewModelInstanceRuntime* instance_vmir = FromHandle(component, instance);
    prop->addInstance(instance_vmir);
    return true;
}

bool CompRiveRuntimeListRemoveInstance(RiveComponent* component, uint32_t handle, const char* path, uint32_t instance)
{
    rive::ViewModelInstanceRuntime* vmir = FromHandle(component, handle);
    CHECK_VMIR(vmir, handle);

    rive::ViewModelInstanceListRuntime* prop = vmir->propertyList(path);
    CHECK_PROP_RESULT(prop, "list", path);

    rive::ViewModelInstanceRuntime* instance_vmir = FromHandle(component, instance);
    prop->removeInstance(instance_vmir);
    return true;
}


} // namespace

#endif // DM_RIVE_UNSUPPORTED
