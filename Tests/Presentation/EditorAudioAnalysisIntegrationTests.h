#pragma once
#include <filesystem>

// Actual shared PCM decoders are a Client integration concern.
void RunEditorAudioAnalysisIntegrationTests(const std::filesystem::path &songsRoot);
