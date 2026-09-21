#include "TaikoSessionBuilder.h"
#include "TaikoBuildSupport.h"

namespace finger_drum::mode
{
    using namespace taiko_build;

    std::shared_ptr<const rhythm::INoteSoundPolicy> TaikoSessionBuilder::MakeTapSoundPolicy(
        std::string soundId)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        policy->Bind(rhythm::SoundBinding{.eventType = rhythm::NoteEventType::HitAccepted,
                                          .cue = Cue(std::move(soundId))});
        return policy;
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy> TaikoSessionBuilder::MakeBigSoundPolicy(
        std::string soundId)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        policy->Bind(rhythm::SoundBinding{.eventType = rhythm::NoteEventType::HitAccepted,
                                          .maximumGrade = rhythm::JudgementGrade::Good,
                                          .hitIndex = 0,
                                          .cue = Cue(std::move(soundId)),
                                          .stopAfterMatch = true});
        return policy;
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy> TaikoSessionBuilder::MakeTickSoundPolicy(
        std::string soundId)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        for (const TaikoAction action : {TaikoAction::Don, TaikoAction::Kat})
        {
            policy->Bind(rhythm::SoundBinding{
                .eventType = rhythm::NoteEventType::TickAccepted,
                .cue = Cue(soundId.empty() ? (action == TaikoAction::Don
                                                  ? finger_drum::mode::taiko_sound::DonHit
                                                  : finger_drum::mode::taiko_sound::KatHit)
                                           : soundId,
                           "TickSound"),
                .inputAction = ActionValue(action)});
        }
        return policy;
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy> TaikoSessionBuilder::MakeBalloonSoundPolicy(
        std::string soundId)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        policy->Bind(rhythm::SoundBinding{
            .eventType = rhythm::NoteEventType::HitAccepted,
            .cue =
                Cue(soundId.empty() ? finger_drum::mode::taiko_sound::DonHit : std::move(soundId),
                    "TickSound")});
        policy->Bind(rhythm::SoundBinding{.eventType = rhythm::NoteEventType::Completed,
                                          .cue = Cue(finger_drum::mode::taiko_sound::BalloonPop)});
        return policy;
    }

    std::shared_ptr<const rhythm::INoteSoundPolicy> TaikoSessionBuilder::MakeAlternatingSoundPolicy(
        const std::size_t hitCount, std::string soundId)
    {
        auto policy = std::make_shared<rhythm::MappedNoteSoundPolicy>();
        for (std::size_t index = 0; index < hitCount; ++index)
        {
            policy->Bind(rhythm::SoundBinding{
                .eventType = rhythm::NoteEventType::HitAccepted,
                .hitIndex = index,
                .cue = Cue(!soundId.empty() ? soundId
                           : index % 2 == 0 ? finger_drum::mode::taiko_sound::DonHit
                                            : finger_drum::mode::taiko_sound::KatHit,
                           "TickSound"),
                .stopAfterMatch = true});
        }
        return policy;
    }
} // namespace finger_drum::mode
