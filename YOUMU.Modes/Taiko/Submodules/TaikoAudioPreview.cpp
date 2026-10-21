#include "TaikoAudioPreview.h"
#include "TaikoChartAudio.h"
#include "TaikoChartNotes.h"
#include "TaikoNoteOptions.h"
#include <algorithm>
#include <stdexcept>

namespace
{
    std::size_t PreviewTickDivision(const PatternNote &head)
    {
        std::vector<Diagnostic> diagnostics;
        const auto division = ReadPositiveOption(head, TickDivision, 16, diagnostics);
        if (!division)
            throw std::invalid_argument(diagnostics.front().message);
        return *division;
    }
    TaikoAction BuzzAction(const PatternNote &head)
    {
        std::vector<Diagnostic> diagnostics;
        const auto action = ReadBuzzAction(head, diagnostics);
        if (!action)
            throw std::invalid_argument(diagnostics.front().message);
        return *action;
    }
} // namespace
std::vector<TaikoPreviewCue> BuildTaikoAudioPreview(const PatternDocument &,
                                                    const EffectDocument &effects,
                                                    const MusicalTimeline &timeline,
                                                    const std::vector<CompiledPatternNote> &notes)
{
    std::vector<TaikoPreviewCue> cues;
    const auto append = [&](PatternNote note, TaikoAction action, RhythmTime time,
                            bool big = false) {
        if (effects.hitSounds.contains(note.hitSound))
            note.hitSound = ChartSoundId(note.hitSound);
        cues.push_back(
            {time, ResolveSound(NoteSound(note, action, big), time, effects, timeline)});
    };
    std::optional<CompiledPatternNote> head;
    for (const auto &compiled : PlayableInSourceOrder(notes))
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
                    timeline.CompileSubdivisions(head->note.position, note.position, PreviewTickDivision(head->note));
                // The first tick/head was emitted once above. TickRoll's
                // freely chosen input is represented by Don in the preview.
                const std::size_t firstBodyTick =
                    head->positionActive && !ticks.empty() && ticks.front() == head->timing ? 1 : 0;
                for (std::size_t i = firstBodyTick; i < ticks.size(); ++i)
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
