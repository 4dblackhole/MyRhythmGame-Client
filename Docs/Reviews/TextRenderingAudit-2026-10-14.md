# 텍스트 렌더링 심층 조사 — 2026-10-14

현재 DirectWrite + D3D12 글리프 아틀라스 구조를 유지하면서 개선할 수 있는
반복 CPU 작업, 첫 표시 지연, 장시간 사용 시 자원 한계와 표시 정확성을 조사했습니다.
생산 C++ 코드와 공개 API는 변경하지 않았습니다. 비교 프로토타입은 `build/`에만 있습니다.

조사 기준은 Client `182103bdf5adc34fba8324e6ba074e975ff8c2da`,
엔진 `29382d007791e48d6cbb07427d704a56a6a33fb3`입니다.
이 SHA는 조사 재현을 위한 시점 기록이며, 현재 HEAD는 Git에서 확인합니다.

## 확인한 구조

`TextVisualComponent` → Canvas의 `DrawPacket` → D3D12 Visual2D의 글꼴 해석 →
`TextRenderSystem::Submit` → DirectWrite 레이아웃 → 글리프 수집 →
`Flush`의 업로드·페이지 정렬·instance 복사·draw 순서입니다.

- DirectWrite는 글꼴 선택, shaping, 배치, 글리프 분석을 담당합니다.
  화면 합성은 D3D12이며 Direct2D / D3D11-on-12는 사용하지 않습니다.
- 레이아웃 키는 FontHandle 정체성, 문자열, 글꼴 크기, 배치 폭·높이, 정렬입니다.
  위치·색상·클립·transform은 키에 없으므로 이 값만 바뀌면 재배치가 필요하지 않습니다.
- 레이아웃은 2,048항목 LRU입니다. 이전 128항목 전체 삭제 문제는 수정되어 있습니다.
  2,048이 모든 화면에 적합하다는 측정 근거는 없고, 용량보다 큰 반복 작업 집합은 여전히 취약합니다.
- 글리프 키는 실제 font-face의 COM 정체성, 정확한 float 크기 비트,
  glyph index, measuring mode입니다. fallback 글꼴을 다른 글꼴과 혼동하지 않는 점은 유지해야 합니다.
- 글리프는 1024×1024 R8 페이지에 저장하며 최대 16페이지입니다.
  같은 크기·face의 글리프는 여러 문자열이 공유합니다.
- 이미 업로드한 글리프는 다시 업로드하지 않습니다. instance upload buffer도 재사용합니다.
  새 글리프의 upload resource는 GPU 완료까지 보관하고 글리프별 fence 대기는 하지 않습니다.
- 곡 목록은 화면 주변 카드만 생성하고, Visual2D는 clip 밖의 primitive를 Submit 전에 제외합니다.
  전체 곡 수만큼 제목을 매 프레임 렌더링하는 상태는 아닙니다.

소스 경계:
[TextRendering](../../Dependencies/PhantomCore/Engine/Graphics.D3D12/Text/TextRendering.cpp),
[Visual2DRendering](../../Dependencies/PhantomCore/Engine/Graphics.D3D12/Visual2D/Visual2DRendering.cpp),
[Visual2DComponents](../../Dependencies/PhantomCore/Engine/Core/Visual2D/Visual2DComponents.cpp),
[SongSelectCards](../../Client/GameScene/MusicSelectScene/Submodules/SongSelectCards.cpp),
[MarqueeTextComponent](../../Client/Presentation/MarqueeTextComponent.cpp).

## 측정 방법과 범위

PC의 CPU는 Intel Core i5-7600입니다. 장치 목록에는 GTX 1060 3GB와 HD Graphics 630이 있습니다.
이번 실험에서는 선택된 DXGI adapter를 별도로 기록하지 않았습니다.
MSVC x64 Release `/O2 /std:c++20 /MD /DNDEBUG`, 실제 `PhantomCore.lib`,
숨김 D3D12 창과 NoSound로 측정했습니다.

