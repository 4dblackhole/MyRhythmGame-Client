#pragma once

#include "MRG_Core.h"

#include "Audio/GameplayAudioRouter.h"
#include "Mode/PlayGameMode.h"
#include "Time/RhythmTimer.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

// 새 리듬게임 계층을 한 화면에서 검증하는 임시 드라이버다. QPC가
// 기록된 Raw Input을 RhythmTimer로 변환하고, 태고형 GameMode가 만든
// 노트/판정/사운드 요청을 Visual2D와 FMOD 재생 계층에 전달한다.
class RhythmTestScene final : public mrg::scene::GameScene
{
public:
    void Initialize(const mrg::EngineServices& services) override;
    void Update(
        const mrg::UpdateContext& context,
        mrg::scene::SceneManager& scenes) override;
    void Render(const mrg::graphics::RenderContext& context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

private:
    [[nodiscard]] std::unique_ptr<finger_drum::mode::PlaySession>
        CreateDemoSession();
    void CreatePresentation();
    void CreateNoteVisuals();
    void StartTimeline(const mrg::audio::AudioClockSnapshot& clock);
    void ResetTimeline(const mrg::audio::AudioClockSnapshot& clock);
    void ProcessControlKeys(
        const mrg::platform::InputState& input,
        const mrg::audio::AudioClockSnapshot& clock);
    void ProcessRhythmInput(const mrg::platform::InputState& input);
    void UpdateSession(
        const mrg::platform::InputState& input,
        finger_drum::rhythm::RhythmTime time);
    void ConsumeResult(finger_drum::rhythm::NoteProcessResult result);
    void UpdatePresentation(finger_drum::rhythm::RhythmTime time);
    [[nodiscard]] bool IsPatternComplete() const noexcept;
    [[nodiscard]] bool ReturnToLobby(
        mrg::scene::SceneManager& scenes) const;

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    std::unique_ptr<mrg::visual2d::Visual2DCanvas> canvas_;
    std::unique_ptr<finger_drum::mode::PlaySession> session_;
    finger_drum::rhythm::RhythmTimer timer_;
    finger_drum::audio::GameplayAudioRouter audioRouter_;
    std::unordered_map<finger_drum::rhythm::NoteId,
        mrg::visual2d::Visual2DNode*> noteVisuals_;
    mrg::visual2d::Visual2DNode* timelineLabel_{};
    mrg::visual2d::Visual2DNode* resultLabel_{};
    mrg::visual2d::Visual2DNode* audioStatusLabel_{};
    std::uint64_t acceptedHitCount_{};
    double accumulatedScore_{};
    double completedElapsedSeconds_{};
};
