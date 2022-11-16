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
#include "rive_file.h"

#include <jni.h>

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/align.h>
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
    jfieldID    parent;     // int index into array
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
    jfieldID    vertices;   // array of floats. Each vertex is: (x, y, u, v)
    jfieldID    indices;    // array of indices into the vertices array. triangle list. 3 indices define a triangle

    jfieldID    animations;    // array of strings
    jfieldID    stateMachines; // array of state machines
    jfieldID    bones;         // array of bones
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
        GET_FLD_TYPESTR(parent, "I");
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

static jobject CreateBone(JNIEnv* env, dmRive::RiveBone* bone)
{
    jobject obj = env->AllocObject(g_BoneJNI.cls);

    dmDefoldJNI::SetFieldString(env, obj, g_BoneJNI.name, dmRive::GetBoneName(bone));

    dmRive::RiveBone* parent = bone->m_Parent;
    int parent_index = parent ? dmRive::GetBoneIndex(parent) : -1;
    dmDefoldJNI::SetFieldInt(env, obj, g_BoneJNI.parent, parent_index);

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

    return obj;
}

static jobjectArray CreateBones(JNIEnv* env, dmRive::RiveFile* rive_file)
{
    uint32_t count = rive_file->m_Bones.Size();
    jobjectArray arr = env->NewObjectArray(count, g_BoneJNI.cls, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        jobject o = CreateBone(env, rive_file->m_Bones[i]);
        env->SetObjectArrayElement(arr, i, o);
        env->DeleteLocalRef(o);
    }
    return arr;
}


static jobject CreateRiveFile(JNIEnv* env, dmRive::RiveFile* rive_file)
{
    printf("dmRiveJNI::%s -> %p\n", __FUNCTION__, rive_file);

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

    jintArray indices = dmDefoldJNI::CreateIntArray(env, 0, 0);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.indices, indices);
    env->DeleteLocalRef(indices);

    jfloatArray vertices = dmDefoldJNI::CreateFloatArray(env, 0, 0);
    dmDefoldJNI::SetFieldObject(env, obj, g_RiveFileJNI.vertices, vertices);
    env->DeleteLocalRef(vertices);

    return obj;
}

// ******************************************************************************************************************

jobject LoadFileFromBuffer(JNIEnv* env, jclass cls, const char* path, const uint8_t* data, uint32_t data_length)
{
    dmRive::RiveFile* rive_file = dmRive::LoadFileFromBuffer(data, data_length, path);
    printf("MAWE: %s: %p\n", __FUNCTION__, rive_file);

    jobject obj = CreateRiveFile(env, rive_file);
    if (!obj)
    {
        dmLogError("Failed to create JNI representation of a RiveFile");
    }
    return obj;
}

void DestroyFile(JNIEnv* env, jclass cls, jobject rive_file_obj)
{
    jlong pointer = env->GetLongField(rive_file_obj, g_RiveFileJNI.pointer);
    dmRive::RiveFile* rive_file = FromLong(pointer);
    printf("MAWE: %s: %p\n", __FUNCTION__, rive_file);
    if (!rive_file)
        return;

    dmRive::DestroyFile(rive_file);
}

void Update(JNIEnv* env, jclass cls, jobject rive_file, jfloat dt)
{
    // Update animations

    // TODO: Update bone position etc

    // Update render objects

//     uint32_t ro_count;
//     dmRive::DrawDescriptor* draw_descriptors;
//     renderer->getDrawDescriptors(&draw_descriptors, &ro_count);

}

void GetRenderObjects(JNIEnv* env, jclass cls, jobject rive_file)
{

}

} // namespace
