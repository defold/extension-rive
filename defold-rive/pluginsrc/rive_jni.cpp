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

#include <rive/artboard.hpp>
#include <rive/renderer.hpp>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


namespace dmRiveJNI
{

// ******************************************************************************************************************

static bool ConvertPixelsToABGR(dmRive::TexturePixels* pixels,
                                bool flip_y,
                                uint32_t out_width = 0,
                                uint32_t out_height = 0,
                                uint32_t crop_x = 0,
                                uint32_t crop_y = 0)
{
    if (!pixels || !pixels->m_Data)
    {
        return false;
    }

    const bool is_rgba = pixels->m_Format == dmGraphics::TEXTURE_FORMAT_RGBA;
    const bool is_bgra = pixels->m_Format == dmGraphics::TEXTURE_FORMAT_BGRA8U;
    if (!is_rgba && !is_bgra)
    {
        return false;
    }

    const uint32_t src_width = (uint32_t)pixels->m_Width;
    const uint32_t src_height = (uint32_t)pixels->m_Height;
    if (src_width == 0 || src_height == 0)
    {
        return false;
    }

    const uint32_t width = out_width == 0 ? src_width : (out_width < src_width ? out_width : src_width);
    const uint32_t height = out_height == 0 ? src_height : (out_height < src_height ? out_height : src_height);
    if (width == 0 || height == 0)
    {
        return false;
    }

    const uint32_t max_crop_x = src_width - width;
    const uint32_t max_crop_y = src_height - height;
    crop_x = crop_x < max_crop_x ? crop_x : max_crop_x;
    crop_y = crop_y < max_crop_y ? crop_y : max_crop_y;

    const uint32_t src_row_bytes = src_width * 4;
    const uint32_t dst_row_bytes = width * 4;
    const uint32_t data_size = dst_row_bytes * height;

    uint8_t* out = (uint8_t*)malloc(data_size);
    if (!out)
    {
        return false;
    }

    const uint8_t* in = pixels->m_Data;
    for (uint32_t y = 0; y < height; ++y)
    {
        const uint32_t src_y = flip_y ? (src_height - 1 - (crop_y + y)) : (crop_y + y);
        const uint8_t* src_row = in + src_y * src_row_bytes + crop_x * 4;
        uint8_t* dst_row = out + y * dst_row_bytes;
        for (uint32_t x = 0; x < width; ++x)
        {
            const uint8_t* src = src_row + x * 4;
            uint8_t* dst = dst_row + x * 4;

            if (is_rgba)
            {
                // RGBA -> ABGR
                dst[0] = src[3];
                dst[1] = src[2];
                dst[2] = src[1];
                dst[3] = src[0];
            }
            else
            {
                // BGRA -> ABGR
                dst[0] = src[3];
                dst[1] = src[0];
                dst[2] = src[1];
                dst[3] = src[2];
            }
        }
    }

    free(pixels->m_Data);
    pixels->m_Data = out;
    pixels->m_Width = (decltype(pixels->m_Width))width;
    pixels->m_Height = (decltype(pixels->m_Height))height;
    pixels->m_DataSize = data_size;
    // ABGR is not represented by dmGraphics::TextureFormat, keep BGRA marker.
    pixels->m_Format = dmGraphics::TEXTURE_FORMAT_BGRA8U;
    return true;
}

enum ReadbackSource
{
    READBACK_SOURCE_TEXTURE,
    READBACK_SOURCE_BACKBUFFER,
};

static bool ShouldFlipReadbackY(dmGraphics::AdapterFamily adapter_family, ReadbackSource readback_source)
{
    switch (adapter_family)
    {
        case dmGraphics::ADAPTER_FAMILY_VULKAN:
            if (readback_source == READBACK_SOURCE_BACKBUFFER)
            {
                return false;
            }
            return true;

        case dmGraphics::ADAPTER_FAMILY_OPENGL:
        case dmGraphics::ADAPTER_FAMILY_OPENGLES:
            return false;

        default:
            return readback_source == READBACK_SOURCE_TEXTURE;
    }
}

static bool AlignmentEquals(const rive::Alignment& lhs, const rive::Alignment& rhs)
{
    return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

static void AlignmentToCropOffset(const rive::Alignment& alignment,
                                  uint32_t src_width,
                                  uint32_t src_height,
                                  uint32_t dst_width,
                                  uint32_t dst_height,
                                  uint32_t* out_crop_x,
                                  uint32_t* out_crop_y)
{
    if (!out_crop_x || !out_crop_y)
    {
        return;
    }

    const uint32_t extra_x = src_width > dst_width ? src_width - dst_width : 0;
    const uint32_t extra_y = src_height > dst_height ? src_height - dst_height : 0;

    if (extra_x == 0)
    {
        *out_crop_x = 0;
    }
    else if (AlignmentEquals(alignment, rive::Alignment::topLeft) ||
             AlignmentEquals(alignment, rive::Alignment::centerLeft) ||
             AlignmentEquals(alignment, rive::Alignment::bottomLeft))
    {
        *out_crop_x = 0;
    }
    else if (AlignmentEquals(alignment, rive::Alignment::topRight) ||
             AlignmentEquals(alignment, rive::Alignment::centerRight) ||
             AlignmentEquals(alignment, rive::Alignment::bottomRight))
    {
        *out_crop_x = extra_x;
    }
    else
    {
        *out_crop_x = extra_x / 2;
    }

    if (extra_y == 0)
    {
        *out_crop_y = 0;
    }
    else if (AlignmentEquals(alignment, rive::Alignment::topLeft) ||
             AlignmentEquals(alignment, rive::Alignment::topCenter) ||
             AlignmentEquals(alignment, rive::Alignment::topRight))
    {
        *out_crop_y = 0;
    }
    else if (AlignmentEquals(alignment, rive::Alignment::bottomLeft) ||
             AlignmentEquals(alignment, rive::Alignment::bottomCenter) ||
             AlignmentEquals(alignment, rive::Alignment::bottomRight))
    {
        *out_crop_y = extra_y;
    }
    else
    {
        *out_crop_y = extra_y / 2;
    }
}

static rive::Fit ToFit(jint fit)
{
    switch ((rive::Fit)fit)
    {
        case rive::Fit::fill:
        case rive::Fit::contain:
        case rive::Fit::cover:
        case rive::Fit::fitWidth:
        case rive::Fit::fitHeight:
        case rive::Fit::none:
        case rive::Fit::scaleDown:
        case rive::Fit::layout:
            return (rive::Fit)fit;
        default:
            return rive::Fit::contain;
    }
}

// Local copy of dmRiveDDF::RiveModelDesc::Alignment values.
enum class RiveModelDescAlignment : jint
{
    ALIGNMENT_TOP_LEFT = 0,
    ALIGNMENT_TOP_CENTER = 1,
    ALIGNMENT_TOP_RIGHT = 2,
    ALIGNMENT_CENTER_LEFT = 3,
    ALIGNMENT_CENTER = 4,
    ALIGNMENT_CENTER_RIGHT = 5,
    ALIGNMENT_BOTTOM_LEFT = 6,
    ALIGNMENT_BOTTOM_CENTER = 7,
    ALIGNMENT_BOTTOM_RIGHT = 8
};

static rive::Alignment ToAlignment(jint alignment)
{
    switch ((RiveModelDescAlignment)alignment)
    {
        case RiveModelDescAlignment::ALIGNMENT_TOP_LEFT:      return rive::Alignment::topLeft;
        case RiveModelDescAlignment::ALIGNMENT_TOP_CENTER:    return rive::Alignment::topCenter;
        case RiveModelDescAlignment::ALIGNMENT_TOP_RIGHT:     return rive::Alignment::topRight;
        case RiveModelDescAlignment::ALIGNMENT_CENTER_LEFT:   return rive::Alignment::centerLeft;
        case RiveModelDescAlignment::ALIGNMENT_CENTER:        return rive::Alignment::center;
        case RiveModelDescAlignment::ALIGNMENT_CENTER_RIGHT:  return rive::Alignment::centerRight;
        case RiveModelDescAlignment::ALIGNMENT_BOTTOM_LEFT:   return rive::Alignment::bottomLeft;
        case RiveModelDescAlignment::ALIGNMENT_BOTTOM_CENTER: return rive::Alignment::bottomCenter;
        case RiveModelDescAlignment::ALIGNMENT_BOTTOM_RIGHT:  return rive::Alignment::bottomRight;
        default:                                              return rive::Alignment::center;
    }
}

// TODO: No need for this
static uint32_t BoundsDimension(float min_value, float max_value, uint32_t fallback)
{
    float size = max_value - min_value;
    if (size <= 0.0f)
    {
        return fallback;
    }

    uint32_t dim = (uint32_t)ceilf(size);
    return dim > 0 ? dim : fallback;
}

static bool IsBoundsValid(const rive::AABB& bounds)
{
    return !bounds.isEmptyOrNaN();
}


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
    jfieldID    bounds; // DefoldJNI.Aabb
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
    jfieldID    value;
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
        GET_FLD_TYPESTR(bounds, "Lcom/dynamo/bob/pipeline/DefoldJNI$Aabb;");
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
        GET_FLD_STRING(value);
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
    if (!rive_file_obj)
    {
        return 0;
    }
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

static void SetRiveFileBounds(JNIEnv* env, jobject obj, const dmRive::RiveFile* rive_file)
{
    if (!rive_file || !obj)
    {
        return;
    }

    const rive::AABB& bounds = rive_file->m_Bounds;
    if (!IsBoundsValid(bounds))
    {
        dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.bounds, 0);
        return;
    }

    dmVMath::Vector4 min(bounds.minX, bounds.minY, 0.0f, 0.0f);
    dmVMath::Vector4 max(bounds.maxX, bounds.maxY, 0.0f, 0.0f);
    jobject aabb_obj = dmDefoldJNI::CreateAABB(env, min, max);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.bounds, aabb_obj);
    env->DeleteLocalRef(aabb_obj);
}

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
        dmDefoldJNI::SetFieldString(env, obj, g_ViewModelPropertyJNI.value, properties[i].m_Value ? properties[i].m_Value : "");
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
    SetRiveFileBounds(env, obj, rive_file);
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
    SetRiveFileBounds(env, rive_file_obj, rive_file);
}

