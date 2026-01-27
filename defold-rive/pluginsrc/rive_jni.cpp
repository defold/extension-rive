// Copyright 2020-2022 The Defold Foundation
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

#include "rive_jni.h"
#include "defold_jni.h"
#include "../commonsrc/file.h"
#include "../commonsrc/texture.h"

#include <common/commands.h>

#include <defold/renderer.h>

#include <dmsdk/dlib/log.h>
#include <dmsdk/graphics/graphics.h>
#include <graphics/graphics.h>

#include <rive/artboard.hpp>
#include <rive/renderer.hpp>


namespace dmRiveJNI
{

// ******************************************************************************************************************


struct RiveFileJNI
{
    jclass      cls;
    jfieldID    pointer;       // Pointer to a RiveFile*
    jfieldID    path;          // string
    jfieldID    artboards;     // array of strings
    jfieldID    stateMachines; // array of strings
    jfieldID    viewModels; // array of strings
} g_RiveFileJNI;

struct RiveTextureJNI
{
    jclass      cls;
    jfieldID    width;
    jfieldID    height;
    jfieldID    format;
    jfieldID    data;
} g_RiveTextureJNI;


void InitializeJNITypes(JNIEnv* env)
{
    #define DM_RIVE_JNI_PACKAGE_NAME "com/dynamo/bob/pipeline/Rive"
    // https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/types.html

    {
        SETUP_CLASS(RiveFileJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "RiveFile"));
        GET_FLD_STRING(path);
        GET_FLD_TYPESTR(pointer, "J");
        GET_FLD_ARRAY(artboards, "java/lang/String");
        GET_FLD_ARRAY(stateMachines, "java/lang/String");
        GET_FLD_ARRAY(viewModels, "java/lang/String");
    }
    {
        SETUP_CLASS(RiveTextureJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "Texture"));
        GET_FLD_TYPESTR(width, "I");
        GET_FLD_TYPESTR(height, "I");
        GET_FLD_TYPESTR(format, "I");
        GET_FLD_TYPESTR(data, "[B");
    }
    #undef DM_RIVE_JNI_PACKAGE_NAME
}

void FinalizeJNITypes(JNIEnv* env)
{

}

static int64_t ToLong(dmRive::RiveFile* rive_file)
{
    union {
        uint64_t ptr;
        int64_t  x;
    } u;
    u.ptr = (uintptr_t)rive_file;
    return u.x;
}

static dmRive::RiveFile* FromLong(jlong x)
{
    union {
        uint64_t ptr;
        int64_t  x;
    } u;
    u.x = x;
    return (dmRive::RiveFile*)u.ptr;
}

static dmRive::RiveFile* FromObject(JNIEnv* env, jobject rive_file_obj)
{
    jlong pointer = env->GetLongField(rive_file_obj, g_RiveFileJNI.pointer);
    return FromLong(pointer);
}

static jobjectArray CreateStringArray(JNIEnv* env, const dmArray<const char*>& names)
{
    uint32_t count = names.Size();
    jobjectArray arr = env->NewObjectArray(count, env->FindClass("java/lang/String"), 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        env->SetObjectArrayElement(arr, i, env->NewStringUTF(names[i]));
    }
    return arr;
}

static jobjectArray CreateArtboards(JNIEnv* env, dmRive::RiveFile* rive_file)
{
    return CreateStringArray(env, rive_file->m_Artboards);
}

static int HashCode(JNIEnv* env, jclass cls, jobject object)
{
    jmethodID hashCode = env->GetMethodID(cls, "hashCode", "()I");
    jint i = env->CallIntMethod(object, hashCode);
    DM_CHECK_JNI_ERROR();
    return i;
}

