#pragma once

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>

namespace rive
{
    class Artboard;
    class Bone;
}

namespace dmRive
{
    struct RiveBone
    {
        // The graph
        RiveBone* m_Parent;
        dmArray<struct RiveBone*> m_Children;
        int m_Index;
        dmhash_t m_NameHash;

        // The payload
        rive::Bone* m_Bone;
    };

    void GetAllBones(rive::Artboard* artboard, dmArray<rive::Bone*>* bones);
    void BuildBoneHierarchy(rive::Artboard* artboard, dmArray<RiveBone*>* roots, dmArray<RiveBone*>* bones);
    void FreeBones(dmArray<RiveBone*>* bones);
    void DebugBoneHierarchy(dmArray<RiveBone*>* roots);

    // Makes sure the bone names are unique
    bool ValidateBoneNames(dmArray<RiveBone*>* bones);

    int GetBoneIndex(RiveBone* bone);
    void GetBonePos(RiveBone* bone, float* x, float* y);
    void GetBoneScale(RiveBone* bone, float* sx, float* sy);
    float GetBoneRotation(RiveBone* bone);
    float GetBoneLength(RiveBone* bone);
    const char* GetBoneName(RiveBone* bone);
}
