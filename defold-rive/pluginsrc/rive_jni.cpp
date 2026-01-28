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
#include <common/file.h>
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
    jfieldID    stateMachines; // HashMap<String, String[]>
    jfieldID    viewModels; // array of strings
    jfieldID    viewModelProperties; // array of ViewModelProperty
    jfieldID    viewModelEnums; // array of ViewModelEnum
    jfieldID    viewModelInstanceNames; // array of ViewModelInstanceNames
    jfieldID    defaultViewModelInfo; // DefaultViewModelInfo
} g_RiveFileJNI;

struct RiveTextureJNI
{
    jclass      cls;
    jfieldID    width;
    jfieldID    height;
    jfieldID    format;
    jfieldID    data;
} g_RiveTextureJNI;

struct ViewModelPropertyJNI
{
    jclass      cls;
    jfieldID    viewModel;
    jfieldID    name;
    jfieldID    type;
    jfieldID    typeName;
    jfieldID    metaData;
} g_ViewModelPropertyJNI;

struct ViewModelEnumJNI
{
    jclass      cls;
    jfieldID    name;
    jfieldID    enumerants;
} g_ViewModelEnumJNI;

struct ViewModelInstanceNamesJNI
{
    jclass      cls;
    jfieldID    viewModel;
    jfieldID    instances;
} g_ViewModelInstanceNamesJNI;

struct DefaultViewModelInfoJNI
{
    jclass      cls;
    jfieldID    viewModel;
    jfieldID    instance;
} g_DefaultViewModelInfoJNI;


void InitializeJNITypes(JNIEnv* env)
{
    #define DM_RIVE_JNI_PACKAGE_NAME "com/dynamo/bob/pipeline/Rive"
    // https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/types.html

    {
        SETUP_CLASS(RiveFileJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "RiveFile"));
        GET_FLD_STRING(path);
        GET_FLD_TYPESTR(pointer, "J");
        GET_FLD_ARRAY(artboards, "java/lang/String");
        GET_FLD_TYPESTR(stateMachines, "Ljava/util/HashMap;");
        GET_FLD_ARRAY(viewModels, "java/lang/String");
        GET_FLD_ARRAY(viewModelProperties, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "ViewModelProperty"));
        GET_FLD_ARRAY(viewModelEnums, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "ViewModelEnum"));
        GET_FLD_ARRAY(viewModelInstanceNames, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "ViewModelInstanceNames"));
        GET_FLD(defaultViewModelInfo, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "DefaultViewModelInfo"));
    }
    {
        SETUP_CLASS(RiveTextureJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "Texture"));
        GET_FLD_TYPESTR(width, "I");
        GET_FLD_TYPESTR(height, "I");
        GET_FLD_TYPESTR(format, "I");
        GET_FLD_TYPESTR(data, "[B");
    }
    {
        SETUP_CLASS(ViewModelPropertyJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "ViewModelProperty"));
        GET_FLD_STRING(viewModel);
        GET_FLD_STRING(name);
        GET_FLD_TYPESTR(type, "I");
        GET_FLD_STRING(typeName);
        GET_FLD_STRING(metaData);
    }
    {
        SETUP_CLASS(ViewModelEnumJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "ViewModelEnum"));
        GET_FLD_STRING(name);
        GET_FLD_ARRAY(enumerants, "java/lang/String");
    }
    {
        SETUP_CLASS(ViewModelInstanceNamesJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "ViewModelInstanceNames"));
        GET_FLD_STRING(viewModel);
        GET_FLD_ARRAY(instances, "java/lang/String");
    }
    {
        SETUP_CLASS(DefaultViewModelInfoJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "DefaultViewModelInfo"));
        GET_FLD_STRING(viewModel);
        GET_FLD_STRING(instance);
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

