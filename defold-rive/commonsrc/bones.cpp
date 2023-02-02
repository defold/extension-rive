#include <rive/artboard.hpp>
#include <rive/bones/bone.hpp>

#include <common/bones.h>

#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/log.h>

#if defined(DM_PLATFORM_OSX)

#include <stdio.h>
#include <signal.h>
#include <stdio.h>
#include <signal.h>
#include <execinfo.h>

#endif

namespace dmRive
{
#if 0 // defined(DM_PLATFORM_OSX)

    static const int SIGNAL_MAX = 64;
    static struct sigaction sigdfl[SIGNAL_MAX];
    static int sighandler_installed = 0;

    void OnCrash(int signo)
    {
        const uint32_t max_num_pointers = 128;
        void* pointers[max_num_pointers];

        uint32_t num_pointers = (uint32_t)backtrace(pointers, max_num_pointers);

        char** stacktrace = backtrace_symbols(pointers, num_pointers);
        for (uint32_t i = 0; i < num_pointers; ++i)
        {
            // Write each symbol on a separate line, just like
            // backgrace_symbols_fd would do.
            printf("%2d: %s\n", i, stacktrace[i]?stacktrace[i]:"null");
        }

        free(stacktrace);
    }

    static void Handler(const int signum, siginfo_t *const si, void *const sc)
    {
        // The previous (default) behavior is restored for the signal.
        // Unless this is done first thing in the signal handler we'll
        // be stuck in a signal-handler loop forever.
        sigaction(signum, &sigdfl[signum], NULL);
        OnCrash(signum);
    }

    void InstallOnSignal(int signum)
    {
        assert(signum >= 0 && signum < SIGNAL_MAX);

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = Handler;
        sa.sa_flags = SA_SIGINFO;

        // The current (default) behavior is stored in sigdfl.
        sigaction(signum, &sa, &sigdfl[signum]);
    }

    void InstallSignalHandler()
    {
        if (sighandler_installed)
            return;

        sighandler_installed = 1;

        InstallOnSignal(SIGSEGV);
        InstallOnSignal(SIGBUS);
        InstallOnSignal(SIGTRAP);
        InstallOnSignal(SIGILL);
        InstallOnSignal(SIGABRT);
    }

#else
    void InstallSignalHandler() {}
#endif


static rive::Bone* FindParentBone(rive::Bone* bone)
{
    rive::Component* o = bone;
    while ( (o = o->parent()) ) {
        if (o->is<rive::Bone>()) {
            return o->as<rive::Bone>();
        }
    }
    return 0;
}

void GetAllBones(rive::Artboard* artboard, dmArray<rive::Bone*>* bones)
{
    const std::vector<rive::Core*>& objects = artboard->objects();
    uint32_t num_objects = (uint32_t)objects.size();

    for (uint32_t oi = 0; oi < num_objects; ++oi)
    {
        rive::Core* obj = objects[oi];
        if (obj && obj->is<rive::Bone>()) {
            rive::Bone* bone = obj->as<rive::Bone>();
            if (bones->Full()) {
                bones->OffsetCapacity(16);
            }
            bones->Push(bone);
        }
    }
}

static int FindBoneIndex(rive::Bone* bone, rive::Bone** bones, uint32_t num_bones)
{
    for (int i = 0; i < (int)num_bones; ++i)
    {
        if (bones[i] == bone)
        {
            return i;
        }
    }
    return -1;
}

// There may be multiple roots, and the bones are interleaved in the hierarchy with the rest of the scene (shapes/paths etc)
void BuildBoneHierarchy(rive::Artboard* artboard, dmArray<RiveBone*>* outroots, dmArray<RiveBone*>* outbones)
{
    InstallSignalHandler();

    dmArray<rive::Bone*> bones;
    GetAllBones(artboard, &bones);

    uint32_t num_bones = bones.Size();

    // Create the graph nodes
    outbones->SetCapacity(num_bones);
    outbones->SetSize(num_bones);

    for (int i = 0; i < (int)num_bones; ++i)
    {
        rive::Bone* bone = bones[i];
        rive::Bone* parent = FindParentBone(bone);

        int parent_index = FindBoneIndex(parent, bones.Begin(), num_bones);

        RiveBone* rivebone = new RiveBone;

        (*outbones)[i] = rivebone;

        rivebone->m_Index = i;
        rivebone->m_Bone = bone;
        rivebone->m_Parent = 0;
        rivebone->m_NameHash = dmHashString64(bone->name().c_str());

        if (parent)
        {
            rivebone->m_Parent = (*outbones)[parent_index];

            if (rivebone->m_Parent->m_Children.Full())
            {
                rivebone->m_Parent->m_Children.OffsetCapacity(8);
            }

            rivebone->m_Parent->m_Children.Push(rivebone);
        }
        else
        {
            if (outroots->Full())
            {
                outroots->OffsetCapacity(4);
            }
            outroots->Push(rivebone);
        }
    }
}

void FreeBones(dmArray<RiveBone*>* bones)
{
    for (uint32_t i = 0; i < bones->Size(); ++i)
    {
        delete (*bones)[i];
    }
}

static void DebugPrintRiveBone(RiveBone* bone, uint32_t indent)
{
    const char* spaces = "  ";
    for (uint32_t i = 0; i < indent; ++i)
    {
        printf("%s", spaces);
    }

    printf("%s\n", bone->m_Bone->name().c_str());

    for (uint32_t i = 0; i < bone->m_Children.Size(); ++i)
    {
        DebugPrintRiveBone(bone->m_Children[i], indent + 1);
    }
}

void DebugBoneHierarchy(dmArray<RiveBone*>* roots)
{
    for (uint32_t i = 0; i < roots->Size(); ++i)
    {
        DebugPrintRiveBone((*roots)[i], 0);
    }
}

bool ValidateBoneNames(dmArray<RiveBone*>* bones)
{
    bool status_ok = true;
    uint32_t num_bones = bones->Size();
    for (uint32_t i = 0; i < num_bones; ++i)
    {
        const char* cur_name = (*bones)[i]->m_Bone->name().c_str();
        if (strcmp(cur_name, "") == 0)
        {
            dmLogWarning("Bone %u has no name! Each bone must have a unique, non empty name.", i);
            status_ok = false;
        }

        for (uint32_t j = i+1; j < num_bones; ++j)
        {
            const char* next_name = (*bones)[j]->m_Bone->name().c_str();
            if (strcmp(cur_name, next_name) == 0)
            {
                dmLogWarning("Two bones have the name '%s'! Each bone must have a unique, non empty name.", cur_name);
                status_ok = false;
            }
        }
    }
    return status_ok;
}

int GetBoneIndex(RiveBone* bone)
{
    return bone->m_Index;
}

const char* GetBoneName(RiveBone* bone)
{
    return bone->m_Bone->name().c_str();
}

void GetBonePos(RiveBone* bone, float* x, float* y)
{
    *x = bone->m_Bone->x();
    *y = bone->m_Bone->y();
}

void GetBoneScale(RiveBone* bone, float* sx, float* sy)
{
    *sx = bone->m_Bone->scaleX();
    *sy = bone->m_Bone->scaleY();
}

float GetBoneRotation(RiveBone* bone)
{
    return bone->m_Bone->rotation();
}

float GetBoneLength(RiveBone* bone)
{
    return bone->m_Bone->x();
}

} // namespace
