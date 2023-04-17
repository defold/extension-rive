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
#include "render_jni.h"
#include "rive_file.h"

#include <dmsdk/dlib/align.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/math.h>
#include <dmsdk/dlib/transform.h>

#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/linear_animation.hpp>
#include <rive/animation/state_machine.hpp>
#include <rive/animation/state_machine_bool.hpp>
#include <rive/animation/state_machine_number.hpp>
#include <rive/animation/state_machine_trigger.hpp>

namespace dmRiveJNI
{

// ******************************************************************************************************************


struct StateMachineInputJNI
{
    jclass      cls;
    jfieldID    name;    // string
    jfieldID    type;    // string
} g_StateMachineInputJNI;

struct StateMachineJNI
{
    jclass      cls;
    jfieldID    name;    // string
    jfieldID    inputs;  // array of StateMachineInput
} g_StateMachineJNI;

struct BoneJNI
{
    jclass      cls;
    jfieldID    name;       // string
    jfieldID    index;      // int index into array
    jfieldID    parent;     // Bone
    jfieldID    children;   // Bone[]
    jfieldID    posX;       // float
    jfieldID    posY;       // float
    jfieldID    rotation;   // float
    jfieldID    scaleX;     // float
    jfieldID    scaleY;     // float
    jfieldID    length;     // float
} g_BoneJNI;

struct RiveFileJNI
{
    jclass      cls;
    jfieldID    pointer;    // Pointer to a RiveFile*
    jfieldID    path;       // string
    jfieldID    aabb;       // Aabb
    jfieldID    vertices;   // array of floats. Each vertex is: (x, y, u, v)
    jfieldID    indices;    // array of indices into the vertices array. triangle list. 3 indices define a triangle

    jfieldID    animations;    // array of strings
    jfieldID    stateMachines; // array of state machines
    jfieldID    bones;         // array of root bones
    jfieldID    renderObjects; // array of render objects
} g_RiveFileJNI;


void InitializeJNITypes(JNIEnv* env)
{
    #define DM_RIVE_JNI_PACKAGE_NAME "com/dynamo/bob/pipeline/Rive"
    #define DM_DEFOLD_JNI_PACKAGE_NAME "com/dynamo/bob/pipeline/DefoldJNI"
    #define DM_RENDER_JNI_PACKAGE_NAME "com/dynamo/bob/pipeline/RenderJNI"
    // https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/types.html

    {
        SETUP_CLASS(StateMachineInputJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "StateMachineInput"));
        GET_FLD_STRING(name);
        GET_FLD_STRING(type);
    }
    {
        SETUP_CLASS(StateMachineJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "StateMachine"));
        GET_FLD_STRING(name);
        GET_FLD_ARRAY(inputs, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "StateMachineInput"));
    }
    {
        SETUP_CLASS(BoneJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "Bone"));
        GET_FLD_STRING(name);
        GET_FLD_TYPESTR(index, "I");
        GET_FLD(parent, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "Bone"));
        GET_FLD_ARRAY(children, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "Bone"));
        GET_FLD_TYPESTR(posX, "F");
        GET_FLD_TYPESTR(posY, "F");
        GET_FLD_TYPESTR(rotation, "F");
        GET_FLD_TYPESTR(scaleX, "F");
        GET_FLD_TYPESTR(scaleY, "F");
        GET_FLD_TYPESTR(length, "F");
    }
    {
        SETUP_CLASS(RiveFileJNI, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "RiveFile"));
        GET_FLD_STRING(path);
        GET_FLD_TYPESTR(pointer, "J");
        GET_FLD_TYPESTR(vertices, "[F");
        GET_FLD_TYPESTR(indices, "[I");
        GET_FLD_ARRAY(animations, "java/lang/String");
        GET_FLD(aabb, MAKE_TYPE_NAME(DM_DEFOLD_JNI_PACKAGE_NAME, "Aabb"));
        GET_FLD_ARRAY(stateMachines, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "StateMachine"));
        GET_FLD_ARRAY(bones, MAKE_TYPE_NAME(DM_RIVE_JNI_PACKAGE_NAME, "Bone"));
        GET_FLD_ARRAY(renderObjects, MAKE_TYPE_NAME(DM_RENDER_JNI_PACKAGE_NAME, "RenderObject"));
    }
    #undef DM_RIVE_JNI_PACKAGE_NAME
    #undef DM_DEFOLD_JNI_PACKAGE_NAME
    #undef DM_RENDER_JNI_PACKAGE_NAME
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

static const char* INPUT_TYPE_BOOL="bool";
static const char* INPUT_TYPE_NUMBER="number";
static const char* INPUT_TYPE_TRIGGER="trigger";
static const char* INPUT_TYPE_UNKNOWN="unknown";

