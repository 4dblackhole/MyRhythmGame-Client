# 텍스트 렌더링 개선 — 2026-10-14

[심층 조사](TextRenderingAudit-2026-10-14.md)에서 확인한 반복 배치·글리프 수집,
첫 업로드 자원 생성, 긴 제목, 아틀라스 포화와 합성 순서를 개선했습니다.
DirectWrite shaping + D3D12 글리프 아틀라스 구조를 유지합니다.

사용자가 확정한 정책은 기본 16MiB 예산을 현재 글자 표시를 위해 일시 확대하고
안전해진 미사용 페이지를 회수하는 것, DirectWrite의 정식 회색조 출력을 사용하는 것입니다.

## 코드와 책임

| 소유 클래스/파일 | 변경과 경계 |
| --- | --- |
| 엔진 `Core/Visual2D/Visual2DNode`의 `TextContent` | 문자열·글꼴 descriptor·크기·정렬을 공유하는 backend-neutral payload. GPU UV나 DirectWrite 객체를 갖지 않음 |
| 엔진 `TextVisualComponent` / `ComboBoxBehaviorComponent` | 내용/스타일 변경 때만 payload 교체. 색상·위치·clip·이동/반복 사본은 같은 payload 사용 |
| 엔진 `Graphics.D3D12/Visual2DRendering` | payload별 해석한 글꼴과 배치 handle 유지. 약한 source 참조로 수명을 추적하고 사용하지 않는 resize 변형/만료 원본 회수 |
| 엔진 `Graphics.D3D12/TextRendering` | DirectWrite 준비 배치, clip 검사, 회색조 rasterization, GPU upload/아틀라스/fence 수명과 제출 순서 소유 |
| Client `MarqueeTextComponent` | 원본 Unicode와 스크롤 시간/픽셀 offset 소유. 엔진의 측정 폭으로 실제 영역 초과 여부 판단; 글리프 배치·GPU 자원은 소유하지 않음 |
| Client `SongSelectCards` / `SongSelectPanels` | 글자 수 창 대신 Marquee의 기존 시간당 이동 속도를 전달하는 조립 호출로 변경 |
| Client `EditorView` | 직접 만드는 text DrawPacket에 공유 TextContent를 전달하도록 공개 계약에 맞춤 |
| 엔진 SDK·Client 표시 테스트 | 공유 payload 계약과 실제 D3D12 배치/clip/업로드/회수/합성 순서 회귀 검사 |

Scene, TextCatalog, 곡 카탈로그와 선택 정책은 그대로 책임을 유지합니다.
새 DirectWrite 의존성을 Client/Core에 추가하지 않았습니다.

## 배치와 반복 비용

- `TextLayoutHandle`은 처음 DirectWrite Draw로 수집한 run/글리프/위치와 보수적인
  bounds를 보관합니다. 같은 라벨을 제출할 때 collector 생성이나 DirectWrite Draw를
  반복하지 않고 face의 COM 정체성도 run별로 한 번만 구합니다.
- 위치·색상·clip 변경은 배치를 유지합니다. 문자열·글꼴·크기·배치 영역·정렬 변경에만
  새 배치가 필요합니다. renderer는 화면/texture별 물리 크기 변형을 관리합니다.
- 2,048항목 LRU는 재사용 보조 캐시입니다. 살아 있는 UI의 handle은 LRU 축출에도
  유효합니다. 문자열 제출 API만으로 2,049개 문구를 순환하면 LRU miss는 여전히 발생할 수
  있으므로 고정 UI는 유지 handle을 사용합니다.
- 문자열 조회는 string_view를 사용하는 투명 해시로 소유 문자열 생성을 miss로 제한하고,
  같은 스타일의 TextFormat도 공유합니다. hash만으로 문자열 동일성을 판정하지 않습니다.
- clip 검사에서는 64개 글리프 블록을 먼저 검사하고 가까운 글리프만 캐시 조회·rasterization·
  instance 생성합니다. 문자 index나 단순 X 증가를 가정하지 않고 변환된 bounds를 사용합니다.

