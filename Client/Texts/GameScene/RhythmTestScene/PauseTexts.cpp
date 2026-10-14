#include "PauseTexts.h"
namespace finger_drum::texts
{
    const PauseTextSet &Pause(Language language) noexcept
    {
        static constexpr PauseTextSet ko{L"일시정지",
                                         L"클리어·HP 사용 불가",
                                         L"↑↓ 선택 · ←→ 값 조절 · Enter 실행",
                                         {L"Resume", L"Rewind", L"Retry", L"Retire"},
                                         L"{:.1f}초 전부터 재생 · 처리한 노트 유지",
                                         L"{}마디 전으로 이동 · 판정·점수 되돌림",
                                         L"처음부터 새 시도",
                                         L"남은 노트를 MISS로 처리하고 결과 보기",
                                         L"오디오 준비 중"};
        static constexpr PauseTextSet en{L"Paused",
                                         L"Clear / HP disabled",
                                         L"↑↓ Select · ←→ Adjust · Enter Confirm",
                                         {L"Resume", L"Rewind", L"Retry", L"Retire"},
                                         L"Resume {:.1f} s earlier · Keep processed notes",
                                         L"Rewind {} measures · Undo judgements / score",
                                         L"Start a new attempt",
                                         L"Mark remaining notes MISS and show results",
                                         L"Preparing audio"};
        return language == Language::Korean ? ko : en;
    }
} // namespace finger_drum::texts
