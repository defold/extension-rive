#ifndef _RIVE_RELATIVE_LOCAL_ASSET_RESOLVER_HPP_
#define _RIVE_RELATIVE_LOCAL_ASSET_RESOLVER_HPP_

#include "rive/file_asset_loader.hpp"
#include "rive/assets/file_asset.hpp"
#include <cstdio>
#include <string>
#include "rive/span.hpp"

namespace rive
{
class FileAsset;
class Factory;

/// An implementation of FileAssetLoader which finds the assets in a local
/// path relative to the original .riv file looking for them.
class RelativeLocalAssetLoader : public FileAssetLoader
{
private:
    std::string m_Path;
    Factory* m_Factory;

public:
    RelativeLocalAssetLoader(std::string filename, Factory* factory) : m_Factory(factory)
    {
        std::size_t finalSlash = filename.rfind('/');

        if (finalSlash != std::string::npos)
        {
            m_Path = filename.substr(0, finalSlash + 1);
        }
    }

    bool loadContents(FileAsset& asset, Span<const uint8_t> inBandBytes) override
    {
        std::string filename = m_Path + asset.uniqueFilename();
        FILE* fp = fopen(filename.c_str(), "rb");

        fseek(fp, 0, SEEK_END);
        const size_t length = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        uint8_t* bytes = new uint8_t[length];
        if (fread(bytes, 1, length, fp) == length)
        {
            asset.decode(Span<const uint8_t>(bytes, length), m_Factory);
        }
        delete[] bytes;
        return true;
    }
};
} // namespace rive
#endif
