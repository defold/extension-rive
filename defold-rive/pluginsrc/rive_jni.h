
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

#ifndef DM_RIVE_JNI_H
#define DM_RIVE_JNI_H

#include <dmsdk/dlib/shared_library.h>

#include <stdint.h>
#include "jni/jni.h"

namespace dmRiveJNI
{
    void InitializeJNITypes(JNIEnv* env);
    void FinalizeJNITypes(JNIEnv* env);

    jobject LoadFileFromBuffer(JNIEnv* env, jclass cls, const char* path, const uint8_t* data, uint32_t data_length);
    void    DestroyFile(JNIEnv* env, jclass cls, jobject rive_file);
    void    Update(JNIEnv* env, jclass cls, jobject rive_file, jfloat dt, const uint8_t* texture_set_data, uint32_t texture_set_data_length);
}


#endif // DM_RIVE_JNI_H