static jobject CreateRiveFile(JNIEnv* env, dmRive::RiveFile* rive_file)
{
    if (!rive_file)
        return 0;

    jobject obj = env->AllocObject(g_RiveFileJNI.cls);

    dmDefoldJNI::SetFieldString(env, obj, g_RiveFileJNI.path, rive_file->m_Path);
    env->SetLongField(obj, g_RiveFileJNI.pointer, ToLong(rive_file));

    jobjectArray artboards = CreateArtboards(env, rive_file);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.artboards, artboards);
    env->DeleteLocalRef(artboards);

    jobjectArray state_machines = CreateStringArray(env, rive_file->m_StateMachines);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.stateMachines, state_machines);
    env->DeleteLocalRef(state_machines);

    jobjectArray view_models = CreateStringArray(env, rive_file->m_ViewModels);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.viewModels, view_models);
    env->DeleteLocalRef(view_models);
    return obj;
}

// ******************************************************************************************************************

jobject LoadFileFromBuffer(JNIEnv* env, jclass cls, const char* path, const uint8_t* data, uint32_t data_length)
{
    dmRive::RiveFile* rive_file = dmRive::LoadFileFromBuffer(data, data_length, path);

    jobject obj = CreateRiveFile(env, rive_file);
    if (!obj)
    {
        dmLogError("Failed to create JNI representation of a RiveFile");
    }
    return obj;
}

void DestroyFile(JNIEnv* env, jclass cls, jobject rive_file_obj)
{
    dmRive::RiveFile* rive_file = FromObject(env, rive_file_obj);
    if (!rive_file)
        return;
    env->SetLongField(rive_file_obj, g_RiveFileJNI.pointer, 0);

    dmRive::DestroyFile(rive_file);
}

void Update(JNIEnv* env, jclass cls, jobject rive_file_obj, jfloat dt)
{
    dmRive::RiveFile* rive_file = FromObject(env, rive_file_obj);
    if (!rive_file)
    {
        return;
    }
    dmRive::Update(rive_file, dt);
}

void SetArtboard(JNIEnv* env, jclass cls, jobject rive_file_obj, const char* artboard)
{
    dmRive::RiveFile* rive_file = FromObject(env, rive_file_obj);
    if (!rive_file)
    {
        return;
    }
    dmRive::SetArtboard(rive_file, artboard);
}

void SetStateMachine(JNIEnv* env, jclass cls, jobject rive_file_obj, const char* state_machine)
{
    dmRive::RiveFile* rive_file = FromObject(env, rive_file_obj);
    if (!rive_file)
    {
        return;
    }
    dmRive::SetStatemachine(rive_file, state_machine);
}

void SetViewModel(JNIEnv* env, jclass cls, jobject rive_file_obj, const char* view_model)
{
    dmRive::RiveFile* rive_file = FromObject(env, rive_file_obj);
    if (!rive_file)
    {
        return;
    }
    dmRive::SetViewModel(rive_file, view_model);
}

