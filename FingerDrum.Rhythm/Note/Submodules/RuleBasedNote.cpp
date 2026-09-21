#include "Note/Submodules/RuleBasedNote.h"
#include "Note/Submodules/RuleHelpers.h"
#include <algorithm>
#include <format>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace finger_drum::rhythm
{
    using namespace detail;

    RuleBasedNote::RuleBasedNote(const NoteId id, const RhythmTime timing,
                                 std::shared_ptr<const JudgementProfile> profile,
                                 std::unique_ptr<INoteRule> rule,
                                 std::shared_ptr<const INoteSoundPolicy> soundPolicy)
        : id_(id), timing_(timing), profile_(std::move(profile)), rule_(std::move(rule)),
          soundPolicy_(std::move(soundPolicy))
    {
        if (profile_ == nullptr || rule_ == nullptr)
        {
            throw std::invalid_argument("A rule-based note requires a profile and input rule.");
        }
        accuracy_.noteId = id_;
        accuracy_.target = rule_->AccuracyTarget();
        if (accuracy_.target.kind == NoteAccuracyKind::TimingHits ||
            accuracy_.target.kind == NoteAccuracyKind::Hold)
        {
            accuracy_.hitScoreRates.resize(accuracy_.target.hits);
        }
    }

    RuleBasedNote::~RuleBasedNote() = default;

    const NoteAccuracy &RuleBasedNote::Accuracy() const noexcept
    {
        return accuracy_;
    }

    NoteId RuleBasedNote::Id() const noexcept
    {
        return id_;
    }

    RhythmTime RuleBasedNote::Timing() const noexcept
    {
        return timing_;
    }

    RhythmTime RuleBasedNote::ExpireTime() const noexcept
    {
        return rule_->ExpireTime(Context());
    }

    NoteState RuleBasedNote::State() const noexcept
    {
        return rule_->State();
    }

    std::optional<NoteProgress> RuleBasedNote::Progress() const noexcept
    {
        return rule_->Progress();
    }

#if defined(_DEBUG)
    std::wstring RuleBasedNote::DebugText() const
    {
        std::wstring result =
            std::format(L"Note #{}  {}  timing={} us  expire={} us", id_, StateName(State()),
                        timing_.count(), ExpireTime().count());
        if (const std::optional<NoteProgress> progress = Progress())
        {
            result += std::format(L"  progress={}/{}", progress->accepted, progress->required);
        }
        return result + L"\n" + accuracy_.DebugText();
    }
#endif

    const JudgementProfile &RuleBasedNote::Profile() const noexcept
    {
        return *profile_;
    }

    JudgementResult RuleBasedNote::Preview(const RhythmInputEvent &input) const noexcept
    {
        return profile_->Evaluate(timing_, input.time);
    }

    bool RuleBasedNote::CanAccept(const RhythmInputEvent &input) const noexcept
    {
        const JudgementResult judgement = Preview(input);
        return rule_->CanAccept(Context(), input, judgement);
    }

    NoteProcessResult RuleBasedNote::ProcessInput(const RhythmInputEvent &input)
    {
        NoteProcessResult result;
        const JudgementResult judgement = Preview(input);
        rule_->ProcessInput(Context(), input, judgement, result);
        for (NoteEvent &event : result.events)
        {
            if (event.type == NoteEventType::HitAccepted ||
                event.type == NoteEventType::TickAccepted)
            {
                event.inputAction = input.action;
            }
        }
        AccumulateAccuracy(result);
        AppendAudioCues(result);
        return result;
    }

    NoteProcessResult RuleBasedNote::Update(const NoteUpdateContext &update)
    {
        NoteProcessResult result;
        rule_->Update(Context(), update, result);
        AccumulateAccuracy(result);
        AppendAudioCues(result);
        return result;
    }

    NoteProcessResult RuleBasedNote::MarkMissed(const RhythmTime time)
    {
        NoteProcessResult result;
        rule_->MarkMissed(Context(), time, result);
        AccumulateAccuracy(result);
        AppendAudioCues(result);
        return result;
    }

    void RuleBasedNote::Reset() noexcept
    {
        rule_->Reset();
        std::ranges::fill(accuracy_.hitScoreRates, 0.0);
        accuracy_.acceptedHits = 0;
        accuracy_.acceptedTicks = 0;
        accuracy_.finalized = false;
    }

    void RuleBasedNote::AccumulateAccuracy(NoteProcessResult &result)
    {
        if (accuracy_.finalized)
        {
            return;
        }
        for (const NoteEvent &event : result.events)
        {
            if (event.type == NoteEventType::HitAccepted)
            {
                if (event.hitIndex < accuracy_.hitScoreRates.size())
                {
                    accuracy_.hitScoreRates[event.hitIndex] = event.judgement.scoreRate;
                }
                ++accuracy_.acceptedHits;
            }
            else if (event.type == NoteEventType::TickAccepted)
            {
                if (accuracy_.target.kind == NoteAccuracyKind::HitCount)
                {
                    ++accuracy_.acceptedHits;
                }
                else
                {
                    ++accuracy_.acceptedTicks;
                }
            }
            else if (event.type == NoteEventType::Completed || event.type == NoteEventType::Missed)
            {
                accuracy_.finalized = true;
                result.finalizedAccuracies.push_back(accuracy_);
                break;
            }
        }
    }

    NoteRuleContext RuleBasedNote::Context() const noexcept
    {
        return {id_, timing_, *profile_};
    }

    void RuleBasedNote::AppendAudioCues(NoteProcessResult &result) const
    {
        if (soundPolicy_ == nullptr)
        {
            return;
        }
        for (const NoteEvent &event : result.events)
        {
            soundPolicy_->AppendAudioCues(event, result.audioCues);
        }
    }
} // namespace finger_drum::rhythm
