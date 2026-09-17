"""포획 검수 프레임을 게임 시각에 맞춰 GIF로 묶는다. 화면은 보정하지 않는다."""
import argparse
import json
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--resolution', default='1920x1080', choices=['1920x1080', '1280x800'])
    args = parser.parse_args()
    source = ROOT / 'Saved' / f'CaptureRecovery-{args.resolution}'
    frames = json.loads((source / 'frames.json').read_text(encoding='utf-8-sig'))
    assert len(frames) >= 20
    lines = ['ffconcat version 1.0']
    for index, frame in enumerate(frames):
        name = f"frame_{frame['frame']:05d}.png"
        assert (source / name).is_file(), name
        duration = frames[index + 1]['time'] - frame['time'] if index + 1 < len(frames) else .15
        assert 0 < duration < 2, duration
        lines.extend([f"file '{name}'", f'duration {duration:.4f}'])
    lines.append(f"file 'frame_{frames[-1]['frame']:05d}.png'")
    listing = source / 'capture.ffconcat'
    listing.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    def build(name, trim=''):
        output = ROOT / 'Docs' / 'Media' / name
        palette = source / 'palette.png'
        chain = trim + 'fps=12,scale=960:-1:flags=lanczos'
        common = ['ffmpeg', '-y', '-loglevel', 'error', '-safe', '0', '-i', str(listing)]
        subprocess.run(common + ['-vf', chain + ',palettegen=stats_mode=diff', str(palette)], check=True)
        subprocess.run(common + ['-i', str(palette), '-lavfi',
            chain + ' [x]; [x][1:v] paletteuse=dither=bayer:bayer_scale=3', str(output)], check=True)
        print(f'{output.name}: {output.stat().st_size / 1048576:.2f} MiB')

    build(f'ux-capture-recovery-{args.resolution}.gif')
    if args.resolution == '1920x1080':
        # GIF를 다시 잘라 양자화하면 노이즈가 커진다. 각 구간을 원본 PNG에서 만든다.
        build('m1-capture-embrace.gif', 'trim=end=3.2,setpts=PTS-STARTPTS,')
        # README의 짧은 포획 장면도 방금 검수한 연속 프레임을 사용한다.
        shutil.copyfile(ROOT / 'Docs/Media/m1-capture-embrace.gif', ROOT / 'Docs/Media/night-listener-chase.gif')
        build('m1-capture-wake-echo.gif', 'trim=start=2.9,setpts=PTS-STARTPTS,')
    print(f'{len(frames)}장, {frames[-1]["time"] - frames[0]["time"]:.2f}초')


if __name__ == '__main__':
    main()
