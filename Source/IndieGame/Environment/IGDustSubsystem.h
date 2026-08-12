#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "IGDustSubsystem.generated.h"

/**
 * One place where the air was stirred, and how hard. Strength decays with age;
 * callers read the decayed value, never this raw one.
 */
struct FIGDustDisturbance
{
	FVector Location = FVector::ZeroVector;

	/** Strength as reported, 0..1. Age is applied at query time. */
	float Strength = 0.0f;

	/** World time the stir happened. */
	double TimeSeconds = 0.0;
};

/**
 * 공기 중 석고 가루 — the building's other memory.
 *
 * 위층 사람이 벽 사이를 기어 지나가면 미장이 부스러져 가루가 뜬다. 그 가루는
 * 어두운 복도에서 보이지 않고, 손전등 빔을 통과할 때만 반짝인다. 그래서 이
 * 서브시스템은 조명 장식이 아니라 정보다: 플레이어는 빛을 들어 그가 방금
 * 어디를 지나갔는지 읽는다 (STORY_BIBLE_MISSING_FLOOR.md §11 V1).
 *
 * UIGNoiseSubsystem과 대칭으로 설계했다. 존재가 흔적을 보고하고, 렌더가 그
 * 자리의 밀도를 질의한다. 어느 쪽도 상대를 알지 않으며, 이 서브시스템은
 * 아무것도 틱하지 않는다 — 감쇠는 질의 시점에 계산된다.
 */
UCLASS()
class INDIEGAME_API UIGDustSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Records that something dragged through here. Repeated reports within
	 * MergeDistance of the newest sample refresh it instead of piling up, so a
	 * slow crawl leaves a readable dotted line rather than a solid smear.
	 */
	void ReportDisturbance(const FVector& Location, float Strength = 1.0f);

	/**
	 * How much thicker the air is at Location: 1.0 is ordinary corridor air,
	 * MaxDensityMultiplier is a lane he has just crawled down.
	 */
	UFUNCTION(BlueprintPure, Category = "Dust")
	float GetDensityMultiplierAt(const FVector& Location) const;

	/**
	 * Live samples inside a sphere, ordered newest first, with age already
	 * folded into Strength. Zero-strength samples are never returned.
	 */
	void CollectDisturbances(
		const FVector& Center,
		float Radius,
		TArray<FIGDustDisturbance>& OutSamples) const;

	/**
	 * Capture reset returns the hour to 04:30, so the air has to forget too —
	 * otherwise a reset player reads a path that no longer exists (§5.4).
	 */
	void ClearDisturbances();

	/** Samples that still carry strength right now. Diagnostics and contracts. */
	UFUNCTION(BlueprintPure, Category = "Dust")
	int32 GetLiveDisturbanceCount() const;

	/**
	 * How long a stir stays readable. Long enough that a player who heard him
	 * pass can still light the lane and see it; short enough that the corridor
	 * is not permanently mapped.
	 */
	static constexpr float DisturbanceLifetimeSeconds = 52.0f;

	/** Radius over which one sample thickens the air, in centimeters. */
	static constexpr float DisturbanceRadius = 165.0f;

	/** §11 V1: 존재가 지나간 자리는 모트 밀도 2배. Exactly twice, never more. */
	static constexpr float MaxDensityMultiplier = 2.0f;

	/** Reports closer than this to the newest sample refresh it in place. */
	static constexpr float MergeDistance = 42.0f;

	/** Ring capacity. At 40 cm spacing this is about 19 m of crawled lane. */
	static constexpr int32 MaxDisturbances = 48;

private:
	/** Drops fully decayed samples. Called from report and collect paths. */
	void PruneExpired() const;

	double GetNow() const;

	/** Mutable so const queries can retire dead samples as they pass them. */
	mutable TArray<FIGDustDisturbance> Disturbances;
};
