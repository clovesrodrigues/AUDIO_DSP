//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "TPcids.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace CV {

struct SoundFontFileInfo
{
    std::filesystem::path path {};
    std::string fileName {};
};

class SoundFontFolderScanner
{
public:
    static constexpr std::size_t kMaxSoundFontFiles = 256;

    static std::filesystem::path findPluginBundleRoot (std::filesystem::path moduleOrBundlePath)
    {
        if (moduleOrBundlePath.empty())
            return {};

        std::error_code error;
        if (std::filesystem::is_regular_file (moduleOrBundlePath, error))
            moduleOrBundlePath = moduleOrBundlePath.parent_path ();

        for (auto current = moduleOrBundlePath; !current.empty(); current = current.parent_path())
        {
            if (equalsIgnoreCase (current.extension().string(), ".vst3"))
                return current;

            if (current == current.parent_path())
                break;
        }

        return {};
    }

    static std::filesystem::path resolveSiblingSoundFontsFolder (const std::filesystem::path& moduleOrBundlePath)
    {
        const auto bundleRoot = findPluginBundleRoot (moduleOrBundlePath);
        if (!bundleRoot.empty())
            return bundleRoot.parent_path() / kCVGMInstrumentLiteSoundFontsFolderName;

        if (!moduleOrBundlePath.empty())
            return moduleOrBundlePath.parent_path() / kCVGMInstrumentLiteSoundFontsFolderName;

        return std::filesystem::path (kCVGMInstrumentLiteSoundFontsFolderName);
    }

    static std::filesystem::path resolveDataFolder (const std::filesystem::path& soundFontsFolder)
    {
        return soundFontsFolder / kCVGMInstrumentLiteDataFolderName;
    }

    static std::vector<SoundFontFileInfo> scanSoundFonts (const std::filesystem::path& soundFontsFolder)
    {
        std::vector<SoundFontFileInfo> files;
        std::error_code error;
        if (!std::filesystem::exists (soundFontsFolder, error) ||
            !std::filesystem::is_directory (soundFontsFolder, error))
            return files;

        std::filesystem::directory_iterator iterator (soundFontsFolder, error);
        const std::filesystem::directory_iterator end {};
        while (!error && iterator != end && files.size() < kMaxSoundFontFiles)
        {
            const auto& entry = *iterator;
            if (entry.is_regular_file (error) && isSoundFontFile (entry.path()))
            {
                files.push_back ({entry.path(), entry.path().filename().string()});
            }

            iterator.increment (error);
        }

        std::sort (files.begin(), files.end(), [] (const SoundFontFileInfo& left, const SoundFontFileInfo& right) {
            return toLowerAscii (left.fileName) < toLowerAscii (right.fileName);
        });

        return files;
    }

    static bool isSoundFontFile (const std::filesystem::path& path)
    {
        return equalsIgnoreCase (path.extension().string(), ".sf2");
    }

private:
    static std::string toLowerAscii (std::string value)
    {
        std::transform (value.begin(), value.end(), value.begin(), [] (unsigned char character) {
            if (character >= 'A' && character <= 'Z')
                return static_cast<char> (character - 'A' + 'a');
            return static_cast<char> (character);
        });
        return value;
    }

    static bool equalsIgnoreCase (std::string left, std::string right)
    {
        return toLowerAscii (std::move (left)) == toLowerAscii (std::move (right));
    }
};

} // namespace CV
