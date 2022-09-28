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

#ifndef DM_RIVE_FACTORY_H
#define DM_RIVE_FACTORY_H

#include <dmsdk/dlib/hash.h>

#include <rive/renderer.hpp>
#include <rive/factory.hpp>

namespace dmRive {

template<typename T>
class DefoldBuffer : public rive::RenderBuffer {
public:
    DefoldBuffer(rive::Span<const T> data)
    : rive::RenderBuffer(data.size())
    , m_Data(new T[count()])
    {
        memcpy(m_Data, data.begin(), sizeof(T)*count());
    }

    ~DefoldBuffer() {
        delete[] m_Data;
    }

    T* m_Data;
};

class DefoldFactory : public rive::Factory {

public:
    DefoldFactory();

    rive::rcp<rive::RenderBuffer> makeBufferU16(rive::Span<const uint16_t>) override;
    rive::rcp<rive::RenderBuffer> makeBufferU32(rive::Span<const uint32_t>) override;
    rive::rcp<rive::RenderBuffer> makeBufferF32(rive::Span<const float>) override;

    rive::rcp<rive::RenderShader> makeLinearGradient(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const rive::ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override;

    rive::rcp<rive::RenderShader> makeRadialGradient(float cx,
                                         float cy,
                                         float radius,
                                         const rive::ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override;

    // Returns a full-formed RenderPath -- can be treated as immutable
    std::unique_ptr<rive::RenderPath> makeRenderPath(rive::RawPath&, rive::FillRule) override;

    std::unique_ptr<rive::RenderPath> makeEmptyRenderPath() override;

    std::unique_ptr<rive::RenderPaint> makeRenderPaint() override;

    std::unique_ptr<rive::RenderImage> decodeImage(rive::Span<const uint8_t> data) override;
};
} // namespace rive
#endif
