


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
#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/linear_animation.hpp>

#include <riverender/rive_render_api.h>


// Due to an X11.h issue (Likely Ubuntu 16.04 issue) we include the Rive/C++17 includes first

#include <dmsdk/sdk.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/shared_library.h>
#include <stdio.h>
#include <stdint.h>

#include "bones.h"

struct RiveBuffer
{
    void*        m_Data;
    unsigned int m_Size;
};

struct RiveInternalVertex
{
    float x, y;
};

struct RivePluginVertex
{
    float x, y, z;
    float u, v;
    float r, g, b, a;
};

struct Vec4
{
    float x, y, z, w;
};

struct AABB
{
    float minX, minY, maxX, maxY;
};

struct Matrix4
{
    float m[16];
};

// struct Command
// {
//     rive::DrawBuffers   m_Buffers;
//     Vec4                m_Color;
//     Matrix4x4           m_Transform;
//     uint32_t            m_IsClipping : 1;
// };

struct RiveFile
{
    rive::HRenderer     m_Renderer; // Separate renderer for multi threading
    rive::File*         m_File;
    dmArray<RivePluginVertex> m_Vertices;
    dmArray<riveplugin::RiveBone*>  m_Roots;
    dmArray<riveplugin::RiveBone*>  m_Bones;
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

#define CHECK_FILE_RETURN(_P_) \
    if (!(_P_) || !(_P_)->m_File) { \
        return 0; \
    }

#define CHECK_ARTBOARD_RETURN(_P_) \
    if (!(_P_)) { \
        dmLogError("%s: File has no artboard", __FUNCTION__); \
        return 0; \
    }


static void UpdateBones(RiveFile* file)
{
    rive::Artboard* artboard = file->m_File->artboard();
    if (!artboard) {
        return;
    }

    file->m_Roots.SetSize(0);
    file->m_Bones.SetSize(0);
    riveplugin::BuildBoneHierarchy(artboard, &file->m_Roots, &file->m_Bones);

    riveplugin::DebugHierarchy(&file->m_Roots);
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

    RiveFile* out = new RiveFile;
    if (file) {
        out->m_File = file;
        out->m_Renderer = rive::createRenderer(rive::g_Ctx);

        rive::setContourQuality(out->m_Renderer, 0.8888888888888889f);
        rive::setClippingSupport(out->m_Renderer, true);

        UpdateBones(out);

        bool bones_ok = riveplugin::ValidateBoneNames(&out->m_Bones);
        if (!bones_ok) {
            riveplugin::FreeBones(&out->m_Bones);
            out->m_Bones.SetSize(0);
            out->m_Roots.SetSize(0);
        }
    }

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
    if (file->m_Renderer) {
        rive::destroyRenderer(file->m_Renderer);
    }
    delete file->m_File;
    delete file;
}

extern "C" DM_DLLEXPORT int32_t RIVE_GetNumAnimations(void* _rive_file) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);

    rive::Artboard* artboard = file->m_File->artboard();
    CHECK_ARTBOARD_RETURN(artboard);

    return artboard ? artboard->animationCount() : 0;
}

extern "C" DM_DLLEXPORT const char* RIVE_GetAnimation(void* _rive_file, int i) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);

    rive::Artboard* artboard = file->m_File->artboard();
    CHECK_ARTBOARD_RETURN(artboard);

    if (i < 0 || i >= artboard->animationCount()) {
        dmLogError("%s: Animation index %d is not in range [0, %zu]", __FUNCTION__, i, artboard->animationCount());
        return 0;
    }

    rive::LinearAnimation* animation = artboard->animation(i);
    const char* name = animation->name().c_str();
    return name;
}


extern "C" DM_DLLEXPORT int32_t RIVE_GetNumBones(void* _rive_file) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);

    return file->m_Bones.Size();
}


extern "C" DM_DLLEXPORT const char* RIVE_GetBone(void* _rive_file, int i) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);

    if (i < 0 || i >= (int)file->m_Bones.Size()) {
        dmLogError("%s: Bone index %d is not in range [0, %u]", __FUNCTION__, i, (uint32_t)file->m_Bones.Size());
        return 0;
    }

    return GetBoneName(file->m_Bones[i]);
}


// static rive::LinearAnimation* FindAnimation(rive::File* riv, const char* name)
// {
//     rive::Artboard* artboard = riv->artboard();
//     int num_animations = artboard->animationCount();
//     for (int i = 0; i < num_animations; ++i)
//     {
//         rive::LinearAnimation* animation = artboard->animation(i);
//         const char* animname = animation->name().c_str();
//         if (strcmp(name, animname) == 0)
//         {
//             return animation;
//         }
//     }
//     return 0;
// }

extern "C" DM_DLLEXPORT int RIVE_GetVertexSize() {
    return sizeof(RivePluginVertex);
}

