
#ifndef DM_RIVE_FILE_H
#define DM_RIVE_FILE_H

#include <dmsdk/dlib/array.h>

#include <common/factory.h>
#include <common/bones.h>       // RiveBone
#include <common/vertices.h>    // RiveVertex
#include <rive/file.hpp>

namespace rive
{
    class StateMachineInstance;
    class LinearAnimationInstance;
}

namespace dmRive {

struct RiveFile
{
    dmRive::DefoldTessRenderer* m_Renderer;
    dmRive::DefoldFactory*      m_Factory;
    rive::File*                 m_File;
    const char*                 m_Path;
    dmArray<dmRive::RiveVertex> m_Vertices;
    dmArray<dmRive::RiveBone*>  m_Roots;
    dmArray<dmRive::RiveBone*>  m_Bones;
    float                       m_AABB[4];

    dmArray<uint16_t>                       m_IndexBufferData;
    dmArray<dmRive::RiveVertex>             m_VertexBufferData;
    dmArray<dmRender::RenderObject>         m_RenderObjects;
    dmArray<dmRender::HNamedConstantBuffer> m_RenderConstants; // 1:1 mapping with the render objects

    std::unique_ptr<rive::ArtboardInstance>         m_ArtboardInstance;
    std::unique_ptr<rive::LinearAnimationInstance>  m_AnimationInstance;
    std::unique_ptr<rive::StateMachineInstance>     m_StateMachineInstance;
};

RiveFile*   LoadFileFromBuffer(const void* buffer, size_t buffer_size, const char* path);
void        DestroyFile(RiveFile* rive_file);
void        SetupBones(RiveFile* file);

void        PlayAnimation(RiveFile* rive_file, int index);
void        Update(RiveFile* rive_file, float dt, const uint8_t* texture_set_data, uint32_t texture_set_data_length);

} // namespace

#endif
