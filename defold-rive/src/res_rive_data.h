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

#ifndef DM_RES_RIVE_DATA_H
#define DM_RES_RIVE_DATA_H

#include <stdint.h>
#include "defold/renderer.h"

#include <rive/refcnt.hpp>
#include <rive/command_queue.hpp>

namespace dmRive
{
    struct RiveSceneData
    {
        dmhash_t         m_PathHash;
        rive::FileHandle m_File;
        HRenderContext   m_RiveRenderContext;
    };
}

#endif // DM_RES_RIVE_DATA_H
