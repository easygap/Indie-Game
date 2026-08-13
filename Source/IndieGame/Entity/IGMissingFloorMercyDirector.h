#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGMissingFloorMercyDirector.generated.h"

class AIGListenerEntity;
class AIGMissingFloorNightThreeDirector;
class UIGMissingFloorNarrativeSubsystem;

/** Which world response the net reached for. Rotates so it never metronomes. */
UENUM(BlueprintType)
enum class EIGMercyResponse : uint8
{
	None,
	/** 배관이 운다 — the riser carries water where it did not a moment ago. */
	PipeCry,
	/** 존재가 필요한 벽에 귀를 댄다 — he stops at the wall that matters. */
	EarToWall,
	/**
	 * 문 아래로 메모가 밀린다 — a folded note comes out from under 401.
	 *
	 * 「낮에 와.」 「문 열어 둘게.」 Two lines in her handwriting, and neither is
	 * about the puzzle. Somebody is awake at half past four, she cannot help
	 * through a door at this hour, and she is telling the player when she can —
	 * which routes them to 황순금, the third safety net, instead of duplicating
	 * her. The nets chain, and this note is the link. It also lands exactly on
	 * §20.3's own promise that she opens the door first in the next day.
	 *
	 * Same discipline as the five-capture note: no interaction, no outline, no
	 * inspect panel, no caption, no objective. You find it by looking down, and
	 * you read it by walking over and looking at it in the world.
	 */
	NoteUnderDoor
};

/**
 * §20.3 좌절 방지 안전망 — the two automatic ones.
 *
 * 1. 관찰 재료 증가: two consecutive resets without learning anything new fire
 *    one environmental hint.
 * 2. 세계의 90초 반응: ninety seconds without a new source and the world moves
 *    on its own — the pipes cry, or he goes and puts his ear to the wall.
 *
 * The third net is 황순금 in the daytime, and she is already in the world.
 *
 * **막힌 플레이어에게 주는 것은 답이 아니라 볼 곳이다.** Nothing here says what
 * to do. Nothing marks a target, prints a hint or unlocks a step. What the nets
 * add is observation material: one more thing happening in the building that the
 * player can choose to walk toward. "쉬워지는" 게 아니라 "관찰 재료가 늘어나는"
 * 방향으로만 돕는다 (§7 난이도 보정).
 *
 * Progress is measured in sources filed, not ground covered. A player who has
 * explored two floors and learned nothing is stuck; one who stood still and
 * found a date on a page is not.
 */
UCLASS()
class INDIEGAME_API AIGMissingFloorMercyDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGMissingFloorMercyDirector();

	virtual void Tick(float DeltaSeconds) override;

	/** The stage hands over what the nets need to move. */
	void Configure(
		AIGListenerEntity* InEntity,
		AIGMissingFloorNightThreeDirector* InNightThree);

	/** Called on every capture reset by the night-loop director. */
	void NotifyCaptureReset();

	/** Called when the hour opens and closes; the clock only runs inside it. */
	void SetHourActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Mercy")
	int32 GetResponseCount() const { return ResponseCount; }

	UFUNCTION(BlueprintPure, Category = "Mercy")
	int32 GetResetHintCount() const { return ResetHintCount; }

	UFUNCTION(BlueprintPure, Category = "Mercy")
	EIGMercyResponse GetLastResponse() const { return LastResponse; }

	UFUNCTION(BlueprintPure, Category = "Mercy")
	float GetSecondsWithoutNewSource() const { return StuckSeconds; }

	UFUNCTION(BlueprintPure, Category = "Mercy")
	bool IsNoteDelivered() const { return bNoteDelivered; }

	UFUNCTION(BlueprintPure, Category = "Mercy")
	bool IsNoteSliding() const { return bNoteSliding; }

	/** Where the note is right now. Contract and diagnostics. */
	FVector GetNoteLocation() const;

	/** Harness hook: runs one net immediately without waiting out the clock. */
	bool ForceWorldResponseForTesting();

	/** §20.3: 새 출처 없이 90초. */
	static constexpr float StuckResponseSeconds = 90.0f;

	/** §20.3-1: 2회 연속 리셋. */
	static constexpr int32 ResetsForEnvironmentHint = 2;

private:
	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	/** Fires one response and rotates which one comes next. Returns what ran. */
	EIGMercyResponse FireWorldResponse();
	bool TryEarToWall();
	bool TryPipeCry();
	bool TryNoteUnderDoor();
	bool InitializeNote();
	void UpdateNoteSlide(float DeltaSeconds);

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGListenerEntity> Entity;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGMissingFloorNightThreeDirector> NightThree;

	/** The folded note that comes out from under 401. Built hidden, once. */
	UPROPERTY(Transient)
	TObjectPtr<class UStaticMeshComponent> Note;

	float StuckSeconds = 0.0f;
	float NoteSlideSeconds = 0.0f;
	bool bNoteDelivered = false;
	bool bNoteSliding = false;
	int32 LastSourceCount = -1;
	int32 ResetsSinceNewSource = 0;
	int32 ResponseCount = 0;
	int32 ResetHintCount = 0;
	EIGMercyResponse LastResponse = EIGMercyResponse::None;
	bool bHourActive = false;
};
