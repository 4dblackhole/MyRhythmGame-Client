#include "EditorSupport.h"

namespace editor_ui
{
    struct EditDialog
    {
        std::wstring title, value;
        std::wstring ok, cancel;
        HWND edit{};
        bool accepted{};
    };
    INT_PTR CALLBACK EditProcedure(HWND dialog, UINT message, WPARAM w, LPARAM l)
    {
        auto *data = reinterpret_cast<EditDialog *>(GetWindowLongPtrW(dialog, DWLP_USER));
        if (message == WM_INITDIALOG)
        {
            data = reinterpret_cast<EditDialog *>(l);
            SetWindowLongPtrW(dialog, DWLP_USER, l);
            SetWindowTextW(dialog, data->title.c_str());
            RECT r{};
            GetClientRect(dialog, &r);
            data->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", data->value.c_str(),
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE |
                                             ES_AUTOVSCROLL | WS_VSCROLL,
                                         12, 12, r.right - 24, r.bottom - 65, dialog,
                                         reinterpret_cast<HMENU>(100), nullptr, nullptr);
            CreateWindowW(L"BUTTON", data->ok.c_str(),
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                          r.right - 195, r.bottom - 42, 85, 30, dialog,
                          reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
            CreateWindowW(L"BUTTON", data->cancel.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                          r.right - 100,
                          r.bottom - 42, 85, 30, dialog, reinterpret_cast<HMENU>(IDCANCEL), nullptr,
                          nullptr);
            SendMessageW(data->edit, WM_SETFONT,
                         reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            SetFocus(data->edit);
            return FALSE;
        }
        if (message == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL))
        {
            if (LOWORD(w) == IDOK)
            {
                const int n = GetWindowTextLengthW(data->edit);
                data->value.resize(static_cast<std::size_t>(n) + 1);
                GetWindowTextW(data->edit, data->value.data(), n + 1);
                data->value.resize(static_cast<std::size_t>(n));
                data->accepted = true;
            }
            EndDialog(dialog, LOWORD(w));
            return TRUE;
        }
        return FALSE;
    }
    std::optional<std::string> EditText(
        const std::wstring &title, const std::string &value,
        const finger_drum::texts::EditorTextSet &texts)
    {
        struct Template
        {
            DLGTEMPLATE dialog;
            WORD menu{}, windowClass{}, title{};
        } layout{};
        layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER;
        layout.dialog.cx = 360;
        layout.dialog.cy = 190;
        EditDialog data{title, Wide(value), std::wstring(texts.ok), std::wstring(texts.cancel)};
        if (DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &layout.dialog, GetActiveWindow(),
                                    EditProcedure, reinterpret_cast<LPARAM>(&data)) == -1)
            throw std::runtime_error("Could not open the text input dialog.");
        return data.accepted ? std::optional{Utf8(data.value)} : std::nullopt;
    }
} // namespace editor_ui
