#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include <ShObjIdl.h>
#include <memory>
#include <wrl/client.h>

using namespace options_ui;

void OptionsPanel::CreateSongsControls()
{
    songsLabel_ = &CreateFieldLabel(*content_, contentHeight_ - 988, PanelWidth, "Options.Songs.Label");
    songsField_ = &CreateButton(*content_, {24, contentHeight_ - 1046, PanelWidth - 48, 46}, L"",
                                               "Options.Songs.Folder");
    songsDefault_ = &CreateButton(*content_, {24, contentHeight_ - 1090, PanelWidth - 48, 32}, L"",
                                                 "Options.Songs.Default");
    for (auto *node : {songsField_, songsDefault_})
    {
        VisualStyle style{};
        style.normal = FieldColor;
        style.hovered = FieldHoverColor;
        style.pressed = ui_colors::options::Pressed;
        style.disabled = ui_colors::options::Disabled;
        RequireComponent<SpriteVisualComponent>(*node).SetStyle(style);
        auto &text = RequireComponent<TextVisualComponent>(*node);
        text.SetFontSize(15);
        text.SetTextColor(HeadingColor);
    }
}

void OptionsPanel::RefreshSongsControls()
{
    const std::array values{std::pair{songsLabel_, std::wstring(texts_.Get("Options.songsFolder"))},
                            std::pair{songsField_, options_.SongsRoot().wstring()},
                            std::pair{songsDefault_, std::wstring(texts_.Get("Options.defaultSongsFolder"))}};
    for (const auto &[node, value] : values)
    {
        auto &text = RequireComponent<TextVisualComponent>(*node);
        text.SetText(value);
        texts_.ApplyFont(text);
    }
}

void OptionsPanel::ChooseSongsFolder()
{
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct ComScope
    {
        HRESULT result;
        ~ComScope()
        {
            if (SUCCEEDED(result))
                CoUninitialize();
        }
    } scope{initialized};
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT result =
        CoCreateInstance(__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(result))
        result = dialog->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    if (SUCCEEDED(result))
        result = dialog->SetTitle(std::wstring(texts_.Get("Options.songsFolder")).c_str());
    if (SUCCEEDED(result))
        result = dialog->Show(GetActiveWindow());
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return;
    Microsoft::WRL::ComPtr<IShellItem> item;
    if (SUCCEEDED(result))
        result = dialog->GetResult(&item);
    PWSTR raw{};
    if (SUCCEEDED(result))
        result = item->GetDisplayName(SIGDN_FILESYSPATH, &raw);
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> path(raw, &CoTaskMemFree);
    if (FAILED(result) || !path)
    {
        status_ = Status::InvalidInput;
        statusDetail_ = "Unable to select the Songs directory.";
    }
    else
    {
        auto candidate = options_.Values();
        candidate.songsPath = path.get();
        SaveOptions(candidate);
    }
    RefreshSongsControls();
    RefreshStatus();
}
