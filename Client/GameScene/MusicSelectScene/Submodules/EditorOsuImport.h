#pragma once
#include <filesystem>
#include <future>
#include <optional>
#include <string>

class TextCatalog;

std::optional<std::filesystem::path> SelectOsuFile(const TextCatalog &texts);

struct EditorOsuImportResult
{
    bool success{};
    std::string message;
};
void ShowOsuImportCompletion(const EditorOsuImportResult &result, const TextCatalog &texts);
// Worker owns only copied paths and its child process, never Scene/audio/UI objects.
class EditorOsuImport final
{
  public:
    void Start(std::filesystem::path input, std::filesystem::path songsRoot);
    [[nodiscard]] bool Busy() const noexcept
    {
        return job_.valid();
    }
    std::optional<EditorOsuImportResult> Poll();

  private:
    std::future<EditorOsuImportResult> job_;
};
