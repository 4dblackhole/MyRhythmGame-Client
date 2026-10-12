# osu!taiko 변환기

`OsuTaikoConverter`는 Client와 같은 부모 폴더에 두는 별도 C++20 저장소입니다.
원격: <https://github.com/4dblackhole/OsuTaikoConverter> (비공개).

```text
부모 폴더/
  MyRhythmGame-Client/
  OsuTaikoConverter/
```

Client 폴더에서 최초 한 번 다음 명령으로 준비합니다.

```powershell
git clone https://github.com/4dblackhole/OsuTaikoConverter.git ../OsuTaikoConverter
```

## 빌드와 책임

Client 솔루션은 변환기의 Core 정적 라이브러리, 콘솔 EXE, 테스트 프로젝트를
포함합니다. `MRG.Client`는 변환기 EXE를 빌드 의존성으로 참조하고 같은 구성의
`OsuTaikoConverter.exe`를 `MyRhythmGame.exe` 옆에 복사합니다. EXE를 링크하지 않습니다.

변환기는 Client의 엔진 비종속 Chart/Modes/Rhythm 라이브러리만 재사용합니다.
도메인 라이브러리가 변환기나 Client 실행 프로젝트를 역참조하지 않습니다.
기존 YMM/YMP/YME 문법, 플레이 규칙, UI 및 엔진은 변경하지 않았습니다.
가져오기 UI나 프로세스 실행 기능을 Scene에 추가한 것은 아닙니다.

- `Core/Submodules/OsuDocument`: 원본 문법, 수치·모드 검증.
- `Core/Submodules/TimingMap`: BPM 기준점, 최근접 틱, omit 마디 길이.
- `Core/Converter`: 노트/효과 대응, 기존 파서 저장 왕복과 실제 Taiko 세션 검증.
- `Core/ConversionFiles`: 음원 복사, 상대 경로, 임시 출력 후 디렉터리 확정.
- `App/Main`: UTF-16 Windows 인자, UTF-8 JSON stdout, stderr/종료 코드.

## 실행 계약

```powershell
.\bin\x64\Release\OsuTaikoConverter.exe `
  --input "D:\Maps\song.osu" `
  --songs-root "D:\Game\Asset\Songs" `
  --output-dir "D:\Game\Asset\Songs\Imported\song" `
  --extra-divisors "40,48"
```

필수 인자는 `--input`, `--songs-root`, `--output-dir`입니다. 상대 경로는 프로세스의
작업 폴더 기준입니다. 출력은 Songs 루트 아래 **존재하지 않는 새 폴더**여야 합니다.
원본 `.osu`와 음원은 변경하지 않으며, 음원은 출력 폴더로 복사합니다.
`music.ymm`, `pattern.ymp`, `effects.yme`, `audio.<원본 확장자>`를 생성합니다.

YMP의 `Music metadata`는 Songs 루트 기준, `Effect file`은 YMP 폴더 기준,
YMM의 `File`은 YMM 폴더 기준으로 기록합니다. 성공 전 파일은 Songs 밖의 같은
볼륨에 임시 저장하며, 검사 후 디렉터리 이름 변경으로 공개합니다.
출력 폴더가 이미 존재하면 실패하며 덮어쓰기 옵션은 없습니다.

stdout은 JSON 객체 한 개입니다(`version: 1`, `success`, 생성 경로, 원본 노트 수,
출력 행 수, 격자 분모, 보정 이벤트 수/최대 이동 ms, 경고). 실패 시에는
`code`, `line`(원본 행, 해당하면), `error`가 들어갑니다. 사람이 읽는 오류는 stderr에도
출력합니다. `--help`는 예외적으로 도움말 텍스트를 출력합니다.

| 종료 코드 | 의미 |
| --- | --- |
| 0 | 변환 완료 |
| 2 | 인자/격자/출력 범위 오류 |
| 3 | 미지원 모드·데이터·범위 |
| 4 | 원본 파일/문법/음원 오류 |
| 5 | 출력 충돌·파일 시스템 오류 |
| 6 | 저장 왕복·시간·세션 검증 실패 |

Client에서 사용할 때 EXE 경로를 직접 지정하고 Windows 인자 인용 규칙에 따라
경로를 전달합니다. 렌더 스레드에서 종료를 기다리지 말고 stdout/stderr를 함께
소비하며, exit 0과 JSON success를 확인한 뒤 Songs catalog를 다시 읽습니다.

## 확정된 변환 규칙

