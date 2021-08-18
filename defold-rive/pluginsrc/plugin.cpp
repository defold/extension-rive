


// Fix for "error: undefined symbol: __declspec(dllimport) longjmp" from libtess2
#if defined(_MSC_VER)
#include <setjmp.h>
static jmp_buf jmp_buffer;
__declspec(dllexport) int dummyFunc()
{
    int r = setjmp(jmp_buffer);
    if (r == 0) longjmp(jmp_buffer, 1);
    return r;
}
#endif


// Rive includes
#include <artboard.hpp>
#include <file.hpp>
#include <animation/linear_animation_instance.hpp>
#include <animation/linear_animation.hpp>

#include <rive/rive_render_api.h>


// Due to an X11.h issue (Likely Ubuntu 16.04 issue) we include the Rive/C++17 includes first

#include <dmsdk/sdk.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/shared_library.h>
#include <stdio.h>
#include <stdint.h>


typedef rive::File* HRiveFile;

struct RiveBuffer
{
    void*        m_Data;
    unsigned int m_Size;
};

static rive::HBuffer RiveRequestBufferCallback(rive::HBuffer buffer, rive::BufferType type, void* data, unsigned int dataSize, void* userData);
static void          RiveDestroyBufferCallback(rive::HBuffer buffer, void* userData);