static jobject CreateStateMachineInput(JNIEnv* env, const rive::StateMachine* state_machine, uint32_t index)
{
    if (index < 0 || index >= (int)state_machine->inputCount()) {
        dmLogError("%s: State machine index %d is not in range [0, %d)", __FUNCTION__, index, (int)state_machine->inputCount());
        return 0;
    }

    const rive::StateMachineInput* state_machine_input = state_machine->input(index);
    if (state_machine_input == 0) {
        printf("state_machine_input == 0\n");
        fflush(stdout);
        return 0;
    }

    const char* type = 0;
    if (state_machine_input->is<rive::StateMachineBool>())
        type = INPUT_TYPE_BOOL;
    else if (state_machine_input->is<rive::StateMachineNumber>())
        type = INPUT_TYPE_NUMBER;
    else if (state_machine_input->is<rive::StateMachineTrigger>())
        type = INPUT_TYPE_TRIGGER;
    else
        type = INPUT_TYPE_UNKNOWN;

    jobject obj = env->AllocObject(g_StateMachineInputJNI.cls);
    dmDefoldJNI::SetFieldString(env, obj, g_StateMachineInputJNI.name, state_machine_input->name().c_str());
    dmDefoldJNI::SetFieldString(env, obj, g_StateMachineInputJNI.type, type);
    return obj;
}

