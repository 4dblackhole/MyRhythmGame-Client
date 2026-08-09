#include "MarqueeTextComponent.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace finger_drum::presentation
{
    MarqueeTextComponent::MarqueeTextComponent(
        std::wstring text,
        const std::size_t maximumVisibleCharacters,
        const double secondsPerCharacter)
        : text_(std::move(text)),
          maximumVisibleCharacters_(std::max<std::size_t>(
              maximumVisibleCharacters,
              1)),
          secondsPerCharacter_(std::max(secondsPerCharacter, 0.05))
    {
    }

    void MarqueeTextComponent::SetText(std::wstring text)
    {
        text_ = std::move(text);
        elapsedSeconds_ = 0.0;
        characterOffset_ = 0;
        displayDirty_ = true;
    }

    std::wstring_view MarqueeTextComponent::Text() const noexcept
    {
        return text_;
    }

    void MarqueeTextComponent::Update(const double elapsedSeconds)
    {
        if (text_.size() <= maximumVisibleCharacters_)
        {
            if (displayDirty_)
            {
                RefreshDisplayedText();
            }
            return;
        }

        elapsedSeconds_ += std::max(elapsedSeconds, 0.0);
        if (characterOffset_ == 0 && elapsedSeconds_ < EndPauseSeconds)
        {
            if (displayDirty_)
            {
                RefreshDisplayedText();
            }
            return;
        }
        if (characterOffset_ == 0)
        {
            elapsedSeconds_ -= EndPauseSeconds;
        }
        if (elapsedSeconds_ < secondsPerCharacter_)
        {
            return;
        }

        const std::size_t steps = static_cast<std::size_t>(
            elapsedSeconds_ / secondsPerCharacter_);
        elapsedSeconds_ -= static_cast<double>(steps) * secondsPerCharacter_;
        const std::size_t cycleLength = text_.size() + GapCharacters;
        characterOffset_ = (characterOffset_ + steps) % cycleLength;
        displayDirty_ = true;
        RefreshDisplayedText();
    }

    void MarqueeTextComponent::RefreshDisplayedText()
    {
        mrg::visual2d::TextVisualComponent* const textVisual =
            Owner().GetComponent<mrg::visual2d::TextVisualComponent>();
        if (textVisual == nullptr)
        {
            throw std::logic_error(
                "MarqueeTextComponent requires TextVisualComponent on its owner.");
        }
        textVisual->SetText(
            text_.size() <= maximumVisibleCharacters_
                ? text_
                : BuildWindow());
        displayDirty_ = false;
    }

    std::wstring MarqueeTextComponent::BuildWindow() const
    {
        const std::wstring cycle = text_ +
            std::wstring(GapCharacters, L' ');
        std::wstring result;
        result.reserve(maximumVisibleCharacters_);
        for (std::size_t index = 0;
             index < maximumVisibleCharacters_;
             ++index)
        {
            result.push_back(cycle[
                (characterOffset_ + index) % cycle.size()]);
        }
        return result;
    }
}
