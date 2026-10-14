#pragma once
#include "Editing/IEditorDocument.h"
#include "EditorValueParsing.h"
#include "MRG_Core.h"
#include "Presentation/UiColors.h"
#include "Texts/EditorScene/EditorTexts.h"
#include <Windows.h>
#include <cmath>
#include <stdexcept>

namespace editor_ui
{
    namespace chart = finger_drum::chart;
    namespace v = mrg::visual2d;
    namespace palette = finger_drum::presentation::ui_colors;
    constexpr auto Background = palette::editor::Background, Paper = palette::editor::Paper;
    constexpr auto Ink = palette::editor::Ink, Blue = palette::editor::Accent, Pale = palette::editor::Pale;
    constexpr auto White = palette::White;
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
    using editor_values::Fraction;
    using editor_values::Integer;
    using editor_values::Number;
    using editor_values::Position;

} // namespace editor_ui

namespace editor_ui
{
    std::optional<std::string> EditText(const std::wstring &, const std::string &,
                                        const finger_drum::texts::EditorTextSet &);
}
