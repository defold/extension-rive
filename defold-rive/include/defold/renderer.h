// Copyright 2020-2023 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

#ifndef DM_RIVE_RENDERER_H
#define DM_RIVE_RENDERER_H

#include <memory>
#include <dmsdk/resource/resource.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/render/render.h>

#include <rive/refcnt.hpp>
#include <rive/math/mat2d.hpp>

namespace rive
{
    class Factory;
    class Renderer;
    class RenderImage;
};

namespace dmRive
{
    typedef void*  HRenderContext;

    struct RenderBeginParams
    {
        bool    m_DoFinalBlit = true;
        uint8_t m_BackbufferSamples = 0;
    };

    HRenderContext               NewRenderContext();
    void                         DeleteRenderContext(HRenderContext context);
    rive::rcp<rive::RenderImage> CreateRiveRenderImage(HRenderContext context, void* bytes, uint32_t byte_count);
    rive::Factory*               GetRiveFactory(HRenderContext context);
    rive::Renderer*              GetRiveRenderer(HRenderContext context);
    rive::Mat2D                  GetViewTransform(HRenderContext context, dmRender::HRenderContext render_context);
    rive::Mat2D                  GetViewProjectionTransform(HRenderContext context, dmRender::HRenderContext render_context);
    void                         GetDimensions(HRenderContext context, uint32_t* width, uint32_t* height);
    void                         RenderBegin(HRenderContext context, dmResource::HFactory factory, const RenderBeginParams& params);
    void                         RenderEnd(HRenderContext context);

    dmGraphics::HTexture         GetBackingTexture(HRenderContext context);
}

#endif /* DM_RIVE_RENDERER_H */
