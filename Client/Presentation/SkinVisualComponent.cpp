#include "SkinVisualComponent.h"
#include "App/SkinSetSelection.h"
#include <stdexcept>
#include <utility>

namespace finger_drum::presentation
{
    namespace v = mrg::visual2d;
    SkinVisualComponent::SkinVisualComponent(v::ScreenVisual2DManager &visuals, std::string scene, std::string key,
                                             v::VisualStyle base, float radius)
        : visuals_(visuals), scene_(std::move(scene)), key_(std::move(key)), base_(base), radius_(radius)
    {
    }
    void SkinVisualComponent::Configure(std::string key, std::string decoration)
    {
        if (key_ != key)
            loaded_ = false;
        key_ = std::move(key);
        decoration_ = std::move(decoration);
        Refresh();
        Apply();
    }
    void SkinVisualComponent::SetBaseStyle(const v::VisualStyle &style)
    {
        base_ = style;
        Apply();
    }
    void SkinVisualComponent::Update(double)
    {
        if (Refresh())
            Apply();
    }
    bool SkinVisualComponent::Refresh()
    {
        const auto &skin = mrg_client::SkinSetSelection::Instance();
        if (loaded_ && revision_ == skin.Revision())
            return false;
        const auto load = [&](const std::string &key)
        {
            const auto path = skin.Image(scene_, key);
            return path.empty() ? v::ImageHandle{} : visuals_.RegisterImage(path);
        };
        images_ = {};
        images_.normalImage = load(key_);
        images_.hoveredImage = load(key_ + ".Hovered");
        images_.pressedImage = load(key_ + ".Pressed");
        images_.disabledImage = load(key_ + ".Disabled");
        revision_ = skin.Revision();
        loaded_ = true;
        return true;
    }
    void SkinVisualComponent::Apply()
    {
        auto *sprite = Owner().GetComponent<v::SpriteVisualComponent>();
        if (!sprite)
            throw std::logic_error("A skin image binding requires a sprite component.");
        v::VisualStyle style = base_;
        const auto image = images_.normalImage;
        if (image)
        {
            style.normalImage = image;
            style.hoveredImage = images_.hoveredImage ? images_.hoveredImage : image;
            style.pressedImage = images_.pressedImage ? images_.pressedImage : image;
            style.disabledImage = images_.disabledImage ? images_.disabledImage : image;
            // Keep the authored RGB instead of multiplying it by the default
            // panel color. The node still controls visibility/input/opacity.
            style.normal = {1, 1, 1, base_.normal.alpha};
            style.hovered = {1, 1, 1, base_.hovered.alpha};
            style.pressed = {1, 1, 1, base_.pressed.alpha};
            style.disabled = {1, 1, 1, base_.disabled.alpha};
        }
        sprite->SetStyle(style);
        sprite->SetCornerRadius(image ? 0.0F : radius_);
        if (!decoration_.empty())
            for (const auto &child : Owner().Children())
                if (child->Name() == decoration_)
                    child->SetVisible(!image);
    }
    void BindSkinImage(v::Visual2DNode &node, v::ScreenVisual2DManager &visuals, std::string scene, std::string key,
                       std::string decoration)
    {
        auto *binding = node.GetComponent<SkinVisualComponent>();
        if (!binding)
        {
            const auto *sprite = node.GetComponent<v::SpriteVisualComponent>();
            if (!sprite)
                throw std::logic_error("A skin image binding requires a sprite component.");
            binding = &node.AddComponent<SkinVisualComponent>(visuals, std::move(scene), key, sprite->Style(),
                                                              sprite->CornerRadius());
        }
        binding->Configure(std::move(key), std::move(decoration));
    }
    void SetSkinBaseStyle(v::Visual2DNode &node, const v::VisualStyle &style)
    {
        if (auto *binding = node.GetComponent<SkinVisualComponent>())
            binding->SetBaseStyle(style);
        else if (auto *sprite = node.GetComponent<v::SpriteVisualComponent>())
            sprite->SetStyle(style);
        else
            throw std::logic_error("A skin style requires a sprite component.");
    }
} // namespace finger_drum::presentation
