"""엔진 기본 큐브의 여섯 면이 각각 텍스처를 어느 손으로 읽는지 잰다.

`CreateBlock`은 대부분 `/Engine/BasicShapes/Cube`를 쓰고, 인쇄 재질은 메시
UV를 그대로 읽는다. 그래서 「이 면에 붙은 글자가 바로 서는가 뒤집히는가」는
전적으로 그 큐브의 UV 배치에 달려 있다. 캡처를 눈으로 읽어 추측하면 축마다
다른 답이 나오므로, 메시에서 직접 잰다.

각 삼각형에서 dP/dU 와 dP/dV 를 풀고 `dot(cross(dPdU, dPdV), N)` 의 부호를
본다. 부호가 같은 면끼리는 같은 손이고, 다른 면은 서로 거울이다.

    UnrealEditor-Cmd <project> -run=pythonscript -script=probe_cube_face_uvs.py
"""

import unreal


AXES = {
    (1, 0, 0): "+X", (-1, 0, 0): "-X",
    (0, 1, 0): "+Y", (0, -1, 0): "-Y",
    (0, 0, 1): "+Z", (0, 0, -1): "-Z",
}


def log(message):
    unreal.log_warning(f"[CUBEUV] {message}")


def face_key(normal):
    best, key = 0.0, None
    for axis, name in AXES.items():
        dot = normal.x * axis[0] + normal.y * axis[1] + normal.z * axis[2]
        if dot > best:
            best, key = dot, name
    return key


def probe():
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    if cube is None:
        log("Cube MISSING")
        return

    mesh = unreal.new_object(unreal.DynamicMesh)
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        cube,
        mesh,
        unreal.GeometryScriptCopyMeshFromAssetOptions(),
        unreal.GeometryScriptMeshReadLOD())

    queries = unreal.GeometryScript_MeshQueries
    # 5.8은 이 이름을 get_triangle_u_vs 로 부른다. 마이너 버전마다 바뀌므로
    # 이름을 하나로 박지 않는다.
    triangle_uvs = None
    for candidate in ("get_triangle_u_vs", "get_triangle_uvs", "get_triangle_u_v_s"):
        triangle_uvs = getattr(queries, candidate, None)
        if triangle_uvs is not None:
            log(f"uv query = {candidate}")
            break
    if triangle_uvs is None:
        log("no triangle UV query binding")
        return
    triangle_count = mesh.get_triangle_count()
    log(f"triangles={triangle_count}")

    tally = {}
    for triangle_id in range(triangle_count * 2):
        positions = queries.get_triangle_positions(mesh, triangle_id)
        if not positions[-1]:
            continue
        p0, p1, p2 = positions[1], positions[2], positions[3]
        uvs = triangle_uvs(mesh, 0, triangle_id)
        if not uvs[-1]:
            continue
        # 위치 질의는 메시를 앞에 한 번 더 돌려주지만 UV 질의는 그러지 않는다.
        # 반환 길이로 갈라 읽는다.
        offset = len(uvs) - 4
        t0, t1, t2 = uvs[offset], uvs[offset + 1], uvs[offset + 2]

        edge1 = p1 - p0
        edge2 = p2 - p0
        du1, dv1 = t1.x - t0.x, t1.y - t0.y
        du2, dv2 = t2.x - t0.x, t2.y - t0.y
        determinant = du1 * dv2 - du2 * dv1
        if abs(determinant) < 1.0e-9:
            continue
        inverse = 1.0 / determinant
        d_p_du = unreal.Vector(
            (edge1.x * dv2 - edge2.x * dv1) * inverse,
            (edge1.y * dv2 - edge2.y * dv1) * inverse,
            (edge1.z * dv2 - edge2.z * dv1) * inverse)
        d_p_dv = unreal.Vector(
            (edge2.x * du1 - edge1.x * du2) * inverse,
            (edge2.y * du1 - edge1.y * du2) * inverse,
            (edge2.z * du1 - edge1.z * du2) * inverse)

        normal = unreal.Vector.cross(edge1, edge2)
        length = normal.length()
        if length < 1.0e-6:
            continue
        normal = normal / length
        handed = unreal.Vector.cross(d_p_du, d_p_dv)
        sign = (handed.x * normal.x + handed.y * normal.y
                + handed.z * normal.z)
        key = face_key(normal)
        if key:
            tally.setdefault(key, []).append(1.0 if sign > 0.0 else -1.0)

    for name in ("+X", "-X", "+Y", "-Y", "+Z", "-Z"):
        signs = tally.get(name, [])
        if not signs:
            log(f"{name}: no triangles")
            continue
        average = sum(signs) / len(signs)
        log(f"{name}: handedness={average:+.1f} over {len(signs)} triangle(s)")
    log("done")


probe()
