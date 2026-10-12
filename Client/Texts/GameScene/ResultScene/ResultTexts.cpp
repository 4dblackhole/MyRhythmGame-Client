#include "ResultTexts.h"
namespace finger_drum::texts
{
    const ResultTextSet &Result(Language language) noexcept
    {
        static constexpr ResultTextSet Korean{L"플레이 결과",
                                              L"완료",
                                              L"중도 종료",
                                              L"최종 정확도  {:.6f}%",
                                              L"전체 {}  ·  처리 {}  ·  미처리 {}",
                                              {L"일반 노트", L"큰 노트 · 보라 노트", L"롤 · 틱롤 · 풍선 · 뎅뎅 · 버즈"},
                                              L"{}   {:.6f}%    ({}/{})",
                                              L"오차 평균 {:.3f}ms  ·  분산 {:.3f}ms²  ·  표준편차 {:.3f}ms",
                                              L"타이밍 판정 입력 없음",
                                              L"시간별 오차",
                                              L"오차 히스토그램",
                                              L"{:.2f}s → {:.2f}s",
                                              L"빠름  −{:.1f}ms    ·    0    ·    늦음  +{:.1f}ms",
                                              L"곡 선택으로 (ESC / Enter)",
                                              L"입력 기록 {}개 (µs)",
                                              L"MAX~BAD: 타이밍 입력 · OK/MISS: 완료 결과 · POOR: 오입력",
                                              L"유지/틱롤 틱: 성공 {} · 실패 {}"};
        static constexpr ResultTextSet English{
            L"Play Result",
            L"Completed",
            L"Interrupted",
            L"Final accuracy  {:.6f}%",
            L"Total {}  ·  Processed {}  ·  Unprocessed {}",
            {L"Normal notes", L"Big / Purple notes", L"Roll / Tick roll / Balloon / DengDeng / Buzz"},
            L"{}   {:.6f}%    ({}/{})",
            L"Mean {:.3f}ms  ·  Variance {:.3f}ms²  ·  Std. deviation {:.3f}ms",
            L"No timing judgement inputs",
            L"Error over time",
            L"Error histogram",
            L"{:.2f}s → {:.2f}s",
            L"Early  −{:.1f}ms    ·    0    ·    Late  +{:.1f}ms",
            L"Song select (ESC / Enter)",
            L"{} input events (µs)",
            L"MAX–BAD: timed hits · OK/MISS: completion results · POOR: wrong inputs",
            L"Hold / tick roll ticks: accepted {} · missed {}"};
        return language == Language::Korean ? Korean : English;
    }
} // namespace finger_drum::texts
