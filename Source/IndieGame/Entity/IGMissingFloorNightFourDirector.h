#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGMissingFloorNightFourDirector.generated.h"

class AIGMissingFloorEvidence;
class AIGPrologueWorldScene;
class AIGListenerEntity;
class APawn;
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
	virtual void Tick(float DeltaSeconds) override;

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
	bool IsFinalRevealPlaying() const { return bFinalRevealActive; }
	bool IsFinalConfrontationComplete() const
	{
		return bFinalConfrontationComplete;
	}
	int32 GetFinalRevealStage() const { return FinalRevealStage; }

	/**
	 * Places the authored finale meshes for the repository's screenshot tour.
	 * This changes no narrative, puzzle, save or AI state.
	 */
	void SetFinaleCapturePreview(bool bShowCavity, bool bShowMok);

	/** Tier-3 capture during night 4 uses the same common-discovery state. */
	bool ResolveFailureEnding();
	/** 엔딩 C 동안 상호작용을 소비하고, 카드가 완전히 열린 뒤에만 재시도한다. */
	bool RequestFailureRetry();
	bool IsFailureEndingActive() const { return bFailureEndingActive; }
	bool IsFailureRetryEnabled() const { return bFailureRetryEnabled; }
	/** 무화면 출시 계약에서 엔딩 카드 타이머만 완료 상태로 진행한다. */
	bool CompleteFailurePresentationForProbe();

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
	void HandleNightFourCapture(APawn* Player);
	void BeginFailureListing();
	void EnableFailureRetry();
	void ResetAfterFailureEnding();
	void ActivateControl(FName ControlId, AIGMissingFloorEvidence* Evidence);
	void StartWaterMaskIfReady();
	bool BuildFinaleVisuals();
	void SetCavityRevealVisible(bool bVisible);
	void SetMokVisible(bool bVisible);
	void UpdateFinaleDetailLayers();
	void BeginCavityReveal();
	void UpdateCavityReveal(float DeltaSeconds);
	void AdvanceCavityReveal();
	void BeginSilenceBeat();
	void PlayDistantReply();
	void PresentMokHansoo();
	void BeginEntityPass();
	void TriggerBlackout();
	void CompleteConfrontation();
	void ResetFinaleTimers();
	void UpdateMokRetreat(float DeltaSeconds);
	void UpdateEndingHammer(float DeltaSeconds);
	void RefreshPresentation();
	void FinishEnding(FName EndingId);
	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGListenerEntity> Listener;
	TWeakObjectPtr<class AIGPlayerCharacter> FailurePlayer;

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

	/** Four separate material groups, all authored 3D and sharing one origin. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CavityRevealVisuals;

	/** Workwear, head/hands and gypsum board; never a near-field sprite. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> MokVisuals;

	/** Lit masked detail over the continuous cavity shadow/silhouette meshes. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CavityDetailCard;

	/** Face/jacket only; the 95 cm board, lower body and shadow remain 3D. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MokDetailCard;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> EndingHammerVisual;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> EndingPhoneVisual;

	bool bHourCurrentlyActive = false;
	bool bWaterMaskActive = false;
	bool bHydraulicAlarmTriggered = false;
	bool bResolvedBroadcast = false;
	bool bFinalRevealActive = false;
	bool bFinalConfrontationComplete = false;
	bool bMokRetreatActive = false;
	bool bEndingHammerMoving = false;
	bool bCavityPresentationVisible = false;
	bool bMokPresentationVisible = false;
	bool bFailureEndingActive = false;
	bool bFailureRetryEnabled = false;
	int32 FinalRevealStage = INDEX_NONE;
	float RevealAttentionSeconds = 0.0f;
	float RevealStageElapsedSeconds = 0.0f;
	float MokRetreatSeconds = 0.0f;
	float EndingHammerSeconds = 0.0f;
	int32 WaterMaskHumHandle = INDEX_NONE;
	FTimerHandle DistantReplyTimer;
	FTimerHandle MokRevealTimer;
	FTimerHandle EntityPassTimer;
	FTimerHandle BlackoutTimer;
	FTimerHandle BlackoutRestoreTimer;
	FTimerHandle FailureListingTimer;
	FTimerHandle FailureRetryTimer;
};