각 일반 실험은 새 프로세스에서 100프레임을 실행했고, 첫 20프레임을 제외한
80프레임의 평균·p50·p95·p99를 기록했습니다. 같은 실험을 2회 실행했습니다.
`Submit`은 문자열 제출의 CPU 시간이고 `Flush`는 업로드와 draw 명령 기록의 CPU 시간입니다.
문구 생성, renderer 초기화, 폰트 로드, 실제 GPU 완료, Present, 전체 프레임 시간은 포함하지 않습니다.
GPU 과도한 겹침을 피하기 위해 1×1px clip을 지정했으나, 현재 Submit은 글리프별 CPU clip을 하지 않습니다.
따라서 긴 문구 실험은 clip 밖의 글리프 처리 비용을 드러냅니다.

프로브는 엔진이 보장하는 frameIndex/renderIndex를 사용합니다. 별도 측정용 TextRenderSystem은
`Run` 반환까지 유지하여 GPU가 참조하는 아틀라스·upload 자원을 먼저 해제하지 않습니다.
새 글리프 준비 시간을 포함한 첫 프레임과 정상 반복 프레임을 분리했습니다.
DirectWrite/OS 글꼴 캐시와 시스템 부하에 따라 첫 프레임 수치는 달라집니다.

| 합성 조건 | Submit 평균, ms | Flush 평균, ms |
| --- | ---: | ---: |
| 고정 문구 200개, 각 32 UTF-16 단위 | 0.559–0.575 | 0.378–0.553 |
| 고정 문구 2,048개, 각 32 단위 | 6.543–6.717 | 14.019–15.606 |
| 고정 문구 2,049개, 각 32 단위 | 61.326–62.330 | 14.279–14.281 |
| 매 프레임 변경 문구 200개, 각 32 단위 | 6.066–6.172 | 0.363–0.415 |
| 고정 문구 32개, 각 32 단위, clip 지정 | 0.098–0.106 | 0.044–0.047 |
| 고정 문구 32개, 각 512 단위, clip 지정 | 1.114–1.177 | 1.292–1.755 |
| 고정 문구 32개, 각 4,096 단위, clip 지정 | 9.267–9.527 | 36.069–36.219 |
| 서로 다른 한글 512개, 32문구 × 16단위, 16px | 0.080–0.115 | 0.031–0.044 |

한글 512개의 첫 Submit은 6.398–54.876ms, 첫 Flush는 94.586–95.874ms였습니다.
이 값은 첫 표시 비용이며 반복 프레임 평균과 혼동하면 안 됩니다.
2,048개나 4,096단위 문구는 경계 스트레스 조건이며 현재 곡 선택 화면의 실제 표시량이 아닙니다.
이 결과를 게임 FPS나 GPU 시간으로 해석하면 안 됩니다.

## 재현한 병목과 개선 방법

### 1. 매 프레임 큰 글리프 배열을 재정렬

`Flush`는 페이지별 draw를 위해 모든 `GlyphInstance`를 `std::stable_sort`합니다.
이미 페이지 순서대로 들어왔거나 모든 글리프가 한 페이지에 있어도 같은 정렬을 수행합니다.

계측용 복사본에서 고정 2,048문구의 99번째 프레임은 63,488 drawable instance를 만들었으며,
정렬만 12.314ms, instance 할당·복사는 0.856ms였습니다.
32개 × 4,096단위 조건에서는 131,040 instance의 정렬이 33.213ms였습니다.

생산 코드와 같은 정렬 결과를 유지하는 `is_sorted` 검사 프로토타입을 별도로 빌드했습니다.
이미 정렬된 경우 정렬을 생략하며, 그 외에는 원래 stable_sort를 호출합니다.

| 조건 | 계측 원본 Flush 평균 | 연구 프로토타입 Flush 평균 |
| --- | ---: | ---: |
| 200개 고정 문구 | 0.381ms | 0.106ms |
| 2,048개 고정 문구 | 13.956ms | 1.802ms |
| 32개 × 4,096단위 | 36.099ms | 3.607ms |

