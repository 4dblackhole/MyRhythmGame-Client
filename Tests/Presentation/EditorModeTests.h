#pragma once
#include "MRG_Core.h"
#include "Texts/TextCatalog.h"
#include <functional>

void TestEditorModes(mrg::visual2d::ScreenVisual2DManager &, const mrg::EngineServices &,
                     finger_drum::texts::TextCatalog &,
                     const std::function<const std::vector<mrg::visual2d::DrawPacket> &()> &);