## GPU 업로드·아틀라스·합성

- 새 글리프를 프레임별 mapped upload arena의 다른 offset에 함께 기록합니다.
  RowPitch 256바이트 / footprint 512바이트 정렬을 지키며 fence 완료 전에 덮어쓰지 않습니다.
  여러 off-screen pass는 같은 renderIndex에서 이어서 할당합니다.
- 아틀라스는 1024×1024 R8 페이지 16개를 기본 예산으로 사용합니다. 현재/in-flight
  페이지를 보호하고 필요하면 확대합니다. 재사용 가능한 페이지는 오래된 순으로 재활용하고,
  초과한 뒤쪽 미사용 페이지는 안전한 프레임에서 해제합니다.
- 페이지 회수 때 해당 글리프 캐시를 지우고 다음 제출에서 이미지를 다시 준비합니다.
  배치에 영구 UV를 저장하지 않아 회수된 좌표를 읽지 않습니다.
- descriptor heap 확장 때 원본 texture에서 SRV를 새로 만들며 이미 기록한 pass의
  heap은 GPU 완료까지 유지합니다. shader-visible heap을 descriptor 복사의 source로 쓰지 않습니다.
- 글리프의 전체 `stable_sort`를 제거했습니다. 연속된 같은 페이지만 묶어 draw하여
  앞뒤 합성 순서를 유지합니다. 페이지를 자주 교차하는 경우 draw 수는 늘어날 수 있습니다.
- `GRAYSCALE` / `DWRITE_TEXTURE_ALIASED_1x1` alpha를 직접 사용합니다.
  ClearType 세 채널의 최댓값 변환과 중간 3채널 버퍼를 제거했습니다.

16MiB는 아틀라스 픽셀의 기본 예산입니다. CPU 배치·글꼴·descriptor·업로드와
instance 버퍼까지 합한 총 텍스트 메모리의 상한은 아닙니다.

## 곡 제목 표시와 API 변경

긴 제목은 전체 원본을 유지하고 실제 측정 폭이 영역을 넘을 때만 픽셀 단위로 이동합니다.
기존 1초 정지와 평균 문자 폭에 따른 이동 속도를 사용하며 같은 payload의 반복 사본으로
순환합니다. 짧은 문구와 영역이 넓어져 들어가는 문구는 정지합니다.
매 스크롤 단계에서 새 문자열/배치를 만들지 않으며 surrogate/combining sequence를 자르지 않습니다.
문자 수는 내용 변경 때만 계산합니다.

공개 `DrawPacket::text`는 wstring에서 `TextContentHandle`로 변경했습니다.
별도 `font`/`fontSize`/`horizontalAlignment` 필드는 payload로 이동했습니다.
이 저장소의 에디터 직접 생성부와 텍스트를 검사하는 회귀 호출부를 함께 갱신하고 SDK 헤더는
생성기로 갱신했습니다. 유지 배치 API와 offset/repeat/측정 계약은
[엔진 TextRendering](../../Dependencies/MRG-Engine/Docs/TextRendering.md)에 있습니다.

## 실제 D3D12 회귀 결과

- 2,049개 유지 배치를 다음 프레임에 제출: 새 layout 0, rasterization 0.
- 4,096개의 W를 좁은 clip에 제출: 실제 제출 글리프 5개.
- 서로 다른 한글 512개를 처음 표시: rasterization 512개, 새 upload arena 1개.
- 64px 한글 6,000개를 동시에 요청: 아틀라스 22페이지. 이후 안전한 프레임에서
  16페이지로 회수하고 회수한 글리프를 다시 요청하면 1개를 재생성.
- Debug D3D12 validation의 ERROR/CORRUPTION 메시지 0.
- 다중 페이지의 빨간 B 뒤에 파란 A를 겹치는 GPU readback:
  수정 전 예상 합성과 평균 채널 오차 7.29608 byte, 수정 후 0.0081604 byte.
  겹친 픽셀 224개를 검사하는 `--text-pixels` 회귀로 등록했습니다.

