# ImageGen 제작 지시서 — 2026-08-24

## M1 포획 포옹 4단계 v2(닫히는 포옹)

- 실행 경로: ChatGPT ImageGen
- 용도 분류: `stylized-concept`
- 입력 이미지: `Content/SourceArt/AI/SheetListenerEntityAnatomyReference.png`
- 대체 대상: `SheetListenerCaptureEmbracePhases_v1_RGBA`
  (v1 원본과 SHA는 [IMAGEGEN_PROMPTS_2026-08-11.md](IMAGEGEN_PROMPTS_2026-08-11.md)에
  증빙으로 남긴다. 지우지 않는다)

### 왜 다시 뽑는가

v1은 생성이 실패한 게 아니라 **지시서대로 나왔다.** 그 지시서에 이렇게 적혀
있었다.

> bottom-right = calm held-close pose with both arms settled around the screen
> edges while the central view remains readable for the blackout
>
> keep the middle 35 percent mostly open

암전이 읽혀야 한다는 이유로 가운데를 비우라고 못 박았고, 생성기는 그대로
따랐다. 결과가 이렇다.

- 네 칸을 순서대로 틀면 손이 안으로 모이는 게 아니라 **바깥으로 벌어져
  화면을 빠져나간다.** 포옹은 안으로 닫히는 동작이므로 이건 포옹이 아니다.
- 마지막 칸(`T_FPCaptureEmbrace3_D`)에는 **손이 아예 없다.** 화면 좌우 끝에
  잘린 팔뚝 두 개가 서 있고 가운데는 완전히 비어 있다. 붙잡고 있어야 할
  마지막 포즈가 무엇이 닿았는지 말해 주지 못한다.
- 그래서 잡히는 순간에 아무것도 닿지 않는다. 팔이 잠깐 보였다 물러나고
  화면이 어두워질 뿐이다.

코드 쪽은 있는 그림으로 할 수 있는 만큼 돌려놨다 — 칸을 뒤에서부터 틀어
최소한 안으로 오므라들게 했고, 팔이 암전보다 먼저 사라지던 것도 없앴다.
그림 자체가 가운데를 비우도록 그려져 있는 것은 재생성으로만 고칠 수 있다.

### 무엇이 달라지는가

**가운데를 비우라는 제약을 걷어낸다.** 암전은 카메라가 따로 돌리고, HUD는
그 위에 그린다. 팔이 시야를 덮어도 암전은 읽힌다 — 오히려 마지막에 남는
그림이 빈 복도가 아니라 팔이어야 한다.

단계도 다시 잡는다. 1인칭에서 포옹은 팔이 **뒤에서 옆구리를 지나 앞으로
감겨 오는** 동작이다.

1. 양옆 가장자리에서 팔뚝만 들어온다. 손은 아직 화면 밖
2. 손이 프레임에 들어와 안쪽·앞쪽으로 온다. 손가락은 펴진 채
3. 손이 화면 가운데를 향해 좁혀지고 팔뚝이 좌우를 크게 덮는다
4. 두 손이 카메라 바로 앞에서 겹쳐 시야의 대부분을 가린다.
   손가락은 오므라들었지만 움켜쥔 게 아니라 감싼 모양

「폭력이 아니라 포옹」은 그대로 간다. 목을 조르거나 할퀴는 포즈는 여전히
금지다. 바뀌는 것은 **닿느냐 마느냐**다.

최종 프롬프트:

```text
Use case: stylized-concept
Asset type: production 2x2 first-person capture-embrace animation sprite sheet for a grounded psychological horror game
Input images: Image 1 is the exact identity, dry gray plaster coating, worn clothing material, proportions, and non-gory tone of the same faceless adult entity.
Primary request: create one clean 2x2 sprite sheet showing four sequential phases of the SAME entity closing its arms around the player from the player's first-person viewpoint, ending with the hands closing over the camera. Only the entity's same two plaster-coated forearms and hands enter the camera frame; never show a face, head, torso, player body, wall, room, or scenery.
Frame order and required progression: the arms must move INWARD and TOWARD the camera across the four cells, never outward and never off-frame. top-left = only the two forearms entering from the left and right edges, hands still outside the frame, center fully open; top-right = both hands now inside the frame, fingers extended, moving inward and closer to the camera, center about half open; bottom-left = hands converging toward the middle of the frame while the forearms thicken and cover the left and right thirds, center narrowing; bottom-right = both hands overlapping directly in front of the camera and covering most of the frame, fingers curled into a soft enveloping cup, not a grip. Every cell must contain two complete hands with all five fingers except top-left, where the hands are deliberately outside the frame. Never crop the hands off in the final cell.
Scene/backdrop: every cell uses the same perfectly flat solid #00ff00 chroma-key background. One uniform color only: no shadows, gradient, texture, reflection, floor plane, lighting variation, or scenery. Add a narrow uniform #00ff00 gutter between cells.
Subject: exactly two anatomically correct adult hands and two forearms total in every cell, same left/right identity and scale across all four cells, five fingers per hand, intact wrists and sleeves, dry gray plaster dust and worn muted clothing continuous from Image 1. The gesture is protective and sorrowful, not violent: no grabbing the throat, no striking, no claws, no injury, no clenched fist.
Style/medium: photorealistic grounded game asset, restrained practical-horror realism, crisp opaque silhouette suitable for chroma-key removal.
Composition/framing: player-eye first-person view; arms originate beyond the left and right cell boundaries so no rectangular sleeve ends are visible; the covered area of the frame grows monotonically from cell to cell; consistent camera and lens in all cells.
Lighting/mood: cool neutral soft frontal light matching Image 1's material readability, subtle shading only on the subject.
Constraints: preserve the same entity identity and material in all frames; exact 2x2 ordering; clear progressive inward motion; hands fully plausible; generous key-color separation; no cast shadow; no contact shadow; no text; no symbols; no watermark; do not use #00ff00 in the subject.
Avoid: arms moving outward, arms leaving the frame, hands cropped by the cell edge, an empty frame center in the final cell, extra hands, extra arms, duplicated fingers, fused fingers, missing fingers, disembodied floating hands, aggressive attack pose, neck restraint, gore, blood, exposed wounds, face, body, environment, doors, walls, UI, vignette, motion blur, internal rectangular cutoffs.
```

### 반입 절차

v1과 같다. 크로마키를 걷어내고 `Prepare-AIArt.ps1`의 네 칸 크롭으로 나눈다.

```powershell
python Scripts/remove_chroma_key.py --auto-key border --soft-matte --despill `
    --edge-contract 1 --transparent-threshold 12 --opaque-threshold 220 --force
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Prepare-AIArt.ps1 `
    -OnlySource SheetListenerCaptureEmbracePhases_v2_RGBA
```

`Prepare-AIArt.ps1`의 `Source`를 v2로 올리고, `IGHorrorHUD.cpp`의 역순 재생을
되돌린다 — v2는 좌상부터가 첫 접촉이므로 시트 순서대로 틀어야 한다. 그
되돌림을 잊으면 새 그림이 다시 거꾸로 돈다.
