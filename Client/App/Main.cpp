#ifdef _DEBUG
// Must precede the C runtime headers so Debug CRT allocations retain the
// source location when they originate from malloc/calloc/realloc.
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif

#include "App/FingerDrumGame.h"
#include "GameFlow/FingerDrumSceneIds.h"

#include <Windows.h>

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

}

int WINAPI wWinMain(
    _In_ HINSTANCE,
    _In_opt_ HINSTANCE,
    _In_ PWSTR,
    _In_ int)
{
    EnableCrtMemoryLeakChecks();

    // The executable constructs only the game-specific Client.  Run owns the
    // Win32, D3D12, input, and audio lifetime around this object.
    const std::wstring_view commandLine = GetCommandLineW();
    const bool smokeLobby =
        commandLine.find(L"--smoke-lobby") != std::wstring_view::npos;
    const bool smokeGameplay =
        commandLine.find(L"--smoke-gameplay") != std::wstring_view::npos;
    const bool smokeTest = smokeLobby || smokeGameplay ||
        commandLine.find(L"--smoke-test") != std::wstring_view::npos;
    std::string initialScene(finger_drum::scene_ids::Logo);
    if (smokeLobby)
    {
        initialScene = finger_drum::scene_ids::Lobby;
    }
    else if (smokeGameplay)
    {
        initialScene = finger_drum::scene_ids::RhythmTest;
    }

    // Specialized hidden routes exercise catalog widgets and layered Taiko
    // assets without changing the normal executable's Logo entry point.
    return mrg::Run(std::make_unique<FingerDrumGame>(
        smokeTest,
        std::move(initialScene)));
}
