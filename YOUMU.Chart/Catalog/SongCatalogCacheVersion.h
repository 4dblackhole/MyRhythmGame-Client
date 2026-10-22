#pragma once
#include <cstdint>

inline constexpr std::uint32_t SongCatalogCacheSchema = 1;
// Bump whenever YMM/YMP/YME parsing, defaults, validation or list metadata
// semantics change, including newly accepted/rejected syntax. This is not a
// chart format version and never changes a user's original files.
inline constexpr std::uint32_t SongCatalogParserRevision = 1;
