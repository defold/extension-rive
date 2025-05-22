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
    rive::ViewModelInstanceRuntime* CreateDataBinding(RiveComponent* component, const char* name)
    {
        dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;
        rive::File* file = data->m_File;
        rive::ViewModelRuntime* vmr;
        if (name)
            vmr = file->viewModelByName(name);
        else
            //vmr = file->defaultArtboardViewModel(component->m_ArtboardInstance.get());
            vmr = file->viewModelByIndex(0);

        return vmr->createInstance();
    }

    void SetViewModelInstance(RiveComponent* component, rive::ViewModelInstanceRuntime* vmir)
    {
        if (component->m_StateMachineInstance)
        {
            component->m_StateMachineInstance->bindViewModelInstance(vmir->instance());
        }
        else
        {
            component->m_ArtboardInstance->bindViewModelInstance(vmir->instance());
        }
    }

    bool SetViewModelPropertyNumber(RiveComponent* component, rive::ViewModelInstanceRuntime* vmir, const char* name, float number)
    {
        rive::ViewModelInstanceNumberRuntime* prop = vmir->propertyNumber(name);
        if (!prop)
        {
            dmLogError("No property named '%s' of type number", name);
            return false;
        }

        prop->value(number);
        return true;
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
}

#endif // DM_RIVE_UNSUPPORTED