프로토타입은 1회 비교이며 GPU/전체 프레임 개선률은 측정하지 않았습니다.
정렬된 배열에 대한 출력 순서는 동일합니다. 다중 페이지의 일반 순서와 화면 정확성은
향후 생산 적용 시 별도 검증해야 합니다.

첫 적용 후보는 이미 정렬됐는지 확인하거나 단일 페이지일 때 생략하는 작은 변경입니다.
이후에는 큰 instance를 정렬하는 대신 작은 참조/인덱스를 사용하거나,
제출 단계에서 유효한 draw 순서를 구성하는 방법을 검토할 수 있습니다.

### 2. 새 글리프마다 작은 committed upload resource 생성

`UploadNewGlyphs`는 글리프마다 `CreateCommittedResource`, Map, 행 복사, Unmap,
CopyTextureRegion을 호출합니다. fence 대기는 제거되어도 리소스 생성 비용은 남아 있습니다.

한글 512개 계측에서 glyph miss 512, committed upload resource 512가 확인됐고,
업로드 함수의 CPU 시간은 98.014ms였습니다. 이 실험의 글리프 수집은 6.419ms였습니다.
업로드 함수에는 생성·Map·복사·명령 기록이 함께 포함되어 있으므로 98ms 전체가
리소스 생성 API 하나의 비용이라고 단정하지 않습니다.

권장 방식은 frame별 persistently mapped upload arena에 글리프들을 함께 배치하는 것입니다.
RowPitch 256바이트, texture footprint 시작 offset 512바이트 정렬을 지키고,
각 glyph copy가 같은 큰 버퍼의 다른 offset을 참조하게 합니다.
이미 안전하게 재사용되는 frame-resource 수명 구조를 사용하면 됩니다.
arena 공간은 fence 완료 전에 덮어쓰면 안 됩니다.
Microsoft도 fence를 이용한 upload heap의 버퍼 내 부분 할당을 설명합니다.
[Fence-based resource management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management)

그 다음 단계는 화면에 필요한 글리프와 인접 행의 글리프를 미리 준비하는 것입니다.
모든 곡 제목이나 CJK 문자 전체를 시작 시 미리 만드는 것은 로딩·메모리 비용을 다른 시점으로 옮길 뿐입니다.
CPU 준비를 worker로 옮긴다면 CPU 결과만 게시하고 아틀라스 할당·GPU 작업은 렌더 스레드가 소유해야 합니다.
현재 TextRenderSystem의 map/vector/frame 상태에 worker가 직접 동시에 접근하는 것은 안전하지 않습니다.
준비 중 표시 정책은 화면 동작을 바꾸므로 구현 전에 별도로 확정해야 합니다.

### 3. LRU 경계와 변경되지 않은 라벨의 반복 처리

계측 원본의 99번째 프레임에서 2,048개 문구는 layout hit 2,048 / miss 0,
2,049개 문구는 hit 0 / miss 2,049였습니다. 모든 글리프 이미지는 이미 캐시에 있었으므로
이 급증은 신규 glyph rasterization 때문이 아닙니다.

2,049개 조건의 레이아웃 찾기·생성 경로는 10.082ms,
collector 생성과 DirectWrite Draw/글리프 수집 경로는 50.609ms였습니다.
2,048개 조건의 후자는 5.642ms였습니다. DirectWrite 내부의 세부 원인은 ETW 등 추가 프로파일링이 필요하지만,
새 레이아웃의 최초 Draw까지 포함한 비용이 반복 발생한다는 것은 확인했습니다.

현재 적중 경로에도 다음 작업이 남습니다.

