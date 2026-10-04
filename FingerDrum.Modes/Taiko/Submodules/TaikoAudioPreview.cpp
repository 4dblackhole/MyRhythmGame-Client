#include "TaikoAudioPreview.h"
#include "TaikoChartAudio.h"
#include "TaikoNoteOptions.h"
#include <algorithm>
#include <stdexcept>

namespace finger_drum::mode
{
    namespace
    {
        std::size_t TickDivision(const chart::PatternNote &head)
        {
            std::vector<chart::Diagnostic> diagnostics;
            const auto division = taiko_options::ReadPositiveOption(head, taiko_option::TickDivision, 16, diagnostics);
            if (!division)
                throw std::invalid_argument(diagnostics.front().message);
            return *division;
        }
        TaikoAction BuzzAction(const chart::PatternNote &head)
        {
            std::vector<chart::Diagnostic> diagnostics;
            const auto action = taiko_options::ReadBuzzAction(head, diagnostics);
            if (!action)
                throw std::invalid_argument(diagnostics.front().message);
            return *action;
        }
    } // namespace
    std::vector<TaikoPreviewCue> BuildTaikoAudioPreview(const chart::PatternDocument &pattern,
                                                        const chart::EffectDocument &effects,
                                                        const chart::MusicalTimeline &timeline,
                                                        const std::vector<chart::CompiledPatternNote> &notes)
    {
        std::vector<TaikoPreviewCue> cues;
        const auto append = [&](chart::PatternNote note, TaikoAction action, rhythm::RhythmTime time,
                                bool big = false) {
            if (pattern.hitSounds.contains(note.hitSound))
                note.hitSound = taiko_audio::ChartSoundId(note.hitSound);
            cues.push_back(
                {time, taiko_audio::ResolveSound(taiko_audio::NoteSound(note, action, big), time, effects, timeline)});
        };
        std::optional<chart::CompiledPatternNote> head;
        for (const auto &compiled : notes)
        {
            const auto &note = compiled.note;
            const auto type = static_cast<TaikoNoteType>(note.keyType);
            const auto action = static_cast<TaikoPatternAction>(note.actionType);
            if (action == TaikoPatternAction::LongNoteStart)
            {
                if (!head)
                    head = compiled;
                continue;
            }
            if (action == TaikoPatternAction::LongNoteEnd)
            {
                if (!head || head->note.keyType != note.keyType)
                    continue;
                const auto input = type == TaikoNoteType::Buzz ? BuzzAction(head->note) : TaikoAction::Don;
                append(head->note, input, head->timing);
                if (type == TaikoNoteType::TickRoll || type == TaikoNoteType::BigTickRoll ||
                    type == TaikoNoteType::Buzz)
                {
                    const auto ticks =
                        timeline.CompileSubdivisions(head->note.position, note.position, TickDivision(head->note));
                    // The first tick/head was emitted once above. TickRoll's
                    // freely chosen input is represented by Don in the preview.
                    for (std::size_t i = 1; i < ticks.size(); ++i)
                        append(head->note, input, ticks[i]);
                }
                head.reset();
                continue;
            }
            if (action != TaikoPatternAction::Down || head)
                continue;
            const auto input =
                type == TaikoNoteType::Kat || type == TaikoNoteType::BigKat ? TaikoAction::Kat : TaikoAction::Don;
            append(note, input, compiled.timing, type == TaikoNoteType::BigDon || type == TaikoNoteType::BigKat);
            if (type == TaikoNoteType::Purple)
                append(note, TaikoAction::Kat, compiled.timing);
        }
        std::ranges::stable_sort(cues, {}, &TaikoPreviewCue::time);
        return cues;
    }
} // namespace finger_drum::mode
