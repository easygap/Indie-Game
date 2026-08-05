# ImageGen Prompt Record — 2026-08-05

## Windows application icon

- built-in ImageGen generation
- source: `Content/SourceArt/AI/ApplicationIcon_raw.png`
- graded PNG: `Build/Windows/ApplicationIcon.png`
- packaged icon: `Build/Windows/Application.ico`
- deterministic post-process: `Scripts/prepare_application_icon.py`

```text
Use case: stylized-concept
Asset type: Windows game application icon for the Korean first-person horror game "4시 44분"
Primary request: a single original, premium square icon built around the circular inspection hatch of an old galvanized rooftop water tank before dawn
Scene/backdrop: nearly black blue-gray pre-dawn darkness, no separate background scene
Subject: centered circular metal hatch seen almost straight-on; the inner opening is deep black; a thin crescent of cold cyan reflected light catches the wet galvanized rim; one very small muted rust-red reflection sits low on the rim
Style/medium: restrained cinematic realism simplified into a strong game-icon silhouette, physically plausible wet metal, subtle corrosion and mineral scale, not painterly fantasy
Composition/framing: perfectly centered, symmetrical at thumbnail scale, generous safe margin, one dominant circular shape, readable at 32x32 pixels
Lighting/mood: quiet Korean everyday horror, humid 04:44 blue hour, oppressive and uncanny without gore
Color palette: charcoal black, desaturated blue-gray, cold cyan highlight, a single subdued rust-red accent
Materials/textures: galvanized steel, condensation, slight age and wear
Constraints: no text, no numbers, no letters, no people, no body, no blood, no ghost, no face, no occult symbols, no decorative horror clichés, no border, no mockup, no watermark, no logos or trademarks; square composition; production-ready app icon
```

The generated source was center-cropped and resampled to 1024×1024. A restrained
midtone gamma lift, contrast pass, desaturation and small-radius unsharp mask keep
the wet steel ring legible on both light and dark Windows shells without lifting
the black hatch interior. The ICO contains 16, 24, 32, 48, 64, 128 and 256 pixel
entries. A nearest-neighbor enlargement of the final 32-pixel level was visually
checked for silhouette and red-accent retention.
