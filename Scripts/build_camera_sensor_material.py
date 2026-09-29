"""1인칭 화면에 싼 디지털 카메라의 질감을 입히는 후처리 재질.

두 참고작(AFTERLIGHT: The Apartment, The Strange Lights)이 사실감을 얻는 곳은
모델이 아니라 화면이다. 다만 이 게임은 2025년이라 VHS 테이프를 씌우면 시대가
어긋난다. 그래서 휴대폰·보급형 카메라가 어두운 곳에서 내는 흠만 고른다.

- 렌즈의 가벼운 통 왜곡과 가장자리 색 번짐
- 센서가 뭉갠 부드러움과 기기 안 샤프닝이 남기는 테두리
- 오른쪽으로 번지는 색(4:2:0 압축)
- 어두운 곳일수록 커지는 노이즈, 약간 뜬 검정

톤매핑 뒤에 건다. 업스케일이 끝난 화면이라 노이즈가 번지지 않고, HUD는 이
재질보다 나중에 그려져서 글자가 흐려지지 않는다. 세기는 UIGCameraSensorComponent가
접근성 설정 「화면 질감」에서 받아 Strength로 넘긴다.
"""

from pathlib import Path
import sys
import unreal

ROOT = Path(unreal.SystemLibrary.get_project_directory())
sys.path.insert(0, str(ROOT / "Scripts"))
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
NAME = "M_PP_CameraSensor"
FOLDER = "/Game/Prototype/Materials"

# 기본값이 곧 「화면 질감 100%」다. 조절은 Strength 하나로 한다.
PARAMETERS = (
    ("Strength", 1.0),
    ("NoiseAmount", 0.06),
    ("NoiseRate", 30.0),
    ("Distortion", 0.055),
    ("Fringe", 1.2),
    ("Soften", 0.6),
    ("Halo", 0.32),
    ("ChromaSmear", 0.55),
    ("BlackLift", 0.003),
)

CODE = r"""
float s = saturate(Strength);
float4 viewSize = GetSceneTextureViewSize(PPI_PostProcessInput0);
float2 inv = viewSize.zw;
float2 vuv = GetViewportUV(Parameters);
float aspect = viewSize.x * viewSize.w;

struct FCameraTap
{
	float3 At(float2 ViewportUV)
	{
		float2 BufferUV = ClampSceneTextureUV(
			ViewportUVToSceneTextureUV(ViewportUV, PPI_PostProcessInput0),
			PPI_PostProcessInput0);
		return SceneTextureLookup(BufferUV, PPI_PostProcessInput0, true).rgb;
	}
};
FCameraTap tap;

// 통 왜곡. 모서리가 화면 밖으로 나가지 않게 전체를 조금 당긴다.
float2 c = vuv - 0.5;
float2 ca = float2(c.x * aspect, c.y);
float r2 = dot(ca, ca);
float cornerR2 = 0.25 * (aspect * aspect + 1.0);
float k = Distortion * s;
float2 duv = 0.5 + c * (1.0 + k * r2) / (1.0 + k * cornerR2);

// 가장자리로 갈수록 커지는 색 번짐. 화면 끝에서 Fringe 픽셀이다.
float2 fringe = c * 2.0 * Fringe * s * inv;
float3 centre = tap.At(duv);
float3 col = float3(tap.At(duv + fringe).r, centre.g, tap.At(duv - fringe).b);

// 센서의 부드러움과 기기 안 샤프닝.
float2 o1 = inv;
float2 o2 = inv * 2.5;
float3 near4 = tap.At(duv + float2(o1.x, 0)) + tap.At(duv - float2(o1.x, 0))
	+ tap.At(duv + float2(0, o1.y)) + tap.At(duv - float2(0, o1.y));
float3 soft = (col * 4.0 + near4) / 8.0;
float3 wide4 = tap.At(duv + o2) + tap.At(duv - o2)
	+ tap.At(duv + float2(o2.x, -o2.y)) + tap.At(duv + float2(-o2.x, o2.y));
float3 wide = (soft * 4.0 + wide4) / 8.0;
float3 img = lerp(col, soft, saturate(Soften * s));
img += (img - wide) * (Halo * s);

// 색은 밝기보다 넓게, 오른쪽으로 번진다.
float3 luma = float3(0.299, 0.587, 0.114);
float3 right = tap.At(duv + float2(o2.x, 0));
float3 chromaSource = (soft * 2.0 + right) / 3.0;
float Y = dot(img, luma);
float3 smeared = chromaSource - dot(chromaSource, luma);
img = Y + lerp(img - Y, smeared, saturate(ChromaSmear * s));

// 어두운 곳일수록 커지는 센서 노이즈. NoiseRate만큼 초마다 새로 뿌린다.
uint2 ip = uint2(vuv * viewSize.xy);
uint frame = (uint)floor(Time * max(NoiseRate, 1.0));
uint h = ip.x * 1973u + ip.y * 9277u + frame * 26699u;
h = (h ^ 61u) ^ (h >> 16);
h *= 9u;
h = h ^ (h >> 4);
h *= 0x27d4eb2du;
h = h ^ (h >> 15);
uint h2 = h * 747796405u + 2891336453u;
h2 ^= h2 >> 13;
h2 *= 0x5bd1e995u;
h2 ^= h2 >> 15;
float grain = (h & 0xFFFFu) / 65535.0 - 0.5;
float grainBlue = (h2 & 0xFFFFu) / 65535.0 - 0.5;
float grainRed = ((h2 >> 16) & 0xFFFFu) / 65535.0 - 0.5;
// 완전한 검정에는 노이즈를 얹지 않는다. 카메라의 노이즈 제거가 먼저 뭉개는
// 곳이고, 여기서 검정을 띄우면 밤 복도의 대비가 죽는다.
float shade = 1.0 - saturate(Y);
float amp = NoiseAmount * s * smoothstep(0.0, 0.08, Y) * (0.25 + 0.75 * shade * shade);
img += grain * amp;
img += float3(grainRed, 0.0, grainBlue) * amp * 0.4;

// 센서의 검정은 완전한 0이 아니다.
float lift = BlackLift * s;
img = img * (1.0 - lift) + lift * float3(0.92, 1.0, 1.07);
return max(img, 0.0);
"""


