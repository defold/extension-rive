// Copyright 2025 The Defold Foundation
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

#include "texture.h"

#include <stdlib.h>

#include <dmsdk/dlib/log.h>
#include <dmsdk/graphics/graphics_vulkan.h>

#if defined(DM_RIVE_USE_OPENGL) && !defined(DM_HEADLESS)
    #include <glad/gles2.h>

    static void DrainOpenGLErrors()
    {
        while (glGetError() != GL_NO_ERROR)
        {
        }
    }
#endif

namespace dmRive
{
    static void ResetOutput(TexturePixels* out)
    {
        if (!out)
        {
            return;
        }
        out->m_Data = 0;
        out->m_DataSize = 0;
        out->m_Width = 0;
        out->m_Height = 0;
        out->m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    }

    static bool IsOpenGLAdapter(dmGraphics::AdapterFamily adapter)
    {
        return adapter == dmGraphics::ADAPTER_FAMILY_OPENGL ||
               adapter == dmGraphics::ADAPTER_FAMILY_OPENGLES;
    }

    static bool IsVulkanAdapter(dmGraphics::AdapterFamily adapter)
    {
        return adapter == dmGraphics::ADAPTER_FAMILY_VULKAN;
    }

    bool ReadPixelsBackBuffer(TexturePixels* out)
    {
        ResetOutput(out);
        if (!out)
        {
            return false;
        }

        dmGraphics::HContext context = dmGraphics::GetInstalledContext();
        if (!context)
        {
            return false;
        }

        uint32_t width = dmGraphics::GetWindowWidth(context);
        uint32_t height = dmGraphics::GetWindowHeight(context);
        if (width == 0 || height == 0 || width > UINT16_MAX || height > UINT16_MAX)
        {
            return false;
        }

        uint32_t data_size = width * height * 4;
        uint8_t* data = (uint8_t*)malloc(data_size);
        if (!data)
        {
            return false;
        }

        dmGraphics::ReadPixels(context, 0, 0, width, height, data, data_size);

        out->m_Data = data;
        out->m_DataSize = data_size;
        out->m_Width = (uint16_t)width;
        out->m_Height = (uint16_t)height;
        out->m_Format = dmGraphics::TEXTURE_FORMAT_BGRA8U;
        return true;
    }

#if defined(DM_RIVE_USE_OPENGL) && !defined(DM_HEADLESS)
    static bool ReadPixelsOpenGL(uint16_t width, uint16_t height, void* native_handle, TexturePixels* out)
    {
        uint32_t data_size = (uint32_t)width * (uint32_t)height * 4;
        uint8_t* data = (uint8_t*)malloc(data_size);
        if (!data)
        {
            return false;
        }

        DrainOpenGLErrors();

        const GLuint* texture_id_ptr = (const GLuint*)native_handle;
        GLuint texture_id = texture_id_ptr ? *texture_id_ptr : 0;
        if (texture_id == 0)
        {
            dmLogWarning("ReadPixels(OpenGL): native texture id was 0");
            DrainOpenGLErrors();
            free(data);
            return false;
        }

        GLint prev_fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

        GLint prev_pack = 0;
        glGetIntegerv(GL_PACK_ALIGNMENT, &prev_pack);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            dmLogWarning("ReadPixels(OpenGL): framebuffer incomplete (status=0x%x)", status);
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
            glDeleteFramebuffers(1, &fbo);
            glPixelStorei(GL_PACK_ALIGNMENT, prev_pack);
            DrainOpenGLErrors();
            free(data);
            return false;
        }

        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
        GLenum read_err = glGetError();

        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
        glDeleteFramebuffers(1, &fbo);
        glPixelStorei(GL_PACK_ALIGNMENT, prev_pack);

        if (read_err != GL_NO_ERROR)
        {
            dmLogWarning("ReadPixels(OpenGL): glReadPixels failed (err=0x%x)", read_err);
            DrainOpenGLErrors();
            free(data);
            return false;
        }

        out->m_Data = data;
        out->m_DataSize = data_size;
        out->m_Width = width;
        out->m_Height = height;
        out->m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
        return true;
    }
#endif

    bool ReadPixels(dmGraphics::HTexture texture, TexturePixels* out)
    {
        ResetOutput(out);
        if (!out || texture == 0)
        {
            return false;
        }

        dmGraphics::HContext context = dmGraphics::GetInstalledContext();
        if (!context)
        {
            return false;
        }

        dmGraphics::AdapterFamily adapter = dmGraphics::GetInstalledAdapterFamily();

        if (IsVulkanAdapter(adapter))
        {
            (void)texture;
            return ReadPixelsBackBuffer(out);
        }

        if (!IsOpenGLAdapter(adapter))
        {
            return false;
        }

        dmGraphics::TextureType type = dmGraphics::GetTextureType(context, texture);
        if (type != dmGraphics::TEXTURE_TYPE_2D &&
            type != dmGraphics::TEXTURE_TYPE_TEXTURE_2D)
        {
            return false;
        }

        uint16_t width = dmGraphics::GetTextureWidth(context, texture);
        uint16_t height = dmGraphics::GetTextureHeight(context, texture);
        if (width == 0 || height == 0)
        {
            return false;
        }

        void* native_handle = 0;
        if (dmGraphics::GetTextureHandle(texture, &native_handle) != dmGraphics::HANDLE_RESULT_OK ||
            native_handle == 0)
        {
            dmLogWarning("ReadPixels(OpenGL): failed to get native texture handle");
            return false;
        }

#if defined(DM_RIVE_USE_OPENGL) && !defined(DM_HEADLESS)
        return ReadPixelsOpenGL(width, height, native_handle, out);
#else
        (void)native_handle;
        return false;
#endif
    }

    void FreePixels(TexturePixels* pixels)
    {
        if (!pixels)
        {
            return;
        }

        if (pixels->m_Data)
        {
            free(pixels->m_Data);
        }

        ResetOutput(pixels);
    }
}
