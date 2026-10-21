#pragma once
#include "Catalog/SongCatalog.h"
#include <stdexcept>
#include <utility>

enum class Failure
{
    InvalidName,
    ExistingFolder,
    InvalidMetadata
};
class Error final : public std::runtime_error
{
  public:
    Error(Failure failure, std::string detail) : std::runtime_error(std::move(detail)), reason(failure)
    {
    }
    Failure reason;
};
// Only metadata is taken from the draft. Notes, timing commands and YME
// automation are empty; the chosen song's YMM remains shared and untouched.
[[nodiscard]] SongCatalogPattern Create(const std::filesystem::path &songsRoot,
                                                            const SongCatalogEntry &song,
                                                            const PatternDocument &metadata);
