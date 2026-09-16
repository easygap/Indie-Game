"""Triangle budgets and LOD chains for the procedurally authored meshes.

`generate_meshes.py` lathes, sweeps and booleans every hero prop from scratch.
That is how they get their real thickness and their real dimensions, and it is
also how a 500 mL bottle ends up with sixty-four radial segments through every
one of its profile rings whether the player is holding it or looking at it from
the far end of the alley.

Until now the mesh build asked the engine for a LOD *group* and stopped, and
the props flagged story-critical asked for nothing at all — those shipped as a
single full-density LOD with no reduction at any distance. This module states
what the meshes actually have to be:

* a triangle budget for LOD0 per class, enforced by simplification at bake
  time rather than by hoping the author counted;
* an explicit LOD chain with authored screen sizes, so the same prop reduces
  the same way on every machine and in a cook, instead of depending on
  whichever LOD group the engine version happens to define;
* a lightmap UV channel, because these are static props in a Lumen scene.

Kept free of ``unreal`` so the release validator and any offline tool can read
the same numbers the bake used.
"""

from __future__ import annotations


class MeshClass:
    """A budget and a reduction curve for one family of props."""

    def __init__(self, name, lod0_triangles, chain, lightmap_resolution):
        self.name = name
        self.lod0_triangles = lod0_triangles
        # (percent of LOD0 triangles, screen size at which the LOD takes over)
        self.chain = tuple(chain)
        self.lightmap_resolution = lightmap_resolution

    @property
    def lod_count(self) -> int:
        return 1 + len(self.chain)


# Screen sizes are the fraction of screen height the mesh's bounding sphere
# covers. 0.5 is "filling half the frame" — a prop in the hands; 0.06 is a
# thing across the room; 0.015 is the far end of the alley.
HERO = MeshClass(
    "hero",
    # A hero prop is inspected at arm's length under a flashlight, so LOD0 is
    # generous. It is still a budget: the pre-budget bake ran to 40k on the
    # crawling body and 26k on the tool cart, which is more silhouette than a
    # 1080p frame can resolve at any distance the player can reach.
    lod0_triangles=12000,
    chain=((0.55, 0.28), (0.25, 0.09), (0.10, 0.025)),
    lightmap_resolution=64,
)

PROP = MeshClass(
    "prop",
    # Shelf and table clutter: bottles, cups, tubs, handles, notes.
    lod0_triangles=3000,
    chain=((0.45, 0.20), (0.18, 0.06), (0.07, 0.015)),
    lightmap_resolution=32,
)

LARGE = MeshClass(
    "large",
    # Fixtures the player walks around: the water tank, the access stair, the
    # service cabinet, the roof door. These hold their silhouette much further
    # out, so the chain starts later and never drops as far.
    lod0_triangles=9000,
    chain=((0.60, 0.14), (0.30, 0.045), (0.12, 0.010)),
    lightmap_resolution=96,
)

MESH_CLASSES = {cls.name: cls for cls in (HERO, PROP, LARGE)}

# Props the story puts in the player's hands or a metre from their face.
HERO_MESHES = frozenset({
    "SM_AlleyCatRun",
    "SM_FirstPersonHoodieSleeve",
    "SM_HornRimGlasses",
    "SM_InspectionRod",
    "SM_CrackedPhone",
    "SM_CarrierBagCollapsed",
    "SM_LadderFailureRung",
    "SM_LadderRungPadLifted",
    "SM_LadderRungRetainingClips",
    "SM_P3ValveWheelLarge",
    "SM_P3ValveWheelSmall",
    "SM_P3PressureGauge",
    "SM_OfferingWaterBowl",
    "SM_CupSleeve",
    "SM_LabelSleeve",
    "SM_StickyNote76mm",
    "SM_CaptureMercyNote",
    "SM_ListenerEntityCrawl",
    "SM_FinalCavityClothingShell",
    "SM_FinalCavityBoneInsert",
    "SM_FinalCavityTarp",
    "SM_FinalCavityBrokenCaster",
    "SM_MokHansooWorkwear",
    "SM_MokHansooHeadHands",
    "SM_MokHansooGypsumBoard",
    "SM_TuningHammer",
    "SM_TunerToolCart",
    "SM_ComplaintLedger",
    "SM_CalendarJournal",
    # 자물쇠·열쇠 세 개·태그가 한 뭉치인 조사 물증이다. 분리된 고리와 원통이
    # 많아 QEM이 3000까지 못 내려가고(실측 5628에서 수렴), 팔 길이에서
    # 읽는 프롭이라 hero 예산이 맞다.
    "SM_RooftopUnlockedPadlockKeys",
})

LARGE_MESH_PREFIXES = (
    "SM_RooftopWaterTank",
    "SM_RooftopTank",
    "SM_TankInternal",
    "SM_TankAccess",
    "SM_TankExterior",
    "SM_RooftopFireDoor",
    "SM_P3ServiceCabinet",
    "SM_RooftopServiceHose",
)

# Meshes whose UVs carry printed artwork placed by hand — a label band, a
# cup sleeve, a sticky note. Simplification is allowed to move vertices but
# never to weld across the UV seam, so these keep split-vertex preservation on
# and never get the more aggressive collapse.
PRINTED_SURFACE_MESHES = frozenset({
    "SM_CupSleeve",
    "SM_LabelSleeve",
    "SM_StickyNote76mm",
    "SM_CaptureMercyNote",
    "SM_ComplaintLedger",
    "SM_CalendarJournal",
    "SM_SnackBag",
    "SM_MilkCarton",
})


def classify(asset_name: str) -> MeshClass:
    """The budget class one generated mesh belongs to."""
    if asset_name.startswith(LARGE_MESH_PREFIXES):
        return LARGE
    if asset_name in HERO_MESHES:
        return HERO
    return PROP


SMALL_ROUND_CHAINS = {
    # 3.4cm 마개에 일반 소품의 20% 전환점을 쓰면 눈앞에서도 원둘레가 무너진다.
    # 마지막 단계는 화면 높이 0.5%(1080p에서 약 5px)부터만 허용한다.
    "SM_BottleCap": ((.80, .035), (.55, .012), (.30, .005)),
    # 투명한 병 어깨는 윤곽과 굴절이 함께 보이므로 팔 길이에서는 원형을 보존한다.
    "SM_WaterBottle": ((.60, .10), (.30, .040), (.12, .012)),
}


def lod_plan(asset_name: str, mesh_class=None):
    """[(lod index, percent triangles, screen size)] for the reduced LODs."""
    mesh_class = mesh_class or classify(asset_name)
    return [
        (index + 1, percent, screen)
        for index, (percent, screen) in enumerate(SMALL_ROUND_CHAINS.get(asset_name, mesh_class.chain))
    ]


def triangle_budget(asset_name: str) -> int:
    return classify(asset_name).lod0_triangles


def preserves_uv_seams(asset_name: str) -> bool:
    return asset_name in PRINTED_SURFACE_MESHES
