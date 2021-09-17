


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
#include <rive/animation/state_machine.hpp>
#include <rive/animation/state_machine_bool.hpp>
#include <rive/animation/state_machine_number.hpp>
#include <rive/animation/state_machine_trigger.hpp>

#include <riverender/rive_render_api.h>


// Due to an X11.h issue (Likely Ubuntu 16.04 issue) we include the Rive/C++17 includes first

#include <dmsdk/sdk.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/shared_library.h>
#include <dmsdk/dlib/static_assert.h>
#include <stdio.h>
#include <stdint.h>

#include <common/bones.h>
#include <common/vertices.h>

// We map these to ints on the java side (See Rive.java)
DM_STATIC_ASSERT(sizeof(dmGraphics::CompareFunc) == sizeof(int), Invalid_struct_size);
DM_STATIC_ASSERT(sizeof(dmGraphics::StencilOp) == sizeof(int), Invalid_struct_size);


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

struct BoneInteral
{
    const char* name;
    int parent;
    float posX, posY, rotation, scaleX, scaleY, length;
};

struct StateMachineInput
{
    const char* name;
    const char* type;
};

struct RiveFile
{
    rive::HRenderer     m_Renderer; // Separate renderer for multi threading
    rive::File*         m_File;
    const char*         m_Path;
    dmArray<RivePluginVertex> m_Vertices;
    dmArray<dmRive::RiveBone*>  m_Roots;
    dmArray<dmRive::RiveBone*>  m_Bones;

    dmArray<int>                    m_IndexBuffer;
    dmArray<dmRive::RiveVertex>     m_VertexBuffer;
    dmArray<dmRive::RenderObject>   m_RenderObjects;
};

typedef RiveFile* HRiveFile;

static void          UpdateRenderData(RiveFile* file);


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
        rive::setRenderMode(rive::g_Ctx, rive::MODE_STENCIL_TO_COVER);
        rive::setBufferCallbacks(rive::g_Ctx, dmRive::RequestBufferCallback, dmRive::DestroyBufferCallback, 0x0);
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

#define CHECK_FILE_RETURN_VALUE(_P_, _VALUE_) \
    if (!(_P_) || !(_P_)->m_File) { \
        return (_VALUE_); \
    }

#define CHECK_ARTBOARD_RETURN(_P_) \
    if (!(_P_)) { \
        dmLogError("%s: File has no artboard", __FUNCTION__); \
        return 0; \
    }

#define CHECK_ARTBOARD_RETURN_VALUE(_P_, _VALUE_) \
    if (!(_P_)) { \
        dmLogError("%s: File has no artboard", __FUNCTION__); \
        return (_VALUE_); \
    }


static void SetupBones(RiveFile* file)
{
    file->m_Roots.SetSize(0);
    file->m_Bones.SetSize(0);

    rive::Artboard* artboard = file->m_File->artboard();
    if (!artboard) {
        return;
    }

    dmRive::BuildBoneHierarchy(artboard, &file->m_Roots, &file->m_Bones);

    //dmRive::DebugBoneHierarchy(&file->m_Roots);

    bool bones_ok = dmRive::ValidateBoneNames(&file->m_Bones);
    if (!bones_ok) {
        dmLogWarning("Failed to validate bones for %s", file->m_Path);
        dmRive::FreeBones(&file->m_Bones);
        file->m_Bones.SetSize(0);
        file->m_Roots.SetSize(0);
    }
}


extern "C" DM_DLLEXPORT void* RIVE_LoadFromBuffer(void* buffer, size_t buffer_size, const char* path) {
    InitRiveContext();

    rive::File* file          = 0;
    rive::BinaryReader reader = rive::BinaryReader((uint8_t*)buffer, buffer_size);
    rive::ImportResult result = rive::File::import(reader, &file);

    if (result != rive::ImportResult::success) {
        file = 0;
    }

    RiveFile* out = new RiveFile;
    out->m_Path = 0;
    out->m_File = 0;
    out->m_Renderer = 0;

    if (file) {
        out->m_Path = strdup(path);
        out->m_File = file;
        out->m_Renderer = rive::createRenderer(rive::g_Ctx);

        rive::setContourQuality(out->m_Renderer, 0.8888888888888889f);
        rive::setClippingSupport(out->m_Renderer, true);

        SetupBones(out);
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

    void* p = RIVE_LoadFromBuffer(buffer, buffer_size, path);
    free(buffer);
    return p;
}

extern "C" DM_DLLEXPORT void RIVE_Destroy(void* _rive_file) {
    RiveFile* file = (RiveFile*)_rive_file;
    if (file == 0)
    {
        return;
    }

    printf("Destroying %s\n", file->m_Path ? file->m_Path : "null");
    fflush(stdout);

    if (file->m_Renderer) {
        rive::destroyRenderer(file->m_Renderer);
    }
    free((void*)file->m_Path);

    dmRive::FreeBones(&file->m_Bones);

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

extern "C" DM_DLLEXPORT void RIVE_GetBoneInternal(void* _rive_file, int i, BoneInteral* outbone) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file) {
        return;
    }

    if (i < 0 || i >= (int)file->m_Bones.Size()) {
        dmLogError("%s: Bone index %d is not in range [0, %u]", __FUNCTION__, i, (uint32_t)file->m_Bones.Size());
        return;
    }

    dmRive::RiveBone* rivebone = file->m_Bones[i];
    outbone->name = dmRive::GetBoneName(rivebone);

    dmRive::RiveBone* parent = rivebone->m_Parent;
    outbone->parent = parent ? dmRive::GetBoneIndex(parent) : -1;

    dmRive::GetBonePos(rivebone, &outbone->posX, &outbone->posY);
    dmRive::GetBoneScale(rivebone, &outbone->scaleX, &outbone->scaleY);
    outbone->rotation = dmRive::GetBoneRotation(rivebone);
    outbone->length = dmRive::GetBoneLength(rivebone);
}

