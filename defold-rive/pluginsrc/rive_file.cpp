
#include "rive_file.h"

#include "common/atlas.h"
#include "common/tess_renderer.h"

#include <dmsdk/dlib/log.h>

#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/linear_animation.hpp>
#include <rive/animation/state_machine.hpp>

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
    out->m_Factory = factory;
    out->m_Renderer = new DefoldTessRenderer;

    if (file) {
        out->m_Path = strdup(path);
        out->m_File = file.release();
        dmLogError("MAWE RENDERER IS NOT IMPLEMENTED YET! %s %d", __FUNCTION__, __LINE__);
        // out->m_Renderer = rive::createRenderer(rive::g_Ctx);

        // rive::setContourQuality(out->m_Renderer, 0.8888888888888889f);
        // rive::setClippingSupport(out->m_Renderer, true);

        dmRive::SetupBones(out);
    }

    dmLogWarning("MAWE: %s: %p", __FUNCTION__, out);
    return out;
}

void DestroyFile(RiveFile* rive_file)
{
    dmLogWarning("MAWE: %s: %p", __FUNCTION__, rive_file);

    free((void*)rive_file->m_Path);

    dmRive::FreeBones(&rive_file->m_Bones);

    delete rive_file->m_Factory;
    delete rive_file->m_Renderer;
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

    //dmRive::DebugBoneHierarchy(&file->m_Roots);

    // bool bones_ok = dmRive::ValidateBoneNames(&file->m_Bones);
    // if (!bones_ok) {
    //     dmLogWarning("Failed to validate bones for %s", file->m_Path);
    //     dmRive::FreeBones(&file->m_Bones);
    //     file->m_Bones.SetSize(0);
    //     file->m_Roots.SetSize(0);
    // }
}

} // namespace
