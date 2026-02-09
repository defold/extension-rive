// Copyright 2026 The Defold Foundation
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

#include "viewmodel_instance_registry.h"

#include <assert.h>

#include <stdint.h>

#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/mutex.h>

#include "script_rive_listeners.h"

namespace dmRive
{
    namespace
    {
        typedef uint64_t                                     RegistryKey;

        dmMutex::HMutex                                      g_ViewModelRegistryMutex = 0;
        dmHashTable<RegistryKey, ViewModelInstanceListener*> g_ViewModelInstanceListeners;

        struct DeleteRegistryContext
        {
        };

        RegistryKey                                          ToRegistryKey(rive::ViewModelInstanceHandle handle)
        {
            return (RegistryKey)(uintptr_t)handle;
        }

        void DeleteRegistryEntry(DeleteRegistryContext*, const RegistryKey*, ViewModelInstanceListener** v)
        {
            if (v && *v)
            {
                delete *v;
                *v = 0;
            }
        }

        void EnsureRegistryCapacity(uint32_t capacity)
        {
            if (!g_ViewModelRegistryMutex)
            {
                g_ViewModelRegistryMutex = dmMutex::New();
            }
            uint32_t current_capacity = g_ViewModelInstanceListeners.Capacity();
            if (current_capacity >= capacity)
            {
                return;
            }
            uint32_t grow = capacity - current_capacity;
            g_ViewModelInstanceListeners.OffsetCapacity((int32_t)grow);
        }
    } // namespace

    void RegisterViewModelInstanceListener(rive::ViewModelInstanceHandle handle, ViewModelInstanceListener* listener)
    {
        if (handle == RIVE_NULL_HANDLE || listener == 0)
        {
            return;
        }

        EnsureRegistryCapacity(32);
        DM_MUTEX_SCOPED_LOCK(g_ViewModelRegistryMutex);
        if (g_ViewModelInstanceListeners.Full())
        {
            EnsureRegistryCapacity(g_ViewModelInstanceListeners.Capacity() * 2);
        }
        const RegistryKey key = ToRegistryKey(handle);
        assert(g_ViewModelInstanceListeners.Get(key) == 0);
        g_ViewModelInstanceListeners.Put(key, listener);
    }

    ViewModelInstanceListener* GetViewModelInstanceListener(rive::ViewModelInstanceHandle handle)
    {
        if (handle == RIVE_NULL_HANDLE)
        {
            return 0;
        }

        EnsureRegistryCapacity(1);
        DM_MUTEX_SCOPED_LOCK(g_ViewModelRegistryMutex);
        const RegistryKey           key = ToRegistryKey(handle);
        ViewModelInstanceListener** entry = g_ViewModelInstanceListeners.Get(key);
        return entry ? *entry : 0;
    }

    void UnregisterViewModelInstanceListener(rive::ViewModelInstanceHandle handle)
    {
        if (handle == RIVE_NULL_HANDLE)
        {
            return;
        }

        EnsureRegistryCapacity(1);
        DM_MUTEX_SCOPED_LOCK(g_ViewModelRegistryMutex);
        const RegistryKey key = ToRegistryKey(handle);
        if (g_ViewModelInstanceListeners.Get(key))
        {
            g_ViewModelInstanceListeners.Erase(key);
        }
    }

    void ClearViewModelInstanceListeners()
    {
        EnsureRegistryCapacity(1);
        DM_MUTEX_SCOPED_LOCK(g_ViewModelRegistryMutex);
        DeleteRegistryContext context;
        g_ViewModelInstanceListeners.Iterate(DeleteRegistryEntry, &context);
        g_ViewModelInstanceListeners.Clear();
    }

} // namespace dmRive