def enum_value(owner, *names):
    for name in names:
        value = getattr(owner, name, None)
        if value is not None:
            return value
    raise RuntimeError(f"{owner.__name__}에 {names} 중 아무것도 없다")


def build():
    asset = unreal.load_asset(f"{FOLDER}/{NAME}")
    if not asset:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    for expression in list(LIB.get_material_expressions(asset)):
        LIB.delete_material_expression(asset, expression)

    asset.set_editor_property(
        "material_domain", enum_value(unreal.MaterialDomain, "MD_POST_PROCESS"))
    asset.set_editor_property(
        "blendable_location",
        enum_value(
            unreal.BlendableLocation,
            "BL_SCENE_COLOR_AFTER_TONEMAPPING",
            "BL_AFTER_TONEMAPPING"))

    def node(kind, **properties):
        result = LIB.create_material_expression(asset, getattr(unreal, kind))
        for key, value in properties.items():
            result.set_editor_property(key, value)
        return result

    def link(a, b, pin):
        if not LIB.connect_material_expressions(a, "", b, pin):
            raise RuntimeError(f"연결 실패: {pin}")

    custom = node(
        "MaterialExpressionCustom",
        code=CODE,
        output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        description="CameraSensor")
    pins = ["SceneTex", "Time"] + [name for name, _ in PARAMETERS]
    inputs = []
    for label in pins:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", label)
        inputs.append(pin)
    custom.set_editor_property("inputs", inputs)

    # 입력값 자체는 쓰지 않는다. 이 노드가 있어야 셰이더가 PostProcessInput0을 묶는다.
    scene = node(
        "MaterialExpressionSceneTexture",
        scene_texture_id=enum_value(
            unreal.SceneTextureId, "PPI_POST_PROCESS_INPUT0"))
    link(scene, custom, "SceneTex")
    link(node("MaterialExpressionTime"), custom, "Time")
    for name, default in PARAMETERS:
        parameter = node(
            "MaterialExpressionScalarParameter",
            parameter_name=name,
            default_value=default)
        link(parameter, custom, name)

    LIB.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.layout_material_expressions(asset)
    LIB.recompile_material(asset)
    ASSETS.save_loaded_asset(asset)
    unreal.log(
        f"CAMERA_SENSOR_MATERIAL PASS domain={asset.get_editor_property('material_domain')} "
        f"location={asset.get_editor_property('blendable_location')}")


build()
