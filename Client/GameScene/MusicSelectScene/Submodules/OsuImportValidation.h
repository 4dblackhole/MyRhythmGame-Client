#pragma once
#include <filesystem>
#include <string_view>

// The Client owns playability checks after the converter has saved its files.
// Throws on a malformed result, inconsistent file references or lost notes.
void ValidateConvertedOsuResult(std::string_view output, const std::filesystem::path &songsRoot);
