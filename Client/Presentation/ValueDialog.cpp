#include "ValueDialog.h"
#include <Windows.h>
#include <stdexcept>

namespace
{
    struct DialogState
    {
        const std::wstring &title, &ok, &cancel;
        std::vector<DialogValue> &fields;
        const std::function<void()> &validate;
        std::vector<HWND> controls;
        bool accepted{};
    };
    HWND Control(HWND dialog, const wchar_t *type, const std::wstring &text, DWORD style, int x, int y, int width,
                 int height, int id)
    {
        HWND control = CreateWindowExW(0, type, text.c_str(), WS_CHILD | WS_VISIBLE | style, x, y, width, height,
                                       dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
        if (control)
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        return control;
    }
    INT_PTR CALLBACK Procedure(HWND dialog, UINT message, WPARAM w, LPARAM l)
    {
        auto *state = reinterpret_cast<DialogState *>(GetWindowLongPtrW(dialog, DWLP_USER));
        if (message == WM_INITDIALOG)
        {
            state = reinterpret_cast<DialogState *>(l);
            SetWindowLongPtrW(dialog, DWLP_USER, l);
            SetWindowTextW(dialog, state->title.c_str());
            RECT bounds{};
            GetClientRect(dialog, &bounds);
            const int width = bounds.right;
            for (std::size_t i = 0; i < state->fields.size(); ++i)
            {
                const auto &field = state->fields[i];
                const int y = 12 + static_cast<int>(i) * 30;
                const int id = 100 + static_cast<int>(i);
                HWND control{};
                if (field.kind == DialogValue::Kind::Check)
                {
                    control = Control(dialog, L"BUTTON", field.label, WS_TABSTOP | BS_AUTOCHECKBOX, 12, y, width - 24,
                                      25, id);
                    SendMessageW(control, BM_SETCHECK, field.value == L"1" ? BST_CHECKED : BST_UNCHECKED, 0);
                }
                else
                {
                    Control(dialog, L"STATIC", field.label, 0, 12, y + 4, width / 2 - 20, 24, 0);
                    if (field.kind == DialogValue::Kind::Choice)
                    {
                        control = Control(dialog, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                          width / 2, y, width / 2 - 12, 160, id);
                        for (const auto &choice : field.choices)
                            SendMessageW(control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.c_str()));
                        SendMessageW(control, CB_SETCURSEL, std::stoul(field.value), 0);
                    }
                    else
                        control = Control(dialog, L"EDIT", field.value, WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                          width / 2, y, width / 2 - 12, 25, id);
                }
                if (!control)
                {
                    EndDialog(dialog, -1);
                    return TRUE;
                }
                SendMessageW(control, EM_SETLIMITTEXT, 4096, 0);
                state->controls.push_back(control);
            }
            const int y = 18 + static_cast<int>(state->fields.size()) * 30;
            Control(dialog, L"BUTTON", state->ok, WS_TABSTOP | BS_DEFPUSHBUTTON, width - 194, y, 85, 30, IDOK);
            Control(dialog, L"BUTTON", state->cancel, WS_TABSTOP, width - 99, y, 85, 30, IDCANCEL);
            return TRUE;
        }
        if (message == WM_COMMAND && state && LOWORD(w) == IDOK)
        {
            try
            {
                for (std::size_t i = 0; i < state->fields.size(); ++i)
                {
                    auto &field = state->fields[i];
                    HWND control = state->controls[i];
                    if (field.kind == DialogValue::Kind::Check)
                        field.value = SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0";
                    else if (field.kind == DialogValue::Kind::Choice)
                        field.value = std::to_wstring(SendMessageW(control, CB_GETCURSEL, 0, 0));
                    else
                    {
                        const int size = GetWindowTextLengthW(control);
                        field.value.resize(static_cast<std::size_t>(size) + 1);
                        GetWindowTextW(control, field.value.data(), size + 1);
                        field.value.resize(static_cast<std::size_t>(size));
                    }
                }
                state->validate();
                state->accepted = true;
                EndDialog(dialog, IDOK);
            }
            catch (const std::exception &error)
            {
                const std::string text = error.what();
                const std::wstring wide(text.begin(), text.end());
                MessageBoxW(dialog, wide.c_str(), state->title.c_str(), MB_OK | MB_ICONERROR);
            }
            return TRUE;
        }
        if (message == WM_COMMAND && LOWORD(w) == IDCANCEL)
        {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    }
} // namespace
bool ShowValueDialog(const std::wstring &title, std::vector<DialogValue> &fields, const std::wstring &ok,
                     const std::wstring &cancel, const std::function<void()> &validate)
{
    if (fields.empty() || fields.size() > 18)
        throw std::invalid_argument("Invalid dialog field count.");
    struct Template
    {
        DLGTEMPLATE dialog;
        WORD menu{}, windowClass{}, title{};
    } layout{};
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER;
    layout.dialog.cx = 360;
    layout.dialog.cy = static_cast<short>(35 + fields.size() * 19);
    DialogState state{title, ok, cancel, fields, validate};
    if (DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &layout.dialog, GetActiveWindow(), Procedure,
                                reinterpret_cast<LPARAM>(&state)) == -1)
        throw std::runtime_error("Could not open the value input dialog.");
    return state.accepted;
}
