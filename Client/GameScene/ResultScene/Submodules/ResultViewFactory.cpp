#include "ResultViewFactory.h"
#include "Taiko/TaikoResultView.h"
#include <stdexcept>
std::unique_ptr<IResultView> CreateResultView(const finger_drum::mode::IPlayResult &result,
                                              mrg::visual2d::ScreenVisual2DManager &visuals,
                                              finger_drum::texts::TextCatalog &texts, ResultViewContext context)
{
    if (const auto *taiko = dynamic_cast<const finger_drum::mode::TaikoPlayResult *>(&result))
        return std::make_unique<TaikoResultView>(*taiko, visuals, texts, std::move(context));
    throw std::invalid_argument("Unsupported result mode: " + std::string(result.ModeId()));
}
