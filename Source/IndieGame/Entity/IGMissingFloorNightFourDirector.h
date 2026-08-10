#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGMissingFloorNightFourDirector.generated.h"

class AIGMissingFloorEvidence;
class AIGPrologueWorldScene;
class UAudioComponent;
class UIGMissingFloorNarrativeSubsystem;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE(FIGNightFourResolvedSignature);

/**
 * Night 4 vertical slice for 없는 층 (story bible v2.2 §7 P5, §8 night 4).
 *
 * The hydraulic mask is one physically named cleaning circuit, not four
 * arbitrary valves: roof cleaning drain -> float-valve bypass -> ground-floor
 * transfer pump. First activation order is persisted, while every order can
 * still reach the same running state. Five separate hammer interactions then
 * remove the real middle gypsum panel. The two final targets change only the
 * mourning choice; discovery and the second report remain common state.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGMissingFloorNightFourDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGMissingFloorNightFourDirector();

	bool Configure(AIGPrologueWorldScene* InScene);
	void SetHourActive(bool bHourActive);
	bool ValidateFixtures() const;

	AIGMissingFloorEvidence* GetEvictionNotice() const { return EvictionNotice; }
	AIGMissingFloorEvidence* GetCleaningDrain() const { return CleaningDrain; }
	AIGMissingFloorEvidence* GetFloatBypass() const { return FloatBypass; }
	AIGMissingFloorEvidence* GetTransferPump() const { return TransferPump; }
	AIGMissingFloorEvidence* GetWallBreakTarget() const { return WallBreakTarget; }
	AIGMissingFloorEvidence* GetEndingATarget() const { return EndingATarget; }
	AIGMissingFloorEvidence* GetEndingBTarget() const { return EndingBTarget; }

	bool WasHydraulicAlarmTriggered() const { return bHydraulicAlarmTriggered; }
	bool IsWaterMaskPlaying() const;

	/** Tier-3 capture during night 4 uses the same common-discovery state. */
	bool ResolveFailureEnding();

	FIGNightFourResolvedSignature OnResolved;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleEvictionNotice(AIGMissingFloorEvidence* Evidence);
	void HandleCleaningDrain(AIGMissingFloorEvidence* Evidence);
	void HandleFloatBypass(AIGMissingFloorEvidence* Evidence);
	void HandleTransferPump(AIGMissingFloorEvidence* Evidence);
	void HandleWallStrike(AIGMissingFloorEvidence* Evidence);
	void HandleEndingA(AIGMissingFloorEvidence* Evidence);
	void HandleEndingB(AIGMissingFloorEvidence* Evidence);
	void ActivateControl(FName ControlId, AIGMissingFloorEvidence* Evidence);
	void StartWaterMaskIfReady();
	void RefreshPresentation();
	void FinishEnding(FName EndingId);
	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> EvictionNotice;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> CleaningDrain;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> FloatBypass;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> TransferPump;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> WallBreakTarget;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> EndingATarget;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> EndingBTarget;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WaterMaskBed;

	/** Non-interactive plumbing and pump parts that keep P5 physically legible. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> EquipmentVisuals;

	bool bHourCurrentlyActive = false;
	bool bWaterMaskActive = false;
	bool bHydraulicAlarmTriggered = false;
	bool bResolvedBroadcast = false;
	int32 WaterMaskHumHandle = INDEX_NONE;
};
