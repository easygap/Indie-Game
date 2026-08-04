#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Interaction/IGInteractableActor.h"
#include "IGChapterTwoHumanGateDirector.generated.h"

class AIGChapterTwoHumanGateDirector;
class AIGInspectable;
class AIGReadableNote;
class AIGZoneTrigger;
class UAudioComponent;
class UBoxComponent;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class EIGChapterTwoHumanCheckAction : uint8
{
	None,
	PhoneMother,
	PhoneEmergency112,
	PhonePatrolManager,
	PhoneApprovalRecord,
	Doorbell401,
	Doorbell402
};

/**
 * Small, traceable CH02-only interaction surface.
 *
 * Phone choices reuse the persistent desk-phone art and expose only an
 * invisible screen row. Doorbells keep a tiny proxy mesh so the functional
 * spike remains playable before the final 401/402 art pass.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGChapterTwoHumanGateAction final
	: public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGChapterTwoHumanGateAction();

	void Configure(
		AIGChapterTwoHumanGateDirector* InDirector,
		EIGChapterTwoHumanCheckAction InAction,
		UStaticMesh* CubeMesh,
		UMaterialInterface* Material,
		const FVector& SizeCentimeters,
		const FText& Prompt,
		bool bShowVisual);

	void SetAvailable(bool bAvailable);

	virtual void CompleteInteraction_Implementation(
		const FIGInteractionContext& Context) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "CH02 Human Gate")
	TObjectPtr<UBoxComponent> InteractionSurface;

	UPROPERTY(VisibleAnywhere, Category = "CH02 Human Gate")
	TObjectPtr<UStaticMeshComponent> PresentationMesh;

	UPROPERTY(Transient)
	TObjectPtr<AIGChapterTwoHumanGateDirector> Director;

	EIGChapterTwoHumanCheckAction Action =
		EIGChapterTwoHumanCheckAction::None;
};

/**
 * Optional reality-check gate after CH02's first strong impossibility.
 *
 * It never blocks the lift, stairs, lobby or store route. It only exposes
 * three ways to look for another person, records their one-shot physical
 * responses, and converges both a help attempt and a direct lobby route on
 * the same human motive: find someone in the lit convenience store.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGChapterTwoHumanGateDirector final : public AActor
{
	GENERATED_BODY()

public:
	AIGChapterTwoHumanGateDirector();

	void Configure(
		AIGInspectable* InPhoneInspectable,
		UStaticMesh* CubeMesh,
		UMaterialInterface* DarkMaterial,
		UMaterialInterface* PanelMaterial,
		UMaterialInterface* LitMaterial);

	void HandleAction(
		EIGChapterTwoHumanCheckAction Action,
		AIGChapterTwoHumanGateAction* Source);

	/**
	 * Exercises the physical handlers in the unattended CH01->CH03 route.
	 * The lobby is committed before any help action to prove that all three
	 * interactions remain optional.
	 */
	bool RunRebirthEndToEndValidation();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	AIGChapterTwoHumanGateAction* SpawnAction(
		EIGChapterTwoHumanCheckAction Action,
		const FTransform& WorldTransform,
		const FVector& SizeCentimeters,
		UStaticMesh* CubeMesh,
		UMaterialInterface* Material,
		const FText& Prompt,
		bool bShowVisual);
	AIGZoneTrigger* SpawnLobbyZone();
	void ResolveTags();
	void ReconcileState(bool bPresentUnlockThought);
	void UpdateActionAvailability();
	void EnsureHumanChecksUnlocked(bool bPresentThought);
	void CommitStoreMotive(bool bQueueThought);
	void PushStoreMotiveThought();
	void PlayAlarmFirstToneOnce();
	void Start401Radio();
	void Stop401Radio();
	void Set402IndicatorLit(bool bLit);
	void RequestCheckpointAutosave() const;
	bool HasState(const FGameplayTag& Tag) const;
	bool AddState(const FGameplayTag& Tag) const;
	FVector ToWorld(const FVector& LocalLocation) const;

	UFUNCTION()
	void HandleStoryStateChanged(FGameplayTag StateTag, bool bAdded);

	UFUNCTION()
	void HandleLobbyEntered(AIGZoneTrigger* Zone);

	UPROPERTY(VisibleAnywhere, Category = "CH02 Human Gate")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "CH02 Human Gate")
	TObjectPtr<UStaticMeshComponent> NightShiftStickerProxy;

	UPROPERTY(VisibleAnywhere, Category = "CH02 Human Gate")
	TObjectPtr<UStaticMeshComponent> Door402IndicatorLens;

	UPROPERTY(VisibleAnywhere, Category = "CH02 Human Gate")
	TObjectPtr<UPointLightComponent> Door402IndicatorLight;

	UPROPERTY(Transient)
	TObjectPtr<AIGInspectable> PhoneInspectable;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AIGChapterTwoHumanGateAction>> PhoneActions;

	UPROPERTY(Transient)
	TObjectPtr<AIGChapterTwoHumanGateAction> PhoneApprovalAction;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> PhoneApprovalRecord;

	UPROPERTY(Transient)
	TObjectPtr<AIGChapterTwoHumanGateAction> Doorbell401Action;

	UPROPERTY(Transient)
	TObjectPtr<AIGChapterTwoHumanGateAction> Doorbell402Action;

	UPROPERTY(Transient)
	TObjectPtr<AIGZoneTrigger> LobbyZone;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Radio401Component;

	FGameplayTag MirrorSightImpossibleTag;
	FGameplayTag MirrorEntryImpossibleTag;
	FGameplayTag LiftImpossibleTag;
	FGameplayTag EnteredStoreTag;
	FGameplayTag HumanChecksUnlockedTag;
	FGameplayTag PhoneAttemptedTag;
	FGameplayTag PhoneMotherTag;
	FGameplayTag PhoneEmergency112Tag;
	FGameplayTag PhonePatrolManagerTag;
	FGameplayTag PhoneApprovalReadTag;
	FGameplayTag AlarmFirstTonePlayedTag;
	FGameplayTag Doorbell401AttemptedTag;
	FGameplayTag Doorbell402AttemptedTag;
	FGameplayTag HelpAttemptedTag;
	FGameplayTag LobbyWitnessedTag;
	FGameplayTag StoreMotiveTag;
	FGameplayTag ChapterIdTag;
	FGameplayTag WokeCheckpointTag;
	FGameplayTag CorridorCheckpointTag;
	FGameplayTag StoreCheckpointTag;

	FTimerHandle StoreMotiveThoughtHandle;
	int32 AlarmFirstTonePlayCount = 0;
	bool bPhoneInteractionWasEnabled = true;
	bool bPhoneCollisionWasEnabled = true;
	bool bPhoneStateCaptured = false;
	bool bConfigured = false;
};
