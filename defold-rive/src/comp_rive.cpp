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

#include <file.hpp>
#include <renderer.hpp>
#include <animation/linear_animation_instance.hpp>
#include <rive/rive_render_api.h>

#include "comp_rive.h"
#include "res_rive_data.h"
#include "res_rive_scene.h"
#include "res_rive_model.h"

#include <string.h> // memset

#include <dmsdk/gameobject/component.h>
#include <dmsdk/resource/resource.h>
#include <dmsdk/dlib/math.h>

#include <dmsdk/gamesys/resources/res_skeleton.h>
#include <dmsdk/gamesys/resources/res_rig_scene.h>
#include <dmsdk/gamesys/resources/res_meshset.h>
#include <dmsdk/gamesys/resources/res_animationset.h>
#include <dmsdk/gamesys/resources/res_textureset.h>


// #include <dmsdk/dlib/array.h>
// #include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/gamesys/property.h>
// #include <dmsdk/dlib/message.h>
// #include <dmsdk/dlib/profile.h>
// #include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/object_pool.h>
// #include <dmsdk/dlib/math.h>
// #include <dmsdk/dlib/vmath.h>

//#include <gameobject/gameobject_ddf.h> // for creating bones where the rive scene bones are
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/render/render.h>
#include <gameobject/gameobject_ddf.h>

namespace rive
{
    // JG: Hmmm should we do it like this? Rive needs these two functions that
    //     are externally linked, but our API is built around a context structure.
    static HContext g_Ctx = 0;
    RenderPath* makeRenderPath()
    {
        return createRenderPath(g_Ctx);
    }

    RenderPaint* makeRenderPaint()
    {
        return createRenderPaint(g_Ctx);
    }
}

namespace dmRive
{
    using namespace dmVMath;