jobject GetTexture(JNIEnv* env, jclass cls, jobject rive_file_obj)
{
    dmRive::RiveFile* rive_file = FromObject(env, rive_file_obj);
    if (!rive_file)
    {
        return 0;
    }

    if (rive_file->m_Artboard == RIVE_NULL_HANDLE)
    {
        return 0;
    }

    dmRive::HRenderContext render_context = dmRiveCommands::GetDefoldRenderContext();
    if (!render_context)
    {
        dmLogError("Rive: missing render context");
        return 0;
    }

    rive::rcp<rive::CommandQueue> queue = dmRiveCommands::GetCommandQueue();
    if (!queue)
    {
        dmLogError("Rive: missing command queue");
        return 0;
    }

    dmGraphics::HContext graphics_context = dmGraphics::GetInstalledContext();
    if (!graphics_context)
    {
        dmLogError("Rive: missing graphics context");
        return 0;
    }

    dmGraphics::AdapterFamily adapter_family = dmGraphics::GetInstalledAdapterFamily();
    bool render_to_backbuffer = adapter_family == dmGraphics::ADAPTER_FAMILY_VULKAN;

    if (render_to_backbuffer)
    {
        dmGraphics::BeginFrame(graphics_context);
    }

    dmRive::RenderBeginParams render_params;
    render_params.m_DoFinalBlit = !render_to_backbuffer;
    render_params.m_BackbufferSamples = 0;

    dmRive::RenderBegin(render_context, 0, render_params);

    rive::Renderer* renderer = dmRive::GetRiveRenderer(render_context);
    if (!renderer)
    {
        dmRive::RenderEnd(render_context);
        return 0;
    }

    uint32_t width = dmGraphics::GetWindowWidth(graphics_context);
    uint32_t height = dmGraphics::GetWindowHeight(graphics_context);
    if (width == 0 || height == 0)
    {
        dmRive::RenderEnd(render_context);
        dmLogError("Rive: invalid render size %u x %u (adapter=%d)", width, height, (int)adapter_family);
        return 0;
    }

    float display_factor = dmGraphics::GetDisplayScaleFactor(graphics_context);

    if (rive_file->m_StateMachine != RIVE_NULL_HANDLE)
    {
        queue->advanceStateMachine(rive_file->m_StateMachine, 0.0f);
    }

    const rive::ArtboardHandle artboard_handle = rive_file->m_Artboard;
    const rive::Fit fit = rive::Fit::contain;
    const rive::Alignment alignment = rive::Alignment::center;

    auto drawLoop = [artboard_handle,
                     renderer,
                     fit,
                     alignment,
                     width,
                     height,
                     display_factor](rive::DrawKey drawKey, rive::CommandServer* server)
    {
        rive::ArtboardInstance* artboard = server->getArtboardInstance(artboard_handle);
        if (artboard == nullptr)
        {
            return;
        }

        renderer->save();
        rive::AABB bounds = artboard->bounds();

        if (fit == rive::Fit::layout)
        {
            artboard->width(width / display_factor);
            artboard->height(height / display_factor);
        }

        rive::Mat2D rendererTransform = rive::computeAlignment(fit, alignment, rive::AABB(0, 0, width, height), bounds, display_factor);
        renderer->transform(rendererTransform);
        artboard->draw(renderer);
        renderer->restore();
    };

    rive::DrawKey draw_key = queue->createDrawKey();
    queue->draw(draw_key, drawLoop);

    dmRiveCommands::ProcessMessages();
    dmRive::RenderEnd(render_context);

    if (render_to_backbuffer)
    {
        dmGraphics::Flip(graphics_context);
    }

    dmGraphics::HTexture backing_texture = dmRive::GetBackingTexture(render_context);
    dmRive::TexturePixels pixels = {};
    if (!dmRive::ReadPixels(backing_texture, &pixels))
    {
        dmLogError("Rive: ReadPixels failed (adapter=%d, backbuffer=%d)", (int)adapter_family, (int)render_to_backbuffer);
        return 0;
    }

    if (pixels.m_Format == dmGraphics::TEXTURE_FORMAT_RGBA)
    {
        uint32_t pixel_count = (uint32_t)pixels.m_Width * (uint32_t)pixels.m_Height;
        uint8_t* data = pixels.m_Data;
        for (uint32_t i = 0; i < pixel_count; ++i)
        {
            uint8_t tmp = data[0];
            data[0] = data[2];
            data[2] = tmp;
            data += 4;
        }
        pixels.m_Format = dmGraphics::TEXTURE_FORMAT_BGRA8U;
    }

    if (pixels.m_Format != dmGraphics::TEXTURE_FORMAT_BGRA8U)
    {
        dmRive::FreePixels(&pixels);
        return 0;
    }

    jbyteArray data_arr = env->NewByteArray((jsize)pixels.m_DataSize);
    env->SetByteArrayRegion(data_arr, 0, (jsize)pixels.m_DataSize, (const jbyte*)pixels.m_Data);

    jobject texture_obj = env->AllocObject(g_RiveTextureJNI.cls);
    env->SetIntField(texture_obj, g_RiveTextureJNI.width, (int)pixels.m_Width);
    env->SetIntField(texture_obj, g_RiveTextureJNI.height, (int)pixels.m_Height);
    env->SetIntField(texture_obj, g_RiveTextureJNI.format, (int)pixels.m_Format);
    env->SetObjectField(texture_obj, g_RiveTextureJNI.data, data_arr);
    env->DeleteLocalRef(data_arr);

    dmRive::FreePixels(&pixels);

    return texture_obj;
}

} // namespace
