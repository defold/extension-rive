
#include "rive_file.h"

#include "common/atlas.h"
#include "common/tess_renderer.h"

#include <dmsdk/ddf/ddf.h>
#include <dmsdk/dlib/log.h>

#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/animation/linear_animation.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/state_machine.hpp>
#include <rive/animation/state_machine_instance.hpp>

#include <gamesys/texture_set_ddf.h>

namespace dmRive
{

RiveFile* LoadFileFromBuffer(const void* buffer, size_t buffer_size, const char* path)
{
    dmRive::DefoldFactory* factory = new dmRive::DefoldFactory();

    // Creates DefoldRenderImage with a hashed name for each image resource
    dmRive::AtlasNameResolver atlas_resolver = dmRive::AtlasNameResolver();

    rive::Span<uint8_t> data((uint8_t*)buffer, buffer_size);

    rive::ImportResult result;
    std::unique_ptr<rive::File> file = rive::File::import(data,
                                                    factory,
                                                    &result,
                                                    &atlas_resolver);

    if (result != rive::ImportResult::success) {
        //file = 0;
        dmLogError("Failed to load rive file '%s'", path);
        delete factory;
        return 0;
    }

    RiveFile* out = new RiveFile;
    out->m_Path = 0;
    out->m_File = 0;

    if (file) {
        out->m_Path = strdup(path);
        out->m_File = file.release();
        out->m_Factory = factory;
        out->m_Renderer = new DefoldTessRenderer;

        out->m_ArtboardInstance = out->m_File->artboardAt(0);
        out->m_ArtboardInstance->advance(0.0f);

        dmRive::SetupBones(out);

        PlayAnimation(out, 0);
        Update(out, 0.0f, 0, 0);
    }

    return out;
}

static void DeleteRenderConstants(RiveFile* rive_file)
{
    for (uint32_t i = 0; i < rive_file->m_RenderConstants.Size(); ++i)
    {
        dmRender::DeleteNamedConstantBuffer(rive_file->m_RenderConstants[i]);
    }
}

void DestroyFile(RiveFile* rive_file)
{
    if (rive_file->m_File)
    {
        free((void*)rive_file->m_Path);

        dmRive::FreeBones(&rive_file->m_Bones);

        rive_file->m_ArtboardInstance.reset();
        rive_file->m_AnimationInstance.reset();
        rive_file->m_StateMachineInstance.reset();

        delete rive_file->m_Factory;
        delete rive_file->m_Renderer;
    }

    DeleteRenderConstants(rive_file);

    delete rive_file;
}

void SetupBones(RiveFile* file)
{
    file->m_Roots.SetSize(0);
    file->m_Bones.SetSize(0);

    rive::Artboard* artboard = file->m_File->artboard();
    if (!artboard) {
        return;
    }

    dmRive::BuildBoneHierarchy(artboard, &file->m_Roots, &file->m_Bones);
}

void PlayAnimation(RiveFile* rive_file, int index)
{
    if (index < 0 || index >= rive_file->m_ArtboardInstance->animationCount()) {
        return;
    }
    rive_file->m_AnimationInstance = rive_file->m_ArtboardInstance->animationAt(index);
    rive_file->m_AnimationInstance->inputCount();

    rive_file->m_AnimationInstance->time(rive_file->m_AnimationInstance->startSeconds());
    rive_file->m_AnimationInstance->loopValue((int)rive::Loop::loop);
    rive_file->m_AnimationInstance->direction(1);
}

template<typename T>
static void EnsureCapacity(dmArray<T>& a, uint32_t size)
{
    if (a.Capacity() < size)
    {
        a.SetCapacity(size);
    }
}

template<typename T>
static void EnsureCapacityAndSize(dmArray<T>& a, uint32_t size)
{
    EnsureCapacity(a, size);
    a.SetSize(size);
}

// Generate the indices/vertices and the RenderObjects
static void Render(RiveFile* rive_file)
{
    dmRive::DefoldTessRenderer* renderer = rive_file->m_Renderer;
    rive::AABB bounds = rive_file->m_ArtboardInstance->bounds();
    rive::Mat2D viewTransform = rive::computeAlignment(rive::Fit::contain,
                                                       rive::Alignment::center,
                                                       rive::AABB(0, 0, bounds.maxX-bounds.minX, bounds.maxY-bounds.minY),
                                                       bounds);
    renderer->save();

    rive::Mat2D transform;

    // Rive is using a different coordinate system that defold,
    // we have to adhere to how our projection matrixes are
    // constructed so we flip the renderer on the y axis here
    rive::Vec2D yflip(1.0f,-1.0f);
    transform = transform.scale(yflip);
    renderer->transform(viewTransform * transform);

    renderer->align(rive::Fit::none,
        rive::Alignment::center,
        rive::AABB(-bounds.width(), -bounds.height(),
        bounds.width(), bounds.height()),
        bounds);

    if (rive_file->m_StateMachineInstance) {
        rive_file->m_StateMachineInstance->draw(renderer);
    } else if (rive_file->m_AnimationInstance) {
        rive_file->m_AnimationInstance->draw(renderer);
    } else {
        rive_file->m_ArtboardInstance->draw(renderer);
    }

    renderer->restore();


    rive_file->m_AABB[0] = -bounds.width() * 0.5f;
    rive_file->m_AABB[1] = -bounds.height() * 0.5f;
    rive_file->m_AABB[2] = bounds.width() * 0.5f;
    rive_file->m_AABB[3] = bounds.height() * 0.5f;

    // **************************************************************
    DeleteRenderConstants(rive_file);
    rive_file->m_RenderConstants.SetSize(0);

    uint32_t ro_count         = 0;
    uint32_t vertex_count     = 0;
    uint32_t index_count      = 0;

    dmRive::DrawDescriptor* draw_descriptors;
    renderer->getDrawDescriptors(&draw_descriptors, &ro_count);


    for (int i = 0; i < ro_count; ++i)
    {
        vertex_count += draw_descriptors[i].m_VerticesCount;
        index_count += draw_descriptors[i].m_IndicesCount;
    }

    // Make sure we have enough room for new vertex/index data
    dmArray<dmRive::RiveVertex> &vertex_buffer = rive_file->m_VertexBufferData;
    dmArray<uint16_t> &index_buffer = rive_file->m_IndexBufferData;

    EnsureCapacityAndSize<>(vertex_buffer, vertex_count);
    EnsureCapacityAndSize<>(index_buffer, index_count);

    RiveVertex* vb_begin = vertex_buffer.Begin();
    //RiveVertex* vb_end = vb_begin;

    uint16_t* ix_begin = index_buffer.Begin();
    //uint16_t* ix_end   = ix_begin;

    EnsureCapacityAndSize<>(rive_file->m_RenderObjects, ro_count);
    EnsureCapacityAndSize<>(rive_file->m_RenderConstants, ro_count);

    // We always start from scratch here
    uint32_t ro_offset = 0;
    memset(rive_file->m_RenderConstants.Begin(), 0, sizeof(dmRender::HNamedConstantBuffer)*ro_count);

    dmRender::RenderObject* render_objects = rive_file->m_RenderObjects.Begin() + ro_offset;
    dmRender::HNamedConstantBuffer* render_constants = rive_file->m_RenderConstants.Begin() + ro_offset;

    // We set the material directly from the editor
    //dmRender::HMaterial material = GetMaterial(first, resource);

    dmhash_t constant_names[4] = {UNIFORM_PROPERTIES, UNIFORM_GRADIENT_LIMITS, UNIFORM_COLOR, UNIFORM_STOPS};
    //dmRender::HConstant material_constants[4];
    dmRenderDDF::MaterialDesc::ConstantType material_constant_types[4];

    // Currently, the dmRender::GetMaterialProgramConstant isn't part of the dmSdk
    material_constant_types[0] =
    material_constant_types[1] =
    material_constant_types[2] =
    material_constant_types[3] = dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER;

    // ro_count = 0;
    uint32_t index_offset = 0;
    uint32_t vertex_offset = 0;
    RiveVertex* vb_write = vb_begin;
    uint16_t* ix_write = ix_begin;

    for (int i = 0; i < ro_count; ++i)
    {
        dmRive::DrawDescriptor& draw_desc = draw_descriptors[i];
        dmRender::RenderObject& ro = render_objects[i];

        memset(&ro.m_StencilTestParams, 0, sizeof(ro.m_StencilTestParams));
        ro.m_StencilTestParams.Init();
        ro.Init();

        ro.m_VertexDeclaration = 0;//rive_file->m_VertexDeclaration;
        ro.m_VertexBuffer      = 0;//rive_file->m_VertexBuffer;
        ro.m_IndexBuffer       = 0,//rive_file->m_IndexBuffer;
        ro.m_Material          = 0;//material;
        // While the data in the index buffer currently is uint16_t, we will
        // convert it into an int array for Java.
        ro.m_VertexStart       = index_offset * sizeof(uint16_t);
        //ro.m_VertexStart       = index_offset * sizeof(uint32_t);
        ro.m_VertexCount       = draw_desc.m_IndicesCount; // num indices (3 for a triangle)
        ro.m_IndexType         = dmGraphics::TYPE_UNSIGNED_INT;
        ro.m_PrimitiveType     = dmGraphics::PRIMITIVE_TRIANGLES;

        //printf("Ro: %d, vx %d ix %d\n", i, draw_desc.m_VerticesCount, draw_desc.m_IndicesCount);

        dmRive::CopyVertices(draw_desc, vertex_offset, vb_write, ix_write);

        index_offset += draw_desc.m_IndicesCount;
        vertex_offset += draw_desc.m_VerticesCount;
        ix_write += draw_desc.m_IndicesCount;
        vb_write += draw_desc.m_VerticesCount;

        const dmRive::FsUniforms fs_uniforms = draw_desc.m_FsUniforms;
        const dmRive::VsUniforms vs_uniforms = draw_desc.m_VsUniforms;
        // const int MAX_STOPS = 4;
        // const int MAX_COLORS = 16;
        // const int num_stops = fs_uniforms.stopCount > 1 ? fs_uniforms.stopCount : 1;

        dmVMath::Vector4 properties((float)fs_uniforms.fillType, (float) fs_uniforms.stopCount, 0.0f, 0.0f);
        dmVMath::Vector4 gradient_limits(vs_uniforms.gradientStart.x, vs_uniforms.gradientStart.y, vs_uniforms.gradientEnd.x, vs_uniforms.gradientEnd.y);

        render_constants[i] = dmRender::NewNamedConstantBuffer();
        ro.m_ConstantBuffer = render_constants[i];

        // Follows the size+order of constant_names
        dmVMath::Vector4* constant_values[4] = {
            (dmVMath::Vector4*) &properties,
            (dmVMath::Vector4*) &gradient_limits,
            (dmVMath::Vector4*) fs_uniforms.colors,
            (dmVMath::Vector4*) fs_uniforms.stops
        };
        uint32_t constant_value_counts[4] = {
            1,
            1,
            sizeof(fs_uniforms.colors) / sizeof(dmVMath::Vector4),
            sizeof(fs_uniforms.stops) / sizeof(dmVMath::Vector4)
        };
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(constant_names); ++i)
        {
            dmRender::SetNamedConstant(ro.m_ConstantBuffer, constant_names[i], constant_values[i], constant_value_counts[i], material_constant_types[i]);
        }

        dmRive::ApplyDrawMode(ro, draw_desc.m_DrawMode, draw_desc.m_ClipIndex);

        memcpy(&ro.m_WorldTransform, &vs_uniforms.world, sizeof(vs_uniforms.world));
    }
}

