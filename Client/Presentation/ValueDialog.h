#pragma once
#include <functional>
#include <string>
#include <vector>

struct DialogValue
{
    enum class Kind
    {
        Text,
        Check,
        Choice
    };
    std::wstring label, value;
    Kind kind{Kind::Text};
    std::vector<std::wstring> choices;
};
// Validation runs on OK. Failed validation leaves the dialog and edits open.
bool ShowValueDialog(const std::wstring &title, std::vector<DialogValue> &fields, const std::wstring &ok,
                     const std::wstring &cancel, const std::function<void()> &validate);