extern "C" DM_DLLEXPORT int RIVE_GetNumChildBones(void* _rive_file, int bone_index)
{
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);

    if (bone_index < 0 || bone_index >= (int)file->m_Bones.Size()) {
        dmLogError("%s: Bone index %d is not in range [0, %u]", __FUNCTION__, bone_index, (uint32_t)file->m_Bones.Size());
        return 0;
    }

    dmRive::RiveBone* bone = file->m_Bones[bone_index];
    return (int)bone->m_Children.Size();
}

extern "C" DM_DLLEXPORT int RIVE_GetChildBone(void* _rive_file, int bone_index, int child_index)
{
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);

    if (bone_index < 0 || bone_index >= (int)file->m_Bones.Size()) {
        dmLogError("%s: Bone index %d is not in range [0, %u)", __FUNCTION__, bone_index, (uint32_t)file->m_Bones.Size());
        return -1;
    }

    dmRive::RiveBone* bone = file->m_Bones[bone_index];

    if (child_index < 0 || child_index >= (int)bone->m_Children.Size()) {
        dmLogError("%s: Child index %d is not in range [0, %u)", __FUNCTION__, child_index, (uint32_t)bone->m_Children.Size());
        return -1;
    }

    dmRive::RiveBone* child = bone->m_Children[child_index];
    return dmRive::GetBoneIndex(child);
}

///////////////////////////////////////////////////////////////////////////////
// State machines

extern "C" DM_DLLEXPORT int RIVE_GetNumStateMachines(void* _rive_file)
{
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN_VALUE(file, 0);

    rive::Artboard* artboard = file->m_File->artboard();
    CHECK_ARTBOARD_RETURN_VALUE(artboard, 0);

    return (int)artboard->stateMachineCount();
}

extern "C" DM_DLLEXPORT const char* RIVE_GetStateMachineName(void* _rive_file, int index)
{
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN_VALUE(file, "null");

    rive::Artboard* artboard = file->m_File->artboard();
    CHECK_ARTBOARD_RETURN_VALUE(artboard, "null");

    if (index < 0 || index >= (int)artboard->stateMachineCount()) {
        dmLogError("%s: State machine index %d is not in range [0, %d)", __FUNCTION__, index, (int)artboard->stateMachineCount());
        return "null";
    }

    rive::StateMachine* state_machine = artboard->stateMachine(index);
    return state_machine->name().c_str();
}

extern "C" DM_DLLEXPORT int RIVE_GetNumStateMachineInputs(void* _rive_file, int index)
{
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN_VALUE(file, 0);

    rive::Artboard* artboard = file->m_File->artboard();
    CHECK_ARTBOARD_RETURN_VALUE(artboard, 0);

    if (index < 0 || index >= (int)artboard->stateMachineCount()) {
        dmLogError("%s: State machine index %d is not in range [0, %d)", __FUNCTION__, index, (int)artboard->stateMachineCount());
        return 0;
    }

    rive::StateMachine* state_machine = artboard->stateMachine(index);
    return (int)state_machine->inputCount();
}

static const char* INPUT_TYPE_BOOL="bool";
static const char* INPUT_TYPE_NUMBER="number";
static const char* INPUT_TYPE_TRIGGER="trigger";
static const char* INPUT_TYPE_UNKNOWN="unknown";

