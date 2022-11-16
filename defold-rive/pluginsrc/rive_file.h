
#ifndef DM_RIVE_FILE_H
#define DM_RIVE_FILE_H

#include <dmsdk/dlib/array.h>

#include <common/factory.h>
#include <common/bones.h>       // RiveBone
#include <common/vertices.h>    // RiveVertex
#include <rive/file.hpp>

namespace dmRive {

// struct BoneInteral
// {
//     const char* name;
//     int parent;
//     float posX, posY, rotation, scaleX, scaleY, length;
// };

// struct StateMachineInput
// {
//     const char* name;
//     const char* type;
// };

struct RiveFile
{
    dmRive::DefoldTessRenderer* m_Renderer;
    dmRive::DefoldFactory*      m_Factory;
    rive::File*                 m_File;
    const char*                 m_Path;
    dmArray<dmRive::RiveVertex> m_Vertices;
    dmArray<dmRive::RiveBone*>  m_Roots;
    dmArray<dmRive::RiveBone*>  m_Bones;

    dmArray<int>                    m_IndexBuffer;
    dmArray<dmRive::RiveVertex>     m_VertexBuffer;
    dmArray<dmRender::RenderObject> m_RenderObjects;
};

RiveFile*   LoadFileFromBuffer(const void* buffer, size_t buffer_size, const char* path);
void        DestroyFile(RiveFile* rive_file);
void        SetupBones(RiveFile* file);

} // namespace

#endif
