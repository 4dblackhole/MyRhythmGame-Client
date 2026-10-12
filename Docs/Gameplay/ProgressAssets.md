# 진행바 PNG 제작 기록

2026-10-12 built-in `image_gen` 도구 / `imagegen` 스킬을 사용했습니다.
기존 `GameProgressBar.png`를 참고하여 배경·트랙·채움·마커 각각을 별도 호출로 생성했습니다.
모든 호출은 투명 배경을 요청했습니다. 생성본은 alpha 128 이상인 영역에 1px 여백을 더해
자르고 Windows System.Drawing의 HighQualityBicubic으로 표시 원본 크기에 맞췄습니다.
프롬프트에 넣은 HEX는 원본 색상 유지 지침이며 생성본의 모든 픽셀을 강제하는 값은 아닙니다.

최종 저장 경로는 `FingerDrum.Assets/Assets/Skins/Default Skin/InGame/Progress`입니다.
파일은 `Background.png` 1820×30, `Track.png` 1704×10, `Fill.png` 1704×10,
`Marker.png` 14×14이며 시간 텍스트가 없는 RGBA PNG입니다.

## 최종 프롬프트

### Background.png

```text
Use case: precise-object-edit. Asset type: standalone PNG component for the provided rhythm-game progress bar. Input image is the edit target. Extract ONLY the dark navy rounded outer background panel of that bar. Remove ALL lettering, the internal gray track and the circular marker. Retain its flat dark navy color (#101421), very slightly rounded corners and extreme horizontal width. ONE extremely wide thin panel centered in transparent space, sharp clean flat UI asset, no shadows, gradients, decorative elements or text. Background must be truly transparent around the panel.
```

### Track.png

```text
Use case: precise-object-edit. Asset type: standalone PNG component for the provided rhythm-game progress bar. Input image is the edit target. Extract ONLY the muted slate gray horizontal track (#586071), as one extremely wide thin round-ended horizontal strip centered in transparent space. Remove background panel, lettering and circular marker. Flat uniform color, clean smooth edges, no shading, shadows, decorative elements or text. Background must be truly transparent.
```

### Fill.png

```text
Use case: precise-object-edit. Asset type: standalone PNG progress fill matching the provided rhythm-game progress bar. Input image is the style reference and edit target. Make ONLY one extremely wide thin horizontal strip for the completed portion, flat pale icy blue (#d9edf4) matching the original marker. No marker, no background panel, no track, no lettering. Clean smooth slightly rounded ends, flat uniform color, no shading, shadows, decorative elements or text. Center it in truly transparent space.
```

### Marker.png

```text
Use case: background-extraction. Asset type: standalone PNG component for the provided rhythm-game progress bar. Input image is the edit target. Extract ONLY the single small perfectly round pale icy blue progress-position marker (#d9edf4). Remove everything else. ONE centered solid circle on a truly transparent background. Flat uniform color, smooth antialiased circular edge, no shadow, gradient, outline or text.
```
