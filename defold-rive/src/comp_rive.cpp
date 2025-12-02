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


#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/loop.hpp>
#include <rive/animation/state_machine.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/bones/bone.hpp>
#include <rive/custom_property.hpp>
#include <rive/custom_property_boolean.hpp>
#include <rive/custom_property_number.hpp>
#include <rive/custom_property_string.hpp>
#include <rive/file.hpp>
#include <rive/math/mat2d.hpp>
#include <rive/nested_artboard.hpp> // Artboard
#include <rive/renderer.hpp>
#include <rive/text/text.hpp>

// Rive extension
#include "comp_rive.h"
#include "comp_rive_private.h"
#include "res_rive_data.h"
#include "res_rive_scene.h"
#include "res_rive_model.h"

#include <common/bones.h>
#include <common/vertices.h>
#include <common/factory.h>

// Defold Rive Renderer
#include <defold/renderer.h>

// DMSDK
#include <dmsdk/script.h>
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
#include <dmsdk/gamesys/resources/res_material.h>

// Not in dmSDK yet
namespace dmScript
{
    bool GetURL(lua_State* L, dmMessage::URL* out_url);
    void PushURL(lua_State* L, const dmMessage::URL& m);
}

DM_PROPERTY_GROUP(rmtp_Rive, "Rive", 0);
DM_PROPERTY_U32(rmtp_RiveBones, 0, PROFILE_PROPERTY_FRAME_RESET, "# rive bones", &rmtp_Rive);
DM_PROPERTY_U32(rmtp_RiveComponents, 0, PROFILE_PROPERTY_FRAME_RESET, "# rive components", &rmtp_Rive);

namespace dmRive
{
    using namespace dmVMath;

    static const dmhash_t PROP_ANIMATION          = dmHashString64("animation");
    static const dmhash_t PROP_CURSOR             = dmHashString64("cursor");
    static const dmhash_t PROP_PLAYBACK_RATE      = dmHashString64("playback_rate");
    static const dmhash_t PROP_MATERIAL           = dmHashString64("material");
    static const dmhash_t MATERIAL_EXT_HASH       = dmHashString64("materialc");
    static const dmhash_t PROP_RIVE_FILE          = dmHashString64("rive_file");

