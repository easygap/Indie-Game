#pragma once

#include "CoreMinimal.h"
#include "IGListenerTuning.generated.h"

/**
 * 난이도 4종 (STORY_BIBLE_MISSING_FLOOR.md §20.4).
 *
 * "쉬움/보통/어려움"이라 부르지 않는다 — 이름도 세계의 언어여야 한다. 어느
 * 모드도 서사·진실·엔딩·저장 호환성을 잠그지 않으며, 언제든 바꿀 수 있다.
 */
UENUM(BlueprintType)
enum class EIGNightDifficulty : uint8
{
	/** 조용한 밤 — 공포는 원하되 추격 압박이 부담스러운 플레이어. */
	Quiet,
	/** 기본 — §20.2 튜닝 테이블 그대로. 밸런스의 기준선이다. */
	Standard,
	/** 성급한 밤 — 재도전·상급. 티어가 1에서 시작한다. */
	Hasty,
	/**
	 * 듣기만 하는 밤 — 접근성. 존재는 INVESTIGATE까지만 전이하고 포획이
	 * 없다. 서사·퍼즐·엔딩은 전부 동일하며, 포획이 없으므로 엔딩 C의 도달
	 * 조건이 밤4의 05:30 벽 미개방으로 대체된다. 실패의 문이 완전히 닫히면
	 * 성공의 무게도 사라진다.
	 */
	ListenOnly,
	Count UMETA(Hidden)
};

/**
 * 한 밤에 대해 완전히 해결된 숫자들. 밤 인덱스(§20.2)와 난이도(§20.4),
 * 공격성 티어(§4.3-7)의 세 축을 곱해 나온 결과이며, 존재는 이 구조체만 읽는다.
 */
struct FIGListenerTuning
{
	/**
	 * 청취 감도. §20.2의 기본 청취 반경 9/11/11/13m을 밤1 기준으로 정규화한
	 * 배율이다. 소음 이벤트의 반경(소음 크기 × 전달 거리)에 곱해지므로 소리의
	 * 크기가 가진 의미는 그대로 남고, 밤이 깊어질수록 같은 소리가 더 멀리서
	 * 걸린다.
	 */
	float HearingSensitivity = 1.0f;

	/** LISTENING 창. 밤 기준값에 티어 배율이 곱해진 최종 초. */
	float ListenWindowSeconds = 8.0f;

	/** 순찰 노드 수. 밤이 깊어질수록 그가 도는 곳이 늘어난다. */
	int32 PatrolNodeCount = 6;

	/** CHASE 속도. 기는 속도에 §20.2의 배율을 곱한 절대값(cm/s). */
	float ChaseSpeed = 264.0f;

	/** INVESTIGATE 도착 후 제자리 청취 초. */
	float InvestigateHoldSeconds = 6.0f;

	/** §5.6 히트맵이 순찰 선택에 개입하는 정도. 0이면 통계를 보지 않는다. */
	float HeatmapWeight = 0.0f;

	/** 티어3에서 가장 뜨거운 구역에 미리 가 기다릴 수 있는가. */
	bool bTierThreeAmbushAllowed = false;

	/** WAITING 시간 배율. 조용한 밤만 1.5로 늘린다. */
	float WaitScale = 1.0f;

	/** 포획이 성립하는가. 듣기만 하는 밤에서만 거짓이다. */
	bool bCaptureEnabled = true;

	/** CHASE로 전이할 수 있는가. 듣기만 하는 밤은 INVESTIGATE까지다. */
	bool bChaseEnabled = true;
};

/**
 * §20.2 튜닝 테이블과 §20.4 난이도의 해석기.
 *
 * 순수 함수와 하나의 저장값만으로 이루어져 있다. 밸런스 기준은 항상
 * 기본 난이도이며, 마이크 모드는 난이도가 아니다(§5.7).
 */
namespace IGListenerTuning
{
	/** 밤 인덱스는 1~4로 고정된다. 프롤로그와 낮은 존재가 없다. */
	constexpr int32 FirstNight = 1;
	constexpr int32 LastNight = 4;

	/** §20.2의 기본 청취 반경을 정규화하는 기준(밤1의 9m). */
	constexpr float ReferenceHearingRadius = 900.0f;

	/**
	 * 세 축을 곱해 최종 숫자를 낸다. NightIndex가 범위를 벗어나면 가장 가까운
	 * 밤으로 고정하므로, 아직 밤이 시작되지 않은 프레임에도 안전하다.
	 */
	INDIEGAME_API FIGListenerTuning Resolve(
		int32 NightIndex,
		EIGNightDifficulty Difficulty,
		int32 AggressionTier);

	/** 세계의 언어로 된 모드 이름. UI와 로그가 같은 문자열을 쓴다. */
	INDIEGAME_API FText GetDifficultyLabel(EIGNightDifficulty Difficulty);

	/**
	 * 난이도는 사용자 설정이므로 GameUserSettings.ini에 남는다. 진행 중
	 * 세이브에는 들어가지 않는다 — 저장본이 난이도를 잠그면 §20.4의 "언제든
	 * 변경할 수 있다"가 깨진다.
	 */
	INDIEGAME_API EIGNightDifficulty LoadPersistedDifficulty();
	INDIEGAME_API void SavePersistedDifficulty(EIGNightDifficulty Difficulty);

	/** 명령행 오버라이드. 저장값을 덮어쓰지 않고 이 실행에만 적용된다. */
	INDIEGAME_API EIGNightDifficulty ResolveActiveDifficulty();
}
