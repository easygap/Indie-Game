#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Narrative/IGMissingFloorNarrativeTypes.h"
#include "IGMissingFloorPuzzleOneDirector.generated.h"

class AIGMissingFloorEvidence;
class AIGPrologueWorldScene;
class AIGReadableNote;
class UAudioComponent;
class UIGMissingFloorNarrativeSubsystem;

DECLARE_MULTICAST_DELEGATE(FIGPuzzleOneSolvedSignature);

/**
 * P1 「다섯 번째 바늘」. 공용 조명을 끄고 무명 회로의 전원을 바꾸며
 * 계량기의 멈춤과 회전을 비교한다. 검침표까지 대조하면 숨은 회로를
 * 확인한다. 전원을 올린 상태로 조사를 끝내야 위층의 안정기 소리가 이어진다.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGMissingFloorPuzzleOneDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGMissingFloorPuzzleOneDirector();
	AIGMissingFloorEvidence* GetMeterAction() const { return MeterDialEvidence; }
	AIGMissingFloorEvidence* GetBreakerAction() const { return BreakerAction; }
	AIGMissingFloorEvidence* GetCommonLightAction() const { return CommonLightAction; }

	/** Spawns the puzzle's interactables against an already-built lobby. */
	bool Configure(AIGPrologueWorldScene* InScene);

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsBallastHumAudible() const { return bBallastHumAudible; }

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool HasBreakerBeenThrown() const { return bBreakerThrown; }

	/**
	 * 낮에는 계전기가 곧장 되돌려서 아무 일도 없다(§7 P1). 위가 켜지는
	 * 것은 그 시간에만이다.
	 */
	void SetHourActive(bool bActive);

	/** 계량기·공용 스위치·무명 회로·검침표가 모두 배치됐는지 확인한다. */
	bool ValidateFixtures() const;

	/**
	 * 회로가 올라가고 T1까지 맞물렸을 때 한 번. 불만 켜고 계량기를 안 본
	 * 밤은 끝나지 않는다 — 퍼즐은 두 기록의 교차지 버튼이 아니다(§7).
	 */
	FIGPuzzleOneSolvedSignature OnSolved;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateMeterMotion();
	void AdvanceMeterDisc();
	FTimerHandle MeterRotationTimer;
	void HandleMeterExamined(AIGMissingFloorEvidence* Evidence);
	void HandleBreakerThrown(AIGMissingFloorEvidence* Evidence);
	void HandleCommonLighting(AIGMissingFloorEvidence* Evidence);

	/** Dynamic delegate target, so it has to be reflected. */
	UFUNCTION()
	void HandleSheetRead(AIGReadableNote* Note, bool bOpened);
	void CreateBallastHum();
	void HandleTruthConfirmed(EIGMissingFloorTruth Truth);
	void AnnounceSolvedIfReady();
	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> MeterDialEvidence;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> BreakerAction;
	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> CommonLightAction;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> ReadingSheet;

	/** The hum from above the ceiling. Created silent, started on the throw. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BallastHum;

	bool bBreakerThrown = false;
	bool bCommonLightsEnabled = true;
	bool bBallastHumAudible = false;
	bool bHourActive = false;
	bool bSolvedAnnounced = false;
	FDelegateHandle TruthHandle;
};
