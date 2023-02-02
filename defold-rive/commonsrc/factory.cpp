
#include <common/factory.h>
#include <rive/renderer.hpp>
#include <rive/shapes/paint/color.hpp>

#include <rive/tess/contour_stroke.hpp>

#include <dmsdk/dlib/array.h>

namespace dmRive {

DefoldFactory::DefoldFactory()
{
}

rive::rcp<rive::RenderBuffer> DefoldFactory::makeBufferU16(rive::Span<const uint16_t> data)
{
    return rive::rcp<rive::RenderBuffer>(new DefoldBuffer<uint16_t>(data));
}

rive::rcp<rive::RenderBuffer> DefoldFactory::makeBufferU32(rive::Span<const uint32_t> data)
{
    return rive::rcp<rive::RenderBuffer>(new DefoldBuffer<uint32_t>(data));
}

rive::rcp<rive::RenderBuffer> DefoldFactory::makeBufferF32(rive::Span<const float> data)
{
    return rive::rcp<rive::RenderBuffer>(new DefoldBuffer<float>(data));
}


} // namespace dmRive