static void GenerateAABB(RiveFile* file);
static void GenerateVertices(RiveFile* file);


extern "C" DM_DLLEXPORT void RIVE_UpdateVertices(void* _rive_file, float dt) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file || !file->m_File) {
        return;
    }

    rive::Artboard* artboard = file->m_File->artboard();
    if (!artboard) {
        return;
    }

    artboard->advance(dt);

    // calculate the vertices and store in buffers for later retrieval
    if (file->m_Vertices.Empty()) {
        GenerateAABB(file);
        //GenerateVertices(file);
    }
}

extern "C" DM_DLLEXPORT int RIVE_GetVertexCount(void* _rive_file) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);
    return file->m_Vertices.Size();
}

extern "C" DM_DLLEXPORT void* RIVE_GetVertices(void* _rive_file, void* _buffer, size_t buffer_size) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);

    float* buffer = (float*)_buffer;

    size_t sz = sizeof(RivePluginVertex) * file->m_Vertices.Size();
    if (sz > buffer_size) {
        dmLogWarning("The output vertex buffer (%u bytes) is smaller than the current buffer (%u bytes)", (uint32_t)buffer_size, (uint32_t)sz);

        sz = buffer_size;
    }

    memcpy(_buffer, (void*)file->m_Vertices.Begin(), sz);

    return 0;
}

extern "C" DM_DLLEXPORT void RIVE_GetAABBInternal(void* _rive_file, AABB* aabb)
{
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file) {
        return;
    }
    rive::Artboard* artboard = file->m_File->artboard();
    if (!artboard) {
        return;
    }

    rive::AABB bounds = artboard->bounds();
    float cx = (bounds.maxX - bounds.minX) * 0.5f;
    float cy = (bounds.maxY - bounds.minY) * 0.5f;
    aabb->minX = bounds.minX - cx;
    aabb->minY = bounds.minY - cy;
    aabb->maxX = bounds.maxX - cx;
    aabb->maxY = bounds.maxY - cy;
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

static void GenerateAABB(RiveFile* file)
{
    if (file->m_Vertices.Capacity() < 6)
        file->m_Vertices.SetCapacity(6);
    file->m_Vertices.SetSize(6);

    rive::Artboard* artboard = file->m_File->artboard();
    rive::AABB bounds = artboard->bounds();

    float cx = (bounds.maxX - bounds.minX) * 0.5f;
    float cy = (bounds.maxY - bounds.minY) * 0.5f;

    float minx = bounds.minX - cx;
    float miny = bounds.minY - cy;
    float maxx = bounds.maxX - cx;
    float maxy = bounds.maxY - cy;

// verts [[min-x min-y 0 0 0 1 1 1 1] [max-x min-y 0 0 0 1 1 1 1] [max-x max-y 0 0 0 1 1 1 1]
//        [max-x max-y 0 0 0 1 1 1 1] [min-x max-y 0 0 0 1 1 1 1] [min-x min-y 0 0 0 1 1 1 1]]

    RivePluginVertex* v = file->m_Vertices.Begin();

    v->x = minx;
    v->y = miny;
    v->z = 0;
    v->u = v->v = 0;
    v->r = v->g = v->b = v->a = 1;
    ++v;

    v->x = maxx;
    v->y = miny;
    v->z = 0;
    v->u = v->v = 0;
    v->r = v->g = v->b = v->a = 1;
    ++v;

    v->x = maxx;
    v->y = maxy;
    v->z = 0;
    v->u = v->v = 0;
    v->r = v->g = v->b = v->a = 1;
    ++v;


    v->x = maxx;
    v->y = maxy;
    v->z = 0;
    v->u = v->v = 0;
    v->r = v->g = v->b = v->a = 1;
    ++v;

    v->x = minx;
    v->y = maxy;
    v->z = 0;
    v->u = v->v = 0;
    v->r = v->g = v->b = v->a = 1;
    ++v;

    v->x = minx;
    v->y = miny;
    v->z = 0;
    v->u = v->v = 0;
    v->r = v->g = v->b = v->a = 1;
    ++v;
}

static inline void Mat2DToMat4(const rive::Mat2D m2, dmVMath::Matrix4& m4)
{
    m4[0][0] = m2[0];
    m4[0][1] = m2[1];
    m4[0][2] = 0.0;
    m4[0][3] = 0.0;

    m4[1][0] = m2[2];
    m4[1][1] = m2[3];
    m4[1][2] = 0.0;
    m4[1][3] = 0.0;

    m4[2][0] = 0.0;
    m4[2][1] = 0.0;
    m4[2][2] = 1.0;
    m4[2][3] = 0.0;

    m4[3][0] = m2[4];
    m4[3][1] = m2[5];
    m4[3][2] = 0.0;
    m4[3][3] = 1.0;
}

