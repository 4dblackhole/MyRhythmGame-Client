#pragma once
#include "Catalog/SongCatalog.h"
#include "Texts/TextCatalog.h"
#include <optional>

// Returns only a fully written pair. Cancel and validation failures create no chart.
std::optional<SongCatalogPattern> CreateDifficultyWithDialog(
    const std::filesystem::path &songsRoot, const SongCatalogEntry &song,
    const TextCatalog &texts);
