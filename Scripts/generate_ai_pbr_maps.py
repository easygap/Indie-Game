"""Generate conservative PBR companion maps from the approved AI material scans.

This is an offline source-art step.  It deliberately keeps the generated bitmap as
BaseColor and derives only low-amplitude surface information from it; hero geometry,
silhouette, collision, and shadows remain the responsibility of the 3D mesh.
"""

from __future__ import annotations

import argparse
import math
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageFilter, ImageOps, ImageStat


@dataclass(frozen=True)
class SurfaceSpec:
    stem: str
    roughness: float
    roughness_min: float
    roughness_max: float
    normal_strength: float
    rough_detail: float = 0.22
    ao_depth: float = 1.35
    wetness: float = 0.0
    metallic: bool = False
    coated_metal: bool = False


SURFACES = (
    # ImageGen supplies only the evenly lit BaseColor scan. Conservative
    # companion maps make the embossed paper read under a moving practical
    # light without turning the tiny floral print into glittering geometry.
    SurfaceSpec(
        "T_ApartmentWallpaperV2", 0.86, 0.72, 0.94, 0.32,
        rough_detail=0.12, ao_depth=0.62),
    # The plain embossed sibling. Same paper, same room light, so the same
    # conservative numbers -- with one difference: the emboss is a real ridge
    # where the floral print is only ink, so the normal may carry it and the
    # roughness must not. A vinyl skin is uniformly matte whatever shape it is
    # pressed into; letting rough_detail follow the ribs would draw them twice
    # and turn a wall into corduroy under the flashlight.
    SurfaceSpec(
        "T_ApartmentWallpaperEmboss", 0.86, 0.74, 0.92, 0.38,
        rough_detail=0.06, ao_depth=0.55),
    # Exterior cement render is deliberately matte. The generated scan owns
    # only colour; this conservative relief keeps rain streaks from becoming
    # deep grooves and remains stable under the moving alley practicals.
    SurfaceSpec(
        "T_KoreanVillaStucco", 0.88, 0.76, 0.96, 0.44,
        rough_detail=0.10, ao_depth=0.66),
    # 무코팅 크라프트지는 확산 반사가 강하다. 상자가 조각처럼 보이지 않는
    # 범위에서 손전등에 세로 골과 섬유 결이 드러나도록 조정한다.
    SurfaceSpec(
        "T_MovingBoxCardboard", 0.87, 0.74, 0.95, 0.34,
        rough_detail=0.12, ao_depth=0.54),
    # One neutral fibre response is shared by the clean, damp, folded and old
    # paper stocks. Their colour maps keep the individual stains and creases;
    # this companion set supplies only sub-millimetre fibre relief, roughness
    # breakup and shallow occlusion. Keeping text out of the height source is
    # important for the runtime-drawn Korean thermal receipt.
    SurfaceSpec(
        "T_PaperClean_V2", 0.84, 0.72, 0.93, 0.24,
        rough_detail=0.10, ao_depth=0.48),
    SurfaceSpec("T_WetHoodie", 0.67, 0.48, 0.84, 0.52, wetness=0.92),
    SurfaceSpec("T_AlleyCatTabby", 0.83, 0.68, 0.94, 0.34, rough_detail=0.14),
    SurfaceSpec("T_WaterTankGalvanized", 0.46, 0.25, 0.68, 0.78, wetness=0.72, metallic=True),
    SurfaceSpec(
        "T_TankInteriorBiofilm", 0.58, 0.22, 0.86, 0.84,
        wetness=0.82, metallic=True, coated_metal=True),
    SurfaceSpec("T_WetServiceHose", 0.61, 0.42, 0.76, 0.66, wetness=0.88),
    SurfaceSpec("T_WetRungPad", 0.70, 0.50, 0.86, 0.72, wetness=0.82),
    SurfaceSpec("T_P3CabinetPaintedSteel", 0.64, 0.48, 0.78, 0.58, wetness=0.42),
    SurfaceSpec("T_CarrierBagFilm", 0.29, 0.18, 0.46, 0.24, rough_detail=0.12),
    SurfaceSpec("T_TankWaterSurface", 0.12, 0.06, 0.24, 0.86, rough_detail=0.10),
    # Dry gypsum is shared by the fifth-floor shell and the listener mesh.
    # Keep it highly diffuse; the flashlight should reveal powder and cracks
    # through N/A, never turn the body into polished stone.
    SurfaceSpec(
        "T_MissingFloorDryPlaster", 0.91, 0.78, 0.97, 0.62,
        rough_detail=0.10, ao_depth=0.86),
    # The listener front card is still a lit surface, not an unlit pasted
    # photo. Conservative relief lets the flashlight pick up pajama folds and
    # plaster grain without pretending the portrait has full geometric depth.
    SurfaceSpec(
        "T_SpriteListenerFront", 0.86, 0.72, 0.96, 0.38,
        rough_detail=0.12, ao_depth=0.58),
    # The four locomotion phases share the same restrained response. Keeping
    # their parameters identical prevents roughness or normal strength from
    # visibly flashing when the runtime advances a frame.
    SurfaceSpec(
        "T_SpriteListenerCrawl0", 0.86, 0.72, 0.96, 0.38,
        rough_detail=0.12, ao_depth=0.58),
    SurfaceSpec(
        "T_SpriteListenerCrawl1", 0.86, 0.72, 0.96, 0.38,
        rough_detail=0.12, ao_depth=0.58),
    SurfaceSpec(
        "T_SpriteListenerCrawl2", 0.86, 0.72, 0.96, 0.38,
        rough_detail=0.12, ao_depth=0.58),
    SurfaceSpec(
        "T_SpriteListenerCrawl3", 0.86, 0.72, 0.96, 0.38,
        rough_detail=0.12, ao_depth=0.58),
    SurfaceSpec(
        "T_SpriteFinalCavity", 0.90, 0.76, 0.97, 0.42,
        rough_detail=0.10, ao_depth=0.72),
    SurfaceSpec(
        "T_SpriteMokFinalUpper", 0.84, 0.68, 0.94, 0.36,
        rough_detail=0.10, ao_depth=0.54),
    # Painted steel stair treads. The diamond tread is a real 3 mm relief that
    # no mesh here will ever carry — the stairs are scaled boxes — so the normal
    # is the only place it can exist.
    #
    # Strength came down from 0.88 to 0.62 when v2 replaced v1. That is not a
    # retreat: 0.88 was propping up a scan with stddev 3.5, and v2 arrives at
    # 19.6, so the same setting would now emboss the plate into corrugation.
    # 0.62 matches the dry plaster, which sits at a comparable contrast.
    #
    # Roughness stays below the concrete family on purpose. Alkyd paint over
    # steel is still a dielectric, so metallic remains 0, but it catches a
    # flashlight in a way troweled concrete cannot. That difference is the
    # point: README rule 2 asks the player to choose a floor by how loud it is,
    # and until now the metal stair and the concrete corridor were the same
    # picture.
    SurfaceSpec(
        "T_MissingFloorSteelStair", 0.68, 0.52, 0.86, 0.62,
        rough_detail=0.14, ao_depth=0.90),
    # Rooftop urethane membrane. A thick rubbery coat: diffuse, but not as dead
    # as concrete, and the roller laps are a soft thickness change rather than
    # cut relief, so the normal stays gentle.
    SurfaceSpec(
        "T_RooftopWaterproofing", 0.82, 0.70, 0.92, 0.45,
        rough_detail=0.14, ao_depth=0.70),
    # Gypsum debris on a raw slab. The strongest normal of the floors, because
    # the shards genuinely sit proud of the concrete and the player is meant to
    # register that this floor is covered in something.
    SurfaceSpec(
        "T_MissingFloorGypsumDebris", 0.93, 0.85, 0.97, 0.66,
        rough_detail=0.16, ao_depth=1.00),
    # Enamel over stamped metal. Nearly flat and the only surface here with a
    # real sheen; the graduation ticks are printed, not engraved, so relief
    # stays minimal or the dial starts to look embossed.
    SurfaceSpec(
        "T_UtilityMeterDial", 0.42, 0.30, 0.58, 0.18,
        rough_detail=0.08, ao_depth=0.40),
    # Waxed carbon coating. The whole read is that used areas catch light
    # differently from unused ones, which is a roughness story rather than a
    # normal one — hence the wide roughness range and the low normal.
    SurfaceSpec(
        "T_CarbonPaper", 0.55, 0.38, 0.74, 0.28,
        rough_detail=0.12, ao_depth=0.50),
    # Powder coat on a door leaf. Flat is correct here: orange peel is a
    # sub-millimetre swell, and any more relief turns a maintained door into a
    # corroded one. Metallic stays 0 — the paint is what the light meets.
    # rough_detail is high for so flat a surface, and has to be: at 0.10 the
    # roughness map came back effectively constant and the art contract caught
    # it. A door with one uniform roughness reads as plastic under a moving
    # flashlight, which ASSET_STYLE forbids outright — and it is wrong anyway.
    # Powder coat collects dust in the orange-peel troughs and polishes where
    # hands pass, so the gloss genuinely varies even when the colour does not.
    SurfaceSpec(
        "T_UnitDoorPaintedSteel", 0.74, 0.62, 0.86, 0.30,
        rough_detail=0.26, ao_depth=0.50),
)


