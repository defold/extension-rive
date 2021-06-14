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

#include "comp_rive.h"
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

#include <file.hpp>
#include <renderer.hpp>
#include <rive/rive_render_api.h>

namespace dmRive
{
    using namespace dmVMath;

    static const dmhash_t PROP_ANIMATION = dmHashString64("animation");
    static const dmhash_t PROP_CURSOR = dmHashString64("cursor");
    static const dmhash_t PROP_PLAYBACK_RATE = dmHashString64("playback_rate");
    static const dmhash_t PROP_MATERIAL = dmHashString64("material");
    static const dmhash_t MATERIAL_EXT_HASH = dmHashString64("materialc");
    static const dmhash_t UNIFORM_COLOR = dmHashString64("color");

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params);
    static void DestroyComponent(struct RiveWorld* world, uint32_t index);

    static rive::HBuffer AppRequestBufferCallback(rive::HBuffer buffer, rive::BufferType type, void* data, unsigned int dataSize, void* userData);
    static void          AppDestroyBufferCallback(rive::HBuffer buffer, void* userData);

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

    // One per collection
    struct RiveWorld
    {
        dmObjectPool<RiveComponent*>        m_Components;
        dmArray<dmRender::RenderObject>     m_RenderObjects;
        dmGraphics::HVertexDeclaration      m_VertexDeclaration;
        dmGraphics::HVertexBuffer           m_VertexBuffer;
        dmArray<RiveVertex>                 m_VertexBufferData;
        dmGraphics::HIndexBuffer            m_IndexBuffer;
        dmArray<int>                        m_IndexBufferData;
        dmArray<RiveDrawEntry>              m_DrawEntries;

        rive::HRenderer                     m_Renderer; // JG: move to context instead?
    };

    // For the entire app's life cycle
    struct CompRiveContext
    {
        CompRiveContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmResource::HFactory     m_Factory;
        dmRender::HRenderContext m_RenderContext;
        dmGraphics::HContext     m_GraphicsContext;
        uint32_t                 m_MaxInstanceCount;
    };

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

        rive::setRenderMode(rive::MODE_TESSELLATION);
        rive::setBufferCallbacks(AppRequestBufferCallback, AppDestroyBufferCallback, (void*) world);
        world->m_Renderer = rive::createRenderer();
        rive::setContourQuality(world->m_Renderer, 0.8888888888888889f);

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

        delete world->m_Renderer;
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
        // dmHashUpdateBuffer32(&state, &resource->m_Scene->m_Texture, sizeof(dmGraphics::HTexture));
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

    static void RenderBatchStencilToCover(RiveWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {

    }

    static void RenderBatchTessellation(RiveWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        RiveComponent* first        = (RiveComponent*) buf[*begin].m_UserData;
        RiveModelResource* resource = first->m_Resource;

        uint32_t vertex_count = 0;
        uint32_t index_count = 0;

        for (uint32_t *i=begin;i!=end;i++)
        {
            const RiveComponent* c = (RiveComponent*) buf[*i].m_UserData;
            vertex_count += c->m_VertexCount;
            index_count += c->m_IndexCount;
        }

        dmArray<RiveVertex> &vertex_buffer = world->m_VertexBufferData;
        if (vertex_buffer.Remaining() < vertex_count)
            vertex_buffer.OffsetCapacity(vertex_count - vertex_buffer.Remaining());

        dmArray<int> &index_buffer = world->m_IndexBufferData;
        if (index_buffer.Remaining() < index_count)
            index_buffer.OffsetCapacity(index_count - index_buffer.Remaining());

        RiveVertex* vb_begin = vertex_buffer.End();
        RiveVertex* vb_end = vb_begin;

        int* ix_begin = index_buffer.End();
        int* ix_end   = ix_begin;

        uint32_t last_ix = 0;
        for (int i = 0; i < world->m_DrawEntries.Size(); ++i)
        {
            const RiveDrawEntry& entry = world->m_DrawEntries[i];
            RiveBuffer* vxData         = (RiveBuffer*) entry.m_Buffers.m_Tessellation.m_VertexBuffer;
            RiveBuffer* ixData         = (RiveBuffer*) entry.m_Buffers.m_Tessellation.m_IndexBuffer;

            int* ix_data_ptr  = (int*) ixData->m_Data;
            uint32_t ix_count = ixData->m_Size / sizeof(int);
            uint32_t vx_count = vxData->m_Size / sizeof(RiveVertex);

            // Note: We offset the indices per path so that we can use the same
            //       vertex buffer for all paths. As all path indices are generated
            //       by libtess we have to offset them manually.
            for (int j = 0; j < ix_count; ++j)
            {
                ix_end[j] = ix_data_ptr[j] + last_ix;
            }

            memcpy(vb_end, vxData->m_Data, vxData->m_Size);

            vb_end  += vx_count;
            ix_end  += ix_count;
            last_ix += vx_count;
        }

        // update the size
        vertex_buffer.SetSize(vb_end - vertex_buffer.Begin());
        index_buffer.SetSize(ix_end - index_buffer.Begin());

        uint32_t ro_start = world->m_RenderObjects.Size();
        world->m_RenderObjects.SetSize(world->m_RenderObjects.Size() + world->m_DrawEntries.Size());

        last_ix = 0;
        for (int i = 0; i < world->m_DrawEntries.Size(); ++i)
        {
            const RiveDrawEntry& entry = world->m_DrawEntries[i];
            dmRender::RenderObject& ro = world->m_RenderObjects[ro_start + i];
            RiveBuffer* ixBuffer      = (RiveBuffer*) entry.m_Buffers.m_Tessellation.m_IndexBuffer;

            ro.Init();
            ro.m_VertexDeclaration = world->m_VertexDeclaration;
            ro.m_VertexBuffer      = world->m_VertexBuffer;
            ro.m_PrimitiveType     = dmGraphics::PRIMITIVE_TRIANGLES;
            ro.m_VertexStart       = last_ix;
            ro.m_VertexCount       = ixBuffer->m_Size / sizeof(int);
            ro.m_Material          = GetMaterial(first, resource);
            ro.m_IndexBuffer       = world->m_IndexBuffer;
            ro.m_IndexType         = dmGraphics::TYPE_UNSIGNED_INT;
            ro.m_WorldTransform    = entry.m_WorldTransform;

            last_ix += ixBuffer->m_Size;

            if (!first->m_RenderConstants)
            {
                first->m_RenderConstants = dmGameSystem::CreateRenderConstants();
            }

            const rive::PaintData draw_entry_paint = rive::getPaintData(entry.m_Paint);
            const float* color = &draw_entry_paint.m_Colors[0];
            dmGameObject::PropertyVar colorVar(Vectormath::Aos::Vector4(color[0], color[1], color[2], color[3]));
            dmGameSystem::SetRenderConstant(first->m_RenderConstants, ro.m_Material, UNIFORM_COLOR, 0, colorVar);
            dmGameSystem::EnableRenderObjectConstants(&ro, first->m_RenderConstants);

            SetBlendMode(ro, resource->m_DDF->m_BlendMode);

            dmRender::AddToRender(render_context, &ro);
        }
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

    static void HandleDrawEventsStencilToCover(RiveWorld* world, RiveComponent& component, int start_index, int end_index)
    {
        bool is_clipping         = false;
        rive::HRenderPaint paint = 0;
        for (int i = start_index; i < end_index; ++i)
        {
            const rive::PathDrawEvent evt = rive::getDrawEvent(world->m_Renderer, i);
            switch(evt.m_Type)
            {
                case rive::EVENT_SET_PAINT:
                    paint = evt.m_Paint;
                    break;
                case rive::EVENT_DRAW_STENCIL:
                {
                    // TODO
                } break;
                case rive::EVENT_DRAW_COVER:
                {
                    // TODO
                } break;
                case rive::EVENT_CLIPPING_BEGIN:
                    is_clipping = true;
                    break;
                case rive::EVENT_CLIPPING_END:
                    is_clipping = false;
                    break;
                default:break;
            }
        }
    }

    static void HandleDrawEventsTessellation(RiveWorld* world, RiveComponent& component, int start_index, int end_index)
    {
        uint32_t vertex_count    = 0;
        uint32_t index_count     = 0;
        bool is_clipping         = false;
        rive::HRenderPaint paint = 0;

        for (int i = start_index; i < end_index; ++i)
        {
            const rive::PathDrawEvent evt = rive::getDrawEvent(world->m_Renderer, i);
            switch(evt.m_Type)
            {
                case rive::EVENT_SET_PAINT:
                    paint = evt.m_Paint;
                    break;
                case rive::EVENT_DRAW:
                {
                    if (!is_clipping)
                    {
                        rive::DrawBuffers buffers = rive::getDrawBuffers(evt.m_Path);
                        RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_Tessellation.m_VertexBuffer;
                        RiveBuffer* ixBuffer      = (RiveBuffer*) buffers.m_Tessellation.m_IndexBuffer;

                        if (vxBuffer != 0 && ixBuffer != 0)
                        {
                            vertex_count += vxBuffer->m_Size / (2 * sizeof(float));
                            index_count  += ixBuffer->m_Size / sizeof(int);

                            if (world->m_DrawEntries.Full())
                            {
                                world->m_DrawEntries.OffsetCapacity(16);
                            }

                            RiveDrawEntry entry;
                            entry.m_Buffers = buffers;
                            entry.m_Paint   = paint;

                            Mat2DToMat4(evt.m_TransformWorld, entry.m_WorldTransform);
                            if (i == 0)
                            {
                                entry.m_WorldTransform = component.m_World * entry.m_WorldTransform;
                            }

                            world->m_DrawEntries.Push(entry);
                        }
                    }
                } break;
                case rive::EVENT_CLIPPING_BEGIN:
                    is_clipping = true;
                    break;
                case rive::EVENT_CLIPPING_END:
                    is_clipping = false;
                    break;
                case rive::EVENT_CLIPPING_DISABLE:
                    is_clipping = false;
                    break;
                default:break;
            }
        }

        component.m_VertexCount = vertex_count;
        component.m_IndexCount  = index_count;
    }

    dmGameObject::UpdateResult CompRiveUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;

        rive::newFrame(world->m_Renderer);
        rive::Renderer* rive_renderer = (rive::Renderer*) world->m_Renderer;
        float dt = params.m_UpdateContext->m_DT;

        dmArray<RiveComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        world->m_DrawEntries.SetSize(0);

        const rive::RenderMode render_mode = rive::getRenderMode();

        for (uint32_t i = 0; i < count; ++i)
        {
            RiveComponent& component = *components[i];
            component.m_DoRender = 0;

            if (!component.m_Enabled || !component.m_AddedToUpdate)
            {
                continue;
            }

            // RIVE UPDATE
            uint32_t start_event_count = rive::getDrawEventCount(world->m_Renderer);
            rive::File* f              = (rive::File*) component.m_Resource->m_Scene->m_Scene;
            rive::Artboard* artboard   = f->artboard();
            rive::AABB artboard_bounds = artboard->bounds();

            rive_renderer->save();
            artboard->advance(dt);
            artboard->draw(rive_renderer);
            rive_renderer->restore();

            // Handle draw events
            if (render_mode == rive::MODE_TESSELLATION)
            {
                HandleDrawEventsTessellation(world, component, start_event_count, rive::getDrawEventCount(world->m_Renderer));
            }
            else if (render_mode == rive::MODE_STENCIL_TO_COVER)
            {
                HandleDrawEventsStencilToCover(world, component, start_event_count, rive::getDrawEventCount(world->m_Renderer));
            }

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
        RiveWorld *world = (RiveWorld *) params.m_UserData;
        rive::RenderMode renderMode = rive::getRenderMode();

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
                if (renderMode == rive::MODE_TESSELLATION)
                {
                    RenderBatchTessellation(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
                }
                else if (renderMode == rive::MODE_STENCIL_TO_COVER)
                {
                    RenderBatchStencilToCover(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
                }
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
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < count; ++i)
        {
            RiveComponent& component = *components[i];
            if (!component.m_DoRender || !component.m_Enabled)
                continue;
            const Vector4 trans = component.m_World.getCol(3);
            write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());
            write_ptr->m_UserData = (uintptr_t) &component;
            write_ptr->m_BatchKey = component.m_MixedHash;
            write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(GetMaterial(&component, component.m_Resource));
            write_ptr->m_Dispatch = dispatch;
            write_ptr->m_MinorOrder = 0;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
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
            // if (params.m_Message->m_Id == dmRiveDDF::SpinePlayAnimation::m_DDFDescriptor->m_NameHash)
            // {
            //     dmRiveDDF::SpinePlayAnimation* ddf = (dmRiveDDF::SpinePlayAnimation*)params.m_Message->m_Data;
            //     if (dmRig::RESULT_OK == dmRig::PlayAnimation(component->m_RigInstance, ddf->m_AnimationId, ddf_playback_map.m_Table[ddf->m_Playback], ddf->m_BlendDuration, ddf->m_Offset, ddf->m_PlaybackRate))
            //     {
            //         component->m_Listener = params.m_Message->m_Sender;
            //     }
            // }
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
        CompRiveContext* rivectx = new CompRiveContext;
        rivectx->m_Factory = ctx->m_Factory;
        rivectx->m_GraphicsContext = *(dmGraphics::HContext*)ctx->m_Contexts.Get(dmHashString64("graphics"));
        rivectx->m_RenderContext = *(dmRender::HRenderContext*)ctx->m_Contexts.Get(dmHashString64("render"));
        rivectx->m_MaxInstanceCount = dmConfigFile::GetInt(ctx->m_Config, "rive.max_instance_count", 128);

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

    static rive::HBuffer AppRequestBufferCallback(rive::HBuffer buffer, rive::BufferType type, void* data, unsigned int dataSize, void* userData)
    {
        RiveWorld* world = (RiveWorld*) userData;
        RiveBuffer* buf = (RiveBuffer*) buffer;

        if (buf == 0)
        {
            buf = new RiveBuffer;
            buf->m_Data = malloc(dataSize);
            buf->m_Size = dataSize;
        }

        if (buf->m_Data != 0 && buf->m_Size != dataSize)
        {
            free(buf->m_Data);
            buf->m_Data = malloc(dataSize);
            buf->m_Size = dataSize;
        }

        memcpy(buf->m_Data, data, dataSize);

        return (rive::HBuffer) buf;
    }

    static void AppDestroyBufferCallback(rive::HBuffer buffer, void* userData)
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
