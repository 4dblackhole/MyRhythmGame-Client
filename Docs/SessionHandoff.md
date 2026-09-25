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
- 기본 스킨/글꼴은 RCDATA fallback, AngelDream MP3·YMM·YMP 3개는 외부 `assets/Songs`입니다.
- 사용자 곡·스킨과 `TODOLIST.txt`를 임의로 stage/덮어쓰지 않습니다.

## 현재 범위와 주의점

- 기본 진입은 로고 → 플레이 또는 에디터 곡 선택 → 플레이/에디터입니다.
- YMP/YME 편집·저장, BPM/마디/히트사운드 변경과 오디오 분석을 지원합니다.
- 결과 Scene/영구 기록, 에디터 Undo/Redo·드래그·다중 선택·음악 재생은 미구현입니다.
  이 목록은 추가 구현 지시가 아닙니다. 기능별 제한은 해당 문서가 기준입니다.
- Scene 전용 폴더와 `Submodules`, Note 계약/규칙 트리를 사용합니다.
  파일 추가 시 프로젝트와 Visual Studio 필터도 같은 트리로 등록합니다.
- 고정 문구와 언어별 글꼴은 `Client/Texts`의 사용 위치별 표가 소유합니다.
  로고와 곡 선택은 공통 `OptionsPanel`에서 한국어/영어와 스킨 폴더를 바꿉니다.
- 선택한 스킨 이름은 `%LOCALAPPDATA%/FingerDrum/skin-set.txt`에 유지하며,
  각 파일이 없을 때 내장 기본 스킨으로 대체합니다. 풍선 완료음 `pop.wav`도
  `Default Skin/HitSounds/TaikoMode`에 포함됩니다.

## 마무리

이번 스킨 선택과 pop.wav 이동 후 Debug/Release x64 전체 재빌드,
로직·카탈로그(1곡/3패턴), 각 구성의 4개 Client smoke와 widgets smoke,
프로젝트/필터/의존성 검사를 통과했습니다. pop.wav의 내장 캐시 추출본은
원본과 SHA-256이 같습니다. 이번 변경의 옵션 클릭·이미지 전환·소리 재생은
실제 화면/청음으로 확인하지 않았습니다.

[Verification](Verification.md)에 따라 검증하고 실제 결과만 보고합니다.
smoke 통과는 픽셀/마우스 수동 검증을 뜻하지 않습니다.
영구 계약은 기능 문서 한 곳에서 갱신하고 이 문서는 현재 주의점만 유지합니다.
엔진 변경 시 엔진 원격 반영 → Client gitlink 순서로 동기화합니다.
