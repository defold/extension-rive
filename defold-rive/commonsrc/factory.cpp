
#include <common/factory.h>
#include <rive/renderer.hpp>
#include <rive/shapes/paint/color.hpp>

#include <rive/tess/contour_stroke.hpp>

#include <dmsdk/dlib/array.h>

namespace dmRive {

DefoldFactory::DefoldFactory()
{
}

rive::rcp<rive::RenderBuffer> DefoldFactory::makeRenderBuffer(rive::RenderBufferType type, rive::RenderBufferFlags flags, size_t sizeInBytes)
{
    return rive::rcp<rive::RenderBuffer>(new DefoldBuffer(type, flags, sizeInBytes));
}

} // namespace dmRive