extern "C" DM_DLLEXPORT int RIVE_GetStateMachineInput(void* _rive_file, int index, int input_index, StateMachineInput* input)
{
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN_VALUE(file, 0);

    rive::Artboard* artboard = file->m_File->artboard();
    CHECK_ARTBOARD_RETURN_VALUE(artboard, 0);

    if (index < 0 || index >= (int)artboard->stateMachineCount()) {
        dmLogError("%s: State machine index %d is not in range [0, %d)", __FUNCTION__, index, (int)artboard->stateMachineCount());
        return 0;
    }

    rive::StateMachine* state_machine = artboard->stateMachine(index);

    if (input_index < 0 || input_index >= (int)state_machine->inputCount()) {
        dmLogError("%s: State machine index %d is not in range [0, %d)", __FUNCTION__, input_index, (int)state_machine->inputCount());
        return 0;
    }

    const rive::StateMachineInput* state_machine_input = state_machine->input(input_index);
    if (state_machine_input == 0) {
        printf("state_machine_input == 0\n");
        fflush(stdout);
    }
    assert(state_machine_input != 0);

    if (input == 0) {
        printf("input == 0\n");
        fflush(stdout);
    }

    input->name = state_machine_input->name().c_str();

    if (state_machine_input->is<rive::StateMachineBool>())
        input->type = INPUT_TYPE_BOOL;
    else if (state_machine_input->is<rive::StateMachineNumber>())
        input->type = INPUT_TYPE_NUMBER;
    else if (state_machine_input->is<rive::StateMachineTrigger>())
        input->type = INPUT_TYPE_TRIGGER;
    else
        input->type = INPUT_TYPE_UNKNOWN;

    return 1;
}

///////////////////////////////////////////////////////////////////////////////



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


extern "C" DM_DLLEXPORT void RIVE_UpdateVertices(void* _rive_file, float dt) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    if (!file || !file->m_File) {
        return;
    }

    rive::Artboard* artboard = file->m_File->artboard();
    if (!artboard) {
        return;
    }


    rive::newFrame(file->m_Renderer);
    file->m_Renderer->save();

    artboard->advance(dt);
    artboard->draw(file->m_Renderer);
    file->m_Renderer->restore();

    // calculate the vertices and store in buffers for later retrieval
    if (file->m_Vertices.Empty()) {
        GenerateAABB(file);
        //GenerateVertices(file);
    }
    UpdateRenderData(file); // Update the draw call list
}

extern "C" DM_DLLEXPORT int RIVE_GetVertexCount(void* _rive_file) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);
    return file->m_Vertices.Size();
}

extern "C" DM_DLLEXPORT void* RIVE_GetVertices(void* _rive_file, void* _buffer, size_t buffer_size) {
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);

    size_t sz = sizeof(RivePluginVertex) * file->m_Vertices.Size();
    if (sz > buffer_size) {
        dmLogWarning("The output vertex buffer (%u bytes) is smaller than the current buffer (%u bytes)", (uint32_t)buffer_size, (uint32_t)sz);

        sz = buffer_size;
    }

    memcpy(_buffer, (void*)file->m_Vertices.Begin(), sz);

    return 0;
}

extern "C" DM_DLLEXPORT dmRive::RiveVertex* RIVE_GetVertexBufferData(void* _rive_file, int* pcount)
{
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);
    *pcount = (int)file->m_VertexBuffer.Size();
    return file->m_VertexBuffer.Begin();
}

extern "C" DM_DLLEXPORT int* RIVE_GetIndexBufferData(void* _rive_file, int* pcount)
{
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);
    *pcount = (int)file->m_IndexBuffer.Size();
    return file->m_IndexBuffer.Begin();
}