밝고 어두운 배경의 영문·한글·일본어 16/24/32px 실제 렌더 이미지를 눈으로 비교했습니다.
회색조의 경계는 조금 얇아졌고 샘플의 글자 누락 없이 합성 순서가 바로잡혔습니다.
이는 렌더 샘플 비교이며 전체 게임의 물리 마우스/키보드 조작·청음 검증은 아닙니다.

## 측정·검증 기록

같은 PC에서 MSVC x64 Release `/O2 /std:c++20 /MD /DNDEBUG`, 실제 MRG.Core.lib,
숨김 D3D12/NoSound 프로세스로 조건마다 100프레임을 두 번 실행했습니다.
첫 20프레임을 제외한 80프레임의 CPU 평균 범위입니다. 문자열 생성·초기화·
유지 handle 최초 준비 시간·GPU 완료/Present는 반복 Submit/Flush 시간에 포함하지 않습니다.

| 조건 | 수정 전 Submit, ms | 수정 후 Submit, ms | 수정 전 Flush, ms | 수정 후 Flush, ms |
| --- | ---: | ---: | ---: | ---: |
| raw 문자열 2,048개 고정 | 6.543–6.717 | 0.535–0.691 | 14.019–15.606 | 0.0004–0.0008 |
| raw 문자열 2,049개 고정 | 61.326–62.330 | 52.353–52.416 | 14.279–14.281 | 약 0.0003 |
| 2,049개 유지 handle 사용 | 위 raw 2,049개 | 0.291–0.313 | 위 raw 2,049개 | 0.0004–0.0012 |
| 긴 clip 문구 32개 × 4,096단위 | 9.267–9.527 | 0.333–0.345 | 36.069–36.219 | 약 0.0004 |

표의 clip은 기존 조사와 같은 1×1px입니다. 수정 전에는 clip 밖 글리프도 준비·
instance 생성·정렬했고 수정 후 이 조건의 실제 제출 글리프는 0개입니다.
따라서 off-screen CPU 작업 제거 비교이며 모든 글자를 그릴 때의 draw 비용이나
게임 FPS 개선율이 아닙니다. raw 2,049개 조건의 LRU miss는 여전히 있으므로
유지 handle 사용과 같은 결과라고 주장하지 않습니다.

별도로 640×720 clip 안에 2,049개 유지 문구의 63,519글리프를 제출한 조건은
Submit 3.120–3.132ms / Flush 1.439–1.495ms였습니다. 마지막 프레임 layout 생성은 0개,
최초 모든 배치 준비는 54.799–56.520ms였습니다. 이 전체 문구 수는 현재 곡 선택의
실제 표시량이 아니라 LRU 경계 스트레스 조건입니다.

한글 512개를 모두 clip 안에 표시하는 별도 첫 프레임은 배치 준비 1.807–2.408ms,
Submit 2.002–2.230ms, Flush 1.483–1.681ms와 upload arena 생성 1개를 기록했습니다.
이전 조사에서는 glyph별 upload 512개와 첫 Flush 94.586–95.874ms였지만,
배치/회색조/clip 조건도 바뀌었으므로 감소를 resource 생성 하나의 개선율로 분리하지 않습니다.
매 프레임 내용이 달라지는 200문구는 여전히 배치 생성이 필요하고 Submit 5.292–6.091ms였습니다.

원시 로그, Release CPU probe와 수정 전후 BMP는 ignored
`build/text-rendering-fixes-2026-10-14/`에 보관합니다. 실행한 필수 검증과 정적 분석은
[Verification](../Verification.md)에 기록합니다.

이번 변경은 GPU timestamp/전체 게임 FPS를 측정한 결과가 아닙니다.
GPU instance의 공통 데이터 분리·MSDF와 worker 사전 준비는 조사에서 추가 계측이나
표시 정책 확정이 필요했던 후속 후보이며 구현하지 않았습니다.
사용자 LaneKeyBeam.cpp, 사용자 곡·스킨과 Debug/Release Option.ini를 보존합니다.