static void GenerateVertices(RiveFile* file)
{
    file->m_Vertices.SetSize(0); // Clear the vertices

    rive::newFrame(file->m_Renderer);

    rive::Mat2D transform;
    rive::Mat2D::identity(transform);

    rive::Vec2D yflip(1.0f,-1.0f);
    rive::Mat2D::scale(transform, transform, yflip);
    rive::setTransform(file->m_Renderer, transform);

    rive::Artboard* artboard = file->m_File->artboard();
    rive::AABB bounds = artboard->bounds();
    float cx = (bounds.maxX - bounds.minX) * 0.5f;
    float cy = (bounds.maxY - bounds.minY) * 0.5f;

    rive::HContext rive_ctx = rive::g_Ctx;
    rive::HRenderer renderer = file->m_Renderer;
    rive::Renderer* rive_renderer = (rive::Renderer*) renderer;

    rive_renderer->align(rive::Fit::none, rive::Alignment::center, bounds, bounds);
    rive_renderer->save();

    // Triggers the buffer allocation callbacks
    artboard->draw(rive_renderer);
    rive_renderer->restore();

    // Now we also need to iterate over the paint events

    rive::HRenderPaint paint = 0;
    bool is_paint_dirty = false;
    bool is_applying_clipping = false;
    bool is_clipping = false;

    float z = 0.0f;

    for (int ei = 0; ei < rive::getDrawEventCount(renderer); ++ei)
    {
        const rive::PathDrawEvent evt = rive::getDrawEvent(renderer, ei);

        switch(evt.m_Type)
        {
            case rive::EVENT_SET_PAINT:
            {
                if (evt.m_Paint != 0 && paint != evt.m_Paint)
                {
                    paint = evt.m_Paint;
                    is_paint_dirty = true;
                }

            } break;
            case rive::EVENT_DRAW:
            {
                rive::DrawBuffers buffers = rive::getDrawBuffers(rive_ctx, renderer, evt.m_Path);
                RiveBuffer* vxBuffer      = (RiveBuffer*) buffers.m_VertexBuffer;
                RiveBuffer* ixBuffer      = (RiveBuffer*) buffers.m_IndexBuffer;

                if (vxBuffer != 0 && ixBuffer != 0)
                {
                    RiveInternalVertex* vx_data_ptr = (RiveInternalVertex*) vxBuffer->m_Data;
                    int*                ix_data_ptr = (int*) ixBuffer->m_Data;
                    uint32_t vx_count = vxBuffer->m_Size / sizeof(RiveInternalVertex);
                    uint32_t ix_count = ixBuffer->m_Size / sizeof(int);

                    if ((file->m_Vertices.Size() + ix_count) > file->m_Vertices.Capacity())
                    {
                        file->m_Vertices.OffsetCapacity(ix_count);
                    }

                    dmVMath::Matrix4 transform;
                    Mat2DToMat4(evt.m_TransformWorld, transform);

                    const float white[] = {1, 1, 1, 0.0f};
                    const float* color = white;

                    if (is_paint_dirty && !is_applying_clipping)
                    {
                        is_paint_dirty = false;

                        const rive::PaintData draw_entry_paint = rive::getPaintData(paint);
                        color = &draw_entry_paint.m_Colors[0];
                    } else {
                        // For the MVP, we want something that resembles the scene, for easier authoring.
                        // So we skip the clipping parts for now
                        continue;
                    }

                    for (int ix = 0; ix < ix_count; ++ix)
                    {
                        int index = ix_data_ptr[ix];

                        RiveInternalVertex rive_p = vx_data_ptr[index];

                        dmVMath::Point3 localp(rive_p.x, rive_p.y, 0);
                        dmVMath::Vector4 worldp = transform * localp;

                        RivePluginVertex vtx;
                        vtx.x = worldp.getX() - cx;
                        vtx.y = worldp.getY() + cy;
                        vtx.z = z;
                        vtx.u = vtx.v = 0;

                        vtx.r = color[0];
                        vtx.g = color[1];
                        vtx.b = color[2];
                        vtx.a = color[3];

                        // // Since some vertices are extremely out of bounds
                        // float border = 100.0f;
                        // if (vtx.x >= (bounds.minX-border) && vtx.x <= (bounds.maxX+border) &&
                        //     vtx.y >= (bounds.minY-border) && vtx.y <= (bounds.maxY+border))
                        // {
                            file->m_Vertices.Push(vtx);
                        //}
                    }

                    z += 0.001f;
                }

            } break;
            case rive::EVENT_CLIPPING_BEGIN:
                is_clipping = true;
                is_applying_clipping = true;
                //clear_clipping_flag = 1;
                break;
            case rive::EVENT_CLIPPING_END:
                is_applying_clipping = false;
                break;
            case rive::EVENT_CLIPPING_DISABLE:
                is_applying_clipping = false;
                is_clipping = false;
                break;
            default:
                //dmLogWarning("Unknown render paint event: %d", evt.m_Type);
                break;
        }
    }
}

