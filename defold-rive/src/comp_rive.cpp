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

#if !defined(DM_RIVE_UNSUPPORTED)

#include <string.h> // memset

// rive-cpp
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/animation/state_machine_trigger.hpp>
#include <rive/animation/state_machine_bool.hpp>
#include <rive/animation/state_machine_number.hpp>
#include <rive/animation/loop.hpp>
#include <rive/bones/bone.hpp>
#include <rive/file.hpp>
#include <rive/renderer.hpp>

// Rive extension
#include "comp_rive.h"
#include "res_rive_data.h"
#include "res_rive_scene.h"
#include "res_rive_model.h"

#include <common/bones.h>
#include <common/vertices.h>
#include <common/factory.h>
#include <common/tess_renderer.h>

// DMSDK
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/math.h>
#include <dmsdk/dlib/object_pool.h>
#include <dmsdk/dlib/profile.h>
#include <dmsdk/gameobject/component.h>
#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/gamesys/property.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/render/render.h>
#include <dmsdk/resource/resource.h>
#include <gameobject/gameobject_ddf.h> // for creating bones where the rive scene bones are

#include <dmsdk/gamesys/resources/res_skeleton.h>
#include <dmsdk/gamesys/resources/res_rig_scene.h>
#include <dmsdk/gamesys/resources/res_meshset.h>
#include <dmsdk/gamesys/resources/res_animationset.h>
#include <dmsdk/gamesys/resources/res_textureset.h>

DM_PROPERTY_GROUP(rmtp_Rive, "Rive");
DM_PROPERTY_U32(rmtp_RiveBones, 0, FrameReset, "# rive bones", &rmtp_Rive);
DM_PROPERTY_U32(rmtp_RiveComponents, 0, FrameReset, "# rive components", &rmtp_Rive);

namespace dmRive
{
    using namespace dmVMath;

