#pragma once
#include "IResultView.h"
#include "Mode/Results/IPlayResult.h"
#include "Texts/TextCatalog.h"
#include <memory>
std::unique_ptr<IResultView> CreateResultView(const IPlayResult &result,
                                              ScreenVisual2DManager &visuals,
                                              TextCatalog &texts, ResultViewContext context = {});
