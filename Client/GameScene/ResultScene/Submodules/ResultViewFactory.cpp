#include "ResultViewFactory.h"
#include "Taiko/TaikoResultView.h"
#include <stdexcept>
std::unique_ptr<IResultView> CreateResultView(const IPlayResult &result,
                                              ScreenVisual2DManager &visuals,
                                              TextCatalog &texts, ResultViewContext context)
{
    if (const auto *taiko = dynamic_cast<const TaikoPlayResult *>(&result))
        return std::make_unique<TaikoResultView>(*taiko, visuals, texts, std::move(context));
    throw std::invalid_argument("Unsupported result mode: " + std::string(result.ModeId()));
}