    static const dmhash_t PROP_ANIMATION          = dmHashString64("animation");
    static const dmhash_t PROP_CURSOR             = dmHashString64("cursor");
    static const dmhash_t PROP_PLAYBACK_RATE      = dmHashString64("playback_rate");
    static const dmhash_t PROP_MATERIAL           = dmHashString64("material");
    static const dmhash_t MATERIAL_EXT_HASH       = dmHashString64("materialc");
    // static const dmhash_t UNIFORM_TRANSFORM_LOCAL = dmHashString64("transform_local");
    // static const dmhash_t UNIFORM_COVER           = dmHashString64("cover");

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params);
    static void DestroyComponent(struct RiveWorld* world, uint32_t index);
    static void CompRiveAnimationReset(RiveComponent* component);
    static bool PlayAnimation(RiveComponent* component, dmRive::RiveSceneData* data, dmhash_t anim_id,
                    dmGameObject::Playback playback_mode, float offset, float playback_rate);
    static bool PlayStateMachine(RiveComponent* component, dmRive::RiveSceneData* data, dmhash_t anim_id, float playback_rate);
    static bool CreateBones(struct RiveWorld* world, RiveComponent* component);
    static void DeleteBones(RiveComponent* component);
    static void UpdateBones(RiveComponent* component);

    // For the entire app's life cycle
    struct CompRiveContext
    {
        CompRiveContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmResource::HFactory        m_Factory;
        dmRender::HRenderContext    m_RenderContext;
        dmGraphics::HContext        m_GraphicsContext;
        dmGraphics::HTexture        m_NullTexture;
        uint32_t                    m_MaxInstanceCount;
    };

    // One per collection
    struct RiveWorld
    {
        CompRiveContext*                    m_Ctx;
        dmRive::DefoldTessRenderer*         m_Renderer;
        dmObjectPool<RiveComponent*>        m_Components;
        dmArray<dmRender::RenderObject>         m_RenderObjects;
        dmArray<dmRender::HNamedConstantBuffer> m_RenderConstants; // 1:1 mapping with the render objects
        dmGraphics::HVertexDeclaration      m_VertexDeclaration;
        dmGraphics::HVertexBuffer           m_VertexBuffer;
        dmArray<RiveVertex>                 m_VertexBufferData;
        dmGraphics::HIndexBuffer            m_IndexBuffer;
        dmArray<uint16_t>                   m_IndexBufferData;
    };

    dmGameObject::CreateResult CompRiveNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        CompRiveContext* context = (CompRiveContext*)params.m_Context;
        RiveWorld* world         = new RiveWorld();

        world->m_Ctx = context;

        world->m_Renderer = new DefoldTessRenderer();
        world->m_Components.SetCapacity(context->m_MaxInstanceCount);
        world->m_RenderObjects.SetCapacity(context->m_MaxInstanceCount);
        world->m_RenderConstants.SetCapacity(context->m_MaxInstanceCount);
        world->m_RenderConstants.SetSize(context->m_MaxInstanceCount);
        memset(world->m_RenderConstants.Begin(), 0, sizeof(dmGameSystem::HComponentRenderConstants)*world->m_RenderConstants.Capacity());

        dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(context->m_GraphicsContext);
        dmGraphics::AddVertexStream(stream_declaration, "position", 2, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);

        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(context->m_GraphicsContext, stream_declaration);
        world->m_VertexBuffer      = dmGraphics::NewVertexBuffer(context->m_GraphicsContext, 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        world->m_IndexBuffer       = dmGraphics::NewIndexBuffer(context->m_GraphicsContext, 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);

        dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

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

        for (uint32_t i = 0; i < world->m_RenderConstants.Size(); ++i)
        {
            if (world->m_RenderConstants[i])
                dmRender::DeleteNamedConstantBuffer(world->m_RenderConstants[i]);
        }

        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline dmRender::HMaterial GetMaterial(const RiveComponent* component, const RiveModelResource* resource) {
        return component->m_Material ? component->m_Material : resource->m_Material->m_Material;
    }

    static void ReHash(RiveComponent* component)
    {
        // material, texture set, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        RiveModelResource* resource = component->m_Resource;
        dmRiveDDF::RiveModelDesc* ddf = resource->m_DDF;
        dmRender::HMaterial material = GetMaterial(component, resource);
        dmGameSystem::TextureSetResource* texture_set = resource->m_Scene->m_TextureSet;
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &material, sizeof(material));
        dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode));
        if (texture_set)
            dmHashUpdateBuffer32(&state, &texture_set, sizeof(texture_set));
        if (component->m_RenderConstants)
            dmGameSystem::HashRenderConstants(component->m_RenderConstants, &state);
        component->m_MixedHash = dmHashFinal32(&state);
        component->m_ReHash = 0;
    }

    static inline RiveComponent* GetComponentFromIndex(RiveWorld* world, int index)
    {
        return world->m_Components.Get(index);
    }

    void* CompRiveGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        return GetComponentFromIndex(world, index);
    }

    dmGameObject::CreateResult CompRiveCreate(const dmGameObject::ComponentCreateParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;

        if (world->m_Components.Full())
        {
            dmLogError("Rive instance could not be created since the buffer is full (%d).", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        RiveComponent* component = new RiveComponent;
        memset(component, 0, sizeof(RiveComponent));

        uint32_t index = world->m_Components.Alloc();
        world->m_Components.Set(index, component);
        component->m_Instance = params.m_Instance;
        component->m_Transform = dmTransform::Transform(Vector3(params.m_Position), params.m_Rotation, 1.0f);
        component->m_Resource = (RiveModelResource*)params.m_Resource;

        CompRiveAnimationReset(component);
        dmMessage::ResetURL(&component->m_Listener);

        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_World = Matrix4::identity();
        component->m_DoRender = 0;
        component->m_RenderConstants = 0;

        dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;

        //Todo: choose artboard by index: m_ArtboardIndex = (index == REQUEST_DEFAULT_SCENE) ? 0 : index;
        //component->m_ArtboardInstance = data->m_File->artboardAt(0);
        component->m_ArtboardInstance = data->m_File->artboardDefault();
        component->m_ArtboardInstance->advance(0.0f);

        if (component->m_Resource->m_CreateGoBones)
        {
            dmRive::GetAllBones(component->m_ArtboardInstance.get(), &component->m_Bones);
            CreateBones(world, component);
            UpdateBones(component);
        }

        dmhash_t empty_id = dmHashString64("");
        dmhash_t anim_id = dmHashString64(component->m_Resource->m_DDF->m_DefaultAnimation);
        dmhash_t state_machine_id = dmHashString64(component->m_Resource->m_DDF->m_DefaultStateMachine);

        if (empty_id != state_machine_id)
        {
            if (!PlayStateMachine(component, data, state_machine_id, 1.0f))
            {
                dmLogError("Couldn't play state machine named '%s'", dmHashReverseSafe64(state_machine_id));
            }
        }
        else if (empty_id != anim_id)
        {
            if (!PlayAnimation(component, data, anim_id, dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f, 1.0f))
            {
                dmLogError("Couldn't play animation named '%s'", dmHashReverseSafe64(anim_id));
            }
        }

        component->m_ReHash = 1;

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void DestroyComponent(RiveWorld* world, uint32_t index)
    {
        RiveComponent* component = GetComponentFromIndex(world, index);
        dmGameObject::DeleteBones(component->m_Instance);

        if (component->m_RenderConstants)
            dmGameSystem::DestroyRenderConstants(component->m_RenderConstants);

        component->m_ArtboardInstance.reset();
        component->m_AnimationInstance.reset();
        component->m_StateMachineInstance.reset();

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

    static void AddToRender(dmRender::HRenderContext render_context, dmRender::RenderObject* render_objects, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            dmRender::AddToRender(render_context, &render_objects[i]);
        }
    }

    /*
    rive::BlendMode::srcOver
    rive::BlendMode::screen
    rive::BlendMode::overlay
    rive::BlendMode::darken
    rive::BlendMode::lighten
    rive::BlendMode::colorDodge
    rive::BlendMode::colorBurn
    rive::BlendMode::hardLight
    rive::BlendMode::softLight
    rive::BlendMode::difference
    rive::BlendMode::exclusion
    rive::BlendMode::multiply
    rive::BlendMode::hue
    rive::BlendMode::saturation
    rive::BlendMode::color
    rive::BlendMode::luminosity
    */

    static void GetBlendFactorsFromBlendMode(rive::BlendMode blend_mode, dmGraphics::BlendFactor* src, dmGraphics::BlendFactor* dst)
    {
        switch(blend_mode)
        {
            case rive::BlendMode::srcOver:
                // JG: Some textures look strange without one here for src, but the sokol viewer sets these blend modes so not sure what to do here
                //*src = dmGraphics::BLEND_FACTOR_ONE;
                *src = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
                *dst = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case rive::BlendMode::colorDodge:
                *src = dmGraphics::BLEND_FACTOR_DST_COLOR; // SG_BLENDFACTOR_SRC_ALPHA
                *dst = dmGraphics::BLEND_FACTOR_ONE;       // SG_BLENDFACTOR_ONE;
            break;

            case rive::BlendMode::multiply:
                *src = dmGraphics::BLEND_FACTOR_DST_COLOR;           // SG_BLENDFACTOR_DST_COLOR
                *dst = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
            break;

            case rive::BlendMode::screen:
                *src = dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR;
                *dst = dmGraphics::BLEND_FACTOR_ONE;
            break;

            default:
                if ((int) blend_mode != 0)
                {
                    dmLogOnceWarning("Blend mode '%s' (%d) is not supported, defaulting to '%s'", dmRive::BlendModeToStr(blend_mode), (int) blend_mode, BlendModeToStr(rive::BlendMode::srcOver));
                }
                *src = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
                *dst = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;
        }
    }

    static void GetBlendFactorsFromBlendMode(dmRiveDDF::RiveModelDesc::BlendMode blend_mode, dmGraphics::BlendFactor* src, dmGraphics::BlendFactor* dst)
    {
        switch (blend_mode)
        {
            case dmRiveDDF::RiveModelDesc::BLEND_MODE_ALPHA:
                *src = dmGraphics::BLEND_FACTOR_ONE;
                *dst = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmRiveDDF::RiveModelDesc::BLEND_MODE_ADD:
                *src = dmGraphics::BLEND_FACTOR_ONE;
                *dst = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmRiveDDF::RiveModelDesc::BLEND_MODE_MULT:
                *src = dmGraphics::BLEND_FACTOR_DST_COLOR;
                *dst = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmRiveDDF::RiveModelDesc::BLEND_MODE_SCREEN:
                *src = dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR;
                *dst = dmGraphics::BLEND_FACTOR_ONE;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
                assert(0);
            break;
        }
    }

    static void RenderBatch(RiveWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        dmRive::DefoldTessRenderer* renderer = world->m_Renderer;
        RiveComponent*              first    = (RiveComponent*) buf[*begin].m_UserData;
        RiveModelResource*          resource = first->m_Resource;

        uint32_t ro_count         = 0;
        uint32_t vertex_count     = 0;
        uint32_t index_count      = 0;

        dmRive::DrawDescriptor* draw_descriptors;
        renderer->getDrawDescriptors(&draw_descriptors, &ro_count);

        for (int i = 0; i < ro_count; ++i)
        {
            vertex_count += draw_descriptors[i].m_VerticesCount;
            index_count += draw_descriptors[i].m_IndicesCount;
        }

        // Make sure we have enough free render objects
        if (world->m_RenderObjects.Remaining() < ro_count)
        {
            uint32_t grow = ro_count - world->m_RenderObjects.Remaining();
            world->m_RenderObjects.OffsetCapacity(grow);

            uint32_t prev_size = world->m_RenderConstants.Size();
            world->m_RenderConstants.OffsetCapacity(grow);
            world->m_RenderConstants.SetSize(world->m_RenderConstants.Capacity());
            memset(world->m_RenderConstants.Begin() + prev_size, 0, sizeof(dmGameSystem::HComponentRenderConstants)*grow);

            assert(world->m_RenderObjects.Capacity() == world->m_RenderConstants.Capacity());
        }

        uint32_t ro_index = world->m_RenderObjects.Size();

        // Make sure we have enough room for new vertex data
        dmArray<RiveVertex> &vertex_buffer = world->m_VertexBufferData;
        if (vertex_buffer.Remaining() < vertex_count)
        {
            vertex_buffer.OffsetCapacity(vertex_count - vertex_buffer.Remaining());
        }

        RiveVertex* vb_begin = vertex_buffer.End();
        RiveVertex* vb_end = vb_begin;
        vertex_buffer.SetSize(vertex_buffer.Capacity());

        // Make sure we have enough room for new index data
        dmArray<uint16_t> &index_buffer = world->m_IndexBufferData;
        if (index_buffer.Remaining() < index_count)
        {
            index_buffer.OffsetCapacity(index_count - index_buffer.Remaining());
        }

        uint16_t* ix_begin = index_buffer.End();
        index_buffer.SetSize(index_buffer.Capacity());

        uint32_t ro_offset = world->m_RenderObjects.Size();
        world->m_RenderObjects.SetSize(world->m_RenderObjects.Size() + ro_count);

        dmRender::RenderObject* render_objects = world->m_RenderObjects.Begin() + ro_offset;
        dmRender::HNamedConstantBuffer* render_constants = world->m_RenderConstants.Begin() + ro_offset;

        dmRender::HMaterial material = GetMaterial(first, resource);

        dmhash_t constant_names[4] = {UNIFORM_PROPERTIES, UNIFORM_GRADIENT_LIMITS, UNIFORM_COLOR, UNIFORM_STOPS};
        dmRender::HConstant material_constants[4];
        dmRenderDDF::MaterialDesc::ConstantType material_constant_types[4];

        // Currently, the dmRender::GetMaterialProgramConstant isn't part of the dmSdk
        material_constant_types[0] =
        material_constant_types[1] =
        material_constant_types[2] =
        material_constant_types[3] = dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER;

        // ro_count = 0;
        uint32_t index_offset = 0;
        uint32_t vertex_offset = 0;
        RiveVertex* vb_write = vb_begin;
        uint16_t* ix_write = (uint16_t*) ix_begin;

        bool debug_info = false;
    #define DO_LOG(...) \
        if (debug_info) printf(__VA_ARGS__);

        DO_LOG("Ro_Count: %d\n", ro_count);

        for (int i = 0; i < ro_count; ++i)
        {
            if (!render_constants[i])
            {
                render_constants[i] = dmRender::NewNamedConstantBuffer();
            }

            dmRive::DrawDescriptor& draw_desc = draw_descriptors[i];
            dmRender::RenderObject& ro = render_objects[i];

            memset(&ro.m_StencilTestParams, 0, sizeof(ro.m_StencilTestParams));
            ro.m_StencilTestParams.Init();
            ro.Init();

            ro.m_VertexDeclaration = world->m_VertexDeclaration;
            ro.m_VertexBuffer      = world->m_VertexBuffer;
            ro.m_IndexBuffer       = world->m_IndexBuffer;
            ro.m_Material          = material;
            ro.m_VertexStart       = index_offset * sizeof(uint16_t); // byte offset
            ro.m_VertexCount       = draw_desc.m_IndicesCount;
            ro.m_IndexType         = dmGraphics::TYPE_UNSIGNED_SHORT;
            ro.m_PrimitiveType     = dmGraphics::PRIMITIVE_TRIANGLES;
            ro.m_SetBlendFactors   = 1;

            if (resource->m_Scene->m_TextureSet)
            {
                ro.m_Textures[0] = resource->m_Scene->m_TextureSet->m_Texture->m_Texture;
            }

            DO_LOG("Ro: %d, vx %d ix %d\n", i, draw_desc.m_VerticesCount, draw_desc.m_IndicesCount);

            dmRive::CopyVertices(draw_desc, vertex_offset, vb_write, ix_write);

            index_offset += draw_desc.m_IndicesCount;
            vertex_offset += draw_desc.m_VerticesCount;
            ix_write += draw_desc.m_IndicesCount;
            vb_write += draw_desc.m_VerticesCount;

            DO_LOG("\n");

        #undef DO_LOG

            const dmRive::FsUniforms fs_uniforms = draw_desc.m_FsUniforms;
            const dmRive::VsUniforms vs_uniforms = draw_desc.m_VsUniforms;
            const int MAX_STOPS = 4;
            const int MAX_COLORS = 16;
            const int num_stops = fs_uniforms.stopCount > 1 ? fs_uniforms.stopCount : 1;

            dmVMath::Vector4 properties((float)fs_uniforms.fillType, (float) fs_uniforms.stopCount, 0.0f, 0.0f);
            dmVMath::Vector4 gradient_limits(vs_uniforms.gradientStart.x, vs_uniforms.gradientStart.y, vs_uniforms.gradientEnd.x, vs_uniforms.gradientEnd.y);

            ro.m_ConstantBuffer = render_constants[i];

            // Follows the size+order of constant_names
            dmVMath::Vector4* constant_values[4] = {
                (dmVMath::Vector4*) &properties,
                (dmVMath::Vector4*) &gradient_limits,
                (dmVMath::Vector4*) fs_uniforms.colors,
                (dmVMath::Vector4*) fs_uniforms.stops
            };
            uint32_t constant_value_counts[4] = {
                1,
                1,
                sizeof(fs_uniforms.colors) / sizeof(dmVMath::Vector4),
                sizeof(fs_uniforms.stops) / sizeof(dmVMath::Vector4)
            };
            for (uint32_t i = 0; i < DM_ARRAY_SIZE(constant_names); ++i)
            {
                dmRender::SetNamedConstant(ro.m_ConstantBuffer, constant_names[i], constant_values[i], constant_value_counts[i], material_constant_types[i]);
            }

            dmRive::ApplyDrawMode(ro, draw_desc.m_DrawMode, draw_desc.m_ClipIndex);

            memcpy(&ro.m_WorldTransform, &vs_uniforms.world, sizeof(vs_uniforms.world));

            GetBlendFactorsFromBlendMode(draw_desc.m_BlendMode, &ro.m_SourceBlendFactor, &ro.m_DestinationBlendFactor);
        }

        // uint32_t num_ros_used = ProcessRiveEvents(ctx, renderer, vb_begin, ix_begin, RiveEventCallback_RenderObject, &engine_ctx);

        // if (num_ros_used < ro_count)
        // {
        //     world->m_RenderObjects.SetSize(world->m_RenderObjects.Size() - (ro_count - num_ros_used));
        //     ro_count = num_ros_used;
        // }

        AddToRender(render_context, render_objects, ro_count);
    }

    void UpdateTransforms(RiveWorld* world)
    {
        DM_PROFILE("UpdateTransforms");

        dmArray<RiveComponent*>& components = world->m_Components.GetRawObjects();
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

    static bool GetSender(RiveComponent* component, dmMessage::URL* out_sender)
    {
        dmMessage::URL sender;
        sender.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_Instance));
        if (dmMessage::IsSocketValid(sender.m_Socket))
        {
            dmGameObject::Result go_result = dmGameObject::GetComponentId(component->m_Instance, component->m_ComponentIndex, &sender.m_Fragment);
            if (go_result == dmGameObject::RESULT_OK)
            {
                sender.m_Path = dmGameObject::GetIdentifier(component->m_Instance);
                *out_sender = sender;
                return true;
            }
        }
        return false;
    }

    static void CompRiveAnimationReset(RiveComponent* component)
    {
        component->m_AnimationIndex        = 0xff;
        component->m_AnimationCallbackRef  = 0;
        component->m_AnimationPlaybackRate = 1.0f;
        component->m_AnimationPlayback     = dmGameObject::PLAYBACK_NONE;

        component->m_AnimationInstance.reset();
        component->m_StateMachineInstance.reset();

        component->m_StateMachineInputs.SetSize(0);
    }

    static void CompRiveAnimationDoneCallback(RiveComponent& component)
    {
        assert(component.m_AnimationInstance);
        dmMessage::URL sender;
        dmMessage::URL receiver  = component.m_Listener;

        if (!GetSender(&component, &sender))
        {
            dmLogError("Could not send animation_done to listener because of incomplete component.");
            return;
        }

        dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component.m_Resource->m_Scene->m_Scene;
        dmhash_t message_id         = dmRiveDDF::RiveAnimationDone::m_DDFDescriptor->m_NameHash;

        dmRiveDDF::RiveAnimationDone message;
        message.m_AnimationId = data->m_LinearAnimations[component.m_AnimationIndex];
        message.m_Playback    = component.m_AnimationPlayback;

        uintptr_t descriptor = (uintptr_t)dmRiveDDF::RiveAnimationDone::m_DDFDescriptor;
        uint32_t data_size   = sizeof(dmRiveDDF::RiveAnimationDone);

        dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, component.m_AnimationCallbackRef, descriptor, &message, data_size, 0);
        dmMessage::ResetURL(&component.m_Listener);
        if (result != dmMessage::RESULT_OK)
        {
            dmLogError("Could not send animation_done to listener.");
        }
    }

    dmGameObject::UpdateResult CompRiveUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        DM_PROFILE("RiveModel");

        RiveWorld*                  world    = (RiveWorld*)params.m_World;
        dmRive::DefoldTessRenderer* renderer = world->m_Renderer;

        renderer->reset();

        float dt = params.m_UpdateContext->m_DT;

        dmArray<RiveComponent*>& components = world->m_Components.GetRawObjects();
        const uint32_t count = components.Size();
        DM_PROPERTY_ADD_U32(rmtp_RiveComponents, count);

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

            if (!artboard)
            {
                component.m_Enabled = false;
                continue;
            }
            rive::AABB artboard_bounds  = artboard->bounds();

            if (component.m_StateMachineInstance)
            {
                component.m_StateMachineInstance->advanceAndApply(dt * component.m_AnimationPlaybackRate);
            }
            else if (component.m_AnimationInstance)
            {
                component.m_AnimationInstance->advanceAndApply(dt * component.m_AnimationPlaybackRate);

                if (component.m_AnimationInstance->didLoop())
                {
                    bool did_finish = false;
                    switch(component.m_AnimationPlayback)
                    {
                        case dmGameObject::PLAYBACK_ONCE_FORWARD:
                            did_finish = true;
                            break;
                        case dmGameObject::PLAYBACK_ONCE_BACKWARD:
                            did_finish = true;
                            break;
                        case dmGameObject::PLAYBACK_ONCE_PINGPONG:
                            did_finish = component.m_AnimationInstance->direction() > 0;
                            break;
                        default:break;
                    }

                    if (did_finish)
                    {
                        CompRiveAnimationDoneCallback(component);
                        CompRiveAnimationReset(&component);
                    }
                }
            }
            else {
                component.m_ArtboardInstance->advance(dt * component.m_AnimationPlaybackRate);
            }

            if (component.m_Resource->m_CreateGoBones)
                UpdateBones(&component); // after the artboard->advance();

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
        RiveWorld* world = (RiveWorld*) params.m_UserData;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
            {
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
                dmGraphics::SetIndexBufferData(world->m_IndexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);

                world->m_RenderObjects.SetSize(0);
                dmArray<RiveVertex>& vertex_buffer = world->m_VertexBufferData;
                vertex_buffer.SetSize(0);

                dmArray<uint16_t>& index_buffer = world->m_IndexBufferData;
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
                                                world->m_VertexBufferData.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);

                if (world->m_IndexBufferData.Size() > 0) // Until 1.4.3 is out with a fix
                {
                    dmGraphics::SetIndexBufferData(world->m_IndexBuffer, sizeof(uint16_t) * world->m_IndexBufferData.Size(),
                                                    world->m_IndexBufferData.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
                }
                break;
            }
            default:
                assert(false);
                break;
        }
    }

    dmGameObject::UpdateResult CompRiveRender(const dmGameObject::ComponentsRenderParams& params)
    {
        DM_PROFILE("RiveModel");

        CompRiveContext* context = (CompRiveContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        RiveWorld* world = (RiveWorld*)params.m_World;
        dmRive::DefoldTessRenderer* renderer = world->m_Renderer;

        dmArray<RiveComponent*>& components = world->m_Components.GetRawObjects();
        const uint32_t count = components.Size();
        if (!count)
        {
            return dmGameObject::UPDATE_RESULT_OK;
        }

        UpdateTransforms(world);

        for (uint32_t i = 0; i < count; ++i)
        {
            RiveComponent* c = components[i];

            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;
            rive::AABB bounds = c->m_ArtboardInstance->bounds();
            rive::Mat2D viewTransform = rive::computeAlignment(rive::Fit::contain,
                                                               rive::Alignment::center,
                                                               rive::AABB(0, 0, bounds.maxX-bounds.minX, bounds.maxY-bounds.minY),
                                                               bounds);
            renderer->save();

            rive::Mat2D transform;
            Mat4ToMat2D(c->m_World, transform);

            // Rive is using a different coordinate system that defold,
            // we have to adhere to how our projection matrixes are
            // constructed so we flip the renderer on the y axis here
            rive::Vec2D yflip(1.0f,-1.0f);
            transform = transform.scale(yflip);

            renderer->transform(viewTransform * transform);

            renderer->align(rive::Fit::none,
                rive::Alignment::center,
                rive::AABB(-bounds.width(), -bounds.height(),
                bounds.width(), bounds.height()),
                bounds);

            // Store the inverse view so we can later go from screen to world.
            //m_InverseViewTransform = viewTransform.invertOrIdentity();

            renderer->SetAtlas(c->m_Resource->m_Scene->m_Atlas);

            if (c->m_StateMachineInstance) {
                c->m_StateMachineInstance->draw(renderer);
            } else if (c->m_AnimationInstance) {
                c->m_AnimationInstance->draw(renderer);
            } else {
                c->m_ArtboardInstance->draw(renderer);
            }

            renderer->restore();
        }

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

    static void CompRiveSetConstantCallback(void* user_data, dmhash_t name_hash, int32_t value_index, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        RiveComponent* component = (RiveComponent*)user_data;
        if (!component->m_RenderConstants)
            component->m_RenderConstants = dmGameSystem::CreateRenderConstants();
        dmGameSystem::SetRenderConstant(component->m_RenderConstants, GetMaterial(component, component->m_Resource), name_hash, value_index, element_index, var);
        component->m_ReHash = 1;
    }

    static int FindAnimationIndex(dmhash_t* entries, uint32_t num_entries, dmhash_t anim_id)
    {
        for (int i = 0; i < num_entries; ++i)
        {
            if (entries[i] == anim_id)
            {
                return i;
            }
        }
        return -1;
    }

    static rive::LinearAnimation* FindAnimation(rive::Artboard* artboard, dmRive::RiveSceneData* data, int* animation_index, dmhash_t anim_id)
    {
        int index = FindAnimationIndex(data->m_LinearAnimations.Begin(), data->m_LinearAnimations.Size(), anim_id);
        if (index == -1) {
            return 0;
        }
        *animation_index = index;
        return artboard->animation(index);
    }

    static rive::StateMachine* FindStateMachine(rive::Artboard* artboard, dmRive::RiveSceneData* data, int* state_machine_index, dmhash_t anim_id)
    {
        int index = FindAnimationIndex(data->m_StateMachines.Begin(), data->m_StateMachines.Size(), anim_id);
        if (index == -1) {
            return 0;
        }
        *state_machine_index = index;
        return artboard->stateMachine(index);
    }

    static bool PlayAnimation(RiveComponent* component, dmRive::RiveSceneData* data, dmhash_t anim_id,
                                dmGameObject::Playback playback_mode, float offset, float playback_rate)
    {
        int animation_index;
        rive::LinearAnimation* animation = FindAnimation(component->m_ArtboardInstance.get(), data, &animation_index, anim_id);

        if (!animation) {
            return false;
        }

        CompRiveAnimationReset(component);

        rive::Loop loop_value = rive::Loop::oneShot;
        int play_direction    = 1;
        float play_time       = animation->startSeconds();
        float offset_value    = animation->durationSeconds() * offset;

        switch(playback_mode)
        {
            case dmGameObject::PLAYBACK_ONCE_FORWARD:
                loop_value     = rive::Loop::oneShot;
                break;
            case dmGameObject::PLAYBACK_ONCE_BACKWARD:
                loop_value     = rive::Loop::oneShot;
                play_direction = -1;
                play_time      = animation->endSeconds();
                offset_value   = -offset_value;
                break;
            case dmGameObject::PLAYBACK_ONCE_PINGPONG:
                loop_value     = rive::Loop::pingPong;
                break;
            case dmGameObject::PLAYBACK_LOOP_FORWARD:
                loop_value     = rive::Loop::loop;
                break;
            case dmGameObject::PLAYBACK_LOOP_BACKWARD:
                loop_value     = rive::Loop::loop;
                play_direction = -1;
                play_time      = animation->endSeconds();
                offset_value   = -offset_value;
                break;
            case dmGameObject::PLAYBACK_LOOP_PINGPONG:
                loop_value     = rive::Loop::pingPong;
                break;
            default:break;
        }

        component->m_AnimationIndex        = animation_index;
        component->m_AnimationPlaybackRate = playback_rate;
        component->m_AnimationPlayback     = playback_mode;
        component->m_StateMachineInstance  = nullptr;
        component->m_AnimationInstance     = component->m_ArtboardInstance->animationAt(animation_index);
        component->m_AnimationInstance->inputCount();

        component->m_AnimationInstance->time(play_time + offset_value);
        component->m_AnimationInstance->loopValue((int)loop_value);
        component->m_AnimationInstance->direction(play_direction);
        return true;
    }

    static bool PlayStateMachine(RiveComponent* component, dmRive::RiveSceneData* data, dmhash_t anim_id, float playback_rate)
    {
        int state_machine_index;
        rive::StateMachine* state_machine = FindStateMachine(component->m_ArtboardInstance.get(), data, &state_machine_index, anim_id);

        if (!state_machine) {
            return false;
        }

        CompRiveAnimationReset(component);

        component->m_AnimationInstance      = nullptr;
        component->m_StateMachineInstance   = component->m_ArtboardInstance->stateMachineAt(state_machine_index);

        component->m_AnimationPlaybackRate = playback_rate;

        // update the list of current state machine inputs
        uint32_t count = component->m_StateMachineInstance->inputCount();
        if (count > component->m_StateMachineInputs.Capacity())
        {
            component->m_StateMachineInputs.SetCapacity(count);
        }
        component->m_StateMachineInputs.SetSize(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            const rive::SMIInput* input = component->m_StateMachineInstance->input(i);
            component->m_StateMachineInputs[i] = dmHashString64(input->name().c_str());
        }
        return true;
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

                dmhash_t anim_id = ddf->m_AnimationId;

                if (ddf->m_IsStateMachine)
                {
                    bool result = PlayStateMachine(component, data, anim_id, ddf->m_PlaybackRate);
                    if (result) {
                        //component->m_AnimationCallbackRef  = params.m_Message->m_UserData2;
                        //component->m_Listener              = params.m_Message->m_Sender;
                    } else {
                        dmLogError("Couldn't play state machine named '%s'", dmHashReverseSafe64(anim_id));
                    }

                } else {
                    bool result = PlayAnimation(component, data, anim_id, (dmGameObject::Playback)ddf->m_Playback, ddf->m_Offset, ddf->m_PlaybackRate);
                    if (result) {
                        component->m_AnimationCallbackRef  = params.m_Message->m_UserData2;
                        component->m_Listener              = params.m_Message->m_Sender;
                    } else {
                        dmLogError("Couldn't play animation named '%s'", dmHashReverseSafe64(anim_id));
                    }
                }
            }
            else if (params.m_Message->m_Id == dmRiveDDF::RiveCancelAnimation::m_DDFDescriptor->m_NameHash)
            {
                CompRiveAnimationReset(component);
            }
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

    static int FindStateMachineInputIndex(RiveComponent* component, dmhash_t property_name)
    {
        uint32_t count = component->m_StateMachineInputs.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            if (component->m_StateMachineInputs[i] == property_name)
            {
                return (int)i;
            }
        }
        return -1;
    }

    static dmGameObject::PropertyResult SetStateMachineInput(RiveComponent* component, int index, const dmGameObject::ComponentSetPropertyParams& params)
    {
        const rive::StateMachine* state_machine = component->m_StateMachineInstance->stateMachine();
        const rive::StateMachineInput* input = state_machine->input(index);
        rive::SMIInput* input_instance = component->m_StateMachineInstance->input(index);

        if (input->is<rive::StateMachineTrigger>())
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_BOOLEAN)
            {
                dmLogError("Found property %s of type trigger, but didn't receive a boolean", input->name().c_str());
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }

            // The trigger can only respond to the value "true"
            if (!params.m_Value.m_Bool)
            {
                dmLogError("Found property %s of type trigger, but didn't receive a boolean of true", input->name().c_str());
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }

            rive::SMITrigger* trigger = (rive::SMITrigger*)input_instance;
            trigger->fire();
        }
        else if (input->is<rive::StateMachineBool>())
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_BOOLEAN)
            {
                dmLogError("Found property %s of type bool, but didn't receive a boolean", input->name().c_str());
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }

            rive::SMIBool* v = (rive::SMIBool*)input_instance;
            v->value(params.m_Value.m_Bool);
        }
        else if (input->is<rive::StateMachineNumber>())
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                dmLogError("Found property %s of type number, but didn't receive a number", input->name().c_str());
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }

            rive::SMINumber* v = (rive::SMINumber*)input_instance;
            v->value(params.m_Value.m_Number);
        }

        return dmGameObject::PROPERTY_RESULT_OK;
    }

    static dmGameObject::PropertyResult GetStateMachineInput(RiveComponent* component, int index,
            const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        const rive::StateMachine* state_machine = component->m_StateMachineInstance->stateMachine();
        const rive::StateMachineInput* input = state_machine->input(index);
        rive::SMIInput* input_instance = component->m_StateMachineInstance->input(index);

        if (input->is<rive::StateMachineTrigger>())
        {
            dmLogError("Cannot get value of input type trigger ( %s )", input->name().c_str());
            return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        }
        else if (input->is<rive::StateMachineBool>())
        {
            rive::SMIBool* v = (rive::SMIBool*)input_instance;
            out_value.m_Variant = dmGameObject::PropertyVar(v->value());
        }
        else if (input->is<rive::StateMachineNumber>())
        {
            rive::SMINumber* v = (rive::SMINumber*)input_instance;
            out_value.m_Variant = dmGameObject::PropertyVar(v->value());
        }

        return dmGameObject::PROPERTY_RESULT_OK;
    }

    dmGameObject::PropertyResult CompRiveGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        CompRiveContext* context = (CompRiveContext*)params.m_Context;
        RiveWorld* world = (RiveWorld*)params.m_World;
        RiveComponent* component = GetComponentFromIndex(world, *params.m_UserData);
        dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;

        if (params.m_PropertyId == PROP_ANIMATION)
        {
            if (component->m_AnimationInstance && component->m_AnimationIndex < data->m_LinearAnimations.Size())
            {
                out_value.m_Variant = dmGameObject::PropertyVar(data->m_LinearAnimations[component->m_AnimationIndex]);
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_CURSOR)
        {
            if (component->m_AnimationInstance)
            {
                const rive::LinearAnimation* animation = component->m_AnimationInstance->animation();
                float cursor_value                     = (component->m_AnimationInstance->time() - animation->startSeconds()) / animation->durationSeconds();
                out_value.m_Variant                    = dmGameObject::PropertyVar(cursor_value);
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_PLAYBACK_RATE)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_AnimationPlaybackRate);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_MATERIAL)
        {
            dmRender::HMaterial material = GetMaterial(component, component->m_Resource);
            return dmGameSystem::GetResourceProperty(context->m_Factory, material, out_value);
        } else {
            if (component->m_StateMachineInstance)
            {
                int index = FindStateMachineInputIndex(component, params.m_PropertyId);
                if (index >= 0)
                {
                    return GetStateMachineInput(component, index, params, out_value);
                }
            }
        }
        return dmGameSystem::GetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, params.m_Options.m_Index, out_value, false, CompRiveGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompRiveSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;
        RiveComponent* component = world->m_Components.Get(*params.m_UserData);
        if (params.m_PropertyId == PROP_CURSOR)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

            if (component->m_AnimationInstance)
            {
                const rive::LinearAnimation* animation = component->m_AnimationInstance->animation();
                float cursor = params.m_Value.m_Number * animation->durationSeconds() + animation->startSeconds();
                component->m_AnimationInstance->time(cursor);
            }

            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_PLAYBACK_RATE)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

            component->m_AnimationPlaybackRate = params.m_Value.m_Number;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_MATERIAL)
        {
            CompRiveContext* context = (CompRiveContext*)params.m_Context;
            dmGameObject::PropertyResult res = dmGameSystem::SetResourceProperty(context->m_Factory, params.m_Value, MATERIAL_EXT_HASH, (void**)&component->m_Material);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
            return res;
        } else {
            if (component->m_StateMachineInstance)
            {
                int index = FindStateMachineInputIndex(component, params.m_PropertyId);
                if (index >= 0)
                {
                    return SetStateMachineInput(component, index, params);
                }
            }
        }
        return dmGameSystem::SetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, params.m_Value, params.m_Options.m_Index, CompRiveSetConstantCallback, component);
    }

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params)
    {
        RiveWorld* world = (RiveWorld*) params.m_UserData;
        dmArray<RiveComponent*>& components = world->m_Components.GetRawObjects();
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

    static dmGameObject::Result ComponentTypeCreate(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        CompRiveContext* rivectx    = new CompRiveContext;
        rivectx->m_Factory          = ctx->m_Factory;
        rivectx->m_GraphicsContext  = *(dmGraphics::HContext*)ctx->m_Contexts.Get(dmHashString64("graphics"));
        rivectx->m_RenderContext    = *(dmRender::HRenderContext*)ctx->m_Contexts.Get(dmHashString64("render"));
        rivectx->m_MaxInstanceCount = dmConfigFile::GetInt(ctx->m_Config, "rive.max_instance_count", 128);
        //rivectx->m_RiveFactory      = new dmRive::DefoldFactory();
        //rivectx->m_RiveRenderer     = new dmRive::DefoldTessRenderer();

        // rive::g_Ctx = rivectx->m_RiveContext;
        // rive::setRenderMode(rivectx->m_RiveContext, rive::MODE_STENCIL_TO_COVER);
        // rive::setBufferCallbacks(rivectx->m_RiveContext, dmRive::RequestBufferCallback, dmRive::DestroyBufferCallback, 0x0);

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

    static dmGameObject::Result ComponentTypeDestroy(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        CompRiveContext* rivectx = (CompRiveContext*)ComponentTypeGetContext(type);
        //delete rivectx->m_RiveFactory;
        delete rivectx;
        return dmGameObject::RESULT_OK;
    }

    static void DeleteBones(RiveComponent* component)
    {
        dmGameObject::HInstance rive_instance = component->m_Instance;
        dmGameObject::HCollection collection = dmGameObject::GetCollection(rive_instance);

        uint32_t num_bones = component->m_BoneGOs.Size();
        for (uint32_t i = 0; i < num_bones; ++i)
        {
            dmGameObject::HInstance bone_instance = component->m_BoneGOs[i];
            if (bone_instance)
            {
                dmGameObject::Delete(collection, bone_instance, false);
            }
        }
        component->m_BoneGOs.SetSize(0);
    }

    static void UpdateBones(RiveComponent* component)
    {
        rive::Artboard* artboard = component->m_ArtboardInstance.get();

        rive::AABB bounds = artboard->bounds();
        float cx = (bounds.maxX - bounds.minX) * 0.5f;
        float cy = (bounds.maxY - bounds.minY) * 0.5f;

        uint32_t num_bones = component->m_BoneGOs.Size();
        DM_PROPERTY_ADD_U32(rmtp_RiveBones, num_bones);

        dmVMath::Point3 go_pos = dmGameObject::GetPosition(component->m_Instance);

        for (uint32_t i = 0; i < num_bones; ++i)
        {
            dmGameObject::HInstance bone_instance = component->m_BoneGOs[i];
            rive::Bone* bone = component->m_Bones[i];

            const rive::Mat2D& rt = bone->worldTransform();

            dmVMath::Vector4 x_axis(rt.xx(), rt.xy(), 0, 0);
            dmVMath::Vector4 y_axis(rt.yx(), rt.yy(), 0, 0);

            float scale_x = length(x_axis);
            float scale_y = length(y_axis);
            dmVMath::Vector3 scale(scale_x, scale_y, 1);

            float angle = atan2f(x_axis.getY(), x_axis.getX());
            Quat rotation = Quat::rotationZ(-angle);

            // Since the Rive space is different, we need to flip the y axis
            dmVMath::Vector3 pos(rt.tx() - cx, -rt.ty() + cy, 0);

            dmVMath::Matrix4 world_transform(rotation, pos);

            dmTransform::Transform transform = dmTransform::ToTransform(world_transform);

            dmGameObject::SetPosition(bone_instance, Point3(transform.GetTranslation()));
            dmGameObject::SetRotation(bone_instance, transform.GetRotation());
            dmGameObject::SetScale(bone_instance, transform.GetScale());
        }
    }

    static bool CreateBones(RiveWorld* world, RiveComponent* component)
    {
        dmGameObject::HInstance rive_instance = component->m_Instance;
        dmGameObject::HCollection collection = dmGameObject::GetCollection(rive_instance);

        uint32_t num_bones = component->m_Bones.Size();

        component->m_BoneGOs.SetCapacity(num_bones);
        component->m_BoneGOs.SetSize(num_bones);

        for (uint32_t i = 0; i < num_bones; ++i)
        {
            dmGameObject::HInstance bone_instance = dmGameObject::New(collection, 0x0);
            if (bone_instance == 0x0) {
                DeleteBones(component);
                return false;
            }

            component->m_BoneGOs[i] = bone_instance;

            uint32_t index = dmGameObject::AcquireInstanceIndex(collection);
            if (index == dmGameObject::INVALID_INSTANCE_POOL_INDEX)
            {
                DeleteBones(component);
                return false;
            }

            dmhash_t id = dmGameObject::ConstructInstanceId(index);
            dmGameObject::AssignInstanceIndex(index, bone_instance);

            dmGameObject::Result result = dmGameObject::SetIdentifier(collection, bone_instance, id);
            if (dmGameObject::RESULT_OK != result)
            {
                DeleteBones(component);
                return false;
            }

            dmGameObject::SetBone(bone_instance, true);

            // Since we're given the "world" coordinates from the rive bones,
            // we don't really need a full hierarchy. So we use the actual game object as parent
            dmGameObject::SetParent(bone_instance, rive_instance);
        }

        return true;
    }

    // ******************************************************************************
    // SCRIPTING HELPER FUNCTIONS
    // ******************************************************************************

    bool CompRiveGetBoneID(RiveComponent* component, dmhash_t bone_name, dmhash_t* id)
    {
        uint32_t num_bones = component->m_Bones.Size();

        // We need the arrays to be matching 1:1 (for lookup using the same indices)
        if (num_bones == 0 || component->m_BoneGOs.Size() != num_bones) {
            return false;
        }

        for (uint32_t i = 0; i < num_bones; ++i)
        {
            rive::Bone* bone = component->m_Bones[i];

            // Getting bones by name is rare, so I think it is ok to do the hash now, as opposed to have an array in each of the components.
            dmhash_t name_hash = dmHashString64(bone->name().c_str());
            if (bone_name == name_hash)
            {
                dmGameObject::HInstance bone_instance = component->m_BoneGOs[i];
                *id = dmGameObject::GetIdentifier(bone_instance);
                return true;
            }
        }

        return false;
    }
}

DM_DECLARE_COMPONENT_TYPE(ComponentTypeRive, "rivemodelc", dmRive::ComponentTypeCreate, dmRive::ComponentTypeDestroy);

#endif
