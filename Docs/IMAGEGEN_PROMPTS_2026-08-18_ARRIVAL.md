# ImageGen asset record — arrival moving box cardboard

Date: 2026-08-18
Tool mode: built-in `image_gen`
Use: production PBR material for the safe-evening arrival moving boxes

## Prompt

```text
Use case: stylized-concept
Asset type: seamless tileable game texture source for first-person moving boxes
Primary request: photorealistic close-up orthographic albedo of ordinary used corrugated cardboard from Korean household moving boxes, completely generic and unbranded
Scene/backdrop: edge-to-edge cardboard surface only
Subject: warm kraft paper fibers, subtle corrugation pressure bands, restrained scuffs, faint tape-lift discoloration, a few shallow handling creases and softened worn patches
Style/medium: photorealistic material scan, production PBR albedo source
Composition/framing: square, flat orthographic, seamless tiling; no single distinctive focal mark
Lighting/mood: uniform neutral diffuse illumination, no directional shadows, no baked highlights, no ambient occlusion
Color palette: natural muted kraft brown with realistic small fiber variation
Materials/textures: centimeter-scale paper fiber and compressed corrugated board detail
Constraints: no text, no Korean characters, no symbols, no shipping labels, no tape, no logos, no trademarks, no watermarks, no objects, no corners, no perspective; maintain physically plausible low-contrast albedo and seamless edges
Avoid: grunge overload, dramatic stains, repeating large crease, studio lighting, specular shine, printed graphics
```

## Files and SHA-256

| Role | Repository path | SHA-256 |
|---|---|---|
| ImageGen original | `Content/SourceArt/AI/TextureMovingBoxCardboard_v1.png` | `6904110877F1567A8D0DC1B5076982FD8468908046A15F619665C9D55AD3BFB8` |
| Albedo | `Content/SourceArt/T_MovingBoxCardboard_D.png` | `18292D6AF8EBF126EB28D640CDB63DD437839CEA9FBD00B4C2F65505233EC469` |
| Normal | `Content/SourceArt/T_MovingBoxCardboard_N.png` | `F3C8F09081568D07FCBFCF57F9DAD0647438C04C2886E9B5BB7830B84B4E4FC4` |
| Roughness | `Content/SourceArt/T_MovingBoxCardboard_R.png` | `232E858EC034A4E98460A0556AC59FCE50B686DA0EAA05D9A35442A844E505D1` |
| Ambient occlusion | `Content/SourceArt/T_MovingBoxCardboard_A.png` | `9B24C2B5DB6B22EB0F4737B9DE2852BCC6D1FC51A0CF9566614D1A2B1202780E` |

The deterministic art pipeline imports the four derived maps as
`/Game/Prototype/Textures/T_MovingBoxCardboard_{D,N,R,A}` and binds them to
`/Game/Prototype/Materials/M_MovingBoxCardboardUV`. The generated image contains
no brand, readable label, trademark, or personal information; story-specific
Korean copy remains authored as runtime text rather than baked into the image.
