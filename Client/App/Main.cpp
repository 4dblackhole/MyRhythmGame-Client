#ifdef _DEBUG
// Must precede the C runtime headers so Debug CRT allocations retain the
// source location when they originate from malloc/calloc/realloc.
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif

#include "App/FingerDrumGame.h"
#include "Examples/ColoredCube/App/ColoredCubeGame.h"
#include "Examples/ColoredCube/GameFlow/SceneIds.h"
#include "FingerDrumAssets.h"
#include "GameFlow/FingerDrumSceneIds.h"

#include <Windows.h>

#include <cstdlib>
#include <memory>
#include <optional>
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

    [[nodiscard]] std::optional<std::string> SelectEngineExampleScene(
        const std::wstring_view commandLine)
    {
        if (commandLine.find(L"--example=mesh") !=
            std::wstring_view::npos)
        {
            return std::string(game::scene_ids::MeshExample);
        }
        if (commandLine.find(L"--example=collision") !=
            std::wstring_view::npos)
        {
            return std::string(game::scene_ids::CollisionExample);
        }
        if (commandLine.find(L"--example=widgets") !=
            std::wstring_view::npos)
        {
            return std::string(game::scene_ids::WidgetExample);
        }
        return std::nullopt;
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
    if (!finger_drum::assets::InitializeBuiltInAssets(assetError))
    {
        MessageBoxA(
            nullptr,
            assetError.c_str(),
            "FingerDrum built-in asset error",
            MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    // The executable selects the requested Client route. Run owns the Win32,
    // D3D12, input, and audio lifetime around that object.
    const std::wstring_view commandLine = GetCommandLineW();
    const bool basicSmokeTest =
        commandLine.find(L"--smoke-test") != std::wstring_view::npos;
    if (std::optional<std::string> exampleScene =
            SelectEngineExampleScene(commandLine))
    {
        const bool showPerformanceOverlay =
            commandLine.find(L"--show-performance-overlay") !=
                std::wstring_view::npos;
        return mrg::Run(std::make_unique<ColoredCubeGame>(
            basicSmokeTest,
            showPerformanceOverlay,
            std::move(*exampleScene)));
    }

    const bool smokeLobby =
        commandLine.find(L"--smoke-lobby") != std::wstring_view::npos;
    const bool smokeGameplay =
        commandLine.find(L"--smoke-gameplay") != std::wstring_view::npos;
    const bool rhythmDebugMode =
        commandLine.find(L"--rhythm-debug") != std::wstring_view::npos;
    const bool smokeTest = smokeLobby || smokeGameplay || basicSmokeTest;
    std::string initialScene(finger_drum::scene_ids::Logo);
    if (smokeLobby)
    {
        initialScene = finger_drum::scene_ids::Lobby;
    }
    else if (smokeGameplay)
    {
        initialScene = finger_drum::scene_ids::RhythmTest;
    }
    else if (rhythmDebugMode)
    {
        initialScene = finger_drum::scene_ids::RhythmTest;
    }

    // Specialized hidden routes exercise catalog widgets and layered Taiko
    // assets without changing the normal executable's Logo entry point.
    return mrg::Run(std::make_unique<FingerDrumGame>(
        smokeTest,
        std::move(initialScene),
        rhythmDebugMode));
}
