# 새 관리자·세션 인수인계

## 시작

1. 루트 `AGENTS.md`를 읽고 Client/엔진 status 및 submodule gitlink를 확인합니다.
2. [작업별 문서 표](README.md)에서 필요한 기능만 선택합니다. 전체 문서/채팅은 읽지 않습니다.
3. [변경 영향 지도](ChangeImpactMap.md)로 소유 파일·호출부·테스트를 찾습니다.
4. 요구가 불명확하거나 해석에 따라 동작/설계가 크게 달라지면 구현을 멈추고 질문합니다.

## 작업 위치와 경계

- 작업 폴더: `D:\Projects\c++\asdf\MyRhythmGame-Client`. 별도 지시 없으면 `main`에서 작업합니다.
- Client: `4dblackhole/MyRhythmGame-Client`, 엔진: private `Dependencies/MRG-Engine` submodule.
- 커밋과 동기화 상태는 Git이 기준입니다. 문서에 고정 SHA를 복제하지 않습니다.
- C++20 / MSVC v143 / Windows / D3D12. Client의 엔진 경계는 `MRG_Core.h` 하나입니다.
- 판정/차트/모드/편집 분석은 엔진 비종속 프로젝트입니다. Scene은 수명주기와 조립을 맡습니다.
- 기본 스킨/글꼴은 RCDATA fallback, AngelDream MP3 1개·YMM 1개·YMP 5개와
  효과 테스트 YME 1개·WAV 2개는 외부 `assets/Songs`입니다.
- 사용자 곡·스킨과 `TODOLIST.txt`를 임의로 stage/덮어쓰지 않습니다.

## 현재 범위와 주의점

- 기본 진입은 로고 → 플레이 또는 에디터 곡 선택 → 플레이/에디터입니다.
- YMP/YME 편집·저장, BPM/마디/히트사운드 변경과 오디오 분석을 지원합니다.
- YMP의 Effect file은 YMP 기준이고 히트사운드 표/파일 경로는 YME 소유/기준입니다.
  YME는 Interpolation/HitSounds 기본 데이터와 Speed/Sounds/Zone 명령을 분리합니다.
  볼륨은 히트사운드 대상입니다. 문법·보간·Whole/Separate·싱코페이션/Kiai는
  [YME](Formats/Yme.md), 실제 AngelDream 효과 테스트는 Presentation의 -Effects를 참고합니다.
- 결과 Scene/영구 기록, 에디터 Undo/Redo·드래그·다중 선택·음악 재생은 미구현입니다.
  이 목록은 추가 구현 지시가 아닙니다. 기능별 제한은 해당 문서가 기준입니다.
- Scene 전용 폴더와 `Submodules`, Note 계약/규칙 트리를 사용합니다.
  파일 추가 시 프로젝트와 Visual Studio 필터도 같은 트리로 등록합니다.
- 에디터는 `IEditorMode`와 `IEditorDocument`로 도구/표시/입력과 파일 편집/저장을
  교체합니다. 현재 구현은 Taiko + YMP/YME이며 BMS/7키는 아직 미구현입니다.
  확장 경계는 [ChartEditor](ChartEditor.md)의 모드/파일 형식 절에서 확인합니다.
- 모드는 Workspace 전체 대신 `IEditorContext`를 참조합니다. 문서 수정/시간 이동은
  Workspace 명령으로 연결하고, 분석은 `IEditorAudioSource` 조회만 사용합니다.
  Taiko 옵션/사운드 예측은 플레이의 공통 규칙을 사용합니다.
- 편집 원본 노트/BPM/마디/이펙트는 위치 기반 균형 트리로 관리합니다. 고유 노트 ID는
  배열 인덱스가 아니며 저장/플레이/렌더링 조회는 읽기 전용 snapshot입니다.
  분석 취소는 Scene에서 기다리지 않고, 미리듣기는 StreamAsync의 준비 상태를 확인합니다.
- AngelDream `All Notes Verification`은 전 노트 및 밀집 Don/Kat Buzz를 재현합니다.
  채보 위치와 +50ms GOOD 입력의 틱 기대값은 [Gameplay Debugging](Gameplay/Debugging.md)을 참고합니다.
- 고정 문구와 언어별 글꼴은 `Client/Texts`의 사용 위치별 표가 소유합니다.
  플레이의 판정 구간/타이밍 마커와 7종 링·문구는 `GameplayJudgementView`가 소유하며,
  기본 범위를 세션 중 유지합니다. 동작/스킨 경로는 [Presentation](Gameplay/Presentation.md) 참고.
  로고와 곡 선택은 공통 `OptionsPanel`에서 언어·스킨·FMOD·자동/WASAPI/ASIO·드라이버를 선택합니다.
  출력 방식/드라이버는 즉시 적용하고 모든 옵션은 EXE 옆 `Option.ini`에 유지합니다.
  `OptionSettings`는 파일 저장/읽기만 담당하며 별도 옵션 Controller는 없습니다.
- 선택한 스킨 이름은 `Option.ini`에 유지하며(기존 skin-set.txt는 최초 생성 시 가져옴),
  각 파일이 없을 때 내장 기본 스킨으로 대체합니다. 풍선 완료음 `pop.wav`도
  `Default Skin/HitSounds/TaikoMode`에 포함됩니다.
- ColoredCube 기술 예제는 Debug에서만 컴파일합니다. `Client/Assets/Unused`는
  어떤 구성에서도 로드·복사하지 않으며 원본은 보존합니다.

## 마무리

판정 표시 작업은 Debug/Release x64 전체 재빌드, 로직·카탈로그(1곡/5패턴),
각 구성의 4개 Client smoke와 판정 표시/키빔 회귀를 통과했습니다.
실제 D3D12/FMOD 자동 replay도 효과 채보 15개 노트와 전체 종류 채보 19개 노트를
두 구성에서 완료했습니다. Client MSVC 정적 분석은 새 경고 없이 통과했으며
기존 코드의 3종 경고를 Verification에 구분해 기록했습니다.
프로젝트/필터/의존성/엔진 경계를 통과했고 엔진은 변경하지 않았습니다.
실제 화면/물리 입력/청음은 수동 확인하지 않았습니다. 상세 결과는 Verification이 기준입니다.

[Verification](Verification.md)에 따라 검증하고 실제 결과만 보고합니다.
smoke 통과는 픽셀/마우스 수동 검증을 뜻하지 않습니다.
영구 계약은 기능 문서 한 곳에서 갱신하고 이 문서는 현재 주의점만 유지합니다.
엔진 변경 시 엔진 원격 반영 → Client gitlink 순서로 동기화합니다.
완료 보고에는 변경 클래스/파일과 책임 분리, 기존 동작에 미치는 영향 및 검증 결과를 브리핑합니다.
