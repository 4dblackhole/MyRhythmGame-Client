#pragma once
#include "Catalog/SongCatalog.h"
#include "Texts/TextCatalog.h"
#include <optional>

// Returns only a fully written pair. Cancel and validation failures create no chart.
std::optional<finger_drum::chart::SongCatalogPattern> CreateDifficultyWithDialog(
    const std::filesystem::path &songsRoot, const finger_drum::chart::SongCatalogEntry &song,
    const finger_drum::texts::TextCatalog &texts);