- `Mode: 1`인 native Taiko만 지원합니다. 현재 입력 형식 범위는 osu! v5~v14입니다.
- 격자 `1/N`은 **4분음표 한 박을 N등분**합니다. 기본 분모는 1~32의 모든 정수이고,
  `--extra-divisors`로 1~1024 범위의 정수 분모를 추가합니다. 중복은 제거합니다.
- 노트 시작/끝과 효과 시각은 시간상 가장 가까운 틱으로 이동합니다. 최대 허용 오차는
  없고 동률이면 앞 틱을 택합니다. BPM 경계 양쪽의 유효한 후보도 비교합니다.
- 빨간 포인트는 원본 시각의 BPM·격자 기준점입니다. omit이 꺼진 중간 빨간 포인트는
  앞 마디를 짧게 종료합니다. omit이 켜지면 앞 부분과 새 박자표의 한 마디를 합칩니다.
  예: 2.5박 뒤 3/4 + omit이면 5.5박(`11/8`) 마디, 이후 3박 마디입니다.
  격자는 합친 마디 시작이 아닌 새 빨간 포인트에서 다시 시작합니다.
- YMP/YME 저장 위치는 기존대로 **온음표 기준**입니다. 격자 한 박은 파일의 `1/4`입니다.
- Don/Kat/큰 노트는 hitsound 비트로 구분합니다. 모든 슬라이더는 TickRoll 또는
  BigTickRoll, 스피너는 DengDeng입니다. 슬라이더 종료는 head의 BPM/SV/길이/반복 횟수로
  계산하고 시작·끝을 각각 보정합니다. tick rate 3은 `TickDivision=12`, 그 외는 16입니다.
- DengDeng의 HitCount는 원본 지속 시간과 osu! OD의 요구 입력률에서 계산합니다.
  판정 레벨은 Client 기본값이며 osu! 점수/판정창을 복제하지 않습니다.
- 스크롤은 Separate이며 `(SliderMultiplier / 1.4) × SV × (현재 BPM / Base BPM)`입니다.
  SV는 Taiko의 0.01~10 범위를 적용하며 빨간 포인트에서 1로 초기화합니다.
  같은 원본 시각에서는 빨간 포인트 적용 후 초록 포인트가 상태를 덮어씁니다.
- TimingPoints 볼륨은 HitSound/TickSound/UserInputFeedback에 `volume / 100`으로 기록합니다.
  Kiai는 YME ON/OFF 상태로 기록합니다. 현재 Client의 Kiai는 상태만 제공하고 연출은 없습니다.

## 제한과 검증

커스텀 샘플 뱅크/파일·노트별 샘플 override, 배경·영상·스토리보드·break는 내보내지
않으며 JSON 경고로 알립니다. 기본 Client 스킨 히트사운드를 사용합니다.
판정·점수·틱 끝점·롱노트 입력 동작은 [Client 판정 규칙](Gameplay/Rules.md)이 설명을 소유합니다.
변환기의 README와 JSON 안내도 Client `main`의 같은 문서를 링크하며 세부 플레이 규칙을
별도로 복제하지 않습니다. 변환기에는 객체 대응과 내보내는 파라미터의 설명만 유지합니다.
osu! 판정/점수를 재현하는 변환은 아니며, 원작과 다른 대응이나 미지원 데이터는 경고로 알립니다.

스냅 후 같은 위치의 일반 노트는 유지합니다. 롱노트 길이가 0이 되거나 기존 단일
Taiko lane이 노트를 누락시킬 겹침은 실패합니다. 첫 빨간 포인트 이전 노트,
24시간 범위 밖 시각, 100,000마디 초과, 1024회 초과 DengDeng은 미지원입니다.
원본 숫자는 유한해야 하며 입력 크기/객체 수 제한도 검사합니다.

빨간 기준점은 온음표 `1e-9` 단위로 수치 표현합니다(음악 격자 보정과 별개).
선택된 틱과 기존 MusicalTimeline이 복원한 시각의 차이는 최대 2µs여야 합니다.
저장 왕복, 파서 진단, 실제 Taiko 세션의 노트 개수 일치도 확인한 뒤 성공합니다.
변환기 테스트는 `..\OsuTaikoConverter\bin\x64\<구성>\OsuTaikoConverter.Tests.exe`입니다.
전체 Client 검증 명령은 [Verification](Verification.md)을 사용합니다.

실제 맵 복사본과 음원은 변환기 저장소의 gitignore 대상 `TestData/`에만 둡니다.
게임 기본곡이나 원격 저장소에 추가하지 않습니다.