- DrawPacket이 문자열과 TextFont 문자열을 매번 소유 복사합니다.
- ResolveFont가 이미 알려진 이름/경로를 다시 키로 구성·정규화·소문자 변환합니다.
- 레이아웃 찾기 전에 `std::wstring(text)`와 문자열 해시를 만듭니다.
- 레이아웃 miss마다 같은 스타일의 TextFormat도 새로 생성합니다.
- Submit마다 GlyphRunCollector를 heap에 만들고 DirectWrite Draw를 호출합니다.
- 글리프마다 IUnknown QueryInterface와 AddRef/Release, hash lookup을 반복합니다.
  2,048문구 조건에서는 hit 상태에서도 65,536번 글리프 lookup을 수행했습니다.

권장 순서는 실제 화면에 살아 있는 라벨이 불변 배치 결과를 유지하고,
문자열·폰트·크기·배치 영역·정렬이 변경될 때만 그 결과를 갱신하는 것입니다.
공유 LRU는 살아 있는 라벨의 유일한 소유 저장소가 아니라 재사용 보조 캐시로 둡니다.
그러면 LRU에서 제거되더라도 사용 중인 정적 라벨은 계속 배치 결과를 사용할 수 있습니다.

DirectWrite 객체는 Graphics 구현 안에 두고 Core/Client에는 backend-neutral한 상태나 handle만 전달해야 합니다.
Font, TextFormat, Layout/배치, Glyph bitmap의 수명을 별도로 관리해야
색상·위치 변경이 비싼 배치 생성까지 전파되지 않습니다.
회수 가능한 아틀라스의 UV를 영구 캐시하면 안 되므로 배치와 아틀라스 residency는 구분해야 합니다.

작은 우선 개선은 font-face COM 정체성 조회를 glyph마다가 아니라 glyph run마다 한 번 수행하는 것입니다.
run 안의 fontFace는 동일하지만 fallback으로 run이 바뀌면 별도로 해석해야 합니다.
TextFormat 공유, 투명 해시 조회(string_view 조회), font descriptor 해석 유지도 적용 후보입니다.
문자열 hash만으로 동일성을 판정하거나 raw pointer를 수명 보장 없이 유지하면 안 됩니다.

