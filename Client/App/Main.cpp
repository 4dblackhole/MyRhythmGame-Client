#ifdef _DEBUG
// Must precede the C runtime headers so Debug CRT allocations retain the
// source location when they originate from malloc/calloc/realloc.
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif

#include "ColoredCubeGame.h"

#include <Windows.h>

#include <memory>
#include <string_view>

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
    const bool smokeTest =
        commandLine.find(L"--smoke-test") != std::wstring_view::npos;
    const bool showPerformanceOverlay =
        commandLine.find(L"--show-performance-overlay") !=
        std::wstring_view::npos;

    // The hidden smoke-test path uses the identical initialization and frame
    // loop, but asks the Client configuration to exit after three renders.
    // The overlay option exists for visual regression capture; normal runs
    // still start hidden and let Raw Input F1 toggle the display.
    return mrg::Run(std::make_unique<ColoredCubeGame>(
        smokeTest,
        showPerformanceOverlay));
}
