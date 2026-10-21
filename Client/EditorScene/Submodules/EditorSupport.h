#pragma once
#include "Editing/IEditorDocument.h"
#include "EditorValueParsing.h"
#include "PhantomCore.h"
#include "Presentation/UiColors.h"
#include "Texts/EditorScene/EditorTexts.h"
#include <Windows.h>
#include <cmath>
#include <stdexcept>

namespace editor_ui
{

    constexpr auto Background = ui_colors::editor::Background, Paper = ui_colors::editor::Paper;
    constexpr auto Ink = ui_colors::editor::Ink, Blue = ui_colors::editor::Accent, Pale = ui_colors::editor::Pale;
    constexpr auto White = ui_colors::White;
    inline std::wstring Wide(const std::string &s)
    {
        const int n =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        if (n)
            MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
        return w;
    }
    inline std::string Utf8(const std::wstring &w)
    {
        const int n =
            WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        std::string s(static_cast<std::size_t>(n), '\0');
        if (n)
            WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
        return s;
    }

} // namespace editor_ui

namespace editor_ui
{
    std::optional<std::string> EditText(const std::wstring &, const std::string &,
                                        const EditorTextSet &);
}