static jobject CreateStateMachinesByArtboard(JNIEnv* env, const dmArray<dmRive::ArtboardStateMachines>& entries)
{
    jclass hash_map_cls = env->FindClass("java/util/HashMap");
    jmethodID init = env->GetMethodID(hash_map_cls, "<init>", "()V");
    jmethodID put = env->GetMethodID(hash_map_cls, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject map = env->NewObject(hash_map_cls, init);

    for (uint32_t i = 0; i < entries.Size(); ++i)
    {
        const dmRive::ArtboardStateMachines& entry = entries[i];
        const char* artboard_name = entry.m_Artboard ? entry.m_Artboard : "";
        jstring key = env->NewStringUTF(artboard_name);
        jobjectArray values = CreateStringArray(env, entry.m_StateMachines);
        env->CallObjectMethod(map, put, key, values);
        DM_CHECK_JNI_ERROR();
        env->DeleteLocalRef(key);
        env->DeleteLocalRef(values);
    }

    env->DeleteLocalRef(hash_map_cls);
    return map;
}

static const char* DataTypeToString(rive::DataType type);

static jobjectArray CreateViewModelProperties(JNIEnv* env, const dmArray<dmRive::ViewModelProperty>& properties)
{
    uint32_t count = properties.Size();
    jobjectArray arr = env->NewObjectArray(count, g_ViewModelPropertyJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject obj = env->AllocObject(g_ViewModelPropertyJNI.cls);
        dmDefoldJNI::SetFieldString(env, obj, g_ViewModelPropertyJNI.viewModel, properties[i].m_ViewModel ? properties[i].m_ViewModel : "");
        dmDefoldJNI::SetFieldString(env, obj, g_ViewModelPropertyJNI.name, properties[i].m_Name ? properties[i].m_Name : "");
        dmDefoldJNI::SetFieldString(env, obj, g_ViewModelPropertyJNI.metaData, properties[i].m_MetaData ? properties[i].m_MetaData : "");
        env->SetIntField(obj, g_ViewModelPropertyJNI.type, (int)properties[i].m_Type);
        dmDefoldJNI::SetFieldString(env, obj, g_ViewModelPropertyJNI.typeName, DataTypeToString(properties[i].m_Type));
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}

static jobjectArray CreateViewModelEnums(JNIEnv* env, const dmArray<dmRive::ViewModelEnum>& enums)
{
    uint32_t count = enums.Size();
    jobjectArray arr = env->NewObjectArray(count, g_ViewModelEnumJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject obj = env->AllocObject(g_ViewModelEnumJNI.cls);
        dmDefoldJNI::SetFieldString(env, obj, g_ViewModelEnumJNI.name, enums[i].m_Name ? enums[i].m_Name : "");
        jobjectArray enumerants = CreateStringArray(env, enums[i].m_Enumerants);
        dmDefoldJNI::SetFieldObject(env, obj, g_ViewModelEnumJNI.enumerants, enumerants);
        env->DeleteLocalRef(enumerants);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}

static jobjectArray CreateViewModelInstanceNames(JNIEnv* env, const dmArray<dmRive::ViewModelInstanceNames>& instances)
{
    uint32_t count = instances.Size();
    jobjectArray arr = env->NewObjectArray(count, g_ViewModelInstanceNamesJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject obj = env->AllocObject(g_ViewModelInstanceNamesJNI.cls);
        dmDefoldJNI::SetFieldString(env, obj, g_ViewModelInstanceNamesJNI.viewModel, instances[i].m_ViewModel ? instances[i].m_ViewModel : "");
        jobjectArray names = CreateStringArray(env, instances[i].m_Instances);
        dmDefoldJNI::SetFieldObject(env, obj, g_ViewModelInstanceNamesJNI.instances, names);
        env->DeleteLocalRef(names);
        env->SetObjectArrayElement(arr, i, obj);
        env->DeleteLocalRef(obj);
    }
    return arr;
}

static jobject CreateDefaultViewModelInfo(JNIEnv* env, const dmRive::DefaultViewModelInfo& info, bool has_info)
{
    if (!has_info)
    {
        return 0;
    }
    jobject obj = env->AllocObject(g_DefaultViewModelInfoJNI.cls);
    dmDefoldJNI::SetFieldString(env, obj, g_DefaultViewModelInfoJNI.viewModel, info.m_ViewModel ? info.m_ViewModel : "");
    dmDefoldJNI::SetFieldString(env, obj, g_DefaultViewModelInfoJNI.instance, info.m_Instance ? info.m_Instance : "");
    return obj;
}

static int HashCode(JNIEnv* env, jclass cls, jobject object)
{
    jmethodID hashCode = env->GetMethodID(cls, "hashCode", "()I");
    jint i = env->CallIntMethod(object, hashCode);
    DM_CHECK_JNI_ERROR();
    return i;
}

static const char* DataTypeToString(rive::DataType type)
{
    switch (type)
    {
        case rive::DataType::none: return "none";
        case rive::DataType::string: return "string";
        case rive::DataType::number: return "number";
        case rive::DataType::boolean: return "boolean";
        case rive::DataType::color: return "color";
        case rive::DataType::list: return "list";
        case rive::DataType::enumType: return "enum";
        case rive::DataType::trigger: return "trigger";
        case rive::DataType::viewModel: return "viewModel";
        case rive::DataType::integer: return "integer";
        case rive::DataType::symbolListIndex: return "symbolListIndex";
        case rive::DataType::assetImage: return "assetImage";
        case rive::DataType::artboard: return "artboard";
        case rive::DataType::input: return "input";
        case rive::DataType::any: return "any";
        default: return "unknown";
    }
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

    jobject state_machines = CreateStateMachinesByArtboard(env, rive_file->m_StateMachinesByArtboard);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.stateMachines, state_machines);
    env->DeleteLocalRef(state_machines);

    jobjectArray view_models = CreateStringArray(env, rive_file->m_ViewModels);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.viewModels, view_models);
    env->DeleteLocalRef(view_models);

    jobjectArray view_model_properties = CreateViewModelProperties(env, rive_file->m_ViewModelProperties);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.viewModelProperties, view_model_properties);
    env->DeleteLocalRef(view_model_properties);

    jobjectArray view_model_enums = CreateViewModelEnums(env, rive_file->m_ViewModelEnums);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.viewModelEnums, view_model_enums);
    env->DeleteLocalRef(view_model_enums);

    jobjectArray view_model_instance_names = CreateViewModelInstanceNames(env, rive_file->m_ViewModelInstanceNames);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.viewModelInstanceNames, view_model_instance_names);
    env->DeleteLocalRef(view_model_instance_names);

    jobject default_view_model_info = CreateDefaultViewModelInfo(env, rive_file->m_DefaultViewModelInfo, rive_file->m_HasDefaultViewModelInfo);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.defaultViewModelInfo, default_view_model_info);
    if (default_view_model_info)
    {
        env->DeleteLocalRef(default_view_model_info);
    }
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
    dmRive::DrawArtboardParams draw_params;
    draw_params.m_Fit = rive::Fit::contain;
    draw_params.m_Alignment = rive::Alignment::center;
    draw_params.m_Width = width;
    draw_params.m_Height = height;
    draw_params.m_DisplayFactor = display_factor;

    auto drawLoop = [artboard_handle,
                     renderer,
                     draw_params](rive::DrawKey drawKey, rive::CommandServer* server)
    {
        rive::ArtboardInstance* artboard = server->getArtboardInstance(artboard_handle);
        if (artboard == nullptr)
        {
            return;
        }
        dmRive::DrawArtboard(artboard, renderer, draw_params, 0);
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
