#pragma once
#include "Catalog/SongCatalog.h"
#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <ranges>

namespace song_select
{
    [[nodiscard]] inline std::wstring DecodeDisplayText(const std::string_view value)
    {
        if (value.empty())
        {
            return {};
        }
        UINT codePage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;
        int length = MultiByteToWideChar(codePage, flags, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
        if (length <= 0)
        {
            codePage = CP_ACP;
            flags = 0;
            length = MultiByteToWideChar(codePage, flags, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
        }
        if (length <= 0)
        {
            return L"?";
        }
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(codePage, flags, value.data(), static_cast<int>(value.size()),
                            result.data(), length);
        return result;
    }

    [[nodiscard]] inline std::wstring SongTitle(const finger_drum::chart::SongCatalogEntry &song)
    {
        return song.music.names.empty() ? song.metadataPath.stem().wstring()
                                        : DecodeDisplayText(song.music.names.front());
    }

    [[nodiscard]] inline std::wstring SongArtist(const finger_drum::chart::SongCatalogEntry &song)
    {
        return song.music.artists.empty() ? L"—" : DecodeDisplayText(song.music.artists.front());
    }

    [[nodiscard]] inline std::wstring PatternName(
        const finger_drum::chart::SongCatalogPattern &pattern)
    {
        return pattern.pattern.name.empty() ? pattern.patternPath.stem().wstring()
                                            : DecodeDisplayText(pattern.pattern.name);
    }

    [[nodiscard]] inline std::wstring Lowercase(std::wstring value)
    {
        std::ranges::transform(value, value.begin(), [](const wchar_t character) {
            return static_cast<wchar_t>(std::towlower(character));
        });
        return value;
    }

    [[nodiscard]] inline bool MatchesSearch(const finger_drum::chart::SongCatalogEntry &song,
                                            const std::wstring &searchText)
    {
        if (searchText.empty())
        {
            return true;
        }
        const std::wstring needle = Lowercase(searchText);
        return Lowercase(SongTitle(song)).find(needle) != std::wstring::npos ||
               Lowercase(SongArtist(song)).find(needle) != std::wstring::npos;
    }
} // namespace song_select
