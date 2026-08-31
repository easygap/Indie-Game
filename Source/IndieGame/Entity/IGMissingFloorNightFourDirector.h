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

	/**
	 * The §20.4 substitute route to ending C: dawn arrived on night four with
	 * the wall still closed. 듣기만 하는 밤 has no captures, so the tier never
	 * reaches three and the ordinary route above can never fire — without this
	 * the accessibility mode would be missing an ending, and §20.5 requires all
	 * three to stay reachable in every mode.
	 */
	bool ResolveDawnFailureEnding();

	/** Shared tail of both ending-C routes: capture record, staging, cards. */
	bool CommitFailureEnding(bool bRecordCapture);

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
	/** 선택이 열리는 프레임에 한 번. 양쪽을 다 보는 값을 낮춘다(§22.4). */
	/**
	 * §22.3. 목한수 앞에서 유담이 무엇을 말할 수 있는가는 그 회차에 무엇을
	 * 봤느냐로 정해진다. 아무것도 못 봤으면 아무 말도 하지 않는다 — 없는
	 * 말을 쥐여 주지 않는 것이 이 절의 규칙이다.
	 *
	 * 최대 세 줄이다. 그 이상은 목격이 아니라 목록 낭독이 된다. 앞은 그를
	 * 겨냥하는 서류이고 **마지막 줄은 가능하면 사람**이다 — 증거로 시작해
	 * 사람으로 끝나야 이 장면이 고발이 아니라 애도가 된다.
	 */
	void BuildConfrontationReplyLines(TArray<FText>& OutLines) const;

	void RequestEndingChoiceAutosave();
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
	/** 엔딩 C 암전과 이동 잠금을 걷는다. 정상 복귀가 끊긴 자리에서만 부른다. */
	void AbortFailureBlackout(const TCHAR* Reason);

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
