


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


struct RiveBuffer
{
    void*        m_Data;
    unsigned int m_Size;
};

struct RiveFile
{
    rive::HRenderer m_Renderer; // Separate renderer for multi threading
    rive::File*     m_File;
};

typedef RiveFile* HRiveFile;

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
}

static void InitRiveContext() {
    if (!rive::g_Ctx) {
        rive::g_Ctx = rive::createContext();
        rive::setRenderMode(rive::g_Ctx, rive::MODE_TESSELLATION);
    }
}

static RiveFile* ToRiveFile(void* _rive_file, const char* fnname)
{
    if (!_rive_file) {
        dmLogError("%s: File handle is null", fnname);
    }
    return (RiveFile*)_rive_file;
}

#define TO_RIVE_FILE(_P_) ToRiveFile(_P_, __FUNCTION__);

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

    RiveFile* out = new RiveFile;
    out->m_File = file;
    out->m_Renderer = rive::createRenderer(rive::g_Ctx);

    rive::setContourQuality(out->m_Renderer, 0.8888888888888889f);
    rive::setClippingSupport(out->m_Renderer, true);

    return (void*)out;
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
    RiveFile* file = (RiveFile*)_rive_file;
    delete file->m_File;
    rive::destroyRenderer(file->m_Renderer);
    delete file;
}

extern "C" DM_DLLEXPORT int32_t RIVE_GetNumAnimations(void* _rive_file) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file) {
        return 0;
    }

    rive::Artboard* artboard = file->m_File->artboard();
    return artboard ? artboard->animationCount() : 0;
}

extern "C" DM_DLLEXPORT const char* RIVE_GetAnimation(void* _rive_file, int i) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file) {
        return 0;
    }

    rive::Artboard* artboard = file->m_File->artboard();
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

extern "C" DM_DLLEXPORT int RIVE_GetVertexSize() {
    return (3 + 2 + 4) * sizeof(float);
}

extern "C" DM_DLLEXPORT void RIVE_UpdateVertices(void* _rive_file, float dt) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file) {
        return;
    }

    // calculate the vertices and store in buffers for later retrieval
}

extern "C" DM_DLLEXPORT int RIVE_GetVertexCount(void* _rive_file) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file) {
        return 0;
    }

    return 6;
}

extern "C" DM_DLLEXPORT void* RIVE_GetVertices(void* _rive_file, void* _buffer, size_t buffer_size) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file) {
        return 0;
    }

    float* buffer = (float*)_buffer;

    //int count = RIVE_GetVertexCount(_rive_file);

    float sz = 512.0f;

    rive::Artboard* artboard = file->m_File->artboard();
    rive::AABB bounds = artboard->bounds();
    float minx = bounds.minX;
    float miny = bounds.minY;
    float maxx = bounds.maxX;
    float maxy = bounds.maxY;

// verts [[min-x min-y 0 0 0 1 1 1 1] [max-x min-y 0 0 0 1 1 1 1] [max-x max-y 0 0 0 1 1 1 1]
//        [max-x max-y 0 0 0 1 1 1 1] [min-x max-y 0 0 0 1 1 1 1] [min-x min-y 0 0 0 1 1 1 1]]

    buffer[0] = minx;
    buffer[1] = miny;
    buffer[2] = 0;
    buffer[3] = buffer[4] = 0;
    buffer[5] = buffer[6] = buffer[7] = buffer[8] = 1;
    buffer += 9;

    buffer[0] = maxx;
    buffer[1] = miny;
    buffer[2] = 0;
    buffer[3] = buffer[4] = 0;
    buffer[5] = buffer[6] = buffer[7] = buffer[8] = 1;
    buffer += 9;

    buffer[0] = maxx;
    buffer[1] = maxy;
    buffer[2] = 0;
    buffer[3] = buffer[4] = 0;
    buffer[5] = buffer[6] = buffer[7] = buffer[8] = 1;
    buffer += 9;


    buffer[0] = maxx;
    buffer[1] = maxy;
    buffer[2] = 0;
    buffer[3] = buffer[4] = 0;
    buffer[5] = buffer[6] = buffer[7] = buffer[8] = 1;
    buffer += 9;

    buffer[0] = minx;
    buffer[1] = maxy;
    buffer[2] = 0;
    buffer[3] = buffer[4] = 0;
    buffer[5] = buffer[6] = buffer[7] = buffer[8] = 1;
    buffer += 9;

    buffer[0] = minx;
    buffer[1] = miny;
    buffer[2] = 0;
    buffer[3] = buffer[4] = 0;
    buffer[5] = buffer[6] = buffer[7] = buffer[8] = 1;
    buffer += 9;

    // rive::newFrame(renderer);
    // rive::Renderer* rive_renderer = (rive::Renderer*) renderer;

    // rive::Mat2D transform;
    // rive::Mat2D::identity(transform);

    // rive::Vec2D yflip(1.0f,-1.0f);
    // rive::Mat2D::scale(transform, transform, yflip);
    // rive::setTransform(renderer, transform);

    // HRiveFile file = (HRiveFile)_rive_file;
    // rive::File* riv = (rive::File*)file;

    // rive::Artboard* artboard = riv->artboard();
    // return artboard ? artboard->animationCount() : 0;


    // rive::newFrame(renderer);
    // rive::Renderer* rive_renderer = (rive::Renderer*) renderer;

    // Now that we have a vertex buffer and an index buffer,
    // we need to unpack it inso a vertex buffer only

    return 0;
}

extern "C" DM_DLLEXPORT float RIVE_GetAABBMinX(void* _rive_file) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file) {
        return 0;
    }
    rive::Artboard* artboard = file->m_File->artboard();
    rive::AABB bounds = artboard->bounds();
    return bounds.minX;
}

extern "C" DM_DLLEXPORT float RIVE_GetAABBMinY(void* _rive_file) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file) {
        return 0;
    }
    rive::Artboard* artboard = file->m_File->artboard();
    rive::AABB bounds = artboard->bounds();
    return bounds.minY;
}

extern "C" DM_DLLEXPORT float RIVE_GetAABBMaxX(void* _rive_file) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file) {
        return 0;
    }
    rive::Artboard* artboard = file->m_File->artboard();
    rive::AABB bounds = artboard->bounds();
    return bounds.maxX;
}

extern "C" DM_DLLEXPORT float RIVE_GetAABBMaxY(void* _rive_file) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file) {
        return 0;
    }

    rive::Artboard* artboard = file->m_File->artboard();
    rive::AABB bounds = artboard->bounds();
    return bounds.maxY;
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