extern "C" DM_DLLEXPORT dmRive::RenderObject* RIVE_GetRenderObjectData(void* _rive_file, int* pcount)
{
    RiveFile* file = TO_RIVE_FILE(_rive_file);
    CHECK_FILE_RETURN(file);
    *pcount = (int)file->m_RenderObjects.Size();
    return file->m_RenderObjects.Begin();
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

// *******************************************************************************************************

// Used when processing the Rive draw events, to produce draw calls
struct RiveEventsDrawcallContext
{
    dmRive::RiveVertex*     m_VertexBuffer;
    int*                    m_IndexBuffer;
    dmRive::RenderObject*   m_RenderObjects;
    //dmRiveDDF::RiveModelDesc::BlendMode m_BlendMode;
};

static void RiveEventCallback_Plugin(dmRive::RiveEventsContext* ctx)
{
    static const dmhash_t UNIFORM_COLOR = dmHashString64("color");
    static const dmhash_t UNIFORM_COVER = dmHashString64("cover");

    RiveEventsDrawcallContext* plugin_ctx = (RiveEventsDrawcallContext*)ctx->m_UserContext;

    switch(ctx->m_Event.m_Type)
    {
    case rive::EVENT_DRAW_STENCIL:
        {
            dmRive::RenderObject& ro = plugin_ctx->m_RenderObjects[ctx->m_Index];
            ro.Init();

            ro.m_VertexStart       = ctx->m_IndexOffsetBytes; // byte offset
            ro.m_VertexCount       = ctx->m_IndexCount;
            ro.m_SetStencilTest    = 1;
            ro.m_SetFaceWinding    = 1;
            ro.m_FaceWindingCCW    = ctx->m_FaceWinding == dmGraphics::FACE_WINDING_CCW;

            dmRive::SetStencilDrawState(&ro.m_StencilTestParams, ctx->m_Event.m_IsClipping, ctx->m_ClearClippingFlag);

            ro.AddConstant(UNIFORM_COVER, dmVMath::Vector4(0.0f, 0.0f, 0.0f, 0.0f));

            dmRive::Mat2DToMat4(ctx->m_Event.m_TransformWorld, ro.m_WorldTransform);
        }
        break;

    case rive::EVENT_DRAW_COVER:
        {
            dmRive::RenderObject& ro = plugin_ctx->m_RenderObjects[ctx->m_Index];
            ro.Init();
            ro.m_VertexStart       = ctx->m_IndexOffsetBytes; // byte offset
            ro.m_VertexCount       = ctx->m_IndexCount;
            ro.m_SetStencilTest    = 1;

            ro.m_SetFaceWinding    = 1;
            ro.m_FaceWindingCCW    = ctx->m_FaceWinding == dmGraphics::FACE_WINDING_CCW;

            dmRive::SetStencilCoverState(&ro.m_StencilTestParams, ctx->m_Event.m_IsClipping, ctx->m_IsApplyingClipping);

            if (!ctx->m_IsApplyingClipping)
            {
                // GetBlendFactorsFromBlendMode(plugin_ctx->m_BlendMode, &ro.m_SourceBlendFactor, &ro.m_DestinationBlendFactor);
                // ro.m_SetBlendFactors = 1;

                const rive::PaintData draw_entry_paint = rive::getPaintData(ctx->m_Paint);
                const float* color                     = &draw_entry_paint.m_Colors[0];

                ro.AddConstant(UNIFORM_COLOR, dmVMath::Vector4(color[0], color[1], color[2], color[3]));
            }

            // If we are fullscreen-covering, we don't transform the vertices
            float no_projection = (float) ctx->m_Event.m_IsClipping && ctx->m_IsApplyingClipping;

            ro.AddConstant(UNIFORM_COVER, dmVMath::Vector4(no_projection, 0.0f, 0.0f, 0.0f));

            dmRive::Mat2DToMat4(ctx->m_Event.m_TransformWorld, ro.m_WorldTransform);
        }
        break;

    default:
        break;
    }
}

template<typename T>
static T* AdjustArraySize(dmArray<T>& array, uint32_t count)
{
    if (array.Remaining() < count)
    {
        array.OffsetCapacity(count - array.Remaining());
    }

    T* p = array.End();
    array.SetSize(array.Size()+count); // no reallocation, since we've preallocated
    return p;
}

static void UpdateRenderData(RiveFile* file)
{
    uint32_t ro_count         = 0;
    uint32_t vertex_count     = 0;
    uint32_t index_count      = 0;
    dmRive::GetRiveDrawParams(rive::g_Ctx, file->m_Renderer, vertex_count, index_count, ro_count);

    if (!ro_count || !vertex_count || !index_count)
    {
        return;
    }

    // reset the buffers (corresponding to the dmRender::RENDER_LIST_OPERATION_BEGIN)
    // we don't need to share the index/vertex buffer with another instance so
    file->m_RenderObjects.SetSize(0);
    file->m_VertexBuffer.SetSize(0);
    file->m_IndexBuffer.SetSize(0);

    // Make sure we have enough free render objects
    dmRive::RenderObject*   ro_begin = AdjustArraySize(file->m_RenderObjects, ro_count);
    dmRive::RiveVertex*     vb_begin = AdjustArraySize(file->m_VertexBuffer, vertex_count);
    int*                    ix_begin = AdjustArraySize(file->m_IndexBuffer, index_count);

    RiveEventsDrawcallContext plugin_ctx;

    plugin_ctx.m_VertexBuffer = vb_begin;
    plugin_ctx.m_IndexBuffer = ix_begin;
    plugin_ctx.m_RenderObjects = ro_begin;

    uint32_t num_ros_used = ProcessRiveEvents(rive::g_Ctx, file->m_Renderer, vb_begin, ix_begin, RiveEventCallback_Plugin, &plugin_ctx);

    dmLogWarning("ro_count: %u  vertex_count: %u  index_count: %u", ro_count, vertex_count, index_count)
}

