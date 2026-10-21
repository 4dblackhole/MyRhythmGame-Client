#include "Note/Submodules/RuleBasedNote.h"
#include "Note/Submodules/DrumRollInputRule.h"
#include "Note/Submodules/TickRollInputRule.h"
#include "Note/Submodules/TimedSequenceInputRule.h"
#include "Note/Submodules/HoldInputRule.h"
#include "TaikoBuildSupport.h"
#include "TaikoSessionBuilder.h"

void TaikoSessionBuilder::AddLongNote(const CompiledPatternNote &compiledHead,
                                      const CompiledPatternNote &compiledTail,
                                      const MusicalTimeline &timeline,
                                      const std::shared_ptr<const AccuracyRange> &profile,
                                      Lane &lane, PlaySession &session, NoteId &nextId,
                                      std::vector<Diagnostic> &diagnostics)
{
    const PatternNote &head = compiledHead.note;
    const PatternNote &tail = compiledTail.note;
    const RhythmTime begin = compiledHead.timing;
    const RhythmTime end = compiledTail.timing;
    if (end < begin)
    {
        diagnostics.push_back({DiagnosticSeverity::Warning, tail.source,
                               "Ignored a long note whose compiled end precedes its start."});
        return;
    }

    // Translate only the Taiko-specific options here. The resulting rule
    // and sound policy remain backend-neutral rhythm objects.
    const TaikoNoteType type = static_cast<TaikoNoteType>(head.keyType);
    std::unique_ptr<INoteRule> rule;
    std::shared_ptr<const INoteSoundPolicy> soundPolicy;
    std::optional<TaikoAction> buzzAction;
    std::vector<RhythmTime> tickTimes;
    switch (type)
    {
    case TaikoNoteType::Roll:
    case TaikoNoteType::BigRoll: {
        const auto hitCount = ReadHitCount(
            head, std::max(std::size_t{1}, timeline.CountSubdivisions(head.position, tail.position, 12)),
            diagnostics);
        if (!hitCount.has_value())
        {
            break;
        }
        rule = std::make_unique<DrumRollInputRule>(
            std::vector<NoteAction>{ActionValue(TaikoAction::Don), ActionValue(TaikoAction::Kat)}, end,
            *hitCount);
        soundPolicy = MakeTickSoundPolicy(head.hitSound);
        break;
    }
    case TaikoNoteType::TickRoll:
    case TaikoNoteType::BigTickRoll:
        if (const auto division =
                ReadPositiveOption(head, TickDivision, 16, diagnostics))
        {
            tickTimes = timeline.CompileSubdivisions(head.position, tail.position, *division);
            rule = std::make_unique<TickRollInputRule>(
                std::vector<NoteAction>{ActionValue(TaikoAction::Don), ActionValue(TaikoAction::Kat)}, end,
                tickTimes);
            soundPolicy = MakeTickSoundPolicy(head.hitSound);
        }
        break;
    case TaikoNoteType::Balloon:
        if (const auto hitCount = ReadHitCount(
                head, std::max(std::size_t{1}, timeline.CountSubdivisions(head.position, tail.position, 12)),
                diagnostics))
        {
            rule = std::make_unique<TimedSequenceInputRule>(
                std::vector<NoteAction>{ActionValue(TaikoAction::Don)}, *hitCount, end);
            soundPolicy = MakeBalloonSoundPolicy(head.hitSound);
        }
        break;
    case TaikoNoteType::DengDeng:
        if (const auto hitCount = ReadHitCount(
                head, std::max(std::size_t{1}, timeline.CountSubdivisions(head.position, tail.position, 12)),
                diagnostics))
        {
            rule = std::make_unique<TimedSequenceInputRule>(
                std::vector<NoteAction>{ActionValue(TaikoAction::Don), ActionValue(TaikoAction::Kat)},
                *hitCount, end, true);
            soundPolicy = MakeAlternatingSoundPolicy(head.hitSound);
        }
        break;
    case TaikoNoteType::Buzz:
        buzzAction = ReadBuzzAction(head, diagnostics);
        if (const auto division =
                ReadPositiveOption(head, TickDivision, 16, diagnostics);
            buzzAction.has_value() && division.has_value())
        {
            tickTimes = timeline.CompileSubdivisions(head.position, tail.position, *division);
            // The head has its own timing score, not a body tick.
            if (compiledHead.positionActive && !tickTimes.empty() && tickTimes.front() == begin)
            {
                tickTimes.erase(tickTimes.begin());
            }
            rule = std::make_unique<HoldInputRule>(ActionValue(*buzzAction), end, tickTimes);
            const std::string buzzSound = NoteSound(head, *buzzAction);
            auto buzzPolicy = std::make_shared<MappedNoteSoundPolicy>();
            buzzPolicy->Bind(
                SoundBinding{.eventType = NoteEventType::HitAccepted, .cue = Cue(buzzSound)});
            buzzPolicy->Bind(SoundBinding{.eventType = NoteEventType::TickAccepted,
                                                  .cue = Cue(buzzSound, "TickSound")});
            soundPolicy = std::move(buzzPolicy);
        }
        break;
    default:
        diagnostics.push_back({DiagnosticSeverity::Error, head.source,
                               "The long-note key type is not supported by Taiko mode."});
        return;
    }
    if (rule == nullptr || soundPolicy == nullptr)
    {
        return;
    }

    // One rule-based note owns the complete interval; presentation keeps
    // the tail time separate from its input and scoring rule.
    const NoteId noteId = nextId++;
    lane.AddNote(
        std::make_unique<RuleBasedNote>(noteId, begin, profile, std::move(rule), std::move(soundPolicy)));
    session.SetNotePresentation(noteId, {LongNoteVisualId(type, buzzAction), end, true, std::move(tickTimes)});
}
