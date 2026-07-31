#include "Sequence/IGMorningRoutineDirector.h"

#include "Audio/IGChapterOnePresenceAudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IndieGame.h"
#include "Interaction/IGZoneTrigger.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryHelpers.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Save/IGSaveSubsystem.h"
#include "TimerManager.h"

namespace IGMorningDirector
{
	// Streetlight failure: intensity multipliers stepped on a fixed cadence.
	constexpr float FlickerStepSeconds = 0.11f;
	constexpr float FlickerPattern[] = {0.15f, 1.0f, 0.05f, 0.8f, 0.3f, 0.9f, 0.0f, 0.0f, 0.35f, 0.0f};
	constexpr int32 FlickerStepCount = UE_ARRAY_COUNT(FlickerPattern);
}

AIGMorningRoutineDirector::AIGMorningRoutineDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	PresenceAudio = CreateDefaultSubobject<UIGChapterOnePresenceAudioComponent>(
		TEXT("ChapterOnePresenceAudio"));
}

void AIGMorningRoutineDirector::SetSceneReferences(
	UPointLightComponent* InFlickerLight,
	AIGZoneTrigger* InApartmentExitZone)
{
	FlickerLight = InFlickerLight;
	FlickerBaseIntensity = FlickerLight ? FlickerLight->Intensity : 0.0f;

	if (ApartmentExitZone)
	{
		ApartmentExitZone->OnZoneTriggered.RemoveDynamic(
			this,
			&ThisClass::HandleApartmentExitZoneTriggered);
	}
	ApartmentExitZone = InApartmentExitZone;
	if (ApartmentExitZone)
	{
		ApartmentExitZone->OnZoneTriggered.AddUniqueDynamic(
			this,
			&ThisClass::HandleApartmentExitZoneTriggered);
	}
}

void AIGMorningRoutineDirector::BeginPlay()
{
	Super::BeginPlay();
	ResolveDefaultTags();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->OnStoryStateTagChanged.AddUniqueDynamic(
				this,
				&ThisClass::HandleStoryStateChanged);
		}
	}

	// Legacy saves may restore their story tags before this actor exists.
	// Mirror those completed beats into the REBIRTH snapshot once, without
	// replaying any presentation or checkpoint side effects.
	BootstrapRebirthStateFromLegacy();

	// Initial evaluation restores a checkpointed phase without side effects.
	RefreshPhase(false);
}

void AIGMorningRoutineDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PresenceAudio)
	{
		// EnterChapterTwo destroys this director at the black-frame transition.
		// Silence the lived-in CH01 layer before any CH02 sound can begin.
		PresenceAudio->SetPresenceEnabled(false);
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->OnStoryStateTagChanged.RemoveDynamic(
				this,
				&ThisClass::HandleStoryStateChanged);
		}
	}

	if (ApartmentExitZone)
	{
		ApartmentExitZone->OnZoneTriggered.RemoveDynamic(
			this,
			&ThisClass::HandleApartmentExitZoneTriggered);
	}

	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

void AIGMorningRoutineDirector::ResolveDefaultTags()
{
	auto Resolve = [](FGameplayTag& Tag, const TCHAR* TagName)
	{
		if (!Tag.IsValid())
		{
			Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		}
	};

	Resolve(ChapterId, TEXT("Chapter.CH01"));
	Resolve(StandingStateTag, TEXT("State.CH01.Wake.Standing"));
	Resolve(FridgeCheckedStateTag, TEXT("State.CH01.Morning.FridgeChecked"));
	Resolve(HasWalletStateTag, TEXT("State.CH01.Morning.HasWallet"));
	Resolve(LeftApartmentStateTag, TEXT("State.CH01.Morning.LeftApartment"));
	Resolve(LeftHomeStateTag, TEXT("State.CH01.Morning.LeftHome"));
	Resolve(EnteredStoreStateTag, TEXT("State.CH01.Morning.EnteredStore"));
	Resolve(HasWaterStateTag, TEXT("State.CH01.Morning.HasWater"));
	Resolve(WaterPurchasedStateTag, TEXT("State.CH01.Morning.WaterPurchased"));
	Resolve(FridgeCheckpointTag, TEXT("Checkpoint.CH01.FridgeChecked"));
	Resolve(AlleyCheckpointTag, TEXT("Checkpoint.CH01.AlleyEntrance"));
	Resolve(StoreCheckpointTag, TEXT("Checkpoint.CH01.StoreEntrance"));
	Resolve(PurchasedCheckpointTag, TEXT("Checkpoint.CH01.WaterPurchased"));
}

