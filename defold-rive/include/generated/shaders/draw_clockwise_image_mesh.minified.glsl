/*
 * Copyright 2024 Rive
 */

#ifdef VERTEX
ATTR_BLOCK_BEGIN(PositionAttr)
ATTR(0, float2, _EXPORTED_a_position);
ATTR_BLOCK_END

ATTR_BLOCK_BEGIN(UVAttr)
ATTR(1, float2, _EXPORTED_a_texCoord);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float2, v_texCoord);
VARYING_BLOCK_END

#ifdef VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

IMAGE_MESH_VERTEX_MAIN(_EXPORTED_drawVertexMain,
                       PositionAttr,
                       position,
                       UVAttr,
                       uv,
                       _vertexID)
{
    ATTR_UNPACK(_vertexID, position, _EXPORTED_a_position, float2);
    ATTR_UNPACK(_vertexID, uv, _EXPORTED_a_texCoord, float2);

    VARYING_INIT(v_texCoord, float2);

    float2 vertexPosition =
        MUL(make_float2x2(imageDrawUniforms.viewMatrix), _EXPORTED_a_position) +
        imageDrawUniforms.translate;
    v_texCoord = _EXPORTED_a_texCoord;
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(v_texCoord);
    EMIT_VERTEX(pos);
}
#endif

#ifdef FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(PER_DRAW_BINDINGS_SET, IMAGE_TEXTURE_IDX, _EXPORTED_imageTexture);
FRAG_TEXTURE_BLOCK_END

SAMPLER_MIPMAP(IMAGE_TEXTURE_IDX, imageSampler)

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
FRAG_STORAGE_BUFFER_BLOCK_END

FRAG_DATA_MAIN(half4, _EXPORTED_drawFragmentMain)
{
    VARYING_UNPACK(v_texCoord, float2);

    half4 meshColor = TEXTURE_SAMPLE(_EXPORTED_imageTexture, imageSampler, v_texCoord);
    meshColor.w *= imageDrawUniforms.opacity;

    EMIT_FRAG_DATA(meshColor);
}
#endif // FRAGMENT
