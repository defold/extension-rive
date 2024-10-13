// Copyright 2020-2024 The Defold Foundation
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

#ifndef DM_DEFOLD_GRAPHICS_H
#define DM_DEFOLD_GRAPHICS_H

#include <stdint.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/graphics/graphics.h>

namespace dmGraphics
{
	// Texture filter
    enum TextureFilter
    {
        TEXTURE_FILTER_DEFAULT                = 0,
        TEXTURE_FILTER_NEAREST                = 1,
        TEXTURE_FILTER_LINEAR                 = 2,
        TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = 3,
        TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  = 4,
        TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  = 5,
        TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   = 6,
    };

    enum TextureUsageHint
    {
        TEXTURE_USAGE_HINT_NONE       = 0,
        TEXTURE_USAGE_HINT_SAMPLE     = 1,
        TEXTURE_USAGE_HINT_MEMORYLESS = 2,
        TEXTURE_USAGE_HINT_INPUT      = 4,
        TEXTURE_USAGE_HINT_COLOR      = 8,
        TEXTURE_USAGE_HINT_STORAGE    = 16,
    };

    enum TextureType
    {
        TEXTURE_TYPE_2D       = 0,
        TEXTURE_TYPE_2D_ARRAY = 1,
        TEXTURE_TYPE_CUBE_MAP = 2,
        TEXTURE_TYPE_IMAGE_2D = 3,
    };

    enum TextureWrap
    {
        TEXTURE_WRAP_CLAMP_TO_BORDER = 0,
        TEXTURE_WRAP_CLAMP_TO_EDGE   = 1,
        TEXTURE_WRAP_MIRRORED_REPEAT = 2,
        TEXTURE_WRAP_REPEAT          = 3,
    };

    enum FaceType
    {
        FACE_TYPE_FRONT          = 0,
        FACE_TYPE_BACK           = 1,
        FACE_TYPE_FRONT_AND_BACK = 2,
    };

    enum State
    {
        STATE_DEPTH_TEST           = 0,
        STATE_SCISSOR_TEST         = 1,
        STATE_STENCIL_TEST         = 2,
        STATE_ALPHA_TEST           = 3,
        STATE_BLEND                = 4,
        STATE_CULL_FACE            = 5,
        STATE_POLYGON_OFFSET_FILL  = 6,
        STATE_ALPHA_TEST_SUPPORTED = 7,
    };

    struct TextureCreationParams
    {
        TextureCreationParams()
        : m_Type(TEXTURE_TYPE_2D)
        , m_Width(0)
        , m_Height(0)
        , m_Depth(1)
        , m_OriginalWidth(0)
        , m_OriginalHeight(0)
        , m_MipMapCount(1)
        , m_UsageHintBits(TEXTURE_USAGE_HINT_SAMPLE)
        {}

        TextureType m_Type;
        uint16_t    m_Width;
        uint16_t    m_Height;
        uint16_t    m_Depth;
        uint16_t    m_OriginalWidth;
        uint16_t    m_OriginalHeight;
        uint8_t     m_MipMapCount;
        uint8_t     m_UsageHintBits;
    };

	struct TextureParams
    {
        TextureParams()
        : m_Data(0x0)
        , m_DataSize(0)
        , m_Format(TEXTURE_FORMAT_RGBA)
        , m_MinFilter(TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
        , m_MagFilter(TEXTURE_FILTER_LINEAR)
        , m_UWrap(TEXTURE_WRAP_CLAMP_TO_EDGE)
        , m_VWrap(TEXTURE_WRAP_CLAMP_TO_EDGE)
        , m_X(0)
        , m_Y(0)
        , m_Z(0)
        , m_Width(0)
        , m_Height(0)
        , m_Depth(0)
        , m_MipMap(0)
        , m_SubUpdate(false)
        {}

        const void*   m_Data;
        uint32_t      m_DataSize;
        TextureFormat m_Format;
        TextureFilter m_MinFilter;
        TextureFilter m_MagFilter;
        TextureWrap   m_UWrap;
        TextureWrap   m_VWrap;

        // For sub texture updates
        uint32_t m_X;
        uint32_t m_Y;
        uint32_t m_Z; // For array texture, this is the slice to set

        uint16_t m_Width;
        uint16_t m_Height;
        uint16_t m_Depth; // For array texture, this is slice count
        uint8_t  m_MipMap    : 7;
        uint8_t  m_SubUpdate : 1;
    };