EIGMorningPhase AIGMorningRoutineDirector::EvaluatePhaseFromState() const
{
	if (!IGStory::HasState(this, StandingStateTag))
	{
		return EIGMorningPhase::Inactive;
	}
	if (IGStory::HasState(this, WaterPurchasedStateTag))
	{
		return EIGMorningPhase::Complete;
	}
	if (IGStory::HasState(this, HasWaterStateTag))
	{
		return EIGMorningPhase::PayAtCounter;
	}
	if (IGStory::HasState(this, EnteredStoreStateTag))
	{
		return EIGMorningPhase::FindWater;
	}
	if (IGStory::HasState(this, LeftApartmentStateTag)
		|| IGStory::HasState(this, LeftHomeStateTag)
		|| IGStory::HasState(this, FridgeCheckedStateTag))
	{
		return EIGMorningPhase::GoToStore;
	}
	return EIGMorningPhase::CheckFridge;
}

void AIGMorningRoutineDirector::RefreshPhase(const bool bLiveTransition)
{
	const EIGMorningPhase NewPhase = EvaluatePhaseFromState();

	// The routine only moves forward; stale tag removal never regresses it.
	if (NewPhase <= Phase)
	{
		return;
	}

	const EIGMorningPhase PreviousPhase = Phase;
	Phase = NewPhase;
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("Morning phase changed: %s -> %s"),
		*UEnum::GetValueAsString(PreviousPhase),
		*UEnum::GetValueAsString(NewPhase));

	if (PreviousPhase == EIGMorningPhase::Inactive)
	{
		EnablePlayerCameraMotion();
		if (bLiveTransition)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGMorning", "WakeThought", "입이 바짝 말랐다. 물부터 마셔야겠다."),
				4.0f);
		}
	}

	OnPhaseChanged.Broadcast(PreviousPhase, NewPhase);
}

void AIGMorningRoutineDirector::BootstrapRebirthStateFromLegacy()
{
	for (const FGameplayTag& StateTag :
		{FridgeCheckedStateTag, EnteredStoreStateTag, WaterPurchasedStateTag})
	{
		if (IGStory::HasState(this, StateTag))
		{
			BridgeRebirthState(StateTag);
		}
	}

	// State.CH01.Morning.LeftHome was emitted at the ground-floor entrance in
	// legacy builds. It is accepted only once as migration input; all visual
	// restoration below reads the canonical EquippedOutfitChapters array.
	RestoreOutfitFromCanonical(true);
}

void AIGMorningRoutineDirector::BridgeRebirthState(
	const FGameplayTag& StateTag)
{
	UGameInstance* GameInstance = GetGameInstance();
	UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!RebirthState)
	{
		return;
	}

	auto Truth = [](const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), false);
	};

	if (StateTag.MatchesTagExact(FridgeCheckedStateTag))
	{
		RebirthState->RegisterTruthSource(
			Truth(TEXT("Truth.NeedWater")),
			FName(TEXT("CH01.EmptyFridge")));
		return;
	}

	if (StateTag.MatchesTagExact(EnteredStoreStateTag))
	{
		RebirthState->MarkLocationVisited(FName(TEXT("CH01.Store")));
		return;
	}

	if (!StateTag.MatchesTagExact(WaterPurchasedStateTag))
	{
		return;
	}

	// Checkout commits only facts that are actually known at the counter.
	// Cat water, waiting, and bottle closure are authored later on the return
	// walk; pre-filling them here made every live choice a decorative override.
	FIGRebirthChoiceState Choices = RebirthState->GetChoices();
	if (Choices.PurchaseProfile == EIGRebirthPurchaseProfile::Unset)
	{
		Choices.PurchaseProfile =
			EIGRebirthPurchaseProfile::ProfileA500MlX2;
	}
	if (Choices.PaymentMethod == EIGRebirthPaymentMethod::Unset)
	{
		Choices.PaymentMethod = IGStory::HasState(this, HasWalletStateTag)
			? EIGRebirthPaymentMethod::WalletCard
			: EIGRebirthPaymentMethod::PocketCard;
	}
	// Choices and truth are committed in the same story-event callback before
	// the checkpoint request below snapshots either subsystem.
	RebirthState->SetChoices(Choices);
	RebirthState->RegisterTruthSource(
		Truth(TEXT("Truth.Purchase0431")),
		FName(TEXT("CH01.PurchaseCommitted0431")));
}

