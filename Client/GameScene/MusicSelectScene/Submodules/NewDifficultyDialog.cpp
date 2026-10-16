#include "NewDifficultyDialog.h"
#include "EditorPatternCreation.h"
#include "EditorScene/Submodules/EditorValueParsing.h"
#include "SongSelectionText.h"
#include <Windows.h>
#include <array>
#include <exception>
#include <sstream>

namespace
{
    namespace chart = finger_drum::chart;
    enum Field : int
    {
        Name = 100,
        Makers,
        Tags,
        Bpm,
        Offset,
        JudgeLevel
    };
    struct Dialog
    {
        const std::filesystem::path &root;
        const chart::SongCatalogEntry &song;
        const finger_drum::texts::TextCatalog &texts;
        std::optional<chart::SongCatalogPattern> result;
        std::exception_ptr failure;
        HWND errorLabel{};
    };
    HWND Control(HWND dialog, const wchar_t *type, std::wstring_view text, DWORD style, int x, int y, int width,
                 int height, int id = 0)
    {
        const auto control =
            CreateWindowExW(type == std::wstring_view{L"EDIT"} ? WS_EX_CLIENTEDGE : 0, type, std::wstring(text).c_str(),
                            WS_CHILD | WS_VISIBLE | style, x, y, width, height, dialog,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
        if (!control)
            throw std::runtime_error("Could not create a difficulty metadata field.");
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        return control;
    }
    std::string Read(HWND dialog, int id)
    {
        const HWND edit = GetDlgItem(dialog, id);
        const int size = GetWindowTextLengthW(edit);
        std::wstring text(static_cast<std::size_t>(size) + 1, L'\0');
        GetWindowTextW(edit, text.data(), size + 1);
        text.resize(static_cast<std::size_t>(size));
        const int bytes =
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), size, nullptr, 0, nullptr, nullptr);
        if (size != 0 && bytes == 0)
            throw std::invalid_argument("Metadata contains invalid Unicode.");
        std::string result(static_cast<std::size_t>(bytes), '\0');
        if (bytes)
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), size, result.data(), bytes, nullptr,
                                nullptr);
        return result;
    }
    std::vector<std::string> Lines(const std::string &value)
    {
        std::vector<std::string> result;
        std::istringstream input(value);
        for (std::string line; std::getline(input, line);)
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty())
                result.push_back(std::move(line));
        }
        return result;
    }
    chart::PatternDocument ReadMetadata(HWND dialog)
    {
        chart::PatternDocument metadata;
        metadata.name = Read(dialog, Name);
        metadata.makers = Lines(Read(dialog, Makers));
        metadata.tags = Lines(Read(dialog, Tags));
        metadata.baseBpm = editor_values::Number(Read(dialog, Bpm));
        metadata.patternOffsetMilliseconds = editor_values::Number(Read(dialog, Offset));
        metadata.judgementLevel = editor_values::JudgeLevel(Read(dialog, JudgeLevel));
        return metadata;
    }
    void Initialize(HWND dialog, Dialog &data)
    {
        SetWindowTextW(dialog, data.texts.Get("MusicSelect.createDifficultyTitle").data());
        RECT bounds{};
        GetClientRect(dialog, &bounds);
        Control(dialog, L"STATIC", data.texts.Get("MusicSelect.newDifficultyDefaults"), 0, 12, 12, bounds.right - 24,
                24);
        struct Row
        {
            int id, y, height;
            const char *key;
            const wchar_t *value;
        };
        const std::array rows{Row{Name, 44, 28, "Editor.patternName", L""},
                              Row{Makers, 82, 54, "Editor.makers", L""},
                              Row{Tags, 146, 54, "Editor.tags", L""},
                              Row{Bpm, 210, 28, "Editor.baseBpm", L"120"},
                              Row{Offset, 248, 28, "Editor.offsetMilliseconds", L"0"},
                              Row{JudgeLevel, 286, 28, "Editor.accuracyDifficulty", L"50.00"}};
        for (const auto &row : rows)
        {
            Control(dialog, L"STATIC", data.texts.Get(row.key), 0, 12, row.y + 4, 180, row.height);
            const bool multiline = row.id == Makers || row.id == Tags;
            Control(dialog, L"EDIT", row.value,
                    WS_TABSTOP |
                        (multiline ? ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL : ES_AUTOHSCROLL),
                    200, row.y, bounds.right - 212, row.height, row.id);
        }
        data.errorLabel = Control(dialog, L"STATIC", L"", 0, 12, 328, bounds.right - 24, bounds.bottom - 386);
        Control(dialog, L"BUTTON", data.texts.Get("MusicSelect.createDifficultyConfirm"), WS_TABSTOP | BS_DEFPUSHBUTTON,
                bounds.right - 224, bounds.bottom - 42, 124, 30, IDOK);
        Control(dialog, L"BUTTON", data.texts.Get("Editor.cancel"), WS_TABSTOP, bounds.right - 92, bounds.bottom - 42,
                80, 30, IDCANCEL);
        SetFocus(GetDlgItem(dialog, Name));
    }
    INT_PTR CALLBACK Procedure(HWND dialog, UINT message, WPARAM w, LPARAM l)
    {
        auto *data = reinterpret_cast<Dialog *>(GetWindowLongPtrW(dialog, DWLP_USER));
        try
        {
            if (message == WM_INITDIALOG)
            {
                data = reinterpret_cast<Dialog *>(l);
                SetWindowLongPtrW(dialog, DWLP_USER, l);
                Initialize(dialog, *data);
                return FALSE;
            }
            if (message == WM_COMMAND && LOWORD(w) == IDOK && data)
            {
                try
                {
                    data->result = editor_pattern_creation::Create(data->root, data->song, ReadMetadata(dialog));
                    EndDialog(dialog, IDOK);
                }
                catch (const std::exception &error)
                {
                    auto key = "MusicSelect.createDifficultyError";
                    if (const auto *specific = dynamic_cast<const editor_pattern_creation::Error *>(&error))
                        if (specific->reason == editor_pattern_creation::Failure::ExistingFolder)
                            key = "MusicSelect.createDifficultyExists";
                        else if (specific->reason == editor_pattern_creation::Failure::InvalidName)
                            key = "MusicSelect.createDifficultyNameError";
                    const auto messageText =
                        std::wstring(data->texts.Get(key)) + L"\r\n" + song_select::DecodeDisplayText(error.what());
                    SetWindowTextW(data->errorLabel, messageText.c_str());
                }
                return TRUE;
            }
            if (message == WM_COMMAND && LOWORD(w) == IDCANCEL)
            {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
        }
        catch (...)
        {
            if (data)
                data->failure = std::current_exception();
            EndDialog(dialog, IDCANCEL);
        }
        return FALSE;
    }
} // namespace

std::optional<chart::SongCatalogPattern> CreateDifficultyWithDialog(const std::filesystem::path &songsRoot,
                                                                    const chart::SongCatalogEntry &song,
                                                                    const finger_drum::texts::TextCatalog &texts)
{
    struct Template
    {
        DLGTEMPLATE dialog;
        WORD menu{}, windowClass{}, title{};
    } layout{};
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER;
    layout.dialog.cx = 390;
    layout.dialog.cy = 310;
    Dialog data{songsRoot, song, texts, {}, {}};
    if (DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &layout.dialog, GetActiveWindow(), Procedure,
                                reinterpret_cast<LPARAM>(&data)) == -1)
        throw std::runtime_error("Could not open the difficulty metadata dialog.");
    if (data.failure)
        std::rethrow_exception(data.failure);
    return std::move(data.result);
}
