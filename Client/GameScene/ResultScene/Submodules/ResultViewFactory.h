#pragma once
#include "IResultView.h"
#include "Mode/Results/IPlayResult.h"
#include "Texts/TextCatalog.h"
#include <memory>
std::unique_ptr<IResultView> CreateResultView(const finger_drum::mode::IPlayResult &result,
                                              mrg::visual2d::ScreenVisual2DManager &visuals,
                                              finger_drum::texts::TextCatalog &texts, ResultViewContext context = {});