void AIGMorningRoutineDirector::RestoreOutfitFromCanonical(
	const bool bAllowLegacyMigration)
{
	UGameInstance* GameInstance = GetGameInstance();
	UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!RebirthState)
	{
		return;
	}

	const FName ChapterOutfitId(TEXT("CH01"));
	bool bCanonicalEquipped =
		RebirthState->BuildSnapshot().EquippedOutfitChapters.Contains(
			ChapterOutfitId);
	if (!bCanonicalEquipped
		&& bAllowLegacyMigration
		&& (IGStory::HasState(this, LeftApartmentStateTag)
			|| IGStory::HasState(this, LeftHomeStateTag)))
	{
		RebirthState->MarkOutfitEquipped(ChapterOutfitId);
		bCanonicalEquipped =
			RebirthState->BuildSnapshot().EquippedOutfitChapters.Contains(
				ChapterOutfitId);
	}

	const APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr)
	{
		// Restore is immediate and silent; it never replays the first-exit
		// sleeve presentation.
		Player->SetRebirthOutfitEquipped(bCanonicalEquipped);
	}
}

void AIGMorningRoutineDirector::CommitOutfitAtFirstExit()
{
	UGameInstance* GameInstance = GetGameInstance();
	UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!RebirthState)
	{
		return;
	}

	const FName ChapterOutfitId(TEXT("CH01"));
	const bool bFirstPresentation =
		RebirthState->MarkOutfitEquipped(ChapterOutfitId);
	const bool bCanonicalEquipped =
		RebirthState->BuildSnapshot().EquippedOutfitChapters.Contains(
			ChapterOutfitId);

	const APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr)
	{
		Player->SetRebirthOutfitEquipped(
			bCanonicalEquipped,
			bFirstPresentation);
	}
}

bool AIGMorningRoutineDirector::RunRebirthEndToEndFirstExit()
{
	UIGRebirthNarrativeSubsystem* RebirthState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	if (!RebirthState)
	{
		return false;
	}

	CommitOutfitAtFirstExit();
	CommitOutfitAtFirstExit();
	const FIGRebirthNarrativeSnapshot Snapshot =
		RebirthState->BuildSnapshot();
	int32 ChapterOneRecords = 0;
	for (const FName ChapterId : Snapshot.EquippedOutfitChapters)
	{
		ChapterOneRecords += ChapterId == FName(TEXT("CH01")) ? 1 : 0;
	}
	return ChapterOneRecords == 1;
}

void AIGMorningRoutineDirector::HandleApartmentExitZoneTriggered(
	AIGZoneTrigger* Zone)
{
	if (Zone == ApartmentExitZone)
	{
		CommitOutfitAtFirstExit();
	}
}

void AIGMorningRoutineDirector::HandleStoryStateChanged(const FGameplayTag StateTag, const bool bAdded)
{
	if (!bAdded)
	{
		return;
	}

	HandleLiveStateSideEffects(StateTag);
	RefreshPhase(true);
}

void AIGMorningRoutineDirector::HandleLiveStateSideEffects(const FGameplayTag& StateTag)
{
	// The REBIRTH state must be complete before the legacy checkpoint captures
	// the same event.
	BridgeRebirthState(StateTag);

	if (StateTag.MatchesTagExact(FridgeCheckedStateTag))
	{
		RequestCheckpointAutosave(FridgeCheckpointTag);
	}
	else if (StateTag.MatchesTagExact(LeftHomeStateTag))
	{
		RequestCheckpointAutosave(AlleyCheckpointTag);
	}
	else if (StateTag.MatchesTagExact(EnteredStoreStateTag))
	{
		RequestCheckpointAutosave(StoreCheckpointTag);
	}
	else if (StateTag.MatchesTagExact(WaterPurchasedStateTag))
	{
		RequestCheckpointAutosave(PurchasedCheckpointTag);
	}
}

