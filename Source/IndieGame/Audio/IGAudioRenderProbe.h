#pragma once

#include "CoreMinimal.h"

class UObject;

/**
 * -IGAudioRenderProbe: 합성 큐를 WAV로 굽는다.
 *
 * 이 게임의 소리는 전부 런타임 합성이라 파일이 없고, 그래서 귀 없이 확인할 길이
 * 없었다. 팩토리마다 파형을 몇 초 렌더해 Saved/AudioProbe/<이름>.wav로 쓰면
 * Scripts/render_audio_probe_sheet.py가 스펙트로그램과 피크·RMS를 그린다. 소리를
 * 눈으로 보는 검수다. -nullrhi -nosound로 돌아도 된다 — 오디오 장치 없이 파형만
 * 만든다.
 */
namespace IGAudioRenderProbe
{
	/** 명령줄에 플래그가 있으면 렌더하고 프로세스를 끝낸다. 없으면 false. */
	INDIEGAME_API bool RunIfRequested(UObject* Outer);
}