static jobjectArray CreateStateMachineInputs(JNIEnv* env, const rive::StateMachine* state_machine)
{
    uint32_t num_values = state_machine->inputCount();
    jobjectArray arr = env->NewObjectArray(num_values, g_StateMachineInputJNI.cls, 0);
    for (uint32_t i = 0; i < num_values; ++i)
    {
        jobject o = CreateStateMachineInput(env, state_machine, i);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

static jobject CreateStateMachine(JNIEnv* env, const rive::StateMachine* state_machine)
{
    jobject obj = env->AllocObject(g_StateMachineJNI.cls);

    dmDefoldJNI::SetFieldString(env, obj, g_StateMachineJNI.name, state_machine->name().c_str());

    jobjectArray inputs = CreateStateMachineInputs(env, state_machine);
    dmDefoldJNI::SetFieldObject(env, obj, g_StateMachineJNI.inputs, inputs);
    env->DeleteLocalRef(inputs);
    return obj;
}

static jobjectArray CreateStateMachines(JNIEnv* env, dmRive::RiveFile* rive_file)
{
    const rive::Artboard* artboard = rive_file->m_File->artboard();
    uint32_t count = artboard->stateMachineCount();

    jobjectArray arr = env->NewObjectArray(count, g_StateMachineJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        const rive::StateMachine* state_machine = artboard->stateMachine(i);
        jobject o = CreateStateMachine(env, state_machine);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

static jobjectArray CreateAnimations(JNIEnv* env, dmRive::RiveFile* rive_file)
{
    rive::Artboard* artboard = rive_file->m_File->artboard();
    int num_animations = artboard->animationCount();

    jobjectArray arr = env->NewObjectArray(num_animations, env->FindClass("java/lang/String"), 0);
    for (int i = 0; i < num_animations; ++i)
    {
        rive::LinearAnimation* animation = artboard->animation(i);
        const char* animname = animation->name().c_str();
        env->SetObjectArrayElement(arr, i, env->NewStringUTF(animname));
    }
    return arr;
}

static void UpdateBone(JNIEnv* env, dmRive::RiveBone* bone, jobject obj)
{
    float posX, posY;
    float scaleX, scaleY;
    dmRive::GetBonePos(bone, &posX, &posY);
    dmRive::GetBoneScale(bone, &scaleX, &scaleY);
    float rotation = dmRive::GetBoneRotation(bone);
    float length = dmRive::GetBoneLength(bone);

    dmDefoldJNI::SetFieldFloat(env, obj, g_BoneJNI.posX, posX);
    dmDefoldJNI::SetFieldFloat(env, obj, g_BoneJNI.posY, posY);
    dmDefoldJNI::SetFieldFloat(env, obj, g_BoneJNI.scaleX, scaleX);
    dmDefoldJNI::SetFieldFloat(env, obj, g_BoneJNI.scaleY, scaleY);
    dmDefoldJNI::SetFieldFloat(env, obj, g_BoneJNI.rotation, rotation);
    dmDefoldJNI::SetFieldFloat(env, obj, g_BoneJNI.length, length);
}

static jobject CreateBone(JNIEnv* env, dmRive::RiveBone* bone, jobject parent_obj)
{
    jobject obj = env->AllocObject(g_BoneJNI.cls);

    dmDefoldJNI::SetFieldString(env, obj, g_BoneJNI.name, dmRive::GetBoneName(bone));
    dmDefoldJNI::SetFieldInt(env, obj, g_BoneJNI.index, dmRive::GetBoneIndex(bone));
    dmDefoldJNI::SetFieldObject(env, obj, g_BoneJNI.parent, parent_obj);

    uint32_t num_children = bone->m_Children.Size();
    jobjectArray children = env->NewObjectArray(num_children, g_BoneJNI.cls, 0);
    for (uint32_t i = 0; i < num_children; ++i)
    {
        jobject o = CreateBone(env, bone->m_Children[i], obj);
        env->SetObjectArrayElement(children, i, o);
        env->DeleteLocalRef(o);
    }
    dmDefoldJNI::SetFieldObject(env, obj, g_BoneJNI.children, children);
    env->DeleteLocalRef(children);

    UpdateBone(env, bone, obj);

    return obj;
}

static jobjectArray CreateBones(JNIEnv* env, dmRive::RiveFile* rive_file)
{
    uint32_t count = rive_file->m_Bones.Size();

    dmArray<dmRive::RiveBone*> root_bones;
    root_bones.SetCapacity(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        if (rive_file->m_Bones[i]->m_Parent)
            root_bones.Push(rive_file->m_Bones[i]);
    }

    count = root_bones.Size();
    jobjectArray arr = env->NewObjectArray(count, g_BoneJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject o = CreateBone(env, root_bones[i], 0);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}

static void UpdateJNIRenderData(JNIEnv* env, jobject rive_file_obj, dmRive::RiveFile* rive_file)
{
    dmArray<int> int_indices;
    uint32_t index_count = rive_file->m_IndexBufferData.Size();
    int_indices.SetCapacity(index_count);
    int_indices.SetSize(index_count);
    for (uint32_t i = 0; i < index_count; ++i)
    {
        int_indices[i] = (int)rive_file->m_IndexBufferData[i];
    }

    jobject aabb = dmDefoldJNI::CreateAABB(env, dmVMath::Vector4(rive_file->m_AABB[0], rive_file->m_AABB[1], 0.0f, 0.0f),
                                                dmVMath::Vector4(rive_file->m_AABB[2], rive_file->m_AABB[3], 0.0f, 0.0f));
    dmDefoldJNI::SetFieldObject(env, rive_file_obj, g_RiveFileJNI.aabb, aabb);
    env->DeleteLocalRef(aabb);

    jintArray indices = dmDefoldJNI::CreateIntArray(env, int_indices.Size(), int_indices.Begin());
    dmDefoldJNI::SetFieldObject(env, rive_file_obj, g_RiveFileJNI.indices, indices);
    env->DeleteLocalRef(indices);

    uint32_t num_floats = rive_file->m_VertexBufferData.Size() * sizeof(dmRive::RiveVertex)/sizeof(float);
    jfloatArray vertices = dmDefoldJNI::CreateFloatArray(env, num_floats, (float*)rive_file->m_VertexBufferData.Begin());
    dmDefoldJNI::SetFieldObject(env, rive_file_obj, g_RiveFileJNI.vertices, vertices);
    env->DeleteLocalRef(vertices);

    jobjectArray renderObjects = dmRenderJNI::CreateRenderObjectArray(env, rive_file->m_RenderObjects.Size(), rive_file->m_RenderObjects.Begin());
    dmDefoldJNI::SetFieldObject(env, rive_file_obj, g_RiveFileJNI.renderObjects, renderObjects);
    env->DeleteLocalRef(renderObjects);
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

    jobjectArray animations = CreateAnimations(env, rive_file);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.animations, animations);
    env->DeleteLocalRef(animations);

    jobjectArray stateMachines = CreateStateMachines(env, rive_file);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.stateMachines, stateMachines);
    env->DeleteLocalRef(stateMachines);

    jobjectArray bones = CreateBones(env, rive_file);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.bones, bones);
    env->DeleteLocalRef(bones);

    UpdateJNIRenderData(env, obj, rive_file);
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

void Update(JNIEnv* env, jclass cls, jobject rive_file_obj, jfloat dt, const uint8_t* texture_set_data, uint32_t texture_set_data_length)
{
    dmRive::RiveFile* rive_file = FromObject(env, rive_file_obj);
    if (!rive_file || !rive_file->m_File)
    {
        return;
    }

    dmRive::Update(rive_file, dt, texture_set_data, texture_set_data_length);

    // TODO: Update bone positions

    UpdateJNIRenderData(env, rive_file_obj, rive_file);
}

} // namespace