Microsoft는 반복 문구에서 TextLayout 재사용을 권장합니다.
[Direct2D text performance](https://learn.microsoft.com/en-us/windows/win32/direct2d/improving-direct2d-performance)
SDL_ttf GPU 구현도 text별 draw_sequence를 보관하고, 텍스트/폰트 변경 시 갱신하며,
glyph 참조와 font generation을 별도로 관리합니다.
[SDL_ttf GPU source](https://raw.githubusercontent.com/libsdl-org/SDL_ttf/main/src/SDL_gpu_textengine.c)

### 4. 긴 제목 스크롤과 화면 밖 글리프

MarqueeTextComponent는 wchar_t 단위로 문자열 창을 잘라 교체합니다.
곡 제목은 0.24초 단위로 다른 문자열이 되어 새로운 레이아웃 변형을 만듭니다.
현재의 문자 수 기준은 비례 글꼴의 실제 픽셀 폭을 정확히 나타내지도 않습니다.

전체 제목의 배치를 유지하고 픽셀 폭·transform·clip으로 이동하는 방식이 적합합니다.
다만 전체 제목으로 바꾸기만 하면 위의 512/4,096단위 실험처럼 화면 밖 글리프까지 매 프레임 처리합니다.
불변 glyph 배치를 한 번 준비한 다음 실제 화면과 겹치는 glyph/블록만 instance로 제출해야 합니다.
수평 문자열은 배치 결과의 구간 검색을 사용할 수 있지만, RTL·겹친 glyph·combining mark를
문자 인덱스와 단순 X 증가로 가정하면 안 됩니다. 일반 transform에는 안전한 bounds 판정이 필요합니다.

이모지 `😀ABCDEFGH`, 창 크기 3, 첫 문자 이동으로 실제 Client 객체를 실행했습니다.
결과 UTF-16 단위는 `DE00 0041 0042`였고 UTF-8 엄격 변환이 error 1113으로 실패했습니다.
surrogate pair의 뒤쪽만 남는 잘못된 문자열입니다. combining sequence도 분리될 수 있습니다.
이 문제는 현재 컬러 이모지 미지원과 별개이며, 원본 문자열을 유지하는 스크롤은 이 손상을 피합니다.

Canvas BuildDrawList만 측정한 200라벨의 평균은 32단위일 때 0.088ms,
4,096단위일 때 0.823ms였습니다. 복사 절감은 필요하지만 이 실험에서는
업로드·정렬·전체 glyph 순회의 우선순위가 더 높습니다.

### 5. 아틀라스가 가득 찼을 때의 회수 정책

현재는 page/glyph 회수 없이 누적하며 16페이지에 도달하면 예외를 던집니다.
64px의 새로운 한글 512개를 매 프레임 추가하는 실험은 두 실행 모두
9번째 프레임(frame index 8)에 `The text glyph atlas reached its page limit.`로 실패했습니다.
이것은 대형 글꼴 스트레스 조건이며 현재 일반 곡 선택에서 같은 시점에 실패한다는 뜻은 아닙니다.

16페이지의 픽셀 데이터는 최대 16MiB이지만 heap/descriptor/CPU map/글꼴 참조/
업로드 버퍼/instance 메모리는 별도입니다. 따라서 총 텍스트 메모리가 16MiB라는 뜻은 아닙니다.
크기 키가 정확한 float 비트이므로 해상도 변경에서 생긴 많은 크기 변형도 별도 glyph를 누적합니다.
현재 glyph entry는 font-face COM 참조를 보관하므로 글리프가 남는 동안 face 수명도 유지됩니다.

회수는 페이지 또는 블록 단위 사용 시점, 현재 제출에서의 pin, 완료 fence,
generation을 함께 관리해야 합니다. CPU 캐시에서 먼저 삭제한 뒤 in-flight GPU 자원을
해제하거나 UV가 유지된 배치가 재사용된 페이지를 읽게 해서는 안 됩니다.
사용 중이지 않은 페이지를 회수하고 해당 generation의 배치만 다시 residency를 확인하는 방식이 적합합니다.
동시에 사용 중인 글리프가 예산 자체를 초과한 경우의 동작은 별도 정책 결정이 필요합니다.

Skia의 참조 구현은 아틀라스 generation과 glyph residency를 검사하고,
use token으로 사용 시점을 갱신하며 변경된 좌표를 재생성합니다.
[Skia TextAtlasManager, pinned reference](https://skia.googlesource.com/skia/+/a2d00a28c563/src/gpu/graphite/text/TextAtlasManager.cpp)
strike 캐시는 사용 바이트와 개수 제한을 함께 관리하고 LRU 순서로 회수합니다.
[Skia StrikeCache](https://skia.googlesource.com/skia/+/main/src/text/gpu/StrikeCache.cpp)

레이아웃은 DirectWrite 내부 할당량을 정확히 공개하지 않으므로,
자체 소유 문자열/배치의 바이트와 항목 수, process memory를 구분해 계측해야 합니다.
측정 없이 새로운 항목 수나 메모리 예산을 최적값으로 고정하지 않는 것이 좋습니다.

### 6. 단일 채널 rasterization과 표시 품질

현재는 ClearType 3채널 커버리지를 만들고 채널의 최댓값을 R8 alpha로 변환합니다.
이는 DirectWrite의 GRAYSCALE 출력과 동일한 계산이 아닙니다.
중간 버퍼와 픽셀 변환 비용이 있고 글자의 두께/경계 표현도 달라질 수 있습니다.

같은 Factory3 분석 API에서 GRAYSCALE과 1채널 alpha 출력을 직접 실험했습니다.

| 16px glyph | GRAYSCALE source | CLEARTYPE source |
| --- | ---: | ---: |
| Segoe UI `A` | 11×11, 121bytes | 12×11, 396bytes |
| Malgun Gothic `한` | 15×14, 210bytes | 16×14, 672bytes |

회색조 출력에는 각각 44/49개의 중간 alpha 값이 있었으므로 단순 이진 aliased 출력이 아니었습니다.
`DWRITE_TEXTURE_ALIASED_1x1`이라는 이름만 보고 회색조 출력이 불가능하다고 판단하면 안 됩니다.
이 실험은 현재 PC에서 지원과 buffer 크기만 확인했으며 속도 향상률이나 실제 화면 품질을 검증하지 않았습니다.

Microsoft는 GRAYSCALE이 픽셀별 한 커버리지 값을 계산하여 투명 bitmap에 합성할 수 있다고 설명합니다.
[Text antialias modes](https://learn.microsoft.com/en-us/windows/win32/api/dwrite_1/ne-dwrite_1-dwrite_text_antialias_mode),
[Factory3 glyph analysis](https://learn.microsoft.com/en-us/windows/win32/api/dwrite_3/nf-dwrite_3-idwritefactory3-createglyphrunanalysis)
전환은 작은 글씨, 한글·일본어, 확대/축소와 여러 배경색의 실제 화면을 비교한 뒤 결정해야 합니다.

## 함께 주의할 정확성·구조

- 페이지별 정렬은 서로 다른 페이지의 텍스트 제출 순서를 바꿉니다.
  텍스트 PSO는 depth를 쓰지 않으므로 alpha가 겹치는 텍스트 사이의 paint order가
  정렬만으로 보장되는 것은 아닙니다. 실제 픽셀 오류 재현은 수행하지 않았습니다.
  페이지별 버킷으로 바꾸기 전 겹침 회귀를 만들고, 필요하면 인접 draw 묶음이나
  Texture2DArray/descriptor indexing으로 순서를 유지해야 합니다.
- `AppendGlyphRun`은 bidiLevel을 읽지만 isSideways를 처리하지 않으며,
  glyph 분석에 새 단일 run을 만들 때 원 run의 방향 정보를 그대로 넘기지 않습니다.
  현재 한국어/영어의 수평 문구와 다른 RTL/세로쓰기 지원 여부는 별도 검증이 필요합니다.
  [DWRITE_GLYPH_RUN contract](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/ns-dwrite-dwrite_glyph_run)
- GPU instance는 132bytes이며 transform/color/clip/depth가 glyph마다 반복됩니다.
  63,488 instance는 한 pass에 약 8.38MB의 instance 데이터를 기록합니다.
  라벨별 공통 데이터와 glyph별 geometry를 분리하면 줄일 수 있지만 GPU의 추가 간접 조회와
  batching 영향을 실제 GPU timestamp로 확인한 뒤 적용해야 합니다.
- 언어/스킨/resize/폰트 변경 시 배치 revision을 갱신하고,
  색상/위치/스크롤 변경은 배치 revision과 분리하는 것이 응집도를 높입니다.
- glyph 순서, advance, offset, fallback face는 DirectWrite 결과를 유지해야 합니다.
  Unicode 문자를 독립 사각형으로 직접 치환하는 방식은 shaping과 kerning을 깨뜨립니다.
- 이미지별 무조건 texture를 만들거나 제목 전체를 큰 bitmap으로 저장하면
  메모리가 문자열 수·길이에 비례합니다. 변하지 않는 작은 UI 묶음에는 선택적으로 가능하지만
  대형 곡 목록 전체에 일괄 적용하는 방식은 적합하지 않습니다.
- MSDF는 확대·회전이 많은 글자에서 검토할 수 있습니다. 현재의 upload resource 생성이나
  레이아웃 반복 처리를 자동으로 해결하는 방식은 아닙니다. 단색 R8보다 채널/생성/셰이더 비용이 추가되므로
  고정 크기 목록에서는 우선순위가 낮습니다.
  [MSDF generator and shader reference](https://github.com/Chlumsky/msdfgen)

## 권장 적용 순서와 검증 기준

| 순서 | 적용 범위 | 확인할 결과 |
| --- | --- | --- |
| 1 | Text Flush의 불필요한 정렬 생략 | 같은 draw 순서 유지, 반복 Flush CPU 감소 |
| 2 | 글리프 upload arena | 512개의 작은 resource 대신 소수의 arena, fence 수명/정렬 정확성 |
| 3 | 유지되는 layout/배치, run별 face 해석, font/format 재사용 | 정적 라벨 hit 경로의 반복 heap/COM/shaping 감소; 2,049항목 반복에서도 사용 중 배치 유지 |
| 4 | 제목 원문 유지 + transform/clip + 가시 glyph 제출 | UTF-16/grapheme 손상 방지, 긴 제목에서도 제출량이 보이는 범위에 제한 |
| 5 | 아틀라스 예산/회수/generation | 장시간 CJK·resize·폰트 변경 반복에서 예산 도달 후 정상 재사용, in-flight UV/자원 보호 |
| 6 | GRAYSCALE 직접 출력과 instance 구조 | 실제 글자 품질 및 GPU 계측을 통과한 변경만 적용 |

모든 후보는 조사/제안이며 생산 코드에는 아직 적용하지 않았습니다.
회수 정책과 준비 중 표시 정책, 글자 품질 변경은 구현 시 명시적으로 정해야 합니다.

최종 성능 검증에는 실제 곡 목록과 스크롤, 최초/재방문, 검색·선택, 한글·영어·일본어,
긴 제목, 여러 해상도, 폰트 변경, 반투명 겹침을 포함하는 것이 좋습니다.
layout/glyph hit·miss, 생성 수, visible glyph, upload resource/bytes, atlas residency,
CPU 구간 p95/p99와 GPU timestamp를 함께 기록해야 원인을 구분할 수 있습니다.

## 실행 기록과 한계

현재 작업 공간의 연구 파일과 원시 결과는 `build/text-rendering-audit-2026-10-14/`에 보관했습니다.

- `RunProbe.ps1`: 생산 라이브러리를 사용하는 9조건 × 2회. 각 100프레임,
  아틀라스 포화 조건은 예상한 예외를 포착하고 안전하게 종료합니다.
- `RunInstrumented.ps1`: 생산 TextRendering.cpp의 `build/` 복사본에
  hit/miss와 구간 시간 계측만 추가한 5조건 실험. map/정렬/업로드 알고리즘은 변경하지 않았습니다.
- `RunSortedPrototype.ps1`: 이미 정렬된 경우를 생략하는 연구 전용 비교, 3조건.
- `RunClientProbe.ps1`: 실제 Release MarqueeTextComponent 객체와 Canvas로 UTF-16 손상/DrawList 비용 확인.
- `RunGrayscaleProbe.ps1`: DirectWrite Factory3의 두 alpha 출력 확인.

모든 최종 프로브 빌드는 `/W4`에서 경고 없이 성공했습니다.
Client 프로브의 최초 링크는 PCH object 누락으로 실패했으며 Pch.obj를 함께 링크한 최종 실행은 성공했습니다.
일반·계측·프로토타입 결과는 서로 다른 실행이므로 미세한 차이를 개선 효과로 해석하면 안 됩니다.
개선 비교 표는 계측 원본과 계측 프로토타입을 비교했습니다.

실제 화면의 픽셀 배치·글자 품질·사용자 마우스 조작·청음 및 전체 게임 FPS/GPU 시간은 확인하지 않았습니다.
생산 C++/프로젝트/엔진은 변경하지 않아 전체 빌드·회귀·정적 분석을 새로 수행하지 않았습니다.
문서 변경에는 `git diff --check`를 수행합니다.
기존 사용자 LaneKeyBeam.cpp 변경, 곡·스킨·Option.ini는 보존했습니다.
