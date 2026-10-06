# 판정 이미지 제작 기록

2026-10-06 built-in `image_gen`으로 MAX를 생성한 뒤 이를 참조해 6종을 편집했습니다.
CLI/API fallback은 사용하지 않았습니다. 모든 호출에 transparent_background=true를 지정했고,
선택한 원본 PNG를 변경 없이 프로젝트에 복사했습니다.

저장 위치는 `FingerDrum.Assets/Assets/Skins/Default Skin/InGame/Judgements/`이며
파일명은 MAX.png, PERFECT.png, GREAT.png, GOOD.png, BAD.png, MISS.png, POOR.png입니다.
실제 결과는 모두 1254×1254 RGBA이며 중심과 모서리 알파가 0입니다.
아래 프롬프트의 요청 크기와 실제 도구 출력 크기는 다릅니다. 런타임은 이미지 크기를 읽고
판정원의 크기에 맞춰 비율을 유지해 표시합니다. 교체/표시 계약은 [Presentation](Presentation.md)에 있습니다.

## MAX 생성 프롬프트

```text
Use case: ui-mockup. Asset type: production 2D rhythm-game judgement overlay PNG on a truly transparent background. Create ONE clean flat circular outline ring plus the exact uppercase word "MAX" laid horizontally across the TOP of that ring, as a combined image. Canvas 1024x1024. Ring MUST be a perfect circle centered exactly at (512,512), outer radius 380 pixels, thin stroke width 22 pixels, empty transparent interior. The word sits at y=125-235, centered x=512, so it overlaps the top arc slightly; bold upright condensed sans serif with a dark navy outline for legibility, white fill. Ring color WHITE with a restrained cyan edge. Match a simple clean game UI: flat crisp geometric graphics, no perspective, no photorealism, no icons, no sparkles, no decorative elements, no other text, no extra circles, no background. Entire ring and word visible with margins. Export with actual alpha transparency. Ring center and radius are critical for overlaying an existing judgement circle. Exact text MAX only.
```

## 편집 프롬프트와 치환값

MAX.png를 referenced_image_paths로 전달했습니다. 각 요청은 아래 템플릿의
{grade}/{color}를 표의 값으로 치환한 별도 도구 호출입니다.

```text
Edit this game judgement overlay asset. Change ONLY the word MAX to exact uppercase text "{grade}" and change the ring and text fill color to {color}. Preserve the exact canvas size, ring center, circle radius, stroke width, overall margins, transparency, flat clean UI style, dark navy text outline, upright bold condensed typography and text position at the top arc. Remove any isolated specks in the transparent interior. No added details, no icons, no extra rings, no background. This is a production sprite: perfectly circular hollow ring with the word {grade} centered above its interior, attached to the top arc. Preserve actual alpha transparency. All seven judgement sprites must align when drawn over the same circle.
```

| grade | color |
| --- | --- |
| PERFECT | bright cyan |
| GREAT | vivid green |
| GOOD | golden yellow |
| BAD | violet |
| MISS | muted grey |
| POOR | red |
