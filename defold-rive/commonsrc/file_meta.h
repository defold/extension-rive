// Copyright 2025 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_RIVE_COMMONSRC_FILE_META_H
#define DM_RIVE_COMMONSRC_FILE_META_H

#include "common/file.h"

#include <string>
#include <vector>

namespace dmRive
{
    class MetadataListener : public rive::CommandQueue::FileListener
    {
    public:
        MetadataListener();
        explicit MetadataListener(RiveFile* file);

        void onFileLoaded(const rive::FileHandle file, uint64_t request_id) override;
        void onFileError(const rive::FileHandle file, uint64_t request_id, std::string error) override;

        bool m_FileLoaded;
        bool m_FileError;
#if defined(DM_RIVE_FILE_META_DATA)
        void onArtboardsListed(const rive::FileHandle file,
                               uint64_t request_id,
                               std::vector<std::string> artboardNames) override;
        void onViewModelsListed(const rive::FileHandle file,
                                uint64_t request_id,
                                std::vector<std::string> viewModelNames) override;
        void onViewModelInstanceNamesListed(const rive::FileHandle file,
                                            uint64_t request_id,
                                            std::string viewModelName,
                                            std::vector<std::string> instanceNames) override;
        void onViewModelPropertiesListed(
            const rive::FileHandle file,
            uint64_t request_id,
            std::string viewModelName,
            std::vector<ViewModelPropertyData> properties) override;
        void onViewModelEnumsListed(const rive::FileHandle file,
                                    uint64_t request_id,
                                    std::vector<rive::ViewModelEnum> enums) override;

        bool m_ArtboardsListed;
        bool m_ViewModelsListed;
        bool m_ViewModelEnumsListed;
        uint32_t m_ViewModelPropertiesListedCount;
        uint32_t m_ViewModelPropertiesExpectedCount;
        uint32_t m_ViewModelInstanceNamesListedCount;
        uint32_t m_ViewModelInstanceNamesExpectedCount;
#endif
        RiveFile* m_File;
    };

#if defined(DM_RIVE_FILE_META_DATA)
    void InitFileMetaData(RiveFile* file);
    void DestroyFileMetaData(RiveFile* file);
    void RequestMetaData(RiveFile* file, rive::rcp<rive::CommandQueue> queue);
    rive::ArtboardHandle InstantiateArtboardNamedWithMeta(RiveFile* file, const char* artboard, rive::rcp<rive::CommandQueue> queue);
    rive::ArtboardHandle InstantiateDefaultArtboardWithMeta(RiveFile* file, rive::rcp<rive::CommandQueue> queue);
    void DebugPrintFileMetaData(const RiveFile* file);
#endif
} // namespace dmRive

#endif // DM_RIVE_COMMONSRC_FILE_META_H