static dmGameSystemDDF::TextureSet* LoadAtlasFromBuffer(void* buffer, size_t buffer_size)
{
    dmGameSystemDDF::TextureSet* texture_set_ddf;
    dmDDF::Result e  = dmDDF::LoadMessage(buffer, (uint32_t)buffer_size, &texture_set_ddf);

    if (e != dmDDF::RESULT_OK)
    {
        return 0;
    }
    return texture_set_ddf;
}

void Update(RiveFile* rive_file, float dt, const uint8_t* texture_set_data, uint32_t texture_set_data_length)
{
    if (!rive_file->m_File)
    {
        return;
    }

    dmGameSystemDDF::TextureSet* texture_set_ddf = 0;
    Atlas* atlas = 0;

    if (texture_set_data && texture_set_data_length > 0)
    {
        texture_set_ddf = LoadAtlasFromBuffer((void*) texture_set_data, texture_set_data_length);
        if (!texture_set_ddf)
        {
            dmLogError("Couldn't load atlas for rive scene %s", rive_file->m_Path);
            return;
        }

        atlas = dmRive::CreateAtlas(texture_set_ddf);
        rive_file->m_Renderer->SetAtlas(atlas);
    }

    rive_file->m_Renderer->reset();
    if (rive_file->m_StateMachineInstance)
    {
        rive_file->m_StateMachineInstance->advanceAndApply(dt);
    }
    else if (rive_file->m_AnimationInstance)
    {
        rive_file->m_AnimationInstance->advanceAndApply(dt);
    }
    else {
        rive_file->m_ArtboardInstance->advance(dt);
    }
    Render(rive_file);

    if (atlas)
    {
        dmRive::DestroyAtlas(atlas);
        dmDDF::FreeMessage(texture_set_ddf);
    }
}


} // namespace