    struct RenderTargetCreationParams
    {
        TextureCreationParams m_ColorBufferCreationParams[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureCreationParams m_DepthBufferCreationParams;
        TextureCreationParams m_StencilBufferCreationParams;
        TextureParams         m_ColorBufferParams[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureParams         m_DepthBufferParams;
        TextureParams         m_StencilBufferParams;
        AttachmentOp          m_ColorBufferLoadOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        AttachmentOp          m_ColorBufferStoreOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        float                 m_ColorBufferClearValue[MAX_BUFFER_COLOR_ATTACHMENTS][4];
        // TODO: Depth/Stencil

        uint8_t               m_DepthTexture   : 1;
        uint8_t               m_StencilTexture : 1;
    };

    struct PipelineState
    {
        uint64_t m_WriteColorMask           : 4;
        uint64_t m_WriteDepth               : 1;
        uint64_t m_PrimtiveType             : 3;
        // Depth Test
        uint64_t m_DepthTestEnabled         : 1;
        uint64_t m_DepthTestFunc            : 3;
        // Stencil Test
        uint64_t m_StencilEnabled           : 1;

        // Front
        uint64_t m_StencilFrontOpFail       : 3;
        uint64_t m_StencilFrontOpPass       : 3;
        uint64_t m_StencilFrontOpDepthFail  : 3;
        uint64_t m_StencilFrontTestFunc     : 3;

        // Back
        uint64_t m_StencilBackOpFail        : 3;
        uint64_t m_StencilBackOpPass        : 3;
        uint64_t m_StencilBackOpDepthFail   : 3;
        uint64_t m_StencilBackTestFunc      : 3;

        uint64_t m_StencilWriteMask         : 8;
        uint64_t m_StencilCompareMask       : 8;
        uint64_t m_StencilReference         : 8;
        // Blending
        uint64_t m_BlendEnabled             : 1;
        uint64_t m_BlendSrcFactor           : 4;
        uint64_t m_BlendDstFactor           : 4;
        // Culling
        uint64_t m_CullFaceEnabled          : 1;
        uint64_t m_CullFaceType             : 2;
        // Face winding
        uint64_t m_FaceWinding              : 1;
        // Polygon offset
        uint64_t m_PolygonOffsetFillEnabled : 1;
    };

	HTexture NewTexture(HContext context, const TextureCreationParams& params);
    void SetTexture(HTexture texture, const TextureParams& params);

    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const RenderTargetCreationParams params);
    void          DeleteRenderTarget(HRenderTarget render_target);
    void          SetRenderTarget(HContext context, HRenderTarget render_target, uint32_t transient_buffer_types);

    HUniformLocation GetUniformLocation(HProgram prog, const char* name);
    uint32_t         GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size);
    uint32_t         GetUniformCount(HProgram prog);
    HProgram         NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program);

    uint32_t GetWindowWidth(HContext context);
	uint32_t GetWindowHeight(HContext context);

	void SetCullFace(HContext context, FaceType face_type);

	void EnableState(HContext context, State state);
    void DisableState(HContext context, State state);

    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor);

    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height);

    void     EnableVertexBuffer(HContext context, HVertexBuffer vertex_buffer, uint32_t binding_index);
    void     DisableVertexBuffer(HContext context, HVertexBuffer vertex_buffer);

    void     EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, uint32_t binding_index, HProgram program);
    void     DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration);

    void        EnableTexture(HContext context, uint32_t unit, uint8_t id_index, HTexture texture);
    void SetSampler(HContext context, HUniformLocation location, int32_t unit);
    void        DisableTexture(HContext context, uint32_t unit, HTexture texture);

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer);
    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count);

    void                 EnableProgram(HContext context, HProgram program);
    void                 DisableProgram(HContext context);

    PipelineState GetPipelineState(HContext context);

    void        SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy);
}

#endif

