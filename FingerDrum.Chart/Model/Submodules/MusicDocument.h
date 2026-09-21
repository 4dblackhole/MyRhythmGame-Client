#pragma once
#include "Model/Submodules/ChartTypes.h"

namespace finger_drum::chart
{
    struct MusicDocument
    {
        int version{1};
        std::filesystem::path sourcePath;
        std::filesystem::path audioFile;
        std::vector<std::string> names;
        std::vector<std::string> artists;
        std::vector<std::string> tags;
    };
} // namespace finger_drum::chart
