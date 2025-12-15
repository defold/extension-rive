// Copyright 2020-2025 The Defold Foundation
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

// Rive extension
#include "comp_rive.h"
#include "comp_rive_private.h"
#include "res_rive_data.h"
#include "res_rive_scene.h"
#include "res_rive_model.h"

#include <common/bones.h>
#include <common/commands.h>
#include <common/vertices.h>
#include <common/factory.h>

// Defold Rive Renderer
#include <defold/rive.h>
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

    static const dmhash_t PROP_ARTBOARD         = dmHashString64("artboard");
    static const dmhash_t PROP_STATE_MACHINE    = dmHashString64("state_machine");
    static const dmhash_t PROP_PLAYBACK_RATE    = dmHashString64("playback_rate");
    static const dmhash_t PROP_MATERIAL         = dmHashString64("material");
    static const dmhash_t MATERIAL_EXT_HASH     = dmHashString64("materialc");
    static const dmhash_t PROP_RIVE_FILE        = dmHashString64("rive_file");

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
        HRenderContext           m_RiveRenderContext;
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

    class SimpleArtboardListener : public rive::CommandQueue::ArtboardListener
    {
    public:
        virtual void onStateMachinesListed(const rive::ArtboardHandle fileHandle, uint64_t requestId,
            std::vector<std::string> stateMachineNames) override
        {
            // we can guarantee that this is the only listener that will receive
            // this callback, so it's fine to move the params
            //m_stateMachineNames = std::move(stateMachineNames);
        }

        RiveComponent* m_Component;

        //std::vector<std::string> m_stateMachineNames;
    };

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
        world->m_RiveRenderContext = context->m_RiveRenderContext;

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
        //dmRiveDDF::RiveModelDesc* ddf = resource->m_DDF;
        dmRender::HMaterial material = GetMaterial(component, resource);
        dmGameSystem::TextureSetResource* texture_set = resource->m_Scene->m_TextureSet;
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &material, sizeof(material));
        //dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode)); // not currently used
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

    rive::FileHandle CompRiveGetFile(RiveComponent* component)
    {
        RiveSceneData* resource = GetRiveResource(component, component->m_Resource);
        return resource->m_File;
    }

    rive::ArtboardHandle CompRiveGetArtboard(RiveComponent* component)
    {
        return component->m_Artboard;
    }

    rive::ArtboardHandle CompRiveSetArtboard(RiveComponent* component, const char* artboard_name)
    {
        dmRive::RiveSceneData* data = (dmRive::RiveSceneData*) component->m_Resource->m_Scene->m_Scene;
        rive::FileHandle file = data->m_File;
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        rive::ArtboardHandle old_handle = component->m_Artboard;

        if (artboard_name && artboard_name[0] != '\0')
        {
            component->m_Artboard = queue->instantiateArtboardNamed(file, artboard_name);
            if (!component->m_Artboard)
            {
                dmLogWarning("Could not find artboard with name '%s'", artboard_name);
            }
            printf("Created artboard by name '%s'\n", artboard_name);
        }

        if (!component->m_Artboard)
        {
            component->m_Artboard = queue->instantiateDefaultArtboard(file);
            printf("Created default artboard\n");
        }

        return old_handle;
    }

    rive::StateMachineHandle CompRiveGetStateMachine(RiveComponent* component)
    {
        return component->m_StateMachine;
    }

    static void BindViewModelInstance(RiveComponent* component)
    {
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        queue->bindViewModelInstance(component->m_StateMachine, component->m_ViewModelInstance);
    }

    rive::StateMachineHandle CompRiveSetStateMachine(RiveComponent* component, const char* state_machine_name)
    {
        rive::ArtboardHandle artboard = component->m_Artboard;
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        rive::StateMachineHandle old_handle = component->m_StateMachine;

        if (state_machine_name && state_machine_name[0] != '\0')
        {
            component->m_StateMachine = queue->instantiateStateMachineNamed(artboard, state_machine_name);
            if (!component->m_StateMachine)
            {
                dmLogWarning("Could not find state_machine with name '%s'", state_machine_name);
            }
            printf("Created state machine by name '%s'\n", state_machine_name);
        }

        if (!component->m_StateMachine)
        {
            component->m_StateMachine = queue->instantiateDefaultStateMachine(artboard);
            printf("Created default state machine: %p\n", component->m_StateMachine);
        }

        component->m_Enabled = component->m_StateMachine != 0;

        if (component->m_StateMachine && component->m_Resource->m_DDF->m_AutoBind)
        {
            CompRiveSetViewModelInstance(component, 0);
            BindViewModelInstance(component);
        }

        return old_handle;
    }

    rive::ViewModelInstanceHandle CompRiveSetViewModelInstance(RiveComponent* component, const char* viewmodel_name)
    {
        rive::FileHandle file = CompRiveGetFile(component);
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        rive::ViewModelInstanceHandle old_handle = component->m_ViewModelInstance;

        if (viewmodel_name && viewmodel_name[0] != '\0')
        {
            component->m_ViewModelInstance = queue->instantiateDefaultViewModelInstance(file, viewmodel_name);
            if (!component->m_ViewModelInstance)
            {
                dmLogWarning("Could not find view model instance with name '%s'", viewmodel_name);
            }
            printf("Created view model instance by name '%s'\n", viewmodel_name);
        }

        if (!component->m_ViewModelInstance)
        {
            component->m_ViewModelInstance = queue->instantiateDefaultViewModelInstance(file, component->m_Artboard);
            printf("Created default view model instance: %p\n", component->m_ViewModelInstance);
        }

        component->m_Enabled = component->m_ViewModelInstance != 0;
        return old_handle;
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

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_World = Matrix4::identity();
        component->m_DoRender = 0;
        component->m_RenderConstants = 0;
        component->m_CurrentViewModelInstanceRuntime = INVALID_HANDLE;
        component->m_DrawKey = queue->createDrawKey();

        dmRiveDDF::RiveModelDesc* ddf = component->m_Resource->m_DDF;

        component->m_Fullscreen == ddf->m_CoordinateSystem == dmRiveDDF::RiveModelDesc::COORDINATE_SYSTEM_FULLSCREEN;
        component->m_Fit = rive::Fit::layout;
        component->m_Alignment = rive::Alignment::center;
        if (!component->m_Fullscreen)
        {
            component->m_Fit = DDFToRiveFit(ddf->m_ArtboardFit);
            component->m_Alignment = DDFToRiveAlignment(ddf->m_ArtboardAlignment);
        }

        CompRiveSetArtboard(component, ddf->m_Artboard);
        CompRiveSetStateMachine(component, ddf->m_DefaultStateMachine);

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
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        RiveComponent* component = GetComponentFromIndex(world, index);
        //dmGameObject::DeleteBones(component->m_Instance);

        if (component->m_RenderConstants)
            dmGameSystem::DestroyRenderConstants(component->m_RenderConstants);

        queue->deleteArtboard(component->m_Artboard);
        component->m_Artboard = 0;

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

    static void RenderBatchEnd(RiveWorld* world, dmRender::HRenderContext render_context)
    {
        if (world->m_RiveRenderContext)
        {
            dmRiveCommands::ProcessMessages(); // Flush any draw() messages
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

    static void RenderBatch(RiveWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        RiveComponent*              first    = (RiveComponent*) buf[*begin].m_UserData;
        dmRive::RiveSceneResource* scene_res = first->m_Resource->m_Scene;
        dmRive::RiveSceneData* data          = (dmRive::RiveSceneData*) scene_res->m_Scene;

        RenderBegin(world->m_RiveRenderContext, world->m_Ctx->m_Factory, g_RenderBeginParams);

        uint32_t width, height;
        GetDimensions(world->m_RiveRenderContext, &width, &height);

        rive::Mat2D viewTransform = GetViewTransform(world->m_RiveRenderContext, render_context);
        rive::Renderer* renderer = GetRiveRenderer(world->m_RiveRenderContext);

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        for (uint32_t *i=begin;i!=end;i++)
        {
            RiveComponent* c = (RiveComponent*) buf[*i].m_UserData;

            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            RiveModelResource* resource = c->m_Resource;
            dmRiveDDF::RiveModelDesc* ddf = resource->m_DDF;

            // From command_buffer_example.cpp

            const rive::ArtboardHandle  artboardHandle  = c->m_Artboard;
            rive::Fit                   fit             = c->m_Fit;
            rive::Alignment             alignment       = c->m_Alignment;
            bool fullscreen                             = c->m_Fullscreen;

            auto drawLoop = [artboardHandle,
                             renderer,
                             fullscreen,
                             fit,
                             alignment,
                             width,
                             height](rive::DrawKey drawKey, rive::CommandServer* server)
            {
                rive::ArtboardInstance* artboard = server->getArtboardInstance(artboardHandle);
                if (artboard == nullptr)
                {
                    return;
                }

                rive::Factory* factory = server->factory();

                if (fullscreen)
                {
                    artboard->width(width);
                    artboard->height(height);
                }
                else if (fit == rive::Fit::layout)
                {
                    artboard->width(width);
                    artboard->height(height);
                }

                // Draw the .riv.
                renderer->save();
                renderer->align(fit,
                                alignment,
                                rive::AABB(0, 0, width, height),
                                artboard->bounds());

                artboard->draw(renderer);
                renderer->restore();
            };

            queue->draw(c->m_DrawKey, drawLoop);
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
    }

    dmGameObject::UpdateResult CompRiveUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        DM_PROFILE("RiveModel");
        RiveWorld* world = (RiveWorld*)params.m_World;

        float dt = params.m_UpdateContext->m_DT;

        dmArray<RiveComponent*>& components = world->m_Components.GetRawObjects();
        const uint32_t count = components.Size();
        DM_PROPERTY_ADD_U32(rmtp_RiveComponents, count);

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        for (uint32_t i = 0; i < count; ++i)
        {
            RiveComponent& component = *components[i];
            component.m_DoRender = 0;

            if (!component.m_Enabled || !component.m_AddedToUpdate)
            {
                continue;
            }

            queue->advanceStateMachine(component.m_StateMachine, dt * component.m_AnimationPlaybackRate);

            // if (component.m_Resource->m_CreateGoBones)
            //     UpdateBones(&component); // after the artboard->advance();

            if (component.m_ReHash || (component.m_RenderConstants && dmGameSystem::AreRenderConstantsUpdated(component.m_RenderConstants)))
            {
                ReHash(&component);
            }

            component.m_DoRender = 1;
        }

        dmRiveCommands::ProcessMessages(); // Update the command server

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

        if (params.m_PropertyId == PROP_ARTBOARD)
        {
            out_value.m_Variant = dmGameObject::PropertyVar((dmhash_t)component->m_Artboard);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_STATE_MACHINE)
        {
            out_value.m_Variant = dmGameObject::PropertyVar((dmhash_t)component->m_StateMachine);
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
        return dmGameSystem::GetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, params.m_Options.m_Index, out_value, false, CompRiveGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompRiveSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        RiveWorld* world = (RiveWorld*)params.m_World;
        RiveComponent* component = world->m_Components.Get(*params.m_UserData);
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        if (params.m_PropertyId == PROP_PLAYBACK_RATE)
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
        rivectx->m_RiveRenderContext = dmRiveCommands::GetDefoldRenderContext();
        assert(rivectx->m_RiveRenderContext != 0);

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
            // ComponentTypeSetLateUpdateFn(type, CompRiveLateUpdate);
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

    // static void DeleteBones(RiveComponent* component)
    // {
    //     dmLogOnceError("MAWE Missing implementation; %s", __FUNCTION__);

    //     // dmGameObject::HInstance rive_instance = component->m_Instance;
    //     // dmGameObject::HCollection collection = dmGameObject::GetCollection(rive_instance);

    //     // uint32_t num_bones = component->m_BoneGOs.Size();
    //     // for (uint32_t i = 0; i < num_bones; ++i)
    //     // {
    //     //     dmGameObject::HInstance bone_instance = component->m_BoneGOs[i];
    //     //     if (bone_instance)
    //     //     {
    //     //         dmGameObject::Delete(collection, bone_instance, false);
    //     //     }
    //     // }
    //     // component->m_BoneGOs.SetSize(0);
    // }

    // static void UpdateBones(RiveComponent* component)
    // {
    //     dmLogOnceError("MAWE Missing implementation; %s", __FUNCTION__);
    //     // rive::Artboard* artboard = component->m_Artboard.get();

    //     // rive::AABB bounds = artboard->bounds();
    //     // float cx = (bounds.maxX - bounds.minX) * 0.5f;
    //     // float cy = (bounds.maxY - bounds.minY) * 0.5f;

    //     // uint32_t num_bones = component->m_BoneGOs.Size();
    //     // DM_PROPERTY_ADD_U32(rmtp_RiveBones, num_bones);

    //     // dmVMath::Point3 go_pos = dmGameObject::GetPosition(component->m_Instance);

    //     // for (uint32_t i = 0; i < num_bones; ++i)
    //     // {
    //     //     dmGameObject::HInstance bone_instance = component->m_BoneGOs[i];
    //     //     rive::Bone* bone = component->m_Bones[i];

    //     //     const rive::Mat2D& rt = bone->worldTransform();

    //     //     dmVMath::Vector4 x_axis(rt.xx(), rt.xy(), 0, 0);
    //     //     dmVMath::Vector4 y_axis(rt.yx(), rt.yy(), 0, 0);

    //     //     float scale_x = length(x_axis);
    //     //     float scale_y = length(y_axis);
    //     //     dmVMath::Vector3 scale(scale_x, scale_y, 1);

    //     //     float angle = atan2f(x_axis.getY(), x_axis.getX());
    //     //     Quat rotation = Quat::rotationZ(-angle);

    //     //     // Since the Rive space is different, we need to flip the y axis
    //     //     dmVMath::Vector3 pos(rt.tx() - cx, -rt.ty() + cy, 0);

    //     //     dmVMath::Matrix4 world_transform(rotation, pos);

    //     //     dmTransform::Transform transform = dmTransform::ToTransform(world_transform);

    //     //     dmGameObject::SetPosition(bone_instance, Point3(transform.GetTranslation()));
    //     //     dmGameObject::SetRotation(bone_instance, transform.GetRotation());
    //     //     dmGameObject::SetScale(bone_instance, transform.GetScale());
    //     // }
    // }

    // static bool CreateBones(RiveWorld* world, RiveComponent* component)
    // {
    //     dmLogOnceError("MAWE Missing implementation; %s", __FUNCTION__);

    //     // dmGameObject::HInstance rive_instance = component->m_Instance;
    //     // dmGameObject::HCollection collection = dmGameObject::GetCollection(rive_instance);

    //     // uint32_t num_bones = component->m_Bones.Size();

    //     // component->m_BoneGOs.SetCapacity(num_bones);
    //     // component->m_BoneGOs.SetSize(num_bones);

    //     // for (uint32_t i = 0; i < num_bones; ++i)
    //     // {
    //     //     dmGameObject::HInstance bone_instance = dmGameObject::New(collection, 0x0);
    //     //     if (bone_instance == 0x0) {
    //     //         DeleteBones(component);
    //     //         return false;
    //     //     }

    //     //     component->m_BoneGOs[i] = bone_instance;

    //     //     uint32_t index = dmGameObject::AcquireInstanceIndex(collection);
    //     //     if (index == dmGameObject::INVALID_INSTANCE_POOL_INDEX)
    //     //     {
    //     //         DeleteBones(component);
    //     //         return false;
    //     //     }

    //     //     dmhash_t id = dmGameObject::CreateInstanceId();
    //     //     dmGameObject::AssignInstanceIndex(index, bone_instance);

    //     //     dmGameObject::Result result = dmGameObject::SetIdentifier(collection, bone_instance, id);
    //     //     if (dmGameObject::RESULT_OK != result)
    //     //     {
    //     //         DeleteBones(component);
    //     //         return false;
    //     //     }

    //     //     dmGameObject::SetBone(bone_instance, true);

    //     //     // Since we're given the "world" coordinates from the rive bones,
    //     //     // we don't really need a full hierarchy. So we use the actual game object as parent
    //     //     dmGameObject::SetParent(bone_instance, rive_instance);
    //     // }

    //     return true;
    // }

    // ******************************************************************************
    // SCRIPTING HELPER FUNCTIONS
    // ******************************************************************************

    // bool CompRiveGetBoneID(RiveComponent* component, dmhash_t bone_name, dmhash_t* id)
    // {
    //     uint32_t num_bones = component->m_Bones.Size();

    //     // We need the arrays to be matching 1:1 (for lookup using the same indices)
    //     if (num_bones == 0 || component->m_BoneGOs.Size() != num_bones) {
    //         return false;
    //     }

    //     for (uint32_t i = 0; i < num_bones; ++i)
    //     {
    //         rive::Bone* bone = component->m_Bones[i];

    //         // Getting bones by name is rare, so I think it is ok to do the hash now, as opposed to have an array in each of the components.
    //         dmhash_t name_hash = dmHashString64(bone->name().c_str());
    //         if (bone_name == name_hash)
    //         {
    //             dmGameObject::HInstance bone_instance = component->m_BoneGOs[i];
    //             *id = dmGameObject::GetIdentifier(bone_instance);
    //             return true;
    //         }
    //     }

    //     return false;
    // }

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

        //rive::Vec2D p_local = component->m_InverseRendererTransform * rive::Vec2D(normalized_x * window_width, normalized_y * window_height);
        rive::Vec2D p_local = rive::Vec2D(normalized_x * window_width, normalized_y * window_height);
        //printf("    WorldToLocal: %f, %f -> %f %f\n", x, y, p_local.x, p_local.y);
        return p_local;
    }

    static void FillPointerEvent(RiveComponent* component, rive::CommandQueue::PointerEvent& event, rive::Vec2D& pos)
    {
        dmGraphics::HContext graphics_context = dmGraphics::GetInstalledContext();
        float window_width = (float) dmGraphics::GetWindowWidth(graphics_context);
        float window_height = (float) dmGraphics::GetWindowHeight(graphics_context);

        event.fit = component->m_Fit;
        event.alignment = component->m_Alignment;
        event.screenBounds.x = window_width;
        event.screenBounds.y = window_height;
        event.position = pos;
        event.scaleFactor = CompRiveGetDisplayScaleFactor();

        //printf("    FillEvent: w/h: %f, %f  fit: %d  align: %f,%f\n", window_width, window_height, (int)event.fit, event.alignment.x(), event.alignment.y());
    }

    void CompRivePointerAction(RiveComponent* component, dmRive::PointerAction action, float x, float y)
    {

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        rive::Vec2D p = WorldToLocal(component, x, y);
        rive::CommandQueue::PointerEvent event;
        FillPointerEvent(component, event, p);

        switch(action)
        {
        case dmRive::PointerAction::POINTER_MOVE:   queue->pointerMove(component->m_StateMachine, event); break;
        case dmRive::PointerAction::POINTER_UP:     queue->pointerUp(component->m_StateMachine, event); break;
        case dmRive::PointerAction::POINTER_DOWN:   queue->pointerDown(component->m_StateMachine, event); break;
        case dmRive::PointerAction::POINTER_EXIT:   queue->pointerExit(component->m_StateMachine, event); break;
        }
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