    static float g_DisplayFactor = 0.0f;
    static float g_OriginalWindowWidth = 0.0f;
    static float g_OriginalWindowHeight = 0.0f;
    static RenderBeginParams g_RenderBeginParams;

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params);
    static void DestroyComponent(struct RiveWorld* world, uint32_t index);
    static void CompRiveAnimationReset(RiveComponent* component);
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
        dmResource::HFactory     m_Factory;
        dmRender::HRenderContext m_RenderContext;
        dmGraphics::HContext     m_GraphicsContext;
        dmGraphics::HTexture     m_NullTexture;
        uint32_t                 m_MaxInstanceCount;
    };

    // One per collection
    struct RiveWorld
    {
        CompRiveContext*                        m_Ctx;
        HRenderContext                          m_RiveRenderContext;
        dmObjectPool<RiveComponent*>            m_Components;
        dmArray<dmRender::RenderObject>         m_RenderObjects;
        dmArray<dmRender::HNamedConstantBuffer> m_RenderConstants; // 1:1 mapping with the render objects
        dmGraphics::HVertexBuffer               m_BlitToBackbufferVertexBuffer;
        dmGraphics::HVertexDeclaration          m_VertexDeclaration;
        dmGameSystem::MaterialResource*         m_BlitMaterial;
        bool                                    m_DidWork;         // did we get any batch workload ?
    };

    dmGameObject::CreateResult CompRiveNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        CompRiveContext* context = (CompRiveContext*)params.m_Context;
        RiveWorld* world         = new RiveWorld();

        world->m_Ctx = context;
        world->m_Components.SetCapacity(context->m_MaxInstanceCount);
        world->m_RenderObjects.SetCapacity(context->m_MaxInstanceCount);
        world->m_RenderConstants.SetCapacity(context->m_MaxInstanceCount);
        world->m_RenderConstants.SetSize(context->m_MaxInstanceCount);
        world->m_BlitMaterial = 0;
        world->m_DidWork = false;

        float bottom = 0.0f;
        float top    = 1.0f;

        // Flip texture coordinates on y axis for OpenGL for the final blit:
        if (dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_OPENGL)
        {
            top    = 0.0f;
            bottom = 1.0f;
        }

        const float vertex_data[] = {
            -1.0f, -1.0f, 0.0f, bottom,  // Bottom-left corner
             1.0f, -1.0f, 1.0f, bottom,  // Bottom-right corner
            -1.0f,  1.0f, 0.0f, top,     // Top-left corner
             1.0f, -1.0f, 1.0f, bottom,  // Bottom-right corner
             1.0f,  1.0f, 1.0f, top,     // Top-right corner
            -1.0f,  1.0f, 0.0f, top      // Top-left corner
        };

        world->m_BlitToBackbufferVertexBuffer = dmGraphics::NewVertexBuffer(context->m_GraphicsContext, sizeof(vertex_data), (void*) vertex_data, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

        dmGraphics::HVertexStreamDeclaration stream_declaration_vertex = dmGraphics::NewVertexStreamDeclaration(context->m_GraphicsContext);
        dmGraphics::AddVertexStream(stream_declaration_vertex, "position",  2, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration_vertex, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);
        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(context->m_GraphicsContext, stream_declaration_vertex);

        memset(world->m_RenderConstants.Begin(), 0, sizeof(dmGameSystem::HComponentRenderConstants)*world->m_RenderConstants.Capacity());

        *params.m_World = world;

        dmResource::RegisterResourceReloadedCallback(context->m_Factory, ResourceReloadedCallback, world);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompRiveDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;

        dmGraphics::DeleteVertexBuffer(world->m_BlitToBackbufferVertexBuffer);
        dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);

        dmResource::UnregisterResourceReloadedCallback(((CompRiveContext*)params.m_Context)->m_Factory, ResourceReloadedCallback, world);

        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    RiveArtboardIdList* FindArtboardIdList(rive::Artboard* artboard, dmRive::RiveSceneData* data)
    {
        dmhash_t artboard_id = dmHashString64(artboard->name().c_str());

        for (int i = 0; i < data->m_ArtboardIdLists.Size(); ++i)
        {
            if (data->m_ArtboardIdLists[i]->m_ArtboardNameHash == artboard_id)
            {
                return data->m_ArtboardIdLists[i];
            }
        }
        return 0x0;
    }

    static inline dmGameSystem::MaterialResource* GetMaterialResource(const RiveComponent* component, const RiveModelResource* resource)
    {
        return component->m_Material ? component->m_Material : resource->m_Material;
    }

    static inline dmRender::HMaterial GetMaterial(const RiveComponent* component, const RiveModelResource* resource)
    {
        dmGameSystem::MaterialResource* m = GetMaterialResource(component, resource);
        return m->m_Material;
    }

    static inline RiveSceneData* GetRiveResource(const RiveComponent* component, const RiveModelResource* resource)
    {
        (void)component;
        return resource->m_Scene->m_Scene;
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
        uint32_t index = (uint32_t)(uintptr_t)params.m_UserData;
        return GetComponentFromIndex(world, index);
    }

    static void InstantiateArtboard(RiveComponent* component)
    {
        dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;

        if (component->m_Resource->m_DDF->m_Artboard && component->m_Resource->m_DDF->m_Artboard[0] != '\0')
        {
            component->m_ArtboardInstance = data->m_File->artboardNamed(component->m_Resource->m_DDF->m_Artboard);

            if (!component->m_ArtboardInstance)
            {
                dmLogWarning("Could not find artboard with name '%s'", component->m_Resource->m_DDF->m_Artboard);
            }
        }

        if (!component->m_ArtboardInstance)
        {
            component->m_ArtboardInstance = data->m_File->artboardDefault();
        }
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
        memset(component, 0, sizeof(RiveComponent)); // yes, this works for dmArray and dmHashTable too

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
        component->m_CurrentViewModelInstanceRuntime = INVALID_HANDLE;
        component->m_HandleCounter = 0;

        InstantiateArtboard(component);

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

        dmRiveDDF::RivePlayAnimation ddf;
        ddf.m_Offset            = 0.0;
        ddf.m_PlaybackRate      = 1.0;

        bool use_default_state_machine = component->m_ArtboardInstance->defaultStateMachineIndex() >= 0;
        if (empty_id != state_machine_id || use_default_state_machine)
        {
            ddf.m_Playback          = dmGameObject::PLAYBACK_NONE;
            ddf.m_AnimationId       = use_default_state_machine ? 0 : state_machine_id;
            ddf.m_IsStateMachine    = true;
            if (!CompRivePlayStateMachine(component, &ddf, 0))
            {
                dmLogError("Couldn't play state machine named '%s'", dmHashReverseSafe64(state_machine_id));
            }
        }
        else
        {
            ddf.m_AnimationId       = anim_id; // May be 0
            ddf.m_Playback          = dmGameObject::PLAYBACK_LOOP_FORWARD;
            ddf.m_IsStateMachine    = false;
            if (!CompRivePlayAnimation(component, &ddf, 0))
            {
                dmLogError("Couldn't play animation named '%s'", dmHashReverseSafe64(anim_id));
            }
        }

        component->m_CurrentViewModelInstanceRuntime = 0xFFFFFFFF;
        if (component->m_Resource->m_DDF->m_AutoBind)
        {
            component->m_CurrentViewModelInstanceRuntime = CompRiveCreateViewModelInstanceRuntime(component, 0);
            if (component->m_CurrentViewModelInstanceRuntime != dmRive::INVALID_HANDLE)
            {
                CompRiveSetViewModelInstanceRuntime(component, component->m_CurrentViewModelInstanceRuntime);
            }
        }
        component->m_ReHash = 1;

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void CompRiveRunCallback(RiveComponent* component, const dmDDF::Descriptor* desc, const char* data, const dmMessage::URL* sender)
    {
        dmScript::LuaCallbackInfo* cbk = component->m_Callback;
        if (!dmScript::IsCallbackValid(cbk))
        {
            dmLogError("Rive callback is invalid.");
            return;
        }

        lua_State* L = dmScript::GetCallbackLuaContext(cbk);
        DM_LUA_STACK_CHECK(L, 0);

        if (!dmScript::SetupCallback(cbk))
        {
            dmLogError("Failed to setup Rive callback");
            return;
        }

        dmScript::PushHash(L, desc->m_NameHash);
        dmScript::PushDDF(L, desc, data, false);
        dmScript::PushURL(L, *sender);
        int ret = dmScript::PCall(L, 4, 0);
        (void)ret;
        dmScript::TeardownCallback(cbk);
    }

    static void CompRiveClearCallback(RiveComponent* component)
    {
        if (component->m_Callback)
        {
            dmScript::DestroyCallback(component->m_Callback);
            component->m_Callback = 0x0;
        }
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
        if (component->m_Callback) {
            CompRiveClearCallback(component);
        }
        DestroyComponent(world, index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    // static void GetBlendFactorsFromBlendMode(dmRiveDDF::RiveModelDesc::BlendMode blend_mode, dmGraphics::BlendFactor* src, dmGraphics::BlendFactor* dst)
    // {
    //     switch (blend_mode)
    //     {
    //         case dmRiveDDF::RiveModelDesc::BLEND_MODE_ALPHA:
    //             *src = dmGraphics::BLEND_FACTOR_ONE;
    //             *dst = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    //         break;

    //         case dmRiveDDF::RiveModelDesc::BLEND_MODE_ADD:
    //             *src = dmGraphics::BLEND_FACTOR_ONE;
    //             *dst = dmGraphics::BLEND_FACTOR_ONE;
    //         break;

    //         case dmRiveDDF::RiveModelDesc::BLEND_MODE_MULT:
    //             *src = dmGraphics::BLEND_FACTOR_DST_COLOR;
    //             *dst = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    //         break;

    //         case dmRiveDDF::RiveModelDesc::BLEND_MODE_SCREEN:
    //             *src = dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    //             *dst = dmGraphics::BLEND_FACTOR_ONE;
    //         break;

    //         default:
    //             dmLogError("Unknown blend mode: %d\n", blend_mode);
    //             assert(0);
    //         break;
    //     }
    // }

    static void RenderBatchEnd(RiveWorld* world, dmRender::HRenderContext render_context)
    {
        if (world->m_RiveRenderContext)
        {
            RenderEnd(world->m_RiveRenderContext);

            if (g_RenderBeginParams.m_DoFinalBlit)
            {
                // Do our own resolve here
                dmRender::RenderObject& ro = *world->m_RenderObjects.End();
                world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);
                ro.Init();
                ro.m_Material          = world->m_BlitMaterial->m_Material;
                ro.m_VertexDeclaration = world->m_VertexDeclaration;
                ro.m_VertexBuffer      = world->m_BlitToBackbufferVertexBuffer;
                ro.m_PrimitiveType     = dmGraphics::PRIMITIVE_TRIANGLES;
                ro.m_VertexStart       = 0;
                ro.m_VertexCount       = 6;
                ro.m_Textures[0]       = GetBackingTexture(world->m_RiveRenderContext);
                dmRender::AddToRender(render_context, &ro);
            }
        }
    }

    static inline rive::Fit DDFToRiveFit(dmRiveDDF::RiveModelDesc::Fit fit)
    {
        switch(fit)
        {
            case dmRiveDDF::RiveModelDesc::FIT_FILL:         return rive::Fit::fill;
            case dmRiveDDF::RiveModelDesc::FIT_CONTAIN:      return rive::Fit::contain;
            case dmRiveDDF::RiveModelDesc::FIT_COVER:        return rive::Fit::cover;
            case dmRiveDDF::RiveModelDesc::FIT_FIT_WIDTH:    return rive::Fit::fitWidth;
            case dmRiveDDF::RiveModelDesc::FIT_FIT_HEIGHT:   return rive::Fit::fitHeight;
            case dmRiveDDF::RiveModelDesc::FIT_NONE:         return rive::Fit::none;
            case dmRiveDDF::RiveModelDesc::FIT_SCALE_DOWN:   return rive::Fit::scaleDown;
            case dmRiveDDF::RiveModelDesc::FIT_LAYOUT:       return rive::Fit::layout;
        }
        return rive::Fit::none;
    }

    static inline rive::Alignment DDFToRiveAlignment(dmRiveDDF::RiveModelDesc::Alignment alignment)
    {
        switch(alignment)
        {
            case dmRiveDDF::RiveModelDesc::ALIGNMENT_TOP_LEFT:      return rive::Alignment::topLeft;
            case dmRiveDDF::RiveModelDesc::ALIGNMENT_TOP_CENTER:    return rive::Alignment::topCenter;
            case dmRiveDDF::RiveModelDesc::ALIGNMENT_TOP_RIGHT:     return rive::Alignment::topRight;
            case dmRiveDDF::RiveModelDesc::ALIGNMENT_CENTER_LEFT:   return rive::Alignment::centerLeft;
            case dmRiveDDF::RiveModelDesc::ALIGNMENT_CENTER:        return rive::Alignment::center;
            case dmRiveDDF::RiveModelDesc::ALIGNMENT_CENTER_RIGHT:  return rive::Alignment::centerRight;
            case dmRiveDDF::RiveModelDesc::ALIGNMENT_BOTTOM_LEFT:   return rive::Alignment::bottomLeft;
            case dmRiveDDF::RiveModelDesc::ALIGNMENT_BOTTOM_CENTER: return rive::Alignment::bottomCenter;
            case dmRiveDDF::RiveModelDesc::ALIGNMENT_BOTTOM_RIGHT:  return rive::Alignment::bottomRight;
        }
        return rive::Alignment::center;
    }

    static void RenderBatch(RiveWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        RiveComponent*              first    = (RiveComponent*) buf[*begin].m_UserData;
        dmRive::RiveSceneResource* scene_res = first->m_Resource->m_Scene;
        dmRive::RiveSceneData* data          = (dmRive::RiveSceneData*) scene_res->m_Scene;
        world->m_RiveRenderContext           = data->m_RiveRenderContext;

        RenderBegin(world->m_RiveRenderContext, world->m_Ctx->m_Factory, g_RenderBeginParams);

        uint32_t width, height;
        GetDimensions(world->m_RiveRenderContext, &width, &height);

        rive::Mat2D viewTransform = GetViewTransform(world->m_RiveRenderContext, render_context);
        rive::Renderer* renderer = GetRiveRenderer(world->m_RiveRenderContext);

        uint32_t window_height = dmGraphics::GetWindowHeight(world->m_Ctx->m_GraphicsContext);

        for (uint32_t *i=begin;i!=end;i++)
        {
            RiveComponent* c = (RiveComponent*) buf[*i].m_UserData;

            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            RiveModelResource* resource = c->m_Resource;
            dmRiveDDF::RiveModelDesc* ddf = resource->m_DDF;

            renderer->save();

            rive::AABB bounds = c->m_ArtboardInstance->bounds();

            if (ddf->m_CoordinateSystem == dmRiveDDF::RiveModelDesc::COORDINATE_SYSTEM_FULLSCREEN)
            {
                // Apply the world matrix from the component to the artboard transform
                rive::Mat2D transform;
                Mat4ToMat2D(c->m_World, transform);

                rive::Mat2D centerAdjustment  = rive::Mat2D::fromTranslate(-bounds.width() / 2.0f, -bounds.height() / 2.0f);
                rive::Mat2D scaleDpi          = rive::Mat2D::fromScale(1,-1);
                rive::Mat2D invertAdjustment  = rive::Mat2D::fromScaleAndTranslation(g_DisplayFactor, -g_DisplayFactor, 0, window_height);
                rive::Mat2D rendererTransform = invertAdjustment * viewTransform * transform * scaleDpi * centerAdjustment;

                renderer->transform(rendererTransform);
                c->m_InverseRendererTransform = rendererTransform.invertOrIdentity();
            }
            else if (ddf->m_CoordinateSystem == dmRiveDDF::RiveModelDesc::COORDINATE_SYSTEM_RIVE)
            {
                rive::Fit rive_fit         = DDFToRiveFit(ddf->m_ArtboardFit);
                rive::Alignment rive_align = DDFToRiveAlignment(ddf->m_ArtboardAlignment);

                if (rive_fit == rive::Fit::layout)
                {
                    c->m_ArtboardInstance->width(width / g_DisplayFactor);
                    c->m_ArtboardInstance->height(height / g_DisplayFactor);
                }

                rive::Mat2D rendererTransform = rive::computeAlignment(rive_fit, rive_align, rive::AABB(0, 0, width, height), bounds, g_DisplayFactor);
                renderer->transform(rendererTransform);
                c->m_InverseRendererTransform = rendererTransform.invertOrIdentity();
            }

            if (c->m_StateMachineInstance) {
                c->m_StateMachineInstance->draw(renderer);
            } else if (c->m_AnimationInstance) {
                c->m_AnimationInstance->draw(renderer);
            } else {
                c->m_ArtboardInstance->draw(renderer);
            }

            renderer->restore();
        }
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
            c->m_World = go_world * local;
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
        component->m_AnimationPlaybackRate = 1.0f;
        component->m_AnimationPlayback     = dmGameObject::PLAYBACK_NONE;

        component->m_AnimationInstance.reset();
        component->m_StateMachineInstance.reset();
        component->m_StateMachineInputs.SetSize(0);
    }

    static void CompRiveAnimationDoneCallback(RiveComponent* component)
    {
        assert(component->m_AnimationInstance);

        dmMessage::URL sender;
        dmMessage::URL receiver  = component->m_Listener;
        if (!GetSender(component, &sender))
        {
            dmLogError("Could not send animation_done to listener because of incomplete component.");
            return;
        }

        dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;

        RiveArtboardIdList* id_list = FindArtboardIdList(component->m_ArtboardInstance.get(), data);

        dmRiveDDF::RiveAnimationDone message;
        message.m_AnimationId = id_list->m_LinearAnimations[component->m_AnimationIndex];
        message.m_Playback    = component->m_AnimationPlayback;

        if (component->m_Callback)
        {
            uint32_t id = component->m_CallbackId;
            CompRiveRunCallback(component, dmRiveDDF::RiveAnimationDone::m_DDFDescriptor, (const char*)&message, &sender);

            // Make sure we're clearing the same callback as we ran above and not a new
            // callback that was started from the original callback
            if (id == component->m_CallbackId)
            {
                CompRiveClearCallback(component);
            }
        }
        else
        {
            dmGameObject::Result result = dmGameObject::PostDDF(&message, &sender, &receiver, 0, true);
            if (result != dmGameObject::RESULT_OK)
            {
                dmLogError("Could not send animation_done to listener: %d", result);
            }
        }
    }

    static bool CompRiveHandleEventTrigger(RiveComponent* component, dmRiveDDF::RiveEventTrigger message)
    {
        dmMessage::URL sender;
        dmMessage::URL receiver = component->m_Listener;
        if (!GetSender(component, &sender))
        {
            dmLogError("Could not send event_trigger to listener because of incomplete component.");
            return false;
        }

        if (component->m_Callback)
        {
            CompRiveRunCallback(component, dmRiveDDF::RiveEventTrigger::m_DDFDescriptor, (const char*)&message, &sender);

            // note that we are not clearing the callback here since multiple events can fire at
            // different times during the playback
        }
        else
        {
            dmGameObject::Result result = dmGameObject::PostDDF(&message, &sender, &receiver, 0, true);
            if (result != dmGameObject::RESULT_OK)
            {
                dmLogError("Could not send event_trigger to listener: %d", result);
                return false;
            }
        }
        return true;
    }

    static void CompRiveEventTriggerCallback(RiveComponent* component, rive::Event* event)
    {
        dmRiveDDF::RiveEventTrigger event_message;
        event_message.m_Name = event->name().c_str();
        event_message.m_Trigger = 0;
        event_message.m_Text = "";
        event_message.m_Number = 0.0f;
        if (!CompRiveHandleEventTrigger(component, event_message))
        {
            return;
        }

        // trigger event once per event property
        for (auto child : event->children())
        {
            if (child->is<rive::CustomProperty>())
            {
                if (!child->name().empty())
                {
                    dmRiveDDF::RiveEventTrigger property_message;
                    property_message.m_Name = child->name().c_str();
                    property_message.m_Trigger = 0;
                    property_message.m_Text = "";
                    property_message.m_Number = 0.0f;
                    switch (child->coreType())
                    {
                        case rive::CustomPropertyBoolean::typeKey:
                        {
                            bool b = child->as<rive::CustomPropertyBoolean>()->propertyValue();
                            property_message.m_Trigger = b;
                            break;
                        }
                        case rive::CustomPropertyString::typeKey:
                        {
                            const char* s = child->as<rive::CustomPropertyString>()->propertyValue().c_str();
                            property_message.m_Text = s;
                            break;
                        }
                        case rive::CustomPropertyNumber::typeKey:
                        {
                            float f = child->as<rive::CustomPropertyNumber>()->propertyValue();
                            property_message.m_Number = f;
                            break;
                        }
                    }
                    if (!CompRiveHandleEventTrigger(component, property_message))
                    {
                        return;
                    }
                }
            }
        }
    }

    dmGameObject::UpdateResult CompRiveUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        DM_PROFILE("RiveModel");
        RiveWorld* world    = (RiveWorld*)params.m_World;

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
            //rive::AABB artboard_bounds  = artboard->bounds();

            if (component.m_StateMachineInstance)
            {
                size_t event_count = component.m_StateMachineInstance->reportedEventCount();
                for (size_t i = 0; i < event_count; i++)
                {
                    rive::EventReport reported_event = component.m_StateMachineInstance->reportedEventAt(i);
                    rive::Event* event = reported_event.event();
                    CompRiveEventTriggerCallback(&component, event);
                }

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
                        CompRiveAnimationDoneCallback(&component);
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
                world->m_RenderObjects.SetSize(0);
                world->m_DidWork = false;
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_BATCH:
            {
                RenderBatch(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
                world->m_DidWork = true;
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_END:
            {
                if (world->m_DidWork)
                {
                    RenderBatchEnd(world, params.m_Context);
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

        dmArray<RiveComponent*>& components = world->m_Components.GetRawObjects();
        const uint32_t count = components.Size();
        if (!count)
        {
            return dmGameObject::UPDATE_RESULT_OK;
        }

        UpdateTransforms(world);

        // Prepare list submit
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, count);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, world);
        dmRender::RenderListEntry* write_ptr   = render_list;

        world->m_BlitMaterial = components[0]->m_Resource->m_BlitMaterial;

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

    int FindAnimationIndex(dmhash_t* entries, uint32_t num_entries, dmhash_t anim_id)
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
        RiveArtboardIdList* id_list = FindArtboardIdList(artboard, data);
        if (!id_list)
        {
            return 0;
        }

        int index = FindAnimationIndex(id_list->m_LinearAnimations.Begin(), id_list->m_LinearAnimations.Size(), anim_id);
        if (index == -1) {
            return 0;
        }
        *animation_index = index;
        return artboard->animation(index);
    }

    bool CompRivePlayAnimation(RiveComponent* component, dmRiveDDF::RivePlayAnimation* ddf, dmScript::LuaCallbackInfo* callback_info)
    {
        dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;
        dmhash_t anim_id = ddf->m_AnimationId;
        dmGameObject::Playback playback_mode = (dmGameObject::Playback)ddf->m_Playback;
        float offset = ddf->m_Offset;
        float playback_rate = ddf->m_PlaybackRate;

        if (playback_mode == dmGameObject::PLAYBACK_NONE)
        {
            return true;
        }

        int animation_index;
        rive::LinearAnimation* animation = 0;
        if (anim_id != 0)
        {
            animation = FindAnimation(component->m_ArtboardInstance.get(), data, &animation_index, anim_id);

            if (!animation) {
                dmLogError("Failed to find animation '%s'", dmHashReverseSafe64(anim_id));
                return false;
            }
        }
        else
        {
            animation_index = 0;
            animation = component->m_ArtboardInstance->animation(animation_index); // Default animation
        }

        CompRiveAnimationReset(component);
        CompRiveClearCallback(component);

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

        component->m_Callback              = callback_info;
        component->m_CallbackId++;
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

    bool CompRivePlayStateMachine(RiveComponent* component, dmRiveDDF::RivePlayAnimation* ddf, dmScript::LuaCallbackInfo* callback_info)
    {
        dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;
        dmhash_t anim_id = ddf->m_AnimationId;
        float playback_rate = ddf->m_PlaybackRate;

        int default_state_machine_index = component->m_ArtboardInstance->defaultStateMachineIndex();
        int state_machine_index = -1;
        if (anim_id != 0)
        {
            rive::StateMachine* state_machine = FindStateMachine(component->m_ArtboardInstance.get(), data, &state_machine_index, anim_id);

            if (!state_machine) {
                dmLogError("Failed to play state machine with id '%s'", dmHashReverseSafe64(anim_id));
                return false;
            }
        }
        else
        {
            state_machine_index = default_state_machine_index;
        }

        CompRiveAnimationReset(component);
        CompRiveClearCallback(component);

        // For now, we need to create a new state machine instance when playing a state machine, because of nested artboards+state machine events
        InstantiateArtboard(component);

        component->m_CallbackId++;
        component->m_Callback              = callback_info;
        component->m_AnimationInstance     = nullptr;
        component->m_AnimationPlaybackRate = playback_rate;

        component->m_StateMachineInstance = component->m_ArtboardInstance->stateMachineAt(state_machine_index);

        if (!component->m_StateMachineInstance.get())
        {
            dmLogError("Failed to start state machine '%s' at index %d", dmHashReverseSafe64(anim_id), state_machine_index);
            return false;
        }

        if (component->m_CurrentViewModelInstanceRuntime != INVALID_HANDLE)
        {
            CompRiveSetViewModelInstanceRuntime(component, component->m_CurrentViewModelInstanceRuntime);
        }

        // update the list of current state machine inputs
        GetStateMachineInputNames(component->m_StateMachineInstance.get(), component->m_StateMachineInputs);

        component->m_StateMachineInstance->advanceAndApply(0);
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
                if (ddf->m_IsStateMachine)
                {
                    bool result = CompRivePlayStateMachine(component, ddf, 0);
                    if (!result)
                    {
                        dmLogError("Couldn't play state machine named '%s'", dmHashReverseSafe64(ddf->m_AnimationId));
                    }
                }
                else
                {
                    bool result = CompRivePlayAnimation(component, ddf, 0);
                    if (!result)
                    {
                        dmLogError("Couldn't play animation named '%s'", dmHashReverseSafe64(ddf->m_AnimationId));
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

    dmGameObject::PropertyResult CompRiveGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        CompRiveContext* context = (CompRiveContext*)params.m_Context;
        RiveWorld* world = (RiveWorld*)params.m_World;
        RiveComponent* component = GetComponentFromIndex(world, *params.m_UserData);
        dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;

        RiveArtboardIdList* id_list = FindArtboardIdList(component->m_ArtboardInstance.get(), data);

        if (params.m_PropertyId == PROP_ANIMATION)
        {
            if (component->m_AnimationInstance && component->m_AnimationIndex < id_list->m_LinearAnimations.Size())
            {
                out_value.m_Variant = dmGameObject::PropertyVar(id_list->m_LinearAnimations[component->m_AnimationIndex]);
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
            dmGameSystem::MaterialResource* resource = GetMaterialResource(component, component->m_Resource);
            return dmGameSystem::GetResourceProperty(context->m_Factory, resource, out_value);
        }
        else if (params.m_PropertyId == PROP_RIVE_FILE)
        {
            RiveSceneData* resource = GetRiveResource(component, component->m_Resource);
            return dmGameSystem::GetResourceProperty(context->m_Factory, resource, out_value);
        }
        else
        {
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

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params)
    {
        RiveWorld* world = (RiveWorld*) params->m_UserData;
        dmArray<RiveComponent*>& components = world->m_Components.GetRawObjects();
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            RiveComponent* component = components[i];
            RiveModelResource* resource = component->m_Resource;
            if (!component->m_Enabled || !resource)
                continue;

            void* current_resource = dmResource::GetResource(params->m_Resource);
            if (resource == current_resource ||
                resource->m_Scene == current_resource ||
                resource->m_Scene->m_Scene == current_resource)
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

        g_RenderBeginParams.m_DoFinalBlit       = dmConfigFile::GetInt(ctx->m_Config, "rive.render_to_texture", 1);
        g_RenderBeginParams.m_BackbufferSamples = dmConfigFile::GetInt(ctx->m_Config, "display.samples", 0);

        if (g_RenderBeginParams.m_DoFinalBlit)
        {
            dmLogInfo("Render to texture enabled");
        }
        else
        {
            dmLogInfo("Render to framebuffer enabled");
        }

        g_OriginalWindowWidth  = dmGraphics::GetWidth(rivectx->m_GraphicsContext);
        g_OriginalWindowHeight = dmGraphics::GetHeight(rivectx->m_GraphicsContext);
        g_DisplayFactor        = dmGraphics::GetDisplayScaleFactor(rivectx->m_GraphicsContext);

        dmLogInfo("Display Factor: %g", g_DisplayFactor);

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

            dmhash_t id = dmGameObject::CreateInstanceId();
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

    float CompRiveGetDisplayScaleFactor()
    {
        return g_DisplayFactor;
    }

    rive::Vec2D WorldToLocal(RiveComponent* component, float x, float y)
    {
        dmGraphics::HContext graphics_context = dmGraphics::GetInstalledContext();

        float window_width = (float) dmGraphics::GetWindowWidth(graphics_context);
        float window_height = (float) dmGraphics::GetWindowHeight(graphics_context);

        // Width/height are in screen coordinates and not window coordinates (i.e backbuffer size)
        float normalized_x = x / g_OriginalWindowWidth;
        float normalized_y = 1 - (y / g_OriginalWindowHeight);

        rive::Vec2D p_local = component->m_InverseRendererTransform * rive::Vec2D(normalized_x * window_width, normalized_y * window_height);
        return p_local;
    }

    static inline rive::TextValueRun* GetTextRun(rive::ArtboardInstance* artboard, const char* name, const char* nested_artboard_path)
    {
        if (nested_artboard_path)
        {
            auto nested = artboard->nestedArtboardAtPath(nested_artboard_path);
            auto nested_instance = nested ? nested->artboardInstance() : 0;

            if (nested_instance)
            {
                return nested_instance->find<rive::TextValueRun>(name);
            }
        }
        else
        {
            return artboard->find<rive::TextValueRun>(name);
        }
        return 0;
    }

    bool CompRiveSetTextRun(RiveComponent* component, const char* name, const char* text_run, const char* nested_artboard_path)
    {
        rive::TextValueRun* text_run_value = GetTextRun(component->m_ArtboardInstance.get(), name, nested_artboard_path);
        if (!text_run_value)
        {
            return false;
        }
        text_run_value->text(text_run);
        return true;
    }

    const char* CompRiveGetTextRun(RiveComponent* component, const char* name, const char* nested_artboard_path)
    {
        rive::TextValueRun* text_run_value = GetTextRun(component->m_ArtboardInstance.get(), name, nested_artboard_path);
        if (!text_run_value)
        {
            return 0;
        }
        return text_run_value->text().c_str();
    }

    void CompRiveDebugSetBlitMode(bool value)
    {
    #if defined (DM_PLATFORM_MACOS) || defined (DM_PLATFORM_IOS)
        dmLogWarning("Changing the blit mode is not supported on this platform.");
    #else
        g_RenderBeginParams.m_DoFinalBlit = value;
    #endif
    }

    RiveSceneData* CompRiveGetRiveSceneData(RiveComponent* component)
    {
        return GetRiveResource(component, component->m_Resource);
    }
}

DM_DECLARE_COMPONENT_TYPE(ComponentTypeRive, "rivemodelc", dmRive::ComponentTypeCreate, dmRive::ComponentTypeDestroy);

#endif