void SetArtboard(JNIEnv* env, jclass cls, jobject rive_file_obj, const char* artboard)
{
    dmRive::RiveFile* rive_file = FromObject(env, rive_file_obj);
    if (!rive_file)
    {
        return;
    }
    dmRive::SetArtboard(rive_file, artboard);
    SetRiveFileBounds(env, rive_file_obj, rive_file);
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

void SetFitAlignment(JNIEnv* env, jclass cls, jobject rive_file_obj, jint fit, jint alignment)
{
    dmRive::RiveFile* rive_file = FromObject(env, rive_file_obj);
    if (!rive_file)
    {
        return;
    }

    rive::Fit fit_value = ToFit(fit);
    rive::Alignment alignment_value = ToAlignment(alignment);

    dmRive::SetFitAlignment(rive_file, fit_value, alignment_value);
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


static bool GetTextureInternal(dmRive::RiveFile* rive_file,
                               dmRive::HRenderContext render_context,
                               rive::rcp<rive::CommandQueue> queue,
                               dmGraphics::HContext graphics_context,
                               dmGraphics::AdapterFamily adapter_family,
                               bool render_to_backbuffer,
                               bool capture_pixels,
                               uint32_t render_width,
                               uint32_t render_height,
                               dmRive::TexturePixels* pixels)
{
    if (!rive_file || !render_context || !queue || !graphics_context || !pixels)
    {
        return false;
    }

    dmRive::RenderBeginParams render_params;
    render_params.m_DoFinalBlit = !render_to_backbuffer;
    render_params.m_BackbufferSamples = 0;
    render_params.m_Width = render_width;
    render_params.m_Height = render_height;

    const float artboard_display_factor = 1.0f;
    const rive::ArtboardHandle artboard_handle = rive_file->m_Artboard;

    if (render_to_backbuffer)
    {
        dmGraphics::BeginFrame(graphics_context);
    }

    dmRive::RenderBegin(render_context, 0, render_params);

    rive::Renderer* renderer = dmRive::GetRiveRenderer(render_context);
    if (!renderer)
    {
        dmRive::RenderEnd(render_context);
        if (render_to_backbuffer)
        {
            dmGraphics::Flip(graphics_context);
        }
        return false;
    }

    if (rive_file->m_StateMachine != RIVE_NULL_HANDLE)
    {
        queue->advanceStateMachine(rive_file->m_StateMachine, 0.0f);
    }

    auto drawLoop = [artboard_handle,
                     renderer,
                     rive_file,
                     render_width,
                     render_height,
                     artboard_display_factor](rive::DrawKey, rive::CommandServer* server)
    {
        rive::ArtboardInstance* artboard = server->getArtboardInstance(artboard_handle);
        if (artboard == nullptr)
        {
            return;
        }

        rive::Mat2D renderer_transform = dmRive::CalcTransformRive(artboard,
                                                                   rive_file->m_Fit,
                                                                   rive_file->m_Alignment,
                                                                   render_width,
                                                                   render_height,
                                                                   artboard_display_factor);
        dmRive::DrawArtboard(artboard, renderer, renderer_transform);
    };

    rive::DrawKey draw_key = queue->createDrawKey();
    queue->draw(draw_key, drawLoop);

    dmRiveCommands::ProcessMessages();
    dmRive::RenderEnd(render_context);

    if (capture_pixels)
    {
        if (render_to_backbuffer)
        {
            bool read_ok = dmRive::ReadPixelsBackBuffer(pixels);
            dmGraphics::Flip(graphics_context);
            return read_ok;
        }

        dmGraphics::HTexture backing_texture = dmRive::GetBackingTexture(render_context);
        if (backing_texture == 0)
        {
            dmLogError("Rive: no readback texture available (adapter=%d, backbuffer=%d)", (int)adapter_family, (int)render_to_backbuffer);
            return false;
        }
        return dmRive::ReadPixels(backing_texture, pixels);
    }

    if (render_to_backbuffer)
    {
        dmGraphics::Flip(graphics_context);
    }

    return true;
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
    ReadbackSource readback_source = render_to_backbuffer ? READBACK_SOURCE_BACKBUFFER
                                                          : READBACK_SOURCE_TEXTURE;
    uint32_t window_width = dmGraphics::GetWindowWidth(graphics_context);
    uint32_t window_height = dmGraphics::GetWindowHeight(graphics_context);

    rive::AABB render_bounds = rive_file->m_Bounds;
    if (!IsBoundsValid(render_bounds))
    {
        render_bounds = rive::AABB(0.0f, 0.0f, (float)window_width, (float)window_height);
    }

    rive::AABB fresh_bounds;
    if (dmRiveCommands::GetBounds(rive_file->m_Artboard, &fresh_bounds) && IsBoundsValid(fresh_bounds))
    {
        render_bounds = fresh_bounds;
    }

    uint32_t target_width = BoundsDimension(render_bounds.minX, render_bounds.maxX, window_width);
    uint32_t target_height = BoundsDimension(render_bounds.minY, render_bounds.maxY, window_height);
    if (target_width == 0 || target_height == 0)
    {
        dmLogError("Rive: invalid render size %u x %u (adapter=%d)", target_width, target_height, (int)adapter_family);
        return 0;
    }

    // Keep editor/plugin thumbnail rendering bounded to avoid oversized render
    // targets on drivers that are unstable with large offscreen FBOs.
    const uint32_t max_preview_dim = 1024;
    if (target_width > max_preview_dim || target_height > max_preview_dim)
    {
        double sx = (double)max_preview_dim / (double)target_width;
        double sy = (double)max_preview_dim / (double)target_height;
        double s = sx < sy ? sx : sy;
        uint32_t clamped_width = (uint32_t)((double)target_width * s);
        uint32_t clamped_height = (uint32_t)((double)target_height * s);
        target_width = clamped_width > 0 ? clamped_width : 1;
        target_height = clamped_height > 0 ? clamped_height : 1;
    }

    uint32_t render_width = target_width;
    uint32_t render_height = target_height;

    if (render_to_backbuffer)
    {
        // Vulkan readback currently pulls from the swapchain image dimensions.
        // Keep render dimensions in sync with the current swapchain.
        render_width = window_width;
        render_height = window_height;
    }

    dmRive::TexturePixels pixels = {};
    bool read_ok = false;
    const uint32_t render_frame_count = render_to_backbuffer ? 5 : 1;
    for (uint32_t frame_idx = 0; frame_idx < render_frame_count; ++frame_idx)
    {
        read_ok = GetTextureInternal(rive_file,
                                     render_context,
                                     queue,
                                     graphics_context,
                                     adapter_family,
                                     render_to_backbuffer,
                                     frame_idx + 1 == render_frame_count, // Actual shapshot frame?
                                     render_width,
                                     render_height,
                                     &pixels);
        if (!read_ok)
        {
            break;
        }
    }

    if (!read_ok)
    {
        dmLogError("Rive: ReadPixels failed (adapter=%d, backbuffer=%d)", (int)adapter_family, (int)render_to_backbuffer);
        return 0;
    }

    uint32_t crop_x = 0;
    uint32_t crop_y = 0;
    AlignmentToCropOffset(rive_file->m_Alignment, (uint32_t)pixels.m_Width, (uint32_t)pixels.m_Height, target_width, target_height, &crop_x, &crop_y);

    bool flip_y = ShouldFlipReadbackY(adapter_family, readback_source);

    if (!ConvertPixelsToABGR(&pixels, flip_y, target_width, target_height, crop_x, crop_y))
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
