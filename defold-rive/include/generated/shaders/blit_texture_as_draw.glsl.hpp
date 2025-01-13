#pragma once

#include "blit_texture_as_draw.exports.h"

namespace rive {
namespace gpu {
namespace glsl {
const char blit_texture_as_draw[] = R"===(/*
 * Copyright 2024 Rive
 */

#ifdef _EXPORTED_VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
VERTEX_STORAGE_BUFFER_BLOCK_END

VERTEX_MAIN(_EXPORTED_blitVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    // Fill the entire screen. The caller will use a scissor test to control the
    // bounds being drawn.
    float2 coord;
    coord.x = (_vertexID & 1) == 0 ? -1. : 1.;
    coord.y = (_vertexID & 2) == 0 ? -1. : 1.;
    float4 pos = float4(coord, 0, 1);
    EMIT_VERTEX(pos);
}
#endif

#ifdef _EXPORTED_FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(PER_FLUSH_BINDINGS_SET, 0, _EXPORTED_blitTextureSource);
FRAG_TEXTURE_BLOCK_END

FRAG_DATA_MAIN(half4, _EXPORTED_blitFragmentMain)
{
    half4 srcColor =
        TEXEL_FETCH(_EXPORTED_blitTextureSource, int2(floor(_fragCoord.xy)));
    EMIT_FRAG_DATA(srcColor);
}
#endif // FRAGMENT
)===";
} // namespace glsl
} // namespace gpu
} // namespace rive