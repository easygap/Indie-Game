#include "Sequence/IGMorningRoutineDirector.h"

#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IndieGame.h"
#include "Interaction/IGSlidingDoor.h"
#include "Kismet/GameplayStatics.h"
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
}

void AIGMorningRoutineDirector::SetSceneReferences(
	AIGSlidingDoor* InStoreDoor,
	UPointLightComponent* InFlickerLight,
	TArray<UPointLightComponent*>&& InStoreLights,
	UAudioComponent* InJingleComponent)
{
	StoreDoor = InStoreDoor;
	FlickerLight = InFlickerLight;
	FlickerBaseIntensity = FlickerLight ? FlickerLight->Intensity : 0.0f;
	JingleComponent = InJingleComponent;

	StoreLights.Reset();
	StoreLightBaseIntensities.Reset();
	for (UPointLightComponent* Light : InStoreLights)
	{
		if (Light)
		{
			StoreLights.Add(Light);
			StoreLightBaseIntensities.Add(Light->Intensity);
		}
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

	// Initial evaluation restores a checkpointed phase without side effects.
	RefreshPhase(false);
}

void AIGMorningRoutineDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
	if (IGStory::HasState(this, FridgeCheckedStateTag)
		&& IGStory::HasState(this, HasWalletStateTag))
	{
		return EIGMorningPhase::GoToStore;
	}
	if (IGStory::HasState(this, FridgeCheckedStateTag))
	{
		return EIGMorningPhase::TakeWallet;
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

	// Restores rebuild ongoing presentation that matches the loaded phase.
	if (!bLiveTransition
		&& Phase >= EIGMorningPhase::GoToStore
		&& Phase < EIGMorningPhase::FindWater
		&& IGStory::HasState(this, LeftHomeStateTag))
	{
		StartDrone();
	}

	OnPhaseChanged.Broadcast(PreviousPhase, NewPhase);
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
	if (StateTag.MatchesTagExact(FridgeCheckedStateTag))
	{
		RequestCheckpointAutosave(FridgeCheckpointTag);
	}
	else if (StateTag.MatchesTagExact(LeftHomeStateTag))
	{
		RequestCheckpointAutosave(AlleyCheckpointTag);
		StartDrone();
	}
	else if (StateTag.MatchesTagExact(EnteredStoreStateTag))
	{
		RequestCheckpointAutosave(StoreCheckpointTag);
		StopDrone();
	}
	else if (StateTag.MatchesTagExact(HasWaterStateTag))
	{
		if (!bWaterBeatConsumed)
		{
			bWaterBeatConsumed = true;

			// The cheerful jingle hiccups and the fluorescents dip, once.
			if (JingleComponent)
			{
				JingleComponent->SetPaused(true);
				GetWorldTimerManager().SetTimer(
					JingleTimerHandle,
					this,
					&ThisClass::HandleJingleResume,
					1.15f,
					false);
			}

			for (int32 LightIndex = 0; LightIndex < StoreLights.Num(); ++LightIndex)
			{
				if (StoreLights[LightIndex])
				{
					StoreLights[LightIndex]->SetIntensity(
						StoreLightBaseIntensities[LightIndex] * 0.22f);
				}
			}
			GetWorldTimerManager().SetTimer(
				StoreLightsTimerHandle,
				this,
				&ThisClass::HandleStoreLightsRestore,
				0.34f,
				false);
		}
	}
	else if (StateTag.MatchesTagExact(WaterPurchasedStateTag))
	{
		RequestCheckpointAutosave(PurchasedCheckpointTag);

		if (!bGhostChimeConsumed)
		{
			bGhostChimeConsumed = true;
			GetWorldTimerManager().SetTimer(
				GhostChimeTimerHandle,
				this,
				&ThisClass::HandleGhostChime,
				2.3f,
				false);
		}
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

void AIGMorningRoutineDirector::StartDrone()
{
	if (bDroneActive)
	{
		return;
	}

	if (!DroneComponent)
	{
		DroneComponent = UGameplayStatics::CreateSound2D(
			this,
			UIGToneSequenceSoundWave::CreateDreadDrone(this),
			1.0f,
			1.0f,
			0.0f,
			nullptr,
			false,
			false);
	}

	if (DroneComponent)
	{
		bDroneActive = true;
		DroneComponent->FadeIn(3.2f, 0.62f);
	}
}

void AIGMorningRoutineDirector::StopDrone()
{
	if (bDroneActive && DroneComponent)
	{
		DroneComponent->FadeOut(1.8f, 0.0f);
	}
	bDroneActive = false;
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

void AIGMorningRoutineDirector::HandleJingleResume()
{
	if (JingleComponent)
	{
		JingleComponent->SetPaused(false);
	}
}

void AIGMorningRoutineDirector::HandleStoreLightsRestore()
{
	for (int32 LightIndex = 0; LightIndex < StoreLights.Num(); ++LightIndex)
	{
		if (StoreLights[LightIndex])
		{
			StoreLights[LightIndex]->SetIntensity(StoreLightBaseIntensities[LightIndex]);
		}
	}
}

void AIGMorningRoutineDirector::HandleGhostChime()
{
	if (StoreDoor && !StoreDoor->IsOpen())
	{
		StoreDoor->PlayChime(0.85f);
		GetWorldTimerManager().SetTimer(
			GhostThoughtTimerHandle,
			this,
			&ThisClass::HandleGhostChimeThought,
			1.4f,
			false);
	}
}

void AIGMorningRoutineDirector::HandleGhostChimeThought()
{
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGMorning", "GhostChime", "…방금, 문은 안 열렸는데."),
		3.8f);
}

FText AIGMorningRoutineDirector::GetObjectiveText() const
{
	switch (Phase)
	{
	case EIGMorningPhase::CheckFridge:
		return NSLOCTEXT("IGMorning", "ObjFridge", "목이 마르다 — 냉장고를 확인하자");
	case EIGMorningPhase::TakeWallet:
		return NSLOCTEXT("IGMorning", "ObjWallet", "물이 없다… 책상 위 지갑을 챙기자");
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
		return TEXT("Thirsty - check the fridge");
	case EIGMorningPhase::TakeWallet:
		return TEXT("No water... grab the wallet on the desk");
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
