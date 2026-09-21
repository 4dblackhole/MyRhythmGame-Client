# YMM 음악 정보

코드: `FingerDrum.Chart/Parsing/Submodules/MusicParser.cpp`, `Model/Submodules/MusicDocument.h`.
검증: `Tests/Rhythm/Submodules/SongCatalogTests.cpp`.

UTF-8 텍스트이며 빈 줄과 `//`로 시작하는 줄을 무시합니다. 경로에는 `\\`와 `/`를 사용할 수 있습니다.

## YMM: 음악 메타데이터

YMM은 한 음원에 대한 정보입니다. 섹션 없이 `이름: 값` 형식으로 작성합니다.

```text
Version: 1
File: angel dream hand shaking.mp3

Music Name Count: 1
Music Name 1: Angel Dream Handshaking

Artist Count: 1
Artist Composer: Takahashi yoko

Tags: taiko no tatsujin,test
```

| 필드 | 의미 |
| --- | --- |
| `Version` | 정수 형식 버전입니다. 생략하면 1입니다. |
| `File` | 필수 음원 경로입니다. YMM 파일이 있는 폴더를 기준으로 해석합니다. |
| `Music Name ...` | 한 개 이상의 표시 곡명입니다. `Music Name Count` 자체는 개수 안내용이며 현재 파서는 사용하지 않습니다. |
| `Artist ...` | 한 개 이상의 표시 아티스트입니다. `Artist Count` 자체는 현재 파서가 사용하지 않습니다. |
| `Tags` | 쉼표로 구분한 태그입니다. |

곡 선택 화면은 첫 번째 곡명과 첫 번째 아티스트를 표시합니다. `File`이 없으면
파싱 오류이며, `Version`이 정수가 아니거나 실제 음원 파일이 없으면 catalog
검증에 실패합니다. 손상된 YMM 하나는 catalog 전체를 중단시키지 않고 해당 곡만
제외합니다.
