#include "MusicSelectTexts.h"

namespace finger_drum::texts
{
    const MusicSelectTextSet &MusicSelect(const Language language) noexcept
    {
        static constexpr MusicSelectTextSet Korean{
            L"전체", L"개인 기록", L"기록 없음", L"배경 미리보기", L"이미지 없음",
            L"선택한 곡 없음", L"난이도", L"제작자",
            L"레벨 —   BPM —   노트 —   모드 —", L"곡 검색", L"{}곡",
            {L"난이도순", L"곡 이름순", L"아티스트 이름순"},
            L"← / →  곡 이동     ↑ / ↓  난이도     ENTER  시작", L"옵션", L"뒤로",
            L"시작", L"사용 가능한 곡 없음", L"난이도 없음", L"난이도 없음",
            L"레벨 —   BPM {}   노트 {}   모드 {}", L"카탈로그 오류: ",
            L"지원하지 않는 모드: ", L"패턴 오류: ", L"알 수 없는 차트 오류"};
        static constexpr MusicSelectTextSet English{
            L"ALL", L"PERSONAL RECORD", L"NO RECORDS", L"BACKGROUND PREVIEW", L"NO IMAGE",
            L"NO SONG SELECTED", L"DIFFICULTY", L"CREATOR",
            L"LEVEL —   BPM —   NOTES —   MODE —", L"SEARCH SONGS", L"{} SONGS",
            {L"DIFFICULTY", L"SONG NAME", L"ARTIST NAME"},
            L"← / →  MOVE SONG     ↑ / ↓  DIFFICULTY     ENTER  GO", L"OPTION SELECT",
            L"BACK", L"GO", L"NO SONGS AVAILABLE", L"NO DIFFICULTIES", L"NO DIFFICULTY",
            L"LEVEL —   BPM {}   NOTES {}   MODE {}", L"CATALOG ERROR: ",
            L"UNSUPPORTED MODE: ", L"PATTERN ERROR: ", L"Unknown chart error"};
        return language == Language::Korean ? Korean : English;
    }
} // namespace finger_drum::texts
