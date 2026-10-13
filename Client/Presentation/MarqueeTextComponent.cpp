#include "MarqueeTextComponent.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace finger_drum::presentation
{
    MarqueeTextComponent::MarqueeTextComponent(std::wstring text, const double secondsPerCharacter)
        : text_(std::move(text)), secondsPerCharacter_(std::max(secondsPerCharacter, 0.05))
    {
        if (!std::isfinite(secondsPerCharacter))
            throw std::invalid_argument("Marquee speed must be finite.");
        characters_ = CharacterCount();
    }

    void MarqueeTextComponent::SetText(std::wstring text)
    {
        if (text_ == text)
            return;
        text_ = std::move(text);
        characters_ = CharacterCount();
        elapsedSeconds_ = 0;
        offset_ = 0;
        displayDirty_ = true;
    }

    std::wstring_view MarqueeTextComponent::Text() const noexcept
    {
        return text_;
    }

    std::size_t MarqueeTextComponent::CharacterCount() const noexcept
    {
        std::size_t count = 0;
        for (std::size_t index = 0; index < text_.size(); ++index)
        {
            const auto unit = static_cast<unsigned>(text_[index]);
            if (unit >= 0xD800 && unit <= 0xDBFF && index + 1 < text_.size())
            {
                const auto next = static_cast<unsigned>(text_[index + 1]);
                if (next >= 0xDC00 && next <= 0xDFFF)
                    ++index;
            }
            ++count;
        }
        return std::max<std::size_t>(count, 1);
    }

    void MarqueeTextComponent::Update(const double elapsedSeconds)
    {
        if (!std::isfinite(elapsedSeconds))
            return;
        // Publish changed content before consulting the previous render's
        // measurement, so an old title's width cannot drive a new title.
        if (displayDirty_)
            RefreshDisplayedText();
        auto &visual = Visual();
        Owner().SetClipRect(visual.ContentBounds());
        const auto measured = visual.MeasuredSize();
        if (!measured || measured->width <= visual.ContentBounds().width)
        {
            elapsedSeconds_ = 0;
            offset_ = 0;
            visual.SetTextOffset({});
            visual.SetRepeatOffset(std::nullopt);
            return;
        }
        const double averageWidth = measured->width / static_cast<double>(characters_);
        const double period = measured->width + averageWidth * static_cast<double>(GapCharacters);
        const double previous = elapsedSeconds_;
        elapsedSeconds_ += std::max(elapsedSeconds, 0.0);
        // Count only the part of this update beyond the initial pause.
        const double activeSeconds =
            std::max(elapsedSeconds_ - EndPauseSeconds, 0.0) - std::max(previous - EndPauseSeconds, 0.0);
        offset_ += activeSeconds * averageWidth / secondsPerCharacter_;
        if (offset_ >= period)
        {
            offset_ = std::fmod(offset_, period);
            elapsedSeconds_ = 0;
        }
        visual.SetTextOffset({-static_cast<float>(offset_), 0});
        visual.SetRepeatOffset(mrg::visual2d::Point{static_cast<float>(period), 0});
    }

    void MarqueeTextComponent::RefreshDisplayedText()
    {
        auto &visual = Visual();
        visual.SetText(text_);
        visual.SetTextOffset({});
        visual.SetRepeatOffset(std::nullopt);
        displayDirty_ = false;
    }

    mrg::visual2d::TextVisualComponent &MarqueeTextComponent::Visual()
    {
        auto *visual = Owner().GetComponent<mrg::visual2d::TextVisualComponent>();
        if (!visual)
            throw std::logic_error("MarqueeTextComponent requires TextVisualComponent on its owner.");
        return *visual;
    }
} // namespace finger_drum::presentation