void AIGMorningRoutineDirector::RequestCheckpointAutosave(const FGameplayTag& CheckpointTag)
{
	if (!bAutosaveAtCheckpoints || !CheckpointTag.IsValid())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UIGSaveSubsystem>())
		{
			const FName MapPackageName =
				GetWorld() ? GetWorld()->GetOutermost()->GetFName() : NAME_None;
			SaveSubsystem->RequestAutosave(ChapterId, MapPackageName, CheckpointTag);
		}
	}
}

void AIGMorningRoutineDirector::EnablePlayerCameraMotion()
{
	const APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (AIGPlayerCharacter* Character = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr)
	{
		Character->SetCameraMotionEnabled(true);
	}
}

void AIGMorningRoutineDirector::TriggerAlleyLightFailure()
{
	if (bFlickerConsumed || !FlickerLight)
	{
		return;
	}

	bFlickerConsumed = true;
	FlickerStepIndex = 0;
	GetWorldTimerManager().SetTimer(
		FlickerTimerHandle,
		this,
		&ThisClass::HandleFlickerStep,
		IGMorningDirector::FlickerStepSeconds,
		true);
}

void AIGMorningRoutineDirector::HandleFlickerStep()
{
	if (!FlickerLight || FlickerStepIndex >= IGMorningDirector::FlickerStepCount)
	{
		GetWorldTimerManager().ClearTimer(FlickerTimerHandle);
		if (FlickerLight)
		{
			// The lamp stays dead; the darkness it leaves behind is the beat.
			FlickerLight->SetIntensity(0.0f);
		}
		return;
	}

	FlickerLight->SetIntensity(
		FlickerBaseIntensity * IGMorningDirector::FlickerPattern[FlickerStepIndex]);
	++FlickerStepIndex;
}

FText AIGMorningRoutineDirector::GetObjectiveText() const
{
	switch (Phase)
	{
	case EIGMorningPhase::CheckFridge:
		return NSLOCTEXT("IGMorning", "ObjFridge", "목이 마르다 — 마실 물을 해결하자");
	case EIGMorningPhase::TakeWallet:
		return NSLOCTEXT(
			"IGMorning",
			"ObjWallet",
			"필요하면 책상 위 지갑을 챙기고 나가자");
	case EIGMorningPhase::GoToStore:
		return NSLOCTEXT("IGMorning", "ObjStore", "골목 끝 편의점에서 물을 사 오자");
	case EIGMorningPhase::FindWater:
		return NSLOCTEXT("IGMorning", "ObjWater", "음료 냉장고에서 생수를 찾자");
	case EIGMorningPhase::PayAtCounter:
		return NSLOCTEXT("IGMorning", "ObjPay", "카운터에서 계산하자");
	case EIGMorningPhase::Complete:
		return NSLOCTEXT("IGMorning", "ObjDone", "물을 샀다. …이제 집으로 돌아가자");
	case EIGMorningPhase::Inactive:
	default:
		return FText::GetEmpty();
	}
}

FString AIGMorningRoutineDirector::GetObjectiveTextAscii() const
{
	switch (Phase)
	{
	case EIGMorningPhase::CheckFridge:
		return TEXT("Thirsty - find a way to get drinking water");
	case EIGMorningPhase::TakeWallet:
		return TEXT("Optionally take the wallet, then leave");
	case EIGMorningPhase::GoToStore:
		return TEXT("Buy water at the corner store");
	case EIGMorningPhase::FindWater:
		return TEXT("Find bottled water in the cooler");
	case EIGMorningPhase::PayAtCounter:
		return TEXT("Pay at the counter");
	case EIGMorningPhase::Complete:
		return TEXT("Bought water. ...Head back home");
	case EIGMorningPhase::Inactive:
	default:
		return FString();
	}
}

float AIGMorningRoutineDirector::GetObjectiveProgress() const
{
	switch (Phase)
	{
	case EIGMorningPhase::CheckFridge:
		return 0.0f;
	case EIGMorningPhase::TakeWallet:
		return 0.2f;
	case EIGMorningPhase::GoToStore:
		return 0.4f;
	case EIGMorningPhase::FindWater:
		return 0.6f;
	case EIGMorningPhase::PayAtCounter:
		return 0.8f;
	case EIGMorningPhase::Complete:
		return 1.0f;
	case EIGMorningPhase::Inactive:
	default:
		return 0.0f;
	}
}
