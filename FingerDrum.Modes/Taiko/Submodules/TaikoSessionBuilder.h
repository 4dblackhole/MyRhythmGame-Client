#pragma once
#include "Taiko/TaikoMode.h"
#include "Note/Submodules/NoteSoundPolicy.h"

namespace finger_drum::mode
{
    class TaikoSessionBuilder final
    {
      public:
        static ModeLoadResult CreateSession(const chart::PatternDocument &pattern,
                                            const chart::EffectDocument &effects);

      private:
        static void AddLongNote(const chart::CompiledPatternNote &head,
                                const chart::CompiledPatternNote &tail,
                                const chart::MusicalTimeline &timeline,
                                const std::shared_ptr<const rhythm::JudgementProfile> &profile,
                                rhythm::Lane &lane, PlaySession &session, rhythm::NoteId &nextId,
                                std::vector<chart::Diagnostic> &diagnostics);
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy> MakeTapSoundPolicy(
            std::string soundId);
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy> MakeBigSoundPolicy(
            std::string soundId);
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy> MakeTickSoundPolicy(
            std::string soundId);
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy> MakeBalloonSoundPolicy(
            std::string soundId);
        [[nodiscard]] static std::shared_ptr<const rhythm::INoteSoundPolicy>
        MakeAlternatingSoundPolicy(std::size_t hitCount, std::string soundId);
    };
} // namespace finger_drum::mode
