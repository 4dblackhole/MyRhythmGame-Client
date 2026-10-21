#pragma once
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <functional>

void TestEditorModes(ScreenVisual2DManager &, const EngineServices &,
                     TextCatalog &,
                     const std::function<const std::vector<DrawPacket> &()> &);