def _clamp(value: float, low: float = 0.0, high: float = 1.0) -> float:
    return min(high, max(low, value))


def _save_l(path: Path, values: bytearray, size: tuple[int, int]) -> None:
    Image.frombytes("L", size, bytes(values)).save(path, optimize=True)


def _generate(spec: SurfaceSpec, source_root: Path, force: bool) -> list[Path]:
    base_path = source_root / f"{spec.stem}_D.png"
    if not base_path.exists():
        raise FileNotFoundError(f"BaseColor source is missing: {base_path}")

    outputs = {
        "N": source_root / f"{spec.stem}_N.png",
        "R": source_root / f"{spec.stem}_R.png",
        "A": source_root / f"{spec.stem}_A.png",
    }
    if spec.wetness > 0.0:
        outputs["W"] = source_root / f"{spec.stem}_W.png"
    if spec.metallic:
        outputs["M"] = source_root / f"{spec.stem}_M.png"

    if not force and all(path.exists() and path.stat().st_mtime >= base_path.stat().st_mtime for path in outputs.values()):
        print(f"[PBR] up-to-date: {spec.stem}")
        return list(outputs.values())

    base = Image.open(base_path).convert("RGB")
    gray = base.convert("L")
    width, height = gray.size
    pixels = gray.tobytes()
    rgb = base.tobytes()
    local_blur = gray.filter(ImageFilter.GaussianBlur(radius=3.0)).tobytes()
    broad_blur = gray.filter(ImageFilter.GaussianBlur(radius=18.0)).tobytes()
    mean_luma = ImageStat.Stat(gray).mean[0]

    normal = bytearray(width * height * 3)
    roughness = bytearray(width * height)
    occlusion = bytearray(width * height)
    wetness = bytearray(width * height) if spec.wetness > 0.0 else None
    metalness = bytearray(width * height) if spec.metallic else None

    for y in range(height):
        y_up = (y - 1) % height
        y_down = (y + 1) % height
        row = y * width
        up_row = y_up * width
        down_row = y_down * width
        for x in range(width):
            x_left = (x - 1) % width
            x_right = (x + 1) % width
            index = row + x
            value = pixels[index]

            dx = (pixels[row + x_right] - pixels[row + x_left]) / 255.0
            dy = (pixels[down_row + x] - pixels[up_row + x]) / 255.0
            nx = -dx * spec.normal_strength
            ny = -dy * spec.normal_strength
            inv_length = 1.0 / math.sqrt(nx * nx + ny * ny + 1.0)
            n_index = index * 3
            normal[n_index] = round((nx * inv_length * 0.5 + 0.5) * 255.0)
            normal[n_index + 1] = round((ny * inv_length * 0.5 + 0.5) * 255.0)
            normal[n_index + 2] = round((inv_length * 0.5 + 0.5) * 255.0)

            local_delta = (local_blur[index] - value) / 255.0
            micro_detail = abs(local_delta)
            tonal_bias = (127.5 - value) / 255.0
            rough = spec.roughness + micro_detail * spec.rough_detail + tonal_bias * 0.08
            roughness[index] = round(_clamp(rough, spec.roughness_min, spec.roughness_max) * 255.0)

            cavity = max(0.0, local_delta)
            ao = _clamp(1.0 - cavity * spec.ao_depth, 0.58, 1.0)
            occlusion[index] = round(ao * 255.0)

            if wetness is not None:
                # Wetness is intentionally low-frequency. Fine weave/rust grain
                # belongs in N/R and must not become glittering wet speckles.
                local_value = local_blur[index]
                broad_cavity = max(0.0, (broad_blur[index] - local_value) / 72.0)
                low_tone = max(0.0, (mean_luma - local_value) / 255.0)
                wet = _clamp(broad_cavity * 0.82 + low_tone * 0.38 - 0.035)
                wetness[index] = round(wet * 255.0)

            if metalness is not None:
                r = rgb[n_index]
                g = rgb[n_index + 1]
                b = rgb[n_index + 2]
                warm_rust = _clamp(max(0.0, r - g) / 54.0 + max(0.0, g - b) / 92.0)
                dark_oxide = _clamp((72.0 - value) / 72.0) * 0.24
                if spec.coated_metal:
                    # Calcite, biofilm and rust are dielectric coatings over
                    # steel. Their pixels must not reflect as bare metal.
                    pale_scale = _clamp((value - 135.0) / 90.0)
                    olive_biofilm = _clamp(
                        max(0.0, g - r) / 40.0 + max(0.0, g - b) / 55.0)
                    coating = max(warm_rust, pale_scale * 0.78, olive_biofilm)
                    metal = _clamp(0.82 - coating * 0.76 - dark_oxide, 0.02, 0.88)
                else:
                    metal = _clamp(0.93 - warm_rust * 0.88 - dark_oxide, 0.03, 0.96)
                metalness[index] = round(metal * 255.0)

    Image.frombytes("RGB", (width, height), bytes(normal)).save(outputs["N"], optimize=True)
    _save_l(outputs["R"], roughness, (width, height))
    _save_l(outputs["A"], occlusion, (width, height))
    if wetness is not None:
        wet_image = Image.frombytes("L", (width, height), bytes(wetness)).filter(ImageFilter.GaussianBlur(radius=8.0))
        wet_image = ImageOps.autocontrast(wet_image, cutoff=(4.0, 1.0))
        wet_image = wet_image.point(lambda value: round(value * spec.wetness))
        wet_image.save(outputs["W"], optimize=True)
    if metalness is not None:
        _save_l(outputs["M"], metalness, (width, height))

    print(f"[PBR] generated: {spec.stem} ({width}x{height}, maps={','.join(outputs)})")
    return list(outputs.values())


def main() -> int:
    parser = argparse.ArgumentParser(description="Build PBR companion maps for AI material scans")
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[1] / "Content" / "SourceArt")
    parser.add_argument("--only", action="append", default=[], help="Surface stem to generate (repeatable)")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    selected = [spec for spec in SURFACES if not args.only or spec.stem in args.only]
    unknown = sorted(set(args.only) - {spec.stem for spec in SURFACES})
    if unknown:
        parser.error(f"Unknown surface stem(s): {', '.join(unknown)}")

    generated: list[Path] = []
    for spec in selected:
        generated.extend(_generate(spec, args.source_root.resolve(), args.force))
    print(f"[PBR] ready: surfaces={len(selected)}, maps={len(generated)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
