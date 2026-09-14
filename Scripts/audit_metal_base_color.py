"""높은 금속성과 검은 색이 겹치는 텍스처를 추려 육안 검수할 후보를 저장한다."""

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=Path('Saved/MetalBaseColorAudit.json'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1] / 'Content/SourceArt/Blender'
findings = []
for file in sorted(root.glob('*/manifest.json')):
    data = json.loads(file.read_text(encoding='utf-8-sig'))
    textures = data.get('textures', {})
    if not all(role in textures for role in ('D', 'ORM')):
        continue
    maps = {}
    for role in ('D', 'ORM'):
        with Image.open(file.parent / textures[role]) as picture:
            maps[role] = np.asarray(picture.convert('RGB').resize((256, 256), Image.Resampling.NEAREST))
    base, orm = maps['D'], maps['ORM']
    # UV 바깥과 깊이 가려진 면을 빼고, 금속 영역의 검은 픽셀 비율을 본다.
    metal = (orm[:, :, 0] > 64) & (orm[:, :, 2] > 230)
    black = metal & (base.max(axis=2) < 8)
    if metal.sum() > 20 and black.sum() / metal.sum() > .4:
        findings.append({'name': data['name'], 'metal_pixels': int(metal.sum()),
                         'black_fraction': round(float(black.sum()/metal.sum()), 4),
                         'generated': data.get('generated_from')})
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(findings, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'METAL_BASE_COLOR_REVIEW candidates={len(findings)} output={args.output}')
