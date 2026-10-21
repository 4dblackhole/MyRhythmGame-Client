#include "TaikoBuildSupport.h"
#include "TaikoSessionBuilder.h"

std::shared_ptr<const INoteSoundPolicy> TaikoSessionBuilder::MakeTapSoundPolicy(std::string soundId)
{
    auto policy = std::make_shared<MappedNoteSoundPolicy>();
    policy->Bind(
        SoundBinding{.eventType = NoteEventType::HitAccepted, .cue = Cue(std::move(soundId))});
    return policy;
}

std::shared_ptr<const INoteSoundPolicy> TaikoSessionBuilder::MakeBigSoundPolicy(std::string soundId)
{
    auto policy = std::make_shared<MappedNoteSoundPolicy>();
    policy->Bind(SoundBinding{.eventType = NoteEventType::HitAccepted,
                                      .maximumGrade = JudgementGrade::Good,
                                      .hitIndex = 0,
                                      .cue = Cue(std::move(soundId)),
                                      .stopAfterMatch = true});
    return policy;
}

std::shared_ptr<const INoteSoundPolicy> TaikoSessionBuilder::MakeTickSoundPolicy(const std::string &soundId)
{
    auto policy = std::make_shared<MappedNoteSoundPolicy>();
    for (const TaikoAction action : {TaikoAction::Don, TaikoAction::Kat})
    {
        policy->Bind(SoundBinding{
            .eventType = NoteEventType::TickAccepted,
            .cue = Cue(soundId.empty() ? InputSound(action) : soundId, "TickSound"),
            .inputAction = ActionValue(action)});
    }
    return policy;
}

std::shared_ptr<const INoteSoundPolicy> TaikoSessionBuilder::MakeBalloonSoundPolicy(std::string soundId)
{
    auto policy = std::make_shared<MappedNoteSoundPolicy>();
    policy->Bind(SoundBinding{
        .eventType = NoteEventType::HitAccepted,
        .cue = Cue(soundId.empty() ? DonHit : std::move(soundId), "TickSound")});
    policy->Bind(SoundBinding{.eventType = NoteEventType::Completed,
                                      .cue = Cue(BalloonPop)});
    return policy;
}

std::shared_ptr<const INoteSoundPolicy> TaikoSessionBuilder::MakeAlternatingSoundPolicy(
    const std::string &soundId)
{
    auto policy = std::make_shared<MappedNoteSoundPolicy>();
    for (const TaikoAction action : {TaikoAction::Don, TaikoAction::Kat})
    {
        policy->Bind(SoundBinding{
            .eventType = NoteEventType::HitAccepted,
            .cue = Cue(soundId.empty() ? InputSound(action) : soundId, "TickSound"),
            .stopAfterMatch = true,
            .inputAction = ActionValue(action)});
    }
    return policy;
}
