#include "EditorOsuImport.h"
#include "SongSelectionText.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"
#include <Windows.h>
#include <chrono>
#include <commdlg.h>
#include <cstddef>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    class Handle final
    {
      public:
        explicit Handle(HANDLE value) : value_(value)
        {
            if (!value || value == INVALID_HANDLE_VALUE)
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        }
        ~Handle()
        {
            CloseHandle(value_);
        }
        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;
        HANDLE Get() const noexcept
        {
            return value_;
        }

      private:
        HANDLE value_;
    };
    std::wstring Quote(std::wstring_view value)
    {
        std::wstring result = L"\"";
        std::size_t slashes{};
        for (const auto c : value)
        {
            if (c == L'\\')
            {
                ++slashes;
                continue;
            }
            result.append(c == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
            slashes = 0;
            result.push_back(c);
        }
        result.append(slashes * 2, L'\\');
        return result + L'\"';
    }
    fs::path ConverterPath()
    {
        std::wstring file(32768, L'\0');
        const auto count = GetModuleFileNameW(nullptr, file.data(), static_cast<DWORD>(file.size()));
        if (!count || count >= file.size())
            throw std::runtime_error("Cannot locate the game executable.");
        file.resize(count);
        return fs::path(file).parent_path() / L"OsuTaikoConverter.exe";
    }
    class Capture final
    {
      public:
        Capture()
        {
            const auto parent = fs::temp_directory_path();
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            for (int retry = 0; retry < 100; ++retry)
            {
                path = parent / (L"YOMU-osu-" + std::to_wstring(nonce) + L"-" + std::to_wstring(retry));
                if (fs::create_directory(path))
                    return;
            }
            throw std::runtime_error("Cannot create converter output capture.");
        }
        ~Capture()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
        Capture(const Capture &) = delete;
        Capture &operator=(const Capture &) = delete;
        fs::path path;
    };
    std::string ReadCapture(const fs::path &path)
    {
        const auto size = fs::file_size(path);
        if (size > 16 * 1024 * 1024)
            throw std::runtime_error("Converter output exceeds 16 MiB.");
        std::ifstream input(path, std::ios::binary);
        std::string text(static_cast<std::size_t>(size), '\0');
        if (!input.read(text.data(), static_cast<std::streamsize>(text.size())))
            throw std::runtime_error("Cannot read converter output.");
        return text;
    }
    DWORD Run(const fs::path &exe, std::wstring command, const fs::path &directory)
    {
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        Handle output(CreateFileW((directory / L"stdout").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        Handle errors(CreateFileW((directory / L"stderr").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0,
                                 nullptr));
        SIZE_T size{};
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        if (!size || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            throw std::runtime_error("Cannot query process handle-list storage.");
        std::vector<std::byte> storage(size);
        auto *attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if (!attributes || !InitializeProcThreadAttributeList(attributes, 1, 0, &size))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        const auto cleanup = std::unique_ptr<_PROC_THREAD_ATTRIBUTE_LIST, decltype(&DeleteProcThreadAttributeList)>(
            attributes, DeleteProcThreadAttributeList);
        HANDLE inherited[]{output.Get(), errors.Get(), input.Get()};
        if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited),
                                       nullptr, nullptr))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdOutput = output.Get();
        startup.StartupInfo.hStdError = errors.Get();
        startup.StartupInfo.hStdInput = input.Get();
        startup.lpAttributeList = attributes;
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo,
                            &process))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        Handle processHandle(process.hProcess), threadHandle(process.hThread);
        if (WaitForSingleObject(processHandle.Get(), INFINITE) != WAIT_OBJECT_0)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        DWORD code{};
        if (!GetExitCodeProcess(processHandle.Get(), &code))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        return code;
    }
    EditorOsuImportResult Import(const fs::path &input, const fs::path &root)
    {
        try
        {
            const auto exe = ConverterPath();
            Capture capture;
            const auto command = Quote(exe.native()) + L" --input " + Quote(input.native()) + L" --songs-root " +
                                 Quote(root.native()) + L" --layout library";
            const auto code = Run(exe, command, capture.path);
            const auto output = ReadCapture(capture.path / L"stdout");
            auto diagnostics = ReadCapture(capture.path / L"stderr");
            // This bundled producer writes one JSON object with a fixed version/success prefix.
            const bool success = code == 0 && output.starts_with("{\"version\":1,\"success\":true,");
            if (!success && diagnostics.empty())
                diagnostics = "Converter failed (exit " + std::to_string(code) + ").";
            return {success, std::move(diagnostics)};
        }
        catch (const std::exception &error)
        {
            return {false, error.what()};
        }
    }
} // namespace
std::optional<std::filesystem::path> SelectOsuFile(const finger_drum::texts::TextCatalog &texts)
{
    std::vector<wchar_t> path(32768);
    const auto title = std::wstring(finger_drum::texts::MusicSelect(texts).convertOsu);
    auto filter = std::wstring(finger_drum::texts::MusicSelect(texts).osuFileFilter);
    filter.push_back(L'\0');
    filter.append(L"*.osu");
    filter.push_back(L'\0');
    filter.push_back(L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = filter.c_str();
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrTitle = title.c_str();
    dialog.lpstrDefExt = L"osu";
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog))
        return fs::path(path.data());
    if (const auto error = CommDlgExtendedError())
        throw std::runtime_error("Cannot open osu! file dialog (" + std::to_string(error) + ").");
    return {};
}
void EditorOsuImport::Start(std::filesystem::path input, std::filesystem::path songsRoot)
{
    if (Busy())
        throw std::logic_error("An osu! import is already running.");
    job_ = std::async(std::launch::async,
                      [input = std::move(input), root = std::move(songsRoot)] { return Import(input, root); });
}
void ShowOsuImportCompletion(const EditorOsuImportResult &result, const finger_drum::texts::TextCatalog &texts)
{
    const auto text = finger_drum::texts::MusicSelect(texts);
    const auto body = std::wstring(text.importComplete) + L"\n\n" + song_select::DecodeDisplayText(result.message);
    MessageBoxW(GetActiveWindow(), body.c_str(), std::wstring(text.importTitle).c_str(), MB_OK | MB_ICONINFORMATION);
}
std::optional<EditorOsuImportResult> EditorOsuImport::Poll()
{
    if (!job_.valid() || job_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
        return {};
    try
    {
        return job_.get();
    }
    catch (const std::exception &error)
    {
        return EditorOsuImportResult{false, error.what()};
    }
}
