#pragma once

#include "Texts/TextCatalog.h"

#include <array>
#include <string_view>

namespace finger_drum::texts
{
    struct MusicSelectTextSet
    {
        std::wstring_view all;
        std::wstring_view personalRecord;
        std::wstring_view noRecords;
        std::wstring_view backgroundPreview;
        std::wstring_view noImage;
        std::wstring_view noSongSelected;
        std::wstring_view difficulty;
        std::wstring_view creator;
        std::wstring_view emptyDetails;
        std::wstring_view searchSongs;
        std::wstring_view songCountFormat;
        std::array<std::wstring_view, 3> sortModes;
        std::wstring_view browserHint;
        std::wstring_view optionSelect;
        std::wstring_view back;
        std::wstring_view go;
        std::wstring_view noSongsAvailable;
        std::wstring_view noDifficulties;
        std::wstring_view noDifficulty;
        std::wstring_view detailsFormat;
        std::wstring_view catalogErrorPrefix;
        std::wstring_view unsupportedModePrefix;
        std::wstring_view patternErrorPrefix;
        std::wstring_view unknownChartError;
    };

    [[nodiscard]] const MusicSelectTextSet &MusicSelect(Language language) noexcept;
} // namespace finger_drum::texts
