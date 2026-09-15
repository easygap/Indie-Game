"""게임에서 저장한 연속 프레임을 촬영 간격대로 묶는다. 검수 영상의 속도를 올리지 않는다."""
import argparse
import os
from pathlib import Path
import statistics
import subprocess
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('frames',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
frames=sorted(args.frames.glob('frame_*.png'))
if len(frames)<8:
    raise SystemExit('연속 프레임이 8장보다 적다.')
times=[p.stat().st_mtime for p in frames]
durations=[b-a for a,b in zip(times,times[1:])]
if any(d<=0 or d>3 for d in durations):
    raise SystemExit('프레임의 저장 시각이 연속된 촬영과 맞지 않는다.')
durations.append(statistics.median(durations))
args.output.parent.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='ig_capture_') as temporary:
    listing=Path(temporary)/'frames.ffconcat'
    lines=['ffconcat version 1.0']
    for frame,duration in zip(frames,durations):
        path=frame.resolve().as_posix().replace("'","'\\''")
        lines += [f"file '{path}'",f'duration {duration:.6f}']
    lines.append(lines[-2])
    listing.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    result=args.output.with_name(args.output.stem+'.pending.gif')
    try:
        subprocess.run(['ffmpeg','-y','-loglevel','error','-f','concat','-safe','0','-i',str(listing),
                        '-vf','scale=960:-1:flags=lanczos,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4',
                        '-fps_mode','vfr','-loop','0',str(result)],check=True)
        os.replace(result,args.output)
    finally:
        result.unlink(missing_ok=True)
print(f'CAPTURE_REVIEW frames={len(frames)} seconds={sum(durations):.2f} bytes={args.output.stat().st_size}')
