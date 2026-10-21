#pragma once
#include "PhantomCore.h"
#include <string>

enum class ResultAction
{
    None,
    Back,
    Retry
};
struct ResultViewContext
{
    std::wstring songTitle;
    std::wstring patternCreator;
    bool canRetry{};
    std::wstring songArtist;
};
class IResultView
{
  public:
    virtual ~IResultView() = default;
    virtual void Initialize(const EngineServices &services) = 0;
    virtual void SetVisible(bool visible) noexcept = 0;
    virtual ResultAction Update(const InputState &input) = 0;
    virtual void OnResize(std::uint32_t width, std::uint32_t height) = 0;
    virtual void Shutdown() noexcept = 0;
};
