#ifdef _DEBUG
// Must precede the C runtime headers so Debug CRT allocations retain the
// source location when they originate from malloc/calloc/realloc.
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif

#include "App/YOUMUGame.h"
#include "YOUMUAssets.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "Texts/GameIdentity.h"
#include "Texts/TextCatalog.h"

#include <Windows.h>

#include <cstdlib>
#include <algorithm>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    void EnableCrtMemoryLeakChecks() noexcept
    {
#ifdef _DEBUG
        // Preserve the CRT's existing diagnostics and request one automatic
        // leak dump in Visual Studio's Debug Output window at process exit.
        const int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
        _CrtSetDbgFlag(
            flags | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
#endif
    }

    void ReportStartupError(const char *error, const bool showWindow)
    {
        // Both startup failure paths decode UTF-8 and use the shared game name.
        const int length = MultiByteToWideChar(CP_UTF8, 0, error, -1, nullptr, 0);
        std::wstring message(static_cast<std::size_t>(std::max(length, 1)), L'\0');
        if (length > 0)
            MultiByteToWideChar(CP_UTF8, 0, error, -1, message.data(), length);
        else
            message = TextCatalog{}.Get("Startup.decodeError");
        OutputDebugStringW(message.c_str());
        if (showWindow)
            MessageBoxW(nullptr, message.c_str(), GameName, MB_OK | MB_ICONERROR);
    }
}

int WINAPI wWinMain(
    _In_ HINSTANCE,
    _In_opt_ HINSTANCE,
    _In_ PWSTR,
    _In_ int)
{
    EnableCrtMemoryLeakChecks();

    std::string assetError;
    if (!InitializeBuiltInAssets(assetError))
    {
        ReportStartupError(assetError.c_str(), true);
        return EXIT_FAILURE;
    }

    // The executable selects the requested Client route. Run owns the Win32,
    // D3D12, input, and audio lifetime around that object.
    const std::wstring_view commandLine = GetCommandLineW();
    const bool basicSmokeTest =
        commandLine.find(L"--smoke-test") != std::wstring_view::npos;

    const bool smokeLobby =
        commandLine.find(L"--smoke-lobby") != std::wstring_view::npos;
    const bool smokeGameplay =
        commandLine.find(L"--smoke-gameplay") != std::wstring_view::npos;
    const bool smokeEditor =
        commandLine.find(L"--smoke-editor") != std::wstring_view::npos;
    const bool rhythmDebugMode =
        commandLine.find(L"--rhythm-debug") != std::wstring_view::npos;
    const bool smokeTest = smokeLobby || smokeGameplay || smokeEditor || basicSmokeTest;
    std::string initialScene(LogoSceneId);
    if (smokeLobby)
    {
        initialScene = LobbySceneId;
    }
    else if (smokeGameplay)
    {
        initialScene = RhythmTestSceneId;
    }
    else if (smokeEditor)
    {
        initialScene = EditorSceneId;
    }
    else if (rhythmDebugMode)
    {
        initialScene = RhythmTestSceneId;
    }

    // Specialized hidden routes exercise catalog widgets and layered Taiko
    // assets without changing the normal executable's Logo entry point.
    try
    {
        return Run(std::make_unique<YOUMUGame>(
            smokeTest, std::move(initialScene), rhythmDebugMode));
    }
    catch (const std::exception &error)
    {
        ReportStartupError(error.what(), !smokeTest);
        return EXIT_FAILURE;
    }
}
