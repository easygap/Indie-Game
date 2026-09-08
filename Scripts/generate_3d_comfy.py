"""기준 이미지 한 장에서 ComfyUI 네이티브 노드(Pixal3D·TRELLIS.2)로 3D 형상을 뽑는다.

산출물은 Blender에서 다듬어야 게임 메시가 된다(Scripts/blender/refine_generated.py).
여기서 나오는 것은 원본 GLB 세 종류다.

    shape_*.glb            형상만, 수백만 삼각형(기본으로 지운다, --keep-raw)
    cleaned-colored_*.glb  리메시·데시메이트한 뒤 정점색을 입힌 것
    pbr_*.glb              UV를 펴고 BaseColor/Metallic/Roughness를 구운 것

그래프는 ComfyUI 0.34 공식 템플릿(3d_pixal3d_trellis2_image_to_model)을
API 형식으로 옮긴 것이다. 단계별 CFG 재설정과 흐름 스케줄을 템플릿 값 그대로
둔다. 서버는 Start-ComfyNative.ps1이 띄운 것을 쓴다.

    python Scripts/generate_3d_comfy.py --image <png> --name ListenerEntityCrawl \
        --route trellis --resolution 1024 --seed 56

산출물은 Content/SourceArt/Generated/<name>/<trial>/ 에 원본 그대로 복사하고
generation.json에 서버·그래프·해시를 남긴다.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import mimetypes
import os
import shutil
import sys
import time
import urllib.error
import urllib.request
import uuid
from datetime import datetime, timezone

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GENERATED_ROOT = os.path.join(REPO, "Content", "SourceArt", "Generated")


def log(message: str) -> None:
    print(f"[comfy3d] {message}", flush=True)


def sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def http_json(url: str, payload=None, timeout: float = 60.0):
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    request = urllib.request.Request(url, data=data, headers=headers)
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.load(response)


def upload_image(url: str, path: str, name: str) -> str:
    """/upload/image 로 올린다. 서버 input 폴더에 name으로 저장된다."""
    boundary = "----IGComfy" + uuid.uuid4().hex
    mime = mimetypes.guess_type(path)[0] or "image/png"
    with open(path, "rb") as handle:
        content = handle.read()
    body = b"".join([
        f"--{boundary}\r\n".encode(),
        f'Content-Disposition: form-data; name="image"; filename="{name}"\r\n'.encode(),
        f"Content-Type: {mime}\r\n\r\n".encode(),
        content, b"\r\n",
        f"--{boundary}\r\n".encode(),
        b'Content-Disposition: form-data; name="overwrite"\r\n\r\ntrue\r\n',
        f"--{boundary}--\r\n".encode(),
    ])
    request = urllib.request.Request(
        url + "/upload/image", data=body,
        headers={"Content-Type": f"multipart/form-data; boundary={boundary}"})
    with urllib.request.urlopen(request, timeout=120) as response:
        result = json.load(response)
    return result.get("name", name)


def make_prompt(image_name, route, resolution, seed, prefix, postprocess="pbr",
                remesh_resolution=512, decimate_faces=200000, texture_size=2048):
    graph = {}

    def node(number, kind, **inputs):
        graph[str(number)] = {"class_type": kind, "inputs": inputs}
        return [str(number), 0]

    image = node(1, "LoadImage", image=image_name)
    bg = node(2, "LoadBackgroundRemovalModel", bg_removal_name="birefnet.safetensors")
    mask = node(3, "RemoveBackground", bg_removal_model=bg, image=image)
    crop = node(4, "ImageCropToMask", images=image, masks=mask, width=1024, height=1024,
                pad_factor=1.1 if route == "pixal" else 1.0, grow_mask=0, background="#000000")
    node(5, "SaveImage", images=crop, filename_prefix=prefix + "/conditioning")
    vision = node(6, "CLIPVisionLoader", clip_name="dino_v3_L_naf_fp32.safetensors")
    if route == "pixal":
        moge = node(7, "LoadMoGeModel", model_name="moge_2_vitl_normal_fp16.safetensors")
        geometry = node(8, "MoGeInference", moge_model=moge, image=crop, resolution_level=9,
                        fov_x_degrees=0, batch_size=1, force_projection=True, apply_mask=True)
        fov = node(9, "MoGeGeometryToFOV", moge_geometry=geometry, axis="horizontal", unit="degrees")
        positive = node(10, "Pixal3DConditioning", clip_vision_model=vision, image=crop, camera_angle_x=fov)
    else:
        positive = node(10, "Trellis2Conditioning", clip_vision_model=vision, image=crop)
    negative = ["10", 1]
    checkpoint = "pixal3d_int8_convrot.safetensors" if route == "pixal" else "trellis_2_int8_convrot.safetensors"
    model = node(11, "UNETLoader", unet_name=checkpoint, weight_dtype="default")
    shape_vae = node(12, "VAELoader", vae_name="trellis_2_shape_vae_bf16.safetensors")
    texture_vae = node(13, "VAELoader", vae_name="trellis_2_texture_vae_bf16.safetensors")
    empty = node(14, "EmptyTrellis2LatentStructure", batch_size=1)

    structure_cfg = node(15, "CFGOverride", model=model, cfg=1, start_percent=0.667, end_percent=1)
    structure_rescale = node(16, "RescaleCFG", model=structure_cfg, multiplier=0.7)
    structure_model = node(17, "ModelSamplingSD3", model=structure_rescale, shift=5)
    structure = node(18, "KSampler", model=structure_model, positive=positive, negative=negative,
                     latent_image=empty, seed=seed, steps=12, cfg=7.5,
                     sampler_name="euler", scheduler="normal", denoise=1)
    voxel = node(19, "VaeDecodeStructureTrellis2", samples=structure, vae=shape_vae, resolution="32")
    shape_cond = node(20, "Trellis2ShapeStage", positive=positive, negative=negative, voxel=voxel)
    shape_cfg = node(21, "CFGOverride", model=model, cfg=1, start_percent=0.769, end_percent=1)
    shape_model = node(22, "RescaleCFG", model=shape_cfg, multiplier=0.5)
    shape = node(23, "KSampler", model=shape_model, positive=shape_cond, negative=["20", 1],
                 latent_image=["20", 2], seed=max(0, seed - 14), steps=20, cfg=7.5,
                 sampler_name="euler", scheduler="normal", denoise=1)
    if resolution > 512:
        up = node(24, "Trellis2UpsampleStage", positive=shape_cond, negative=["20", 1],
                  shape_latent=shape, vae=shape_vae, target_resolution=resolution)
        shape = node(25, "KSampler", model=shape_model, positive=up, negative=["24", 1],
                     latent_image=["24", 2], seed=max(0, seed - 14), steps=12, cfg=7.5,
                     sampler_name="euler", scheduler="simple", denoise=1)
    mesh = node(26, "VaeDecodeShapeTrellis", samples=shape, vae=shape_vae)
    smooth = node(27, "MeshSmoothNormals", mesh=mesh, crease_angle=180)
    node(28, "SaveGLB", mesh=smooth, filename_prefix=prefix + "/shape")
    texture_cond = node(29, "Trellis2TextureStage", positive=positive, negative=negative, shape_latent=shape)
    texture = node(30, "KSampler", model=model, positive=texture_cond, negative=["29", 1],
                   latent_image=["29", 2], seed=max(0, seed - 13), steps=12, cfg=1,
                   sampler_name="euler", scheduler="normal", denoise=1)
    colors = node(31, "VaeDecodeTextureTrellis", samples=texture, vae=texture_vae,
                  shape_subdivides=["26", 1])
    painted = node(32, "PaintMesh", mesh=smooth, voxel_colors=colors)
    node(33, "SaveGLB", mesh=painted, filename_prefix=prefix + "/colored")
    if postprocess != "raw":
        cleaned = node(34, "RemeshMesh", mesh=mesh, resolution=remesh_resolution, sign_mode="udf",
                       band=1.0, project_back=0.0, fix_poles=False, smooth_iters=3,
                       drop_small_components=0.0001, precluster_max_verts=2000000)
        graph["34"]["inputs"].update({
            "sign_mode.qef": False,
            "sign_mode.drop_inverted_components": False,
            "sign_mode.drop_enclosed_components": False,
        })
        reduced = node(35, "DecimateMesh", mesh=cleaned, target_face_count=decimate_faces,
                       placement_mode="midpoint")
        normaled = node(36, "MeshSmoothNormals", mesh=reduced, crease_angle=180)
        colored_clean = node(37, "PaintMesh", mesh=normaled, voxel_colors=colors)
        node(38, "SaveGLB", mesh=colored_clean, filename_prefix=prefix + "/cleaned-colored")
        if postprocess == "pbr":
            uv_mesh = node(39, "UnwrapMesh", mesh=normaled, segmenter="pec", resolution=texture_size,
                           padding=2, weld_distance=0.0002)
            maps = node(40, "BakeTextureFromVoxel", mesh=uv_mesh, voxel_colors=colors,
                        reference_mesh=mesh, texture_size=texture_size)
            material = node(41, "ApplyTextureToMesh", mesh=uv_mesh, base_color=maps,
                            metallic=["40", 1], roughness=["40", 2])
            node(42, "SaveGLB", mesh=material, filename_prefix=prefix + "/pbr")
            node(43, "SaveImage", images=maps, filename_prefix=prefix + "/base-color")
            node(44, "SaveImage", images=["40", 1], filename_prefix=prefix + "/metallic")
            node(45, "SaveImage", images=["40", 2], filename_prefix=prefix + "/roughness")
    return graph


def wait_for_prompt(url: str, prompt_id: str, timeout_seconds: float, graph: dict) -> dict:
    """/history 를 폴링한다. 큐 상태를 같이 읽어 어느 노드에 있는지 찍는다."""
    started = time.monotonic()
    last_report = 0.0
    while True:
        try:
            history = http_json(f"{url}/history/{prompt_id}", timeout=30)
        except (urllib.error.URLError, TimeoutError) as error:
            log(f"history read failed, retrying: {error}")
            history = {}
        entry = history.get(prompt_id)
        if entry:
            status = entry.get("status", {})
            if status.get("completed"):
                return entry
            if status.get("status_str") == "error":
                messages = status.get("messages", [])
                raise RuntimeError(f"execution error: {json.dumps(messages)[:2000]}")
        elapsed = time.monotonic() - started
        if elapsed > timeout_seconds:
            try:
                http_json(f"{url}/interrupt", payload={"prompt_id": prompt_id}, timeout=10)
            except Exception:  # noqa: BLE001
                pass
            raise TimeoutError(f"prompt {prompt_id} exceeded {timeout_seconds:.0f}s")
        if elapsed - last_report >= 30.0:
            last_report = elapsed
            try:
                queue = http_json(f"{url}/queue", timeout=15)
                running = queue.get("queue_running", [])
                position = "running" if any(item[1] == prompt_id for item in running) else \
                    f"pending({len(queue.get('queue_pending', []))})"
            except Exception:  # noqa: BLE001
                position = "?"
            log(f"  {elapsed:5.0f}s {position}")
        time.sleep(5.0)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--image", required=True, help="기준 이미지(PNG/JPG). 배경은 서버가 지운다")
    parser.add_argument("--name", required=True, help="에셋 이름(ASCII). Content/SourceArt/Generated/<name>")
    parser.add_argument("--route", choices=["pixal", "trellis"], default="trellis")
    parser.add_argument("--resolution", type=int, choices=[512, 1024, 1536], default=1024)
    parser.add_argument("--seed", type=int, default=56)
    parser.add_argument("--postprocess", choices=["raw", "remesh", "pbr"], default="pbr")
    parser.add_argument("--decimate-faces", type=int, default=200000)
    parser.add_argument("--texture-size", type=int, default=2048)
    parser.add_argument("--trial", default=None, help="비우면 <route><resolution>-s<seed>")
    parser.add_argument("--url", default="http://127.0.0.1:8199")
    parser.add_argument("--timeout", type=int, default=5400)
    parser.add_argument("--keep-raw", action="store_true",
                        help="shape_/colored_ GLB(수백 MB)도 남긴다. 기본은 pbr_와 cleaned-colored_만")
    args = parser.parse_args()

    if not all(c.isalnum() or c in "-_" for c in args.name):
        sys.exit("--name 은 ASCII 글자·숫자·-·_ 만 쓴다")
    image_path = os.path.abspath(args.image)
    if not os.path.isfile(image_path):
        sys.exit(f"이미지가 없다: {image_path}")
    trial = args.trial or f"{args.route}{args.resolution}-s{args.seed}"
    if not all(c.isalnum() or c in "-_" for c in trial):
        sys.exit("--trial 은 ASCII 글자·숫자·-·_ 만 쓴다")

    stats = http_json(f"{args.url}/system_stats", timeout=15)
    device = (stats.get("devices") or [{}])[0]
    log(f"server v{stats.get('system', {}).get('comfyui_version')} "
        f"vram free {device.get('vram_free', 0) / 2**20:.0f}MiB / {device.get('vram_total', 0) / 2**20:.0f}MiB")

    out_dir = os.path.join(GENERATED_ROOT, args.name, trial)
    if os.path.exists(os.path.join(out_dir, "generation.json")):
        sys.exit(f"이미 있는 시도다. --trial 을 바꿔라: {out_dir}")
    os.makedirs(out_dir, exist_ok=True)

    source_hash = sha256(image_path)
    upload_name = f"{args.name}-{source_hash[:12]}{os.path.splitext(image_path)[1].lower()}"
    uploaded = upload_image(args.url, image_path, upload_name)
    log(f"uploaded {os.path.basename(image_path)} -> {uploaded}")
    shutil.copy2(image_path, os.path.join(out_dir, "source" + os.path.splitext(image_path)[1].lower()))

    prefix = f"ig3d/{args.name}/{trial}"
    graph = make_prompt(uploaded, args.route, args.resolution, args.seed, prefix,
                        args.postprocess, decimate_faces=args.decimate_faces,
                        texture_size=args.texture_size)
    with open(os.path.join(out_dir, "workflow-api.json"), "w", encoding="utf-8") as handle:
        json.dump(graph, handle, indent=2)

    record = {
        "name": args.name, "trial": trial, "status": "Queued",
        "source": os.path.relpath(image_path, REPO).replace("\\", "/"), "sourceSha256": source_hash,
        "route": args.route, "resolution": args.resolution, "seed": args.seed,
        "postprocess": args.postprocess, "decimateFaces": args.decimate_faces,
        "textureSize": args.texture_size,
        "server": {"url": args.url, "version": stats.get("system", {}).get("comfyui_version"),
                   "device": device.get("name")},
        "startedAt": datetime.now(timezone.utc).isoformat(),
        "license": "TRELLIS.2 / Pixal3D weights: Comfy-Org repackage, MIT/permissive; DINOv3, MoGe, BiRefNet: see lunia_z SourceArt/Manifests/ComfyNative.20260908.json",
        "runtimeReady": False,
    }

    def save_record():
        with open(os.path.join(out_dir, "generation.json"), "w", encoding="utf-8") as handle:
            json.dump(record, handle, indent=2, ensure_ascii=False)
            handle.write("\n")

    save_record()
    client_id = uuid.uuid4().hex
    try:
        result = http_json(f"{args.url}/prompt", payload={"prompt": graph, "client_id": client_id}, timeout=120)
    except urllib.error.HTTPError as error:
        body = error.read().decode("utf-8", "replace")
        record.update(status="Rejected", error=body[:4000])
        save_record()
        sys.exit(f"서버가 그래프를 거부했다: {body[:2000]}")
    prompt_id = result["prompt_id"]
    record["promptId"] = prompt_id
    save_record()
    log(f"queued {trial} as {prompt_id}")

    started = time.monotonic()
    try:
        entry = wait_for_prompt(args.url, prompt_id, args.timeout, graph)
        record["status"] = "Generated"
    except Exception as error:  # noqa: BLE001
        record.update(status="Failed", error=str(error)[:4000])
        save_record()
        raise
    finally:
        record["elapsedSeconds"] = round(time.monotonic() - started, 1)
        record["finishedAt"] = datetime.now(timezone.utc).isoformat()

    # 서버 output 폴더에서 우리 산출물만 가져온다.
    output_root = None
    receipt = os.path.join(os.environ.get("LOCALAPPDATA", ""), "IndieGame", "Comfy", "server-receipt.json")
    if os.path.isfile(receipt):
        with open(receipt, "r", encoding="utf-8-sig") as handle:
            output_root = json.load(handle).get("output")
    files = []
    if output_root:
        generated = os.path.join(output_root, "ig3d", args.name, trial)
        if os.path.isdir(generated):
            for root, _, names in os.walk(generated):
                for filename in sorted(names):
                    src = os.path.join(root, filename)
                    dst = os.path.join(out_dir, "raw", os.path.relpath(src, generated))
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    shutil.copy2(src, dst)
                    files.append({"path": os.path.relpath(dst, REPO).replace("\\", "/"),
                                  "bytes": os.path.getsize(dst), "sha256": sha256(dst)})
    # shape_·colored_는 삼각형 천만 개짜리라 200 MB가 넘는다. 시드·그래프가
    # 기록에 있어 다시 뽑을 수 있으니 저장소에는 pbr_·cleaned-colored_만 둔다.
    if not args.keep_raw:
        kept = []
        for item in files:
            full = os.path.join(REPO, item["path"])
            name = os.path.basename(full)
            if name.startswith(("shape_", "colored_")) and name.endswith(".glb"):
                os.remove(full)
                item["pruned"] = True
                log(f"  pruned {name} ({item['bytes'] / 1e6:.0f} MB)")
            kept.append(item)
        files = kept
    record["files"] = files
    record["outputs"] = entry.get("outputs", {}) if isinstance(entry, dict) else {}
    save_record()
    for item in files:
        log(f"  {item['bytes'] / 1e6:7.2f} MB  {item['path']}")
    log(f"done in {record['elapsedSeconds']}s -> {out_dir}")


if __name__ == "__main__":
    main()