// Need to free() the buffer
static uint8_t* ReadFile(const char* path, size_t* file_size) {
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        dmLogError("Failed to read file '%s'", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long _file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* buffer = (uint8_t*)malloc(_file_size);
    if (fread(buffer, 1, _file_size, f) != (size_t) _file_size)
    {
        fclose(f);
        free(buffer);
        return 0;
    }
    fclose(f);

    if (file_size)
        *file_size = _file_size;
    return buffer;
}

// TODO: Make this part optional, via some function setters
namespace rive
{
    HContext g_Ctx = 0;
    RenderPath* makeRenderPath()
    {
        return createRenderPath(g_Ctx);
    }

    RenderPaint* makeRenderPaint()
    {
        return createRenderPaint(g_Ctx);
    }

    HRenderer g_RendererCtx = 0;
}

static void InitRiveContext() {
    if (!rive::g_Ctx) {
        rive::g_Ctx = rive::createContext();
        rive::setRenderMode(rive::g_Ctx, rive::MODE_TESSELLATION);
    }
}

extern "C" DM_DLLEXPORT void TestPrint(const char* name) {
    dmLogInfo("TestPrint: Hello %s!\n", name);
}

extern "C" DM_DLLEXPORT void* RIVE_LoadFromBuffer(void* buffer, size_t buffer_size) {
    InitRiveContext();

    rive::File* file          = 0;
    rive::BinaryReader reader = rive::BinaryReader((uint8_t*)buffer, buffer_size);
    rive::ImportResult result = rive::File::import(reader, &file);

    if (result == rive::ImportResult::success) {
        rive::setRenderMode(rive::g_Ctx, rive::MODE_TESSELLATION);
        rive::setBufferCallbacks(rive::g_Ctx, RiveRequestBufferCallback, RiveDestroyBufferCallback, 0x0);
    } else {
        file = 0;
    }

    return (void*)file;
}

extern "C" DM_DLLEXPORT void* RIVE_LoadFromPath(const char* path) {
    InitRiveContext();

    size_t buffer_size = 0;
    uint8_t* buffer = ReadFile(path, &buffer_size);
    if (!buffer) {
        dmLogError("%s: Failed to read rive file into buffer", __FUNCTION__);
        return 0;
    }

    void* p = RIVE_LoadFromBuffer(buffer, buffer_size);
    free(buffer);
    return p;
}

extern "C" DM_DLLEXPORT void RIVE_Destroy(void* _rive_file) {
    rive::File* file = (rive::File*)_rive_file;
    delete file;
}

extern "C" DM_DLLEXPORT int32_t RIVE_GetNumAnimations(void* _rive_file) {
    if (!_rive_file) {
        dmLogError("%s: File handle is null", __FUNCTION__);
        return 0;
    }

    HRiveFile file = (HRiveFile)_rive_file;
    rive::File* riv = (rive::File*)file;

    rive::Artboard* artboard = riv->artboard();
    return artboard ? artboard->animationCount() : 0;
}

extern "C" DM_DLLEXPORT const char* RIVE_GetAnimation(void* _rive_file, int i) {
    if (!_rive_file) {
        dmLogError("%s: File handle is null", __FUNCTION__);
        return 0;
    }

    HRiveFile file = (HRiveFile)_rive_file;
    rive::File* riv = (rive::File*)file;

    if (!riv) {
        dmLogError("%s: File is invalid: %p", __FUNCTION__, riv);
        return 0;
    }

    rive::Artboard* artboard = riv->artboard();
    if (!artboard) {
        dmLogError("%s: File has no animations", __FUNCTION__);
        return 0;
    }

    if (i < 0 || i >= artboard->animationCount()) {
        dmLogError("%s: Animation index %d is not in range [0, %zu]", __FUNCTION__, i, artboard->animationCount());
        return 0;
    }

    rive::LinearAnimation* animation = artboard->animation(i);
    const char* name = animation->name().c_str();
    return name;
}

static rive::LinearAnimation* FindAnimation(rive::File* riv, const char* name)
{
    rive::Artboard* artboard = riv->artboard();
    int num_animations = artboard->animationCount();
    for (int i = 0; i < num_animations; ++i)
    {
        rive::LinearAnimation* animation = artboard->animation(i);
        const char* animname = animation->name().c_str();
        if (strcmp(name, animname) == 0)
        {
            return animation;
        }
    }
    return 0;
}

static rive::File* ToRiveFile(void* _rive_file, const char* fnname)
{
    if (!_rive_file) {
        dmLogError("%s: File handle is null", fnname);
    }
    HRiveFile file = (HRiveFile)_rive_file;
    rive::File* riv = (rive::File*)file;
    return riv;
}

#define TO_RIVE_FILE(_P_) ToRiveFile(_P_, __FUNCTION__);


extern "C" DM_DLLEXPORT void* RIVE_GetVertices(void* _rive_file, int* num_vertices) {
    rive::File* riv = TO_RIVE_FILE(_rive_file);
    if (!riv) {
        return 0;
    }

    // HRiveFile file = (HRiveFile)_rive_file;
    // rive::File* riv = (rive::File*)file;

    // rive::Artboard* artboard = riv->artboard();
    // return artboard ? artboard->animationCount() : 0;


    // rive::newFrame(renderer);
    // rive::Renderer* rive_renderer = (rive::Renderer*) renderer;


    return 0;
}

extern "C" DM_DLLEXPORT float RIVE_GetArtboardWidth(void* _rive_file) {
    rive::File* riv = TO_RIVE_FILE(_rive_file);
    if (!riv) {
        return 0;
    }

    rive::Artboard* artboard = riv->artboard();
    rive::AABB artboard_bounds = artboard->bounds();
    return artboard_bounds.width();
}

extern "C" DM_DLLEXPORT float RIVE_GetArtboardHeight(void* _rive_file) {
    rive::File* riv = TO_RIVE_FILE(_rive_file);
    if (!riv) {
        return 0;
    }

    rive::Artboard* artboard = riv->artboard();
    rive::AABB artboard_bounds = artboard->bounds();
    return artboard_bounds.height();
}

static rive::HBuffer RiveRequestBufferCallback(rive::HBuffer buffer, rive::BufferType type, void* data, unsigned int dataSize, void* userData)
{
    RiveBuffer* buf = (RiveBuffer*) buffer;
    if (dataSize == 0)
    {
        return 0;
    }

    if (buf == 0)
    {
        buf = new RiveBuffer();
    }

    buf->m_Data = realloc(buf->m_Data, dataSize);
    buf->m_Size = dataSize;
    memcpy(buf->m_Data, data, dataSize);

    return (rive::HBuffer) buf;
}


static void RiveDestroyBufferCallback(rive::HBuffer buffer, void* userData)
{
    RiveBuffer* buf = (RiveBuffer*) buffer;
    if (buf != 0)
    {
        if (buf->m_Data != 0)
        {
            free(buf->m_Data);
        }

        delete buf;
    }
}



