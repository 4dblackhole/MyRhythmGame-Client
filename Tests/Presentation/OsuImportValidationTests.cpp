#include "GameScene/MusicSelectScene/Submodules/OsuImportValidation.h"
#include "Editing/ChartEditor.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    void Check(bool value, const char *message)
    {
        if (!value) throw std::runtime_error(message);
    }
    void Write(const fs::path &file, std::string_view text)
    {
        fs::create_directories(file.parent_path());
        std::ofstream stream(file, std::ios::binary);
        stream << text;
        Check(static_cast<bool>(stream), "Fixture write failed.");
    }
    std::string Read(const fs::path &file)
    {
        std::ifstream stream(file, std::ios::binary);
        Check(static_cast<bool>(stream), "Cannot read fixture/result.");
        return {std::istreambuf_iterator<char>{stream}, {}};
    }
    std::string JsonPath(const fs::path &path)
    {
        const auto value = path.generic_u8string();
        std::string out = "\"";
        for (const char8_t c : value)
        {
            if (c == u8'"' || c == u8'\\') out += '\\';
            out += static_cast<char>(c);
        }
        return out + '"';
    }
    std::string Manifest(const fs::path &root, std::string_view objects = "1", std::string_view rows = "2")
    {
        return "{\"version\":1,\"success\":true,\"files\":{\"ymm\":" + JsonPath(root / L"Music/음악.ymm") +
               ",\"ymp\":" + JsonPath(root / "Patterns/score.ymp") + ",\"yme\":" + JsonPath(root / "Patterns/score.yme") +
               "},\"sourceObjects\":" + std::string(objects) + ",\"noteRows\":" + std::string(rows) +
               ",\"warnings\":[{\"extra\":[true,false,null,-0.5e2]}]}";
    }
    void Rejected(std::string_view output, const fs::path &root)
    {
        bool rejected{};
        try { ValidateConvertedOsuResult(output, root); }
        catch (const std::exception &) { rejected = true; }
        Check(rejected, "Invalid result was accepted.");
    }
}

int wmain(int count, wchar_t *arguments[])
{
    try
    {
        if (count == 3)
        {
            ValidateConvertedOsuResult(Read(arguments[1]), arguments[2]);
            std::cout << "Saved converter result accepted by YOUMU.\n";
            return 0;
        }
        const auto root = fs::absolute(fs::path("build/osu-import-validation") / std::to_string(GetCurrentProcessId()) /
                                       L"한글 ' Songs");
        Check(!fs::exists(root), "Fixture must be new.");
        Write(root / L"Music/음악.ymm", "File: audio.wav\nMusic Name Count: 1\nMusic Name 1: Fixture\nArtist Count: 1\nArtist Composer: Test\n");
        Write(root / "Music/audio.wav", "fixture");
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.name = "Fixture";
        pattern.musicMetadataFile = L"Music/음악.ymm";
        pattern.effectFile = "score.yme";
        pattern.notes.push_back({.position = {0, {0, 1}}, .keyType = 12, .actionType = 1});
        pattern.notes.push_back({.position = {0, {1, 4}}, .keyType = 12, .actionType = 2});
        const auto patternFile = root / "Patterns/score.ymp";
        Write(patternFile, ChartEditor::WritePattern(pattern));
        Write(root / "Patterns/score.yme", ChartEditor::WriteEffects({}));
        const auto valid = Manifest(root);
        ValidateConvertedOsuResult(valid, root);
        // One long note has two serialized rows, but one playable/source object.
        Rejected(Manifest(root, "2"), root);
        Rejected(Manifest(root, "1", "3"), root);
        auto overlap = pattern;
        overlap.notes.insert(overlap.notes.begin() + 1,
                             {.position = {0, {1, 8}}, .keyType = 1, .actionType = 0});
        Write(patternFile, ChartEditor::WritePattern(overlap));
        Rejected(Manifest(root, "2", "3"), root);
        Write(patternFile, ChartEditor::WritePattern(pattern));
        for (const auto number : {"-1", "1.0", "01", "1.", "-.5", "184467440737095516160"})
            Rejected(Manifest(root, number), root);
        Rejected(valid + "false", root);
        Rejected("{}", root);
        Rejected("{\"version\":1,\"version\":1}", root);
        Rejected("{\"version\":1,\"success\":true,\"files\":{\"ymm\":\"\\uD800\"}}", root);
        auto duplicate = valid;
        duplicate.insert(1, "\"sourceObjects\":1,");
        Rejected(duplicate, root);
        Rejected(valid, root / "Patterns");
        auto unsupported = pattern;
        unsupported.mode = "Unsupported";
        Write(patternFile, ChartEditor::WritePattern(unsupported));
        Rejected(valid, root);
        pattern.notes[0].keyType = pattern.notes[1].keyType = 999;
        Write(patternFile, ChartEditor::WritePattern(pattern));
        Rejected(valid, root);
        Check(fs::exists(root / L"Music/음악.ymm") && Read(root / "Music/audio.wav") == "fixture",
              "Validation must preserve existing/shared files.");
        std::cout << "Osu import validation: Unicode paths, long-note counts, malformed results, mode rejection and file preservation passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
