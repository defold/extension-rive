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

#include "script_rive_listeners.h"
#include "viewmodel_instance_registry.h"

#include <common/file.h>
#include <common/commands.h>
#include <common/rive_math.h>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/command_server.hpp>

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
    static RenderBeginParams g_RenderBeginParams;

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params);
    static void DestroyComponent(struct RiveWorld* world, uint32_t index);
    static void CompRiveAnimationReset(RiveComponent* component);

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

    static rive::Mat2D GetViewTransform(dmRender::HRenderContext render_context)
    {
        const dmVMath::Matrix4& view_matrix = dmRender::GetViewMatrix(render_context);
        rive::Mat2D view_transform;
        Mat4ToMat2D(view_matrix, view_transform);
        return view_transform;
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
        return component->m_Material ? component->m_Material : resource->m_BlitMaterial;
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
        }

        if (!component->m_Artboard)
        {
            component->m_Artboard = queue->instantiateDefaultArtboard(file);
        }

        return old_handle;
    }

    rive::StateMachineHandle CompRiveGetStateMachine(RiveComponent* component)
    {
        return component->m_StateMachine;
    }

    rive::ViewModelInstanceHandle CompRiveGetViewModelInstance(RiveComponent* component)
    {
        return component->m_ViewModelInstance;
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
        }

        if (!component->m_StateMachine)
        {
            component->m_StateMachine = queue->instantiateDefaultStateMachine(artboard);
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
        rive::FileHandle              file = CompRiveGetFile(component);
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        rive::ViewModelInstanceHandle old_handle = component->m_ViewModelInstance;

        if (viewmodel_name && viewmodel_name[0] != '\0')
        {
            ViewModelInstanceListener* listener = new ViewModelInstanceListener();
            listener->SetAutoDeleteOnViewModelDeleted(true);
            component->m_ViewModelInstance = queue->instantiateDefaultViewModelInstance(file, viewmodel_name, listener);
            if (!component->m_ViewModelInstance)
            {
                dmLogWarning("Could not find view model instance with name '%s'", viewmodel_name);
                delete listener;
            }
            else
            {
                RegisterViewModelInstanceListener(component->m_ViewModelInstance, listener);
                RequestViewModelInstanceProperties(file, component->m_ViewModelInstance, viewmodel_name);
            }
        }

        if (!component->m_ViewModelInstance)
        {
            ViewModelInstanceListener* listener = new ViewModelInstanceListener();
            listener->SetAutoDeleteOnViewModelDeleted(true);
            component->m_ViewModelInstance = queue->instantiateDefaultViewModelInstance(file, component->m_Artboard, listener);
            if (!component->m_ViewModelInstance)
            {
                delete listener;
            }
            else
            {
                RegisterViewModelInstanceListener(component->m_ViewModelInstance, listener);
                RequestDefaultViewModelInstanceProperties(file, component->m_Artboard, component->m_ViewModelInstance);
            }
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

        component->m_CoordGame = ddf->m_CoordinateSystem == dmRiveDDF::RiveModelDesc::COORDINATE_SYSTEM_GAME;
        component->m_Fit = rive::Fit::layout;
        component->m_Alignment = rive::Alignment::center;
        if (!component->m_CoordGame)
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

    static void DestroyComponent(RiveWorld* world, uint32_t index)
    {
        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        RiveComponent* component = GetComponentFromIndex(world, index);

        if (component->m_RenderConstants)
            dmGameSystem::DestroyRenderConstants(component->m_RenderConstants);

        if (component->m_ViewModelInstance)
            queue->deleteViewModelInstance(component->m_ViewModelInstance);
        component->m_ViewModelInstance = 0;

        if (component->m_StateMachine)
            queue->deleteStateMachine(component->m_StateMachine);
        component->m_StateMachine = 0;

        if (component->m_Artboard)
        {
            dmRiveCommands::DisposeArtboardScripts(component->m_Artboard);
            queue->deleteArtboard(component->m_Artboard);
        }
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
        DestroyComponent(world, index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void RenderBatchEnd(RiveWorld* world, dmRender::HRenderContext render_context)
    {
        if (world->m_RiveRenderContext)
        {
            // Current-frame draw callbacks are executed by the command server
            // after it drains commands, so this fence must stay on the render path.
            dmRiveCommands::ProcessMessages();
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

        uint32_t window_height = dmGraphics::GetWindowHeight(world->m_Ctx->m_GraphicsContext);

        rive::Mat2D view_transform = GetViewTransform(render_context);
        rive::Renderer* renderer = GetRiveRenderer(world->m_RiveRenderContext);

        float display_factor  = g_DisplayFactor;

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();

        for (uint32_t *i=begin;i!=end;i++)
        {
            RiveComponent* c = (RiveComponent*) buf[*i].m_UserData;

            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            const rive::ArtboardHandle  artboardHandle  = c->m_Artboard;
            rive::Fit                   fit             = c->m_Fit;
            rive::Alignment             alignment       = c->m_Alignment;
            bool                        coord_game      = c->m_CoordGame;

            rive::Mat2D world_transform;
            if (coord_game)
            {
                Mat4ToMat2D(c->m_World, world_transform);
            }

            auto drawLoop = [artboardHandle,
                             renderer,
                             fit,
                             alignment,
                             coord_game,
                             world_transform,
                             view_transform,
                             width,
                             height,
                             window_height,
                             display_factor,
                             c](rive::DrawKey, rive::CommandServer* server)
            {
                rive::ArtboardInstance* artboard = server->getArtboardInstance(artboardHandle);
                if (artboard == nullptr)
                {
                    return;
                }

                rive::Mat2D renderer_transform;
                if (coord_game)
                {
                    renderer_transform = dmRive::CalcTransformGame(artboard, view_transform, world_transform, display_factor, window_height);
                }
                else
                {
                    renderer_transform = dmRive::CalcTransformRive(artboard, fit, alignment, width, height, display_factor);
                }

                if (dmRive::DrawArtboard(artboard, renderer, renderer_transform))
                {
                    c->m_InverseRendererTransform = renderer_transform.invertOrIdentity();
                }
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

            if (component.m_ReHash || (component.m_RenderConstants && dmGameSystem::AreRenderConstantsUpdated(component.m_RenderConstants)))
            {
                ReHash(&component);
            }

            component.m_DoRender = 1;
        }

        dmRiveCommands::ProcessMessages(); // Update the command server

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
                // Drain any queued non-draw work before the render batch starts.
                dmRiveCommands::ProcessMessages();
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

        world->m_BlitMaterial = GetMaterialResource(components[0], components[0]->m_Resource);

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

        int32_t value_index = 0;
        dmGameObject::GetPropertyOptionsIndex(params.m_Options, 0, &value_index);
        return dmGameSystem::GetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, value_index, out_value, false, CompRiveGetConstantCallback, component);
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
        int32_t value_index = 0;
        dmGameObject::GetPropertyOptionsIndex(params.m_Options, 0, &value_index);
        return dmGameSystem::SetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, params.m_Value, value_index, CompRiveSetConstantCallback, component);
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
        float logical_width = (float) dmGraphics::GetWidth(graphics_context);
        float logical_height = (float) dmGraphics::GetHeight(graphics_context);

        if (logical_width <= 0.0f)
        {
            logical_width = window_width;
        }
        if (logical_height <= 0.0f)
        {
            logical_height = window_height;
        }

        // Width/height are in screen coordinates and not window coordinates (i.e backbuffer size)
        float normalized_x = x / logical_width;
        float normalized_y = 1 - (y / logical_height);

        rive::Vec2D p_local = component->m_InverseRendererTransform * rive::Vec2D(normalized_x * window_width, normalized_y * window_height);
        return p_local;
    }

    void CompRivePointerAction(RiveComponent* component, dmRive::PointerAction action, float x, float y)
    {
        if (!component || !component->m_StateMachine)
        {
            return;
        }

        rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
        rive::StateMachineHandle state_machine = component->m_StateMachine;
        rive::Vec2D p_local = WorldToLocal(component, x, y);

        queue->runOnce(
            [action, state_machine, p_local](rive::CommandServer* server)
            {
                rive::StateMachineInstance* instance = server->getStateMachineInstance(state_machine);
                if (!instance)
                {
                    return;
                }

                switch (action)
                {
                    case dmRive::PointerAction::POINTER_MOVE: instance->pointerMove(p_local); break;
                    case dmRive::PointerAction::POINTER_UP:   instance->pointerUp(p_local); break;
                    case dmRive::PointerAction::POINTER_DOWN: instance->pointerDown(p_local); break;
                    case dmRive::PointerAction::POINTER_EXIT: instance->pointerExit(p_local); break;
                }
            });
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