    static const dmhash_t PROP_ANIMATION     = dmHashString64("animation");
    static const dmhash_t PROP_CURSOR        = dmHashString64("cursor");
    static const dmhash_t PROP_PLAYBACK_RATE = dmHashString64("playback_rate");
    static const dmhash_t PROP_MATERIAL      = dmHashString64("material");
    static const dmhash_t MATERIAL_EXT_HASH  = dmHashString64("materialc");
    static const dmhash_t UNIFORM_COLOR      = dmHashString64("color");
    static const dmhash_t UNIFORM_COVER      = dmHashString64("cover");

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params);
    static void DestroyComponent(struct RiveWorld* world, uint32_t index);

    static rive::HBuffer RiveRequestBufferCallback(rive::HBuffer buffer, rive::BufferType type, void* data, unsigned int dataSize, void* userData);
    static void          RiveDestroyBufferCallback(rive::HBuffer buffer, void* userData);

    struct RiveVertex
    {
        float x;
        float y;
    };

    struct RiveBuffer
    {
        void*        m_Data;
        unsigned int m_Size;
    };

    // JG: Do we need this or can we loop through events in render batch instead?
    struct RiveDrawEntry
    {
        rive::DrawBuffers  m_Buffers;
        rive::HRenderPaint m_Paint;
        Matrix4            m_WorldTransform;
    };

    // For the entire app's life cycle
    struct CompRiveContext
    {
        CompRiveContext()
        {
            memset(this, 0, sizeof(*this));
        }
        rive::HRenderer          m_RiveRenderer;
        rive::HContext           m_RiveContext;
        dmResource::HFactory     m_Factory;
        dmRender::HRenderContext m_RenderContext;
        dmGraphics::HContext     m_GraphicsContext;
        uint32_t                 m_MaxInstanceCount;
    };

    // One per collection
    struct RiveWorld
    {
        CompRiveContext*                    m_Ctx; // JG: Is this a bad idea?
        dmObjectPool<RiveComponent*>        m_Components;
        dmArray<dmRender::RenderObject>     m_RenderObjects;
        dmGraphics::HVertexDeclaration      m_VertexDeclaration;
        dmGraphics::HVertexBuffer           m_VertexBuffer;
        dmArray<RiveVertex>                 m_VertexBufferData;
        dmGraphics::HIndexBuffer            m_IndexBuffer;
        dmArray<int>                        m_IndexBufferData;
        dmArray<RiveDrawEntry>              m_DrawEntries;
    };

    static inline void Mat4ToMat2D(const Matrix4 m4, rive::Mat2D& m2)
    {
        m2[0] = m4[0][0];
        m2[1] = m4[0][1];

        m2[2] = m4[1][0];
        m2[3] = m4[1][1];

        m2[4] = m4[3][0];
        m2[5] = m4[3][1];
    }

    static inline void Mat2DToMat4(const rive::Mat2D m2, Matrix4& m4)
    {
        m4[0][0] = m2[0];
        m4[0][1] = m2[1];
        m4[0][2] = 0.0;
        m4[0][3] = 0.0;

        m4[1][0] = m2[2];
        m4[1][1] = m2[3];
        m4[1][2] = 0.0;
        m4[1][3] = 0.0;

        m4[2][0] = 0.0;
        m4[2][1] = 0.0;
        m4[2][2] = 1.0;
        m4[2][3] = 0.0;

        m4[3][0] = m2[4];
        m4[3][1] = m2[5];
        m4[3][2] = 0.0;
        m4[3][3] = 1.0;
    }

    dmGameObject::CreateResult CompRiveNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        CompRiveContext* context = (CompRiveContext*)params.m_Context;
        RiveWorld* world         = new RiveWorld();

        world->m_Ctx = context;
        world->m_Components.SetCapacity(context->m_MaxInstanceCount);
        world->m_RenderObjects.SetCapacity(context->m_MaxInstanceCount);
        world->m_DrawEntries.SetCapacity(context->m_MaxInstanceCount);

        dmGraphics::VertexElement ve[] =
        {
            {"position", 0, 2, dmGraphics::TYPE_FLOAT, false}
        };

        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(context->m_GraphicsContext, ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        world->m_VertexBuffer      = dmGraphics::NewVertexBuffer(context->m_GraphicsContext, 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        world->m_IndexBuffer       = dmGraphics::NewIndexBuffer(context->m_GraphicsContext, 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);

        // TODO: Make this count configurable and/or grow accordingly
        world->m_VertexBufferData.SetCapacity(context->m_MaxInstanceCount * 512);
        world->m_IndexBufferData.SetCapacity(context->m_MaxInstanceCount * 512);

        *params.m_World = world;

        dmResource::RegisterResourceReloadedCallback(context->m_Factory, ResourceReloadedCallback, world);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompRiveDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;
        dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(world->m_VertexBuffer);
        dmGraphics::DeleteIndexBuffer(world->m_IndexBuffer);

        dmResource::UnregisterResourceReloadedCallback(((CompRiveContext*)params.m_Context)->m_Factory, ResourceReloadedCallback, world);

        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline dmRender::HMaterial GetMaterial(const RiveComponent* component, const RiveModelResource* resource) {
        return component->m_Material ? component->m_Material : resource->m_Material;
    }

    static void ReHash(RiveComponent* component)
    {
        // material, texture set, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        RiveModelResource* resource = component->m_Resource;
        dmRiveDDF::RiveModelDesc* ddf = resource->m_DDF;
        dmRender::HMaterial material = GetMaterial(component, resource);
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &material, sizeof(material));
        dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode));
        if (component->m_RenderConstants)
            dmGameSystem::HashRenderConstants(component->m_RenderConstants, &state);
        component->m_MixedHash = dmHashFinal32(&state);
        component->m_ReHash = 0;
    }

    void* CompRiveGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        return &world->m_Components.Get(index);
    }

    dmGameObject::CreateResult CompRiveCreate(const dmGameObject::ComponentCreateParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;

        if (world->m_Components.Full())
        {
            dmLogError("Rive instance could not be created since the buffer is full (%d).", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        uint32_t index = world->m_Components.Alloc();
        RiveComponent* component = new RiveComponent;
        memset(component, 0, sizeof(RiveComponent));
        world->m_Components.Set(index, component);
        component->m_Instance = params.m_Instance;
        component->m_Transform = dmTransform::Transform(Vector3(params.m_Position), params.m_Rotation, 1.0f);
        component->m_Resource = (RiveModelResource*)params.m_Resource;
        //dmMessage::ResetURL(&component->m_Listener);

        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_World = Matrix4::identity();
        component->m_DoRender = 0;
        component->m_RenderConstants = 0;

        // TODO: Create the bone hierarchy

        component->m_ReHash = 1;

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline RiveComponent* GetComponentFromIndex(RiveWorld* world, int index)
    {
        return world->m_Components.Get(index);
    }

    static void DestroyComponent(RiveWorld* world, uint32_t index)
    {
        RiveComponent* component = GetComponentFromIndex(world, index);
        dmGameObject::DeleteBones(component->m_Instance);

        if (component->m_RenderConstants)
            dmGameSystem::DestroyRenderConstants(component->m_RenderConstants);

        if (component->m_AnimationInstance)
            delete component->m_AnimationInstance;

        delete component;
        world->m_Components.Free(index, true);
    }

    dmGameObject::CreateResult CompRiveDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        CompRiveContext* ctx = (CompRiveContext*)params.m_Context;
        RiveWorld* world = (RiveWorld*)params.m_World;
        uint32_t index = *params.m_UserData;
        RiveComponent* component = GetComponentFromIndex(world, index);
        if (component->m_Material) {
            dmResource::Release(ctx->m_Factory, (void*)component->m_Material);
        }
        DestroyComponent(world, index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void SetBlendMode(dmRender::RenderObject& ro, dmRiveDDF::RiveModelDesc::BlendMode blend_mode)
    {
        switch (blend_mode)
        {
            case dmRiveDDF::RiveModelDesc::BLEND_MODE_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmRiveDDF::RiveModelDesc::BLEND_MODE_ADD:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmRiveDDF::RiveModelDesc::BLEND_MODE_MULT:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmRiveDDF::RiveModelDesc::BLEND_MODE_SCREEN:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
                assert(0);
            break;
        }

        ro.m_SetBlendFactors = 1;
    }

    static void GetRiveDrawParams(rive::HContext ctx, rive::HRenderer renderer, uint32_t& vertex_count, uint32_t& index_count, uint32_t& render_object_count)
    {
        vertex_count        = 0;
        index_count         = 0;
        render_object_count = 0;

        for (int i = 0; i < rive::getDrawEventCount(renderer); ++i)
        {
            const rive::PathDrawEvent evt = rive::getDrawEvent(renderer, i);

            switch(evt.m_Type)
            {
                case rive::EVENT_DRAW_STENCIL:
                {
                    rive::DrawBuffers buffers = rive::getDrawBuffers(ctx, renderer, evt.m_Path);
                    RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;

                    if (vxBuffer != 0)
                    {
                        uint32_t vx_count = vxBuffer->m_Size / sizeof(RiveVertex);
                        index_count += (vx_count - 5) * 3;
                        vertex_count += vx_count;
                        render_object_count++;
                    }
                } break;
                case rive::EVENT_DRAW_COVER:
                {
                    rive::DrawBuffers buffers = rive::getDrawBuffers(ctx, renderer, evt.m_Path);
                    RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;

                    if (vxBuffer != 0)
                    {
                        index_count += 3 * 2;
                        vertex_count += vxBuffer->m_Size / sizeof(RiveVertex);
                        render_object_count++;
                    }
                } break;
                default:break;
            }
        }
    }

    static void GenerateVertexData(RiveWorld*   world,
        dmRender::HRenderContext                render_context,
        dmGameSystem::HComponentRenderConstants render_constants,
        dmRender::HMaterial                     material,
        dmRiveDDF::RiveModelDesc::BlendMode     blend_mode,
        rive::HContext                          rive_ctx,
        rive::HRenderer                         renderer,
        RiveVertex*&                            vx_ptr,
        int*&                                   ix_ptr,
        unsigned int                            ro_start)
    {
        dmGraphics::FaceWinding last_face_winding = dmGraphics::FACE_WINDING_CCW;
        rive::HRenderPaint paint                  = 0;
        uint32_t last_ix                          = 0;
        uint32_t vx_offset                        = 0;
        uint8_t clear_clipping_flag               = 0;
        bool is_applying_clipping                 = false;
        uint32_t ro_index                         = ro_start;

        rive::DrawBuffers buffers_renderer = rive::getDrawBuffers(rive_ctx, renderer, 0);
        RiveBuffer* ixBuffer               = (RiveBuffer*) buffers_renderer.m_IndexBuffer;
        int* ix_data_ptr                   = (int*) ixBuffer->m_Data;

        for (int i = 0; i < rive::getDrawEventCount(renderer); ++i)
        {
            const rive::PathDrawEvent evt = rive::getDrawEvent(renderer, i);

            switch(evt.m_Type)
            {
                case rive::EVENT_SET_PAINT:
                    paint = evt.m_Paint;
                    break;
                case rive::EVENT_DRAW_STENCIL:
                {
                    rive::DrawBuffers buffers = rive::getDrawBuffers(rive_ctx, renderer, evt.m_Path);
                    RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;

                    if (vxBuffer != 0)
                    {
                        uint32_t vx_count = vxBuffer->m_Size / sizeof(RiveVertex);

                        int triangle_count = vx_count - 5;
                        if (vx_count < 5)
                        {
                            continue;
                        }

                        uint32_t ix_count = triangle_count * 3;

                        for (int j = 0; j < ix_count; ++j)
                        {
                            ix_ptr[j] = ix_data_ptr[j + 6] + vx_offset;
                        }

                        memcpy(vx_ptr, vxBuffer->m_Data, vxBuffer->m_Size);

                        dmRender::RenderObject& ro = world->m_RenderObjects[ro_index];
                        ro.Init();
                        ro.m_VertexDeclaration = world->m_VertexDeclaration;
                        ro.m_VertexBuffer      = world->m_VertexBuffer;
                        ro.m_PrimitiveType     = dmGraphics::PRIMITIVE_TRIANGLES;
                        ro.m_VertexStart       = last_ix; // byte offset
                        ro.m_VertexCount       = ix_count;
                        ro.m_Material          = material;
                        ro.m_IndexBuffer       = world->m_IndexBuffer;
                        ro.m_IndexType         = dmGraphics::TYPE_UNSIGNED_INT;
                        ro.m_SetStencilTest    = 1;
                        ro.m_SetFaceWinding    = 1;

                        dmRender::StencilTestParams& stencil_state = ro.m_StencilTestParams;

                        stencil_state.m_Front = {
                            .m_Func     = dmGraphics::COMPARE_FUNC_ALWAYS,
                            .m_OpSFail  = dmGraphics::STENCIL_OP_KEEP,
                            .m_OpDPFail = dmGraphics::STENCIL_OP_KEEP,
                            .m_OpDPPass = dmGraphics::STENCIL_OP_INCR_WRAP,
                        };

                        stencil_state.m_Back = {
                            .m_Func     = dmGraphics::COMPARE_FUNC_ALWAYS,
                            .m_OpSFail  = dmGraphics::STENCIL_OP_KEEP,
                            .m_OpDPFail = dmGraphics::STENCIL_OP_KEEP,
                            .m_OpDPPass = dmGraphics::STENCIL_OP_DECR_WRAP,
                        };

                        stencil_state.m_Ref                = 0x00;
                        stencil_state.m_RefMask            = 0xFF;
                        stencil_state.m_BufferMask         = 0xFF;
                        stencil_state.m_ColorBufferMask    = 0x00;
                        stencil_state.m_ClearBuffer        = 0;
                        stencil_state.m_SeparateFaceStates = 1;

                        if (evt.m_IsClipping)
                        {
                            stencil_state.m_Front.m_Func = dmGraphics::COMPARE_FUNC_EQUAL;
                            stencil_state.m_Back.m_Func  = dmGraphics::COMPARE_FUNC_EQUAL;
                            stencil_state.m_Ref          = 0x80;
                            stencil_state.m_RefMask      = 0x80;
                            stencil_state.m_BufferMask   = 0x7F;
                        }

                        if (clear_clipping_flag)
                        {
                            stencil_state.m_ClearBuffer = 1;
                            clear_clipping_flag = 0;
                        }

                        if (evt.m_IsEvenOdd && (evt.m_Idx % 2) != 0)
                        {
                            ro.m_FaceWinding = dmGraphics::FACE_WINDING_CW;
                        }
                        else
                        {
                            ro.m_FaceWinding = dmGraphics::FACE_WINDING_CCW;
                        }

                        dmGameObject::PropertyVar apply_clipping_var(Vectormath::Aos::Vector4(0.0f, 0.0f, 0.0f, 0.0f));
                        dmGameSystem::SetRenderConstant(render_constants, ro.m_Material, UNIFORM_COVER, 0, apply_clipping_var);
                        dmGameSystem::EnableRenderObjectConstants(&ro, render_constants);

                        Mat2DToMat4(evt.m_TransformWorld, ro.m_WorldTransform);
                        dmRender::AddToRender(render_context, &ro);

                        last_face_winding  = ro.m_FaceWinding;
                        vx_ptr            += vx_count;
                        ix_ptr            += ix_count;
                        last_ix           += ix_count * sizeof(int);
                        vx_offset         += vx_count;
                        ro_index++;
                    }
                } break;
                case rive::EVENT_DRAW_COVER:
                {
                    rive::DrawBuffers buffers = rive::getDrawBuffers(rive_ctx, renderer, evt.m_Path);
                    RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;

                    if (vxBuffer != 0 && ixBuffer != 0)
                    {
                        int* ix_data_ptr  = (int*) ixBuffer->m_Data;
                        uint32_t vx_count = 4;
                        uint32_t ix_count = 2 * 3;

                        for (int j = 0; j < ix_count; ++j)
                        {
                            ix_ptr[j] = ix_data_ptr[j] + vx_offset;
                        }

                        memcpy(vx_ptr, vxBuffer->m_Data, vxBuffer->m_Size);

                        dmRender::RenderObject& ro = world->m_RenderObjects[ro_index];
                        ro.Init();
                        ro.m_VertexDeclaration = world->m_VertexDeclaration;
                        ro.m_VertexBuffer      = world->m_VertexBuffer;
                        ro.m_PrimitiveType     = dmGraphics::PRIMITIVE_TRIANGLES;
                        ro.m_VertexStart       = last_ix; // byte offset
                        ro.m_VertexCount       = ix_count;
                        ro.m_Material          = material;
                        ro.m_IndexBuffer       = world->m_IndexBuffer;
                        ro.m_IndexType         = dmGraphics::TYPE_UNSIGNED_INT;
                        ro.m_SetStencilTest    = 1;

                        dmRender::StencilTestParams& stencil_state = ro.m_StencilTestParams;
                        stencil_state.m_ClearBuffer = 0;

                        if (is_applying_clipping)
                        {
                            stencil_state.m_Front = {
                                .m_Func     = dmGraphics::COMPARE_FUNC_NOTEQUAL,
                                .m_OpSFail  = dmGraphics::STENCIL_OP_ZERO,
                                .m_OpDPFail = dmGraphics::STENCIL_OP_ZERO,
                                .m_OpDPPass = dmGraphics::STENCIL_OP_REPLACE,
                            };

                            stencil_state.m_Ref             = 0x80;
                            stencil_state.m_RefMask         = 0x7F;
                            stencil_state.m_BufferMask      = 0xFF;
                            stencil_state.m_ColorBufferMask = 0x00;
                        }
                        else
                        {
                            stencil_state.m_Front = {
                                .m_Func     = dmGraphics::COMPARE_FUNC_NOTEQUAL,
                                .m_OpSFail  = dmGraphics::STENCIL_OP_ZERO,
                                .m_OpDPFail = dmGraphics::STENCIL_OP_ZERO,
                                .m_OpDPPass = dmGraphics::STENCIL_OP_ZERO,
                            };

                            stencil_state.m_Ref             = 0x00;
                            stencil_state.m_RefMask         = 0xFF;
                            stencil_state.m_BufferMask      = 0xFF;
                            stencil_state.m_ColorBufferMask = 0x0F;

                            if (evt.m_IsClipping)
                            {
                                stencil_state.m_RefMask    = 0x7F;
                                stencil_state.m_BufferMask = 0x7F;
                            }

                            SetBlendMode(ro, blend_mode);

                            const rive::PaintData draw_entry_paint = rive::getPaintData(paint);
                            const float* color                     = &draw_entry_paint.m_Colors[0];
                            dmGameObject::PropertyVar colorVar(Vectormath::Aos::Vector4(color[0], color[1], color[2], color[3]));
                            dmGameSystem::SetRenderConstant(render_constants, ro.m_Material, UNIFORM_COLOR, 0, colorVar);
                        }

                        // If we are fullscreen-covering, we don't transform the vertices
                        float no_projection = (float) evt.m_IsClipping && is_applying_clipping;
                        dmGameObject::PropertyVar apply_clipping_var(Vectormath::Aos::Vector4(no_projection, 0.0f, 0.0f, 0.0f));
                        dmGameSystem::SetRenderConstant(render_constants, ro.m_Material, UNIFORM_COVER, 0, apply_clipping_var);
                        dmGameSystem::EnableRenderObjectConstants(&ro, render_constants);

                        if (last_face_winding != dmGraphics::FACE_WINDING_CCW)
                        {
                            ro.m_FaceWinding    = dmGraphics::FACE_WINDING_CCW;
                            ro.m_SetFaceWinding = 1;
                        }

                        Mat2DToMat4(evt.m_TransformWorld, ro.m_WorldTransform);
                        dmRender::AddToRender(render_context, &ro);

                        vx_ptr    += vx_count;
                        ix_ptr    += ix_count;
                        last_ix   += ix_count * sizeof(int);
                        vx_offset += vx_count;
                        ro_index++;
                    }
                } break;
                case rive::EVENT_CLIPPING_BEGIN:
                    is_applying_clipping = true;
                    clear_clipping_flag = 1;
                    break;
                case rive::EVENT_CLIPPING_END:
                    is_applying_clipping = false;
                    break;
                case rive::EVENT_CLIPPING_DISABLE:
                    is_applying_clipping = false;
                    break;
                default:break;
            }
        }
    }

    static void RenderBatch(RiveWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        RiveComponent* first        = (RiveComponent*) buf[*begin].m_UserData;
        RiveModelResource* resource = first->m_Resource;
        rive::HContext ctx          = world->m_Ctx->m_RiveContext;
        rive::HRenderer renderer    = world->m_Ctx->m_RiveRenderer;

        uint32_t ro_count         = 0;
        uint32_t vertex_count     = 0;
        uint32_t index_count      = 0;
        GetRiveDrawParams(ctx, renderer, vertex_count, index_count, ro_count);
        uint32_t ro_size = world->m_RenderObjects.Size() + ro_count;

        if (world->m_RenderObjects.Remaining() < ro_size)
        {
            world->m_RenderObjects.OffsetCapacity(ro_size - world->m_RenderObjects.Remaining());
        }

        uint32_t ro_index = world->m_RenderObjects.Size();
        world->m_RenderObjects.SetSize(ro_index + ro_count);

        dmArray<RiveVertex> &vertex_buffer = world->m_VertexBufferData;
        if (vertex_buffer.Remaining() < vertex_count)
        {
            vertex_buffer.OffsetCapacity(vertex_count - vertex_buffer.Remaining());
        }

        dmArray<int> &index_buffer = world->m_IndexBufferData;
        if (index_buffer.Remaining() < index_count)
        {
            index_buffer.OffsetCapacity(index_count - index_buffer.Remaining());
        }

        RiveVertex* vb_begin = vertex_buffer.End();
        RiveVertex* vb_end = vb_begin;

        int* ix_begin = index_buffer.End();
        int* ix_end   = ix_begin;

        if (!first->m_RenderConstants)
        {
            first->m_RenderConstants = dmGameSystem::CreateRenderConstants();
        }

        GenerateVertexData(world, render_context, first->m_RenderConstants,
            GetMaterial(first, resource), resource->m_DDF->m_BlendMode,
            ctx, renderer, vb_end, ix_end, ro_index);

        vertex_buffer.SetSize(vb_end - vertex_buffer.Begin());
        index_buffer.SetSize(ix_end - index_buffer.Begin());
    }

    void UpdateTransforms(RiveWorld* world)
    {
        //DM_PROFILE(SpineModel, "UpdateTransforms");

        dmArray<RiveComponent*>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            RiveComponent* c = components[i];

            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            const Matrix4& go_world = dmGameObject::GetWorldMatrix(c->m_Instance);
            const Matrix4 local = dmTransform::ToMatrix4(c->m_Transform);
            // if (dmGameObject::ScaleAlongZ(c->m_Instance))
            // {
            //     c->m_World = go_world * local;
            // }
            // else
            {
                c->m_World = dmTransform::MulNoScaleZ(go_world, local);
            }
        }
    }

    dmGameObject::CreateResult CompRiveAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        RiveComponent* component = world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompRiveUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        RiveWorld* world         = (RiveWorld*)params.m_World;
        rive::HContext ctx       = world->m_Ctx->m_RiveContext;
        rive::HRenderer renderer = world->m_Ctx->m_RiveRenderer;

        rive::newFrame(renderer);
        rive::Renderer* rive_renderer = (rive::Renderer*) renderer;
        float dt = params.m_UpdateContext->m_DT;

        dmArray<RiveComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        world->m_DrawEntries.SetSize(0);

        const rive::RenderMode render_mode = rive::getRenderMode(ctx);

        for (uint32_t i = 0; i < count; ++i)
        {
            RiveComponent& component = *components[i];
            component.m_DoRender = 0;

            if (!component.m_Enabled || !component.m_AddedToUpdate)
            {
                continue;
            }

            // RIVE UPDATE
            dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component.m_Resource->m_Scene->m_Scene;
            rive::File* f               = data->m_File;
            rive::Artboard* artboard    = f->artboard();
            rive::AABB artboard_bounds  = artboard->bounds();

            if (component.m_AnimationInstance)
            {
                component.m_AnimationInstance->advance(dt);
                component.m_AnimationInstance->apply(artboard, 1.0f);
            }

            rive::Mat2D transform;
            Mat4ToMat2D(component.m_World, transform);

            // JG: Rive is using a different coordinate system that defold,
            //     in their examples they flip the projection but that isn't
            //     really compatible with our setup I don't think?
            rive::Vec2D yflip(1.0f,-1.0f);
            rive::Mat2D::scale(transform, transform, yflip);
            rive::setTransform(renderer, transform);

            rive_renderer->align(rive::Fit::none,
               rive::Alignment::center,
               rive::AABB(-artboard_bounds.width(), -artboard_bounds.height(),
               artboard_bounds.width(), artboard_bounds.height()),
               artboard_bounds);

            rive_renderer->save();
            artboard->advance(dt);
            artboard->draw(rive_renderer);
            rive_renderer->restore();

            if (component.m_ReHash || (component.m_RenderConstants && dmGameSystem::AreRenderConstantsUpdated(component.m_RenderConstants)))
            {
                ReHash(&component);
            }

            component.m_DoRender = 1;
        }

        // If the child bones have been updated, we need to return true
        update_result.m_TransformsUpdated = false;

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        RiveWorld *world            = (RiveWorld *) params.m_UserData;
        rive::RenderMode renderMode = rive::getRenderMode(world->m_Ctx->m_RiveContext);

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
            {
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                dmGraphics::SetIndexBufferData(world->m_IndexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

                world->m_RenderObjects.SetSize(0);
                dmArray<RiveVertex>& vertex_buffer = world->m_VertexBufferData;
                vertex_buffer.SetSize(0);

                dmArray<int>& index_buffer = world->m_IndexBufferData;
                index_buffer.SetSize(0);
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_BATCH:
            {
                RenderBatch(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_END:
            {
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, sizeof(RiveVertex) * world->m_VertexBufferData.Size(),
                                                world->m_VertexBufferData.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                dmGraphics::SetIndexBufferData(world->m_IndexBuffer, sizeof(int) * world->m_IndexBufferData.Size(),
                                                world->m_IndexBufferData.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                break;
            }
            default:
                assert(false);
                break;
        }
    }

    dmGameObject::UpdateResult CompRiveRender(const dmGameObject::ComponentsRenderParams& params)
    {
        CompRiveContext* context = (CompRiveContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        RiveWorld* world = (RiveWorld*)params.m_World;

        UpdateTransforms(world);

        dmArray<RiveComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        // Prepare list submit
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, count);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, world);
        dmRender::RenderListEntry* write_ptr   = render_list;

        for (uint32_t i = 0; i < count; ++i)
        {
            RiveComponent& component = *components[i];
            if (!component.m_DoRender || !component.m_Enabled)
            {
                continue;
            }
            const Vector4 trans        = component.m_World.getCol(3);
            write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());
            write_ptr->m_UserData      = (uintptr_t) &component;
            write_ptr->m_BatchKey      = component.m_MixedHash;
            write_ptr->m_TagListKey    = dmRender::GetMaterialTagListKey(GetMaterial(&component, component.m_Resource));
            write_ptr->m_Dispatch      = dispatch;
            write_ptr->m_MinorOrder    = 0;
            write_ptr->m_MajorOrder    = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompRiveGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        RiveComponent* component = (RiveComponent*)user_data;
        return component->m_RenderConstants && dmGameSystem::GetRenderConstant(component->m_RenderConstants, name_hash, out_constant);
    }

    static void CompRiveSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        RiveComponent* component = (RiveComponent*)user_data;
        if (!component->m_RenderConstants)
            component->m_RenderConstants = dmGameSystem::CreateRenderConstants();
        dmGameSystem::SetRenderConstant(component->m_RenderConstants, GetMaterial(component, component->m_Resource), name_hash, element_index, var);
        component->m_ReHash = 1;
    }

    dmGameObject::UpdateResult CompRiveOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;
        RiveComponent* component = world->m_Components.Get(*params.m_UserData);
        if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 1;
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 0;
        }
        else if (params.m_Message->m_Descriptor != 0x0)
        {
            if (params.m_Message->m_Id == dmRiveDDF::RivePlayAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmRiveDDF::RivePlayAnimation* ddf = (dmRiveDDF::RivePlayAnimation*)params.m_Message->m_Data;
                dmRive::RiveSceneData* data       = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;
                rive::LinearAnimation* animation  = 0;
                rive::Artboard* artboard          = data->m_File->artboard();

                for (int i = 0; i < data->m_LinearAnimationCount; ++i)
                {
                    if (data->m_LinearAnimations[i].m_NameHash == ddf->m_AnimationId)
                    {
                        animation = artboard->animation(data->m_LinearAnimations[i].m_AnimationIndex);
                        break;
                    }
                }

                if (animation)
                {
                    if (component->m_AnimationInstance)
                        delete component->m_AnimationInstance;

                    component->m_AnimationInstance = new rive::LinearAnimationInstance(animation);
                }

                /*
                if (dmRig::RESULT_OK == dmRig::PlayAnimation(component->m_RigInstance, ddf->m_AnimationId, ddf_playback_map.m_Table[ddf->m_Playback], ddf->m_BlendDuration, ddf->m_Offset, ddf->m_PlaybackRate))
                {
                    component->m_Listener = params.m_Message->m_Sender;
                }
                */
            }
            // else if (params.m_Message->m_Id == dmRiveDDF::SpineCancelAnimation::m_DDFDescriptor->m_NameHash)
            // {
            //     dmRig::CancelAnimation(component->m_RigInstance);
            // }
            // else if (params.m_Message->m_Id == dmRiveDDF::SetConstantSpineModel::m_DDFDescriptor->m_NameHash)
            // {
            //     dmRiveDDF::SetConstantSpineModel* ddf = (dmRiveDDF::SetConstantSpineModel*)params.m_Message->m_Data;
            //     dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(GetMaterial(component, component->m_Resource), ddf->m_NameHash,
            //             dmGameObject::PropertyVar(ddf->m_Value), CompRiveSetConstantCallback, component);
            //     if (result == dmGameObject::PROPERTY_RESULT_NOT_FOUND)
            //     {
            //         dmMessage::URL& receiver = params.m_Message->m_Receiver;
            //         dmLogError("'%s:%s#%s' has no constant named '%s'",
            //                 dmMessage::GetSocketName(receiver.m_Socket),
            //                 dmHashReverseSafe64(receiver.m_Path),
            //                 dmHashReverseSafe64(receiver.m_Fragment),
            //                 dmHashReverseSafe64(ddf->m_NameHash));
            //     }
            // }
            // else if (params.m_Message->m_Id == dmRiveDDF::ResetConstantSpineModel::m_DDFDescriptor->m_NameHash)
            // {
            //     dmRiveDDF::ResetConstantSpineModel* ddf = (dmRiveDDF::ResetConstantSpineModel*)params.m_Message->m_Data;
            //     if (component->m_RenderConstants)
            //     {
            //         component->m_ReHash |= dmGameSystem::ClearRenderConstant(component->m_RenderConstants, ddf->m_NameHash);
            //     }
            // }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool OnResourceReloaded(RiveWorld* world, RiveComponent* component, int index)
    {
        // Make it regenerate the batch key
        component->m_ReHash = 1;
        return true;
    }

    void CompRiveOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;
        int index = *params.m_UserData;
        RiveComponent* component = GetComponentFromIndex(world, index);
        component->m_Resource = (RiveModelResource*)params.m_Resource;
        (void)OnResourceReloaded(world, component, index);
    }

    dmGameObject::PropertyResult CompRiveGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        CompRiveContext* context = (CompRiveContext*)params.m_Context;
        RiveWorld* world = (RiveWorld*)params.m_World;
        RiveComponent* component = GetComponentFromIndex(world, *params.m_UserData);
        // if (params.m_PropertyId == PROP_ANIMATION)
        // {
        //     out_value.m_Variant = dmGameObject::PropertyVar(dmRig::GetAnimation(component->m_RigInstance));
        //     return dmGameObject::PROPERTY_RESULT_OK;
        // }
        // else if (params.m_PropertyId == PROP_CURSOR)
        // {
        //     out_value.m_Variant = dmGameObject::PropertyVar(dmRig::GetCursor(component->m_RigInstance, true));
        //     return dmGameObject::PROPERTY_RESULT_OK;
        // }
        // else if (params.m_PropertyId == PROP_PLAYBACK_RATE)
        // {
        //     out_value.m_Variant = dmGameObject::PropertyVar(dmRig::GetPlaybackRate(component->m_RigInstance));
        //     return dmGameObject::PROPERTY_RESULT_OK;
        // }
        // else
        if (params.m_PropertyId == PROP_MATERIAL)
        {
            dmRender::HMaterial material = GetMaterial(component, component->m_Resource);
            return dmGameSystem::GetResourceProperty(context->m_Factory, material, out_value);
        }
        return dmGameSystem::GetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, out_value, true, CompRiveGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompRiveSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;
        RiveComponent* component = world->m_Components.Get(*params.m_UserData);
        // if (params.m_PropertyId == PROP_CURSOR)
        // {
        //     if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
        //         return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        //     dmRig::Result res = dmRig::SetCursor(component->m_RigInstance, params.m_Value.m_Number, true);
        //     if (res == dmRig::RESULT_ERROR)
        //     {
        //         dmLogError("Could not set cursor %f on the spine model.", params.m_Value.m_Number);
        //         return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
        //     }
        //     return dmGameObject::PROPERTY_RESULT_OK;
        // }
        // else if (params.m_PropertyId == PROP_PLAYBACK_RATE)
        // {
        //     if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
        //         return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

        //     dmRig::Result res = dmRig::SetPlaybackRate(component->m_RigInstance, params.m_Value.m_Number);
        //     if (res == dmRig::RESULT_ERROR)
        //     {
        //         dmLogError("Could not set playback rate %f on the spine model.", params.m_Value.m_Number);
        //         return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
        //     }
        //     return dmGameObject::PROPERTY_RESULT_OK;
        // }
        // else
        if (params.m_PropertyId == PROP_MATERIAL)
        {
            CompRiveContext* context = (CompRiveContext*)params.m_Context;
            dmGameObject::PropertyResult res = dmGameSystem::SetResourceProperty(context->m_Factory, params.m_Value, MATERIAL_EXT_HASH, (void**)&component->m_Material);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
            return res;
        }
        return dmGameSystem::SetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, params.m_Value, CompRiveSetConstantCallback, component);
    }

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params)
    {
        RiveWorld* world = (RiveWorld*) params.m_UserData;
        dmArray<RiveComponent*>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            RiveComponent* component = components[i];
            RiveModelResource* resource = component->m_Resource;
            if (!component->m_Enabled || !resource)
                continue;

            if (resource == params.m_Resource->m_Resource ||
                resource->m_Scene == params.m_Resource->m_Resource ||
                resource->m_Scene->m_Scene == params.m_Resource->m_Resource)
            {
                OnResourceReloaded(world, component, i);
            }
        }
    }

    static dmGameObject::Result CompRiveRegister(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        CompRiveContext* rivectx    = new CompRiveContext;
        rivectx->m_Factory          = ctx->m_Factory;
        rivectx->m_GraphicsContext  = *(dmGraphics::HContext*)ctx->m_Contexts.Get(dmHashString64("graphics"));
        rivectx->m_RenderContext    = *(dmRender::HRenderContext*)ctx->m_Contexts.Get(dmHashString64("render"));
        rivectx->m_MaxInstanceCount = dmConfigFile::GetInt(ctx->m_Config, "rive.max_instance_count", 128);
        rivectx->m_RiveContext      = rive::createContext();

        rive::g_Ctx = rivectx->m_RiveContext;
        rive::setRenderMode(rivectx->m_RiveContext, rive::MODE_STENCIL_TO_COVER);
        rive::setBufferCallbacks(rivectx->m_RiveContext, RiveRequestBufferCallback, RiveDestroyBufferCallback, 0x0);

        rivectx->m_RiveRenderer = rive::createRenderer(rivectx->m_RiveContext);
        rive::setContourQuality(rivectx->m_RiveRenderer, 0.8888888888888889f);
        rive::setClippingSupport(rivectx->m_RiveRenderer, true);

        // after script/anim/gui, before collisionobject
        // the idea is to let the scripts/animations update the game object instance,
        // and then let the component update its bones, allowing them to influence collision objects in the same frame
        ComponentTypeSetPrio(type, 350);
        ComponentTypeSetContext(type, rivectx);
        ComponentTypeSetHasUserData(type, true);
        ComponentTypeSetReadsTransforms(type, false);

        ComponentTypeSetNewWorldFn(type, CompRiveNewWorld);
        ComponentTypeSetDeleteWorldFn(type, CompRiveDeleteWorld);
        ComponentTypeSetCreateFn(type, CompRiveCreate);
        ComponentTypeSetDestroyFn(type, CompRiveDestroy);
        ComponentTypeSetAddToUpdateFn(type, CompRiveAddToUpdate);
        ComponentTypeSetUpdateFn(type, CompRiveUpdate);
        ComponentTypeSetRenderFn(type, CompRiveRender);
        ComponentTypeSetOnMessageFn(type, CompRiveOnMessage);
            // ComponentTypeSetOnInputFn(type, CompRiveOnInput);
        ComponentTypeSetOnReloadFn(type, CompRiveOnReload);
        ComponentTypeSetGetPropertyFn(type, CompRiveGetProperty);
        ComponentTypeSetSetPropertyFn(type, CompRiveSetProperty);
            // ComponentTypeSetPropertyIteratorFn(type, CompRiveIterProperties); // for debugging/testing e.g. via extension-poco
        ComponentTypeSetGetFn(type, CompRiveGetComponent);

        return dmGameObject::RESULT_OK;
    }

    static rive::HBuffer RiveRequestBufferCallback(rive::HBuffer buffer, rive::BufferType type, void* data, unsigned int dataSize, void* userData)
    {
        RiveBuffer* buf = (RiveBuffer*) buffer;
        if (dataSize == 0)
        {
            return 0;
        }

        if (buf == 0)
        {
            buf = new RiveBuffer();
        }

        buf->m_Data = realloc(buf->m_Data, dataSize);
        buf->m_Size = dataSize;
        memcpy(buf->m_Data, data, dataSize);

        return (rive::HBuffer) buf;
    }

    static void RiveDestroyBufferCallback(rive::HBuffer buffer, void* userData)
    {
        RiveBuffer* buf = (RiveBuffer*) buffer;
        if (buf != 0)
        {
            if (buf->m_Data != 0)
            {
                free(buf->m_Data);
            }

            delete buf;
        }
    }

    // ******************************************************************************
    // SCRIPTING HELPER FUNCTIONS
    // ******************************************************************************

    /*
    static Vector3 UpdateIKInstanceCallback(dmRig::IKTarget* ik_target)
    {
        RiveComponent* component = (RiveComponent*)ik_target->m_UserPtr;
        dmhash_t target_instance_id = ik_target->m_UserHash;
        dmGameObject::HInstance target_instance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(component->m_Instance), target_instance_id);
        if(target_instance == 0x0)
        {
            // instance have been removed, disable animation
            dmLogError("Could not get IK position for target %s, removed?", dmHashReverseSafe64(target_instance_id))
            ik_target->m_Callback = 0x0;
            ik_target->m_Mix = 0x0;
            return Vector3(0.0f);
        }
        return (Vector3)dmTransform::Apply(dmTransform::Inv(dmTransform::Mul(dmGameObject::GetWorldTransform(component->m_Instance), component->m_Transform)), dmGameObject::GetWorldPosition(target_instance));
    }

    static Vector3 UpdateIKPositionCallback(dmRig::IKTarget* ik_target)
    {
        RiveComponent* component = (RiveComponent*)ik_target->m_UserPtr;
        return (Vector3)dmTransform::Apply(dmTransform::Inv(dmTransform::Mul(dmGameObject::GetWorldTransform(component->m_Instance), component->m_Transform)), (Point3)ik_target->m_Position);
    }

    bool CompRiveSetIKTargetInstance(RiveComponent* component, dmhash_t constraint_id, float mix, dmhash_t instance_id)
    {
        dmRig::IKTarget* target = dmRig::GetIKTarget(component->m_RigInstance, constraint_id);
        if (!target) {
            return false;
        }

        target->m_Callback = UpdateIKInstanceCallback;
        target->m_Mix = mix;
        target->m_UserPtr = component;
        target->m_UserHash = instance_id;
        return true;
    }

    bool CompRiveSetIKTargetPosition(RiveComponent* component, dmhash_t constraint_id, float mix, Point3 position)
    {
        dmRig::IKTarget* target = dmRig::GetIKTarget(component->m_RigInstance, constraint_id);
        if (!target) {
            return false;
        }
        target->m_Callback = UpdateIKPositionCallback;
        target->m_Mix = mix;
        target->m_UserPtr = component;
        target->m_Position = (Vector3)position;
        return true;
    }

    bool CompRiveResetIKTarget(RiveComponent* component, dmhash_t constraint_id)
    {
        return dmRig::ResetIKTarget(component->m_RigInstance, constraint_id);
    }

    bool CompRiveSetSkin(RiveComponent* component, dmhash_t skin_id)
    {
        dmRig::Result r = dmRig::SetMesh(component->m_RigInstance, skin_id);
        return r == dmRig::RESULT_OK;
    }

    bool CompRiveSetSkinSlot(RiveComponent* component, dmhash_t skin_id, dmhash_t slot_id)
    {
        dmRig::Result r = dmRig::SetMeshSlot(component->m_RigInstance, skin_id, slot_id);
        return r == dmRig::RESULT_OK;
    }
    */
}

DM_DECLARE_COMPONENT_TYPE(ComponentTypeRive, "rivemodelc", dmRive::CompRiveRegister);
