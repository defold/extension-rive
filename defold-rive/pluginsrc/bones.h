#pragma once

#include <dmsdk/dlib/array.h>

namespace rive
{
    class Artboard;
    class Bone;
}

namespace riveplugin
{
    struct RiveBone
    {
        // The graph
        RiveBone* m_Parent;
        dmArray<struct RiveBone*> m_Children;

        // The payload
        rive::Bone* m_Bone;
    };

    void BuildBoneHierarchy(rive::Artboard* artboard, dmArray<RiveBone*>* roots, dmArray<RiveBone*>* bones);
    void FreeBones(dmArray<RiveBone*>* bones);
    void DebugHierarchy(dmArray<RiveBone*>* roots);

    // Makes sure the bone names are unique
    bool ValidateBoneNames(dmArray<RiveBone*>* bones);
}