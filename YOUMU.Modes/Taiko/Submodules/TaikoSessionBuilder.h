#pragma once
#include "Taiko/TaikoMode.h"
#include "Note/Submodules/NoteSoundPolicy.h"

class TaikoSessionBuilder final
{
  public:
    static ModeLoadResult CreateSession(const PatternDocument &pattern, const EffectDocument &effects,
                                        const PlaySessionOptions &options, ModeInputLayout inputLayout);

  private:
    static void AddLongNote(const CompiledPatternNote &head,
                            const CompiledPatternNote &tail,
                            const MusicalTimeline &timeline,
                            const std::shared_ptr<const AccuracyRange> &profile,
                            Lane &lane, PlaySession &session, NoteId &nextId,
                            std::vector<Diagnostic> &diagnostics);
    [[nodiscard]] static std::shared_ptr<const INoteSoundPolicy> MakeTapSoundPolicy(
        std::string soundId);
    [[nodiscard]] static std::shared_ptr<const INoteSoundPolicy> MakeBigSoundPolicy(
        std::string soundId);
    [[nodiscard]] static std::shared_ptr<const INoteSoundPolicy> MakeTickSoundPolicy(
        const std::string &soundId);
    [[nodiscard]] static std::shared_ptr<const INoteSoundPolicy> MakeBalloonSoundPolicy(
        std::string soundId);
    [[nodiscard]] static std::shared_ptr<const INoteSoundPolicy> MakeAlternatingSoundPolicy(
        const std::string &soundId);
};
