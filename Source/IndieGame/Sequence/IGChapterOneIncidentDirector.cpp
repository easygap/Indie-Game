#include "Sequence/IGChapterOneIncidentDirector.h"
#include "Accessibility/IGAccessibilitySubsystem.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/IGNeighborhoodLifeDirector.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "IndieGame.h"
#include "Interaction/IGZoneTrigger.h"
#include "Interaction/IGPickupItem.h"
#include "Narrative/IGItemContinuityDressing.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryHelpers.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Save/IGSaveSubsystem.h"
#include "TimerManager.h"

namespace IGChapterOneIncident
{
	FGameplayTag Tag(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	}
}

AIGChapterOneIncidentAction::AIGChapterOneIncidentAction()
{
	PrimaryActorTick.bCanEverTick = false;
	PresentationMesh =
		CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Presentation"));
	SetRootComponent(PresentationMesh);
	PresentationMesh->SetCollisionProfileName(
		UCollisionProfile::BlockAll_ProfileName);
	PresentationMesh->SetGenerateOverlapEvents(false);
	PresentationMesh->SetCanEverAffectNavigation(false);
}

void AIGChapterOneIncidentAction::Configure(
	AIGChapterOneIncidentDirector* InDirector,
	const EIGChapterOneIncidentAction InAction,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& SizeCentimeters,
	const FText& Prompt,
	const float HoldSeconds)
{
	Director = InDirector;
	Action = InAction;
	PresentationMesh->SetStaticMesh(Mesh);
	PresentationMesh->SetMaterial(0, Material);
	PresentationMesh->SetRelativeScale3D(SizeCentimeters / 100.0f);
	InteractionPrompt = Prompt;
	InteractionHoldDuration = FMath::Max(0.0f, HoldSeconds);
	InteractionTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Interaction.Inspect")),
		false);
}

void AIGChapterOneIncidentAction::ConfigureAsInteractionSurface()
{
	PresentationMesh->SetVisibility(false, true);
	PresentationMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PresentationMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PresentationMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AIGChapterOneIncidentAction::CompleteInteraction_Implementation(
	const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);
	if (Director)
	{
		Director->HandleAction(Action, this);
	}
}

AIGChapterOneIncidentDirector::AIGChapterOneIncidentDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root =
		CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AIGChapterOneIncidentDirector::Configure(
	AIGPrologueWorldScene* InWorldScene,
	AIGNeighborhoodLifeDirector* InNeighborhood,
	UStaticMesh* CubeMesh,
	UStaticMesh* CylinderMesh,
	UMaterialInterface* PlasticMaterial,
	UMaterialInterface* PaperMaterial,
	UMaterialInterface* WaterMaterial,
	const FTransform& InSceneTransform)
{
	if (bConfigured || !GetWorld())
	{
		return;
	}
	bConfigured = true;
	WorldScene = InWorldScene;
	Neighborhood = InNeighborhood;
	SceneTransform = InSceneTransform;
	ContinuityCubeMesh = CubeMesh;
	ContinuityCylinderMesh = CylinderMesh;
	ContinuityBagMaterial = PaperMaterial;
	ContinuityBottleMaterial = WaterMaterial;
	ContinuityCapMaterial = PlasticMaterial;

	DrinkZone = SpawnZone(
		FVector(1880.0f, -515.0f, 105.0f),
		FVector(135.0f, 145.0f, 105.0f));
	CatPassZone = SpawnZone(
		FVector(1060.0f, -515.0f, 105.0f),
		FVector(42.0f, 150.0f, 105.0f));
	if (DrinkZone)
	{
		DrinkZone->OnZoneTriggered.AddUniqueDynamic(
			this,
			&ThisClass::HandleDrinkZone);
	}
	if (CatPassZone)
	{
		CatPassZone->OnZoneTriggered.AddUniqueDynamic(
			this,
			&ThisClass::HandleCatPassZone);
	}

	PaperCupAction = SpawnAction(
		EIGChapterOneIncidentAction::TakePaperCup,
		FVector(2585.0f, -318.0f, 106.0f),
		FVector(10.0f, 10.0f, 13.0f),
		CylinderMesh,
		PaperMaterial,
		NSLOCTEXT("IGCH01", "TakePaperCup", "깨끗한 종이컵 챙기기"));
	CapWaterAction = SpawnAction(
		EIGChapterOneIncidentAction::GiveWaterInCap,
		FVector(1230.0f, -438.0f, 9.0f),
		FVector(8.0f, 8.0f, 3.0f),
		CylinderMesh,
		PlasticMaterial,
		NSLOCTEXT("IGCH01", "GiveCapWater", "병뚜껑에 물 따라 주기"),
		0.35f);
	CupWaterAction = SpawnAction(
		EIGChapterOneIncidentAction::GiveWaterInPaperCup,
		FVector(1255.0f, -438.0f, 10.0f),
		FVector(10.0f, 10.0f, 13.0f),
		CylinderMesh,
		PaperMaterial,
		NSLOCTEXT("IGCH01", "GiveCupWater", "종이컵에 물 따라 주기"),
		0.35f);
	WaitAction = SpawnAction(
		EIGChapterOneIncidentAction::WaitForCat,
		FVector(1198.0f, -438.0f, 8.0f),
		FVector(18.0f, 18.0f, 0.8f),
		CylinderMesh,
		WaterMaterial,
		NSLOCTEXT("IGCH01", "WaitForCat", "마실 때까지 잠시 기다리기"),
		1.2f);
	FifthFloorStepAction = SpawnAction(
		EIGChapterOneIncidentAction::ClimbToFifthFloor,
		FVector(-277.5f, -216.0f, 920.0f),
		FVector(85.0f, 22.0f, 4.0f),
		CubeMesh,
		PlasticMaterial,
		NSLOCTEXT(
			"IGCH01",
			"ClimbToFifthFloor",
			"5층으로 향하는 첫 계단 오르기"));
	if (FifthFloorStepAction)
	{
		FifthFloorStepAction->ConfigureAsInteractionSurface();
	}

	ReconcileState();
	if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
	{
		if (AIGHorrorHUD* HUD = Cast<AIGHorrorHUD>(Controller->GetHUD()))
		{
			HUD->SetObjectiveProvider(this);
		}
	}
}

void AIGChapterOneIncidentDirector::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (DrinkZone)
	{
		DrinkZone->OnZoneTriggered.RemoveDynamic(
			this,
			&ThisClass::HandleDrinkZone);
	}
	if (CatPassZone)
	{
		CatPassZone->OnZoneTriggered.RemoveDynamic(
			this,
			&ThisClass::HandleCatPassZone);
	}
	GetWorldTimerManager().ClearAllTimersForObject(this);
	if (APlayerController* Controller =
			GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);
	}
	if (WorldScene)
	{
		WorldScene->SetFixtureLive(0, true, true);
		WorldScene->SuspendCorridorFlicker(false);
	}
	if (AccidentBagDressing)
	{
		AccidentBagDressing->Destroy();
		AccidentBagDressing = nullptr;
	}
	for (AActor* SpawnedActor : SpawnedIncidentActors)
	{
		if (IsValid(SpawnedActor) && !SpawnedActor->IsActorBeingDestroyed())
		{
			SpawnedActor->Destroy();
		}
	}
	SpawnedIncidentActors.Reset();
	Super::EndPlay(EndPlayReason);
}

AIGChapterOneIncidentAction*
AIGChapterOneIncidentDirector::SpawnAction(
	const EIGChapterOneIncidentAction Action,
	const FVector& LocalLocation,
	const FVector& SizeCentimeters,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FText& Prompt,
	const float HoldSeconds)
{
	const FTransform SpawnTransform(
		SceneTransform.TransformRotation(FQuat::Identity),
		ToWorld(LocalLocation));
	AIGChapterOneIncidentAction* Result =
		GetWorld()->SpawnActorDeferred<AIGChapterOneIncidentAction>(
			AIGChapterOneIncidentAction::StaticClass(),
			SpawnTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Result)
	{
		Result->Configure(
			this,
			Action,
			Mesh,
			Material,
			SizeCentimeters,
			Prompt,
			HoldSeconds);
		Result->FinishSpawning(SpawnTransform);
		SpawnedIncidentActors.Add(Result);
	}
	return Result;
}

AIGZoneTrigger* AIGChapterOneIncidentDirector::SpawnZone(
	const FVector& LocalLocation,
	const FVector& HalfExtent)
{
	const FTransform SpawnTransform(
		SceneTransform.TransformRotation(FQuat::Identity),
		ToWorld(LocalLocation));
	AIGZoneTrigger* Result = GetWorld()->SpawnActorDeferred<AIGZoneTrigger>(
		AIGZoneTrigger::StaticClass(),
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Result)
	{
		Result->SetZoneExtent(HalfExtent);
		Result->FinishSpawning(SpawnTransform);
		SpawnedIncidentActors.Add(Result);
	}
	return Result;
}

FVector AIGChapterOneIncidentDirector::ToWorld(
	const FVector& LocalLocation) const
{
	return SceneTransform.TransformPosition(LocalLocation);
}

FVector AIGChapterOneIncidentDirector::GetListenerLocation() const
{
	const APlayerController* Controller =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	return Pawn ? Pawn->GetActorLocation() : GetActorLocation();
}

void AIGChapterOneIncidentDirector::SetVisibleInteractive(
	AIGChapterOneIncidentAction* Action,
	const bool bVisible)
{
	if (!Action)
	{
		return;
	}
	Action->SetActorHiddenInGame(!bVisible);
	Action->SetActorEnableCollision(bVisible);
	Action->SetInteractionEnabled(bVisible);
}

void AIGChapterOneIncidentDirector::SetVisibleDecorative(
	AIGChapterOneIncidentAction* Action,
	const bool bVisible)
{
	if (!Action)
	{
		return;
	}
	Action->SetActorHiddenInGame(!bVisible);
	Action->SetActorEnableCollision(false);
	Action->SetInteractionEnabled(false);
}

bool AIGChapterOneIncidentDirector::ValidateCatWaterAftermath() const
{
	const UIGRebirthNarrativeSubsystem* RebirthState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	if (!RebirthState
		|| !IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.CatChoiceCommitted")))
		|| !CapWaterAction
		|| !CupWaterAction
		|| !WaitAction)
	{
		return false;
	}

	const FIGRebirthChoiceState Choices = RebirthState->GetChoices();
	const bool bCapVisible = !CapWaterAction->IsHidden();
	const bool bCupVisible = !CupWaterAction->IsHidden();
	const bool bWetRingVisible = !WaitAction->IsHidden();
	const bool bAllDecorative =
		!CapWaterAction->GetActorEnableCollision()
		&& !CupWaterAction->GetActorEnableCollision()
		&& !WaitAction->GetActorEnableCollision()
		&& !CapWaterAction->IsInteractionEnabled()
		&& !CupWaterAction->IsInteractionEnabled()
		&& !WaitAction->IsInteractionEnabled();
	if (!bAllDecorative)
	{
		return false;
	}

	if (Choices.CatWaterState == EIGRebirthCatWaterState::BottleCap)
	{
		return Choices.bWaitedForCat
			? !bCapVisible && !bCupVisible && bWetRingVisible
			: bCapVisible && !bCupVisible && !bWetRingVisible;
	}
	if (Choices.CatWaterState == EIGRebirthCatWaterState::PaperCup)
	{
		return !bCapVisible && bCupVisible && !bWetRingVisible;
	}
	if (Choices.CatWaterState == EIGRebirthCatWaterState::PassedBy)
	{
		return !bCapVisible && !bCupVisible && !bWetRingVisible;
	}
	return false;
}

void AIGChapterOneIncidentDirector::ReconcileState()
{
	UIGRebirthNarrativeSubsystem* RebirthState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	if (!RebirthState)
	{
		return;
	}
	FIGRebirthChoiceState Choices = RebirthState->GetChoices();
	const bool bChoiceCommitted = IGStory::HasState(
		this,
		IGChapterOneIncident::Tag(
			TEXT("State.CH01.Incident.CatChoiceCommitted")));
	if (!bChoiceCommitted
		&& Choices.CatWaterState == EIGRebirthCatWaterState::PassedBy)
	{
		// Migration from the old checkout default: it was never a player act.
		Choices.CatWaterState = EIGRebirthCatWaterState::Unset;
		Choices.BottleClosureState = EIGRebirthBottleClosureState::Unset;
		Choices.bWaitedForCat = false;
		RebirthState->SetChoices(Choices);
	}

	const bool bDrank = IGStory::HasState(
		this,
		IGChapterOneIncident::Tag(TEXT("State.CH01.Incident.Drank")));
	if (DrinkZone)
	{
		DrinkZone->SetActorEnableCollision(!bDrank);
	}
	SetVisibleInteractive(PaperCupAction, !Choices.bHasPaperCup && !bChoiceCommitted);
	const bool bChoosingContainer =
		bDrank
		&& !bChoiceCommitted
		&& Choices.CatWaterState == EIGRebirthCatWaterState::Unset;
	const bool bShowCapEvidence =
		Choices.CatWaterState == EIGRebirthCatWaterState::BottleCap
		&& !Choices.bWaitedForCat;
	const bool bShowCupEvidence =
		Choices.CatWaterState == EIGRebirthCatWaterState::PaperCup;
	if (bChoosingContainer)
	{
		SetVisibleInteractive(CapWaterAction, true);
		SetVisibleInteractive(CupWaterAction, Choices.bHasPaperCup);
	}
	else
	{
		SetVisibleDecorative(CapWaterAction, bShowCapEvidence);
		SetVisibleDecorative(CupWaterAction, bShowCupEvidence);
	}
	const bool bCanWait =
		bDrank
		&& !bChoiceCommitted
		&& (Choices.CatWaterState == EIGRebirthCatWaterState::BottleCap
			|| Choices.CatWaterState == EIGRebirthCatWaterState::PaperCup);
	if (bCanWait)
	{
		SetVisibleInteractive(WaitAction, true);
	}
	else
	{
		const bool bShowRecoveredCapWetRing =
			Choices.CatWaterState == EIGRebirthCatWaterState::BottleCap
			&& Choices.bWaitedForCat;
		SetVisibleDecorative(WaitAction, bShowRecoveredCapWetRing);
	}
	const bool bReachedFourthFloor = IGStory::HasState(
		this,
		IGChapterOneIncident::Tag(
			TEXT("State.CH01.Incident.ReachedFourthFloor")));
	SetVisibleInteractive(
		FifthFloorStepAction,
		bReachedFourthFloor
			&& bFifthFloorStepArmed
			&& !bMemoryBoundaryStarted);
	if (CatPassZone)
	{
		CatPassZone->SetActorEnableCollision(!bChoiceCommitted);
	}
	if (bReachedFourthFloor && !bMemoryBoundaryStarted)
	{
		StartFourthFloorCueIfNeeded();
	}
}

void AIGChapterOneIncidentDirector::HandleDrinkZone(AIGZoneTrigger* Zone)
{
	if (Zone == DrinkZone)
	{
		PerformDrink();
	}
}

void AIGChapterOneIncidentDirector::PerformDrink()
{
	const FGameplayTag DrankTag =
		IGChapterOneIncident::Tag(TEXT("State.CH01.Incident.Drank"));
	if (IGStory::HasState(this, DrankTag))
	{
		return;
	}
	UIGRebirthNarrativeSubsystem* RebirthState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	bool bProfileC = false;
	if (RebirthState)
	{
		FIGRebirthChoiceState Choices = RebirthState->GetChoices();
		bProfileC = Choices.PurchaseProfile
			== EIGRebirthPurchaseProfile::ProfileC2LX2;
		Choices.BottleClosureState =
			EIGRebirthBottleClosureState::Resealed;
		RebirthState->SetChoices(Choices);
	}
	IGStory::AddState(this, DrankTag);

	// 04:33, wordless (§10.1: the hand proves it, the subtitle stays quiet).
	// Profile C first puts the 4 kg bag on the concrete, then the cap cracks,
	// a few swallows, the cap ratchets back, and the bag comes up again.
	float DrinkStart = 0.15f;
	if (bProfileC)
	{
		APlayerController* PlayerController = GetWorld()
			? GetWorld()->GetFirstPlayerController()
			: nullptr;
		if (AIGPlayerCharacter* Player = PlayerController
			? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
			: nullptr)
		{
			if (Player->BeginScriptedHeavyBagRest(3.6f))
			{
				DrinkStart = 0.65f;
			}
		}
	}
	const TWeakObjectPtr<AIGChapterOneIncidentDirector> WeakThis(this);
	auto PlayNearHands = [WeakThis](
		UIGToneSequenceSoundWave* (*Factory)(UObject*),
		const float Volume,
		const float Pitch)
	{
		if (AIGChapterOneIncidentDirector* Director = WeakThis.Get())
		{
			IGAudio::SpawnOneShotAt(
				Director,
				Factory(Director),
				Director->GetListenerLocation() - FVector(0, 0, 34),
				Volume,
				Pitch,
				40.0f,
				420.0f);
		}
	};
	FTimerHandle CapHandle;
	GetWorldTimerManager().SetTimer(
		CapHandle,
		[PlayNearHands]()
		{
			PlayNearHands(
				&UIGToneSequenceSoundWave::CreateBottleCapOpen, 0.42f, 1.0f);
		},
		DrinkStart,
		false);
	FTimerHandle SwallowHandle;
	GetWorldTimerManager().SetTimer(
		SwallowHandle,
		[PlayNearHands]()
		{
			PlayNearHands(
				&UIGToneSequenceSoundWave::CreateWaterSwallows, 0.46f, 1.0f);
		},
		DrinkStart + 0.55f,
		false);
	FTimerHandle ResealHandle;
	GetWorldTimerManager().SetTimer(
		ResealHandle,
		[PlayNearHands]()
		{
			PlayNearHands(
				&UIGToneSequenceSoundWave::CreateBottleReseal, 0.40f, 1.0f);
		},
		DrinkStart + 2.45f,
		false);

	ReconcileState();
	RequestReturnCheckpointAutosave();
	PushObjectiveRefresh();
}

void AIGChapterOneIncidentDirector::HandleAction(
	const EIGChapterOneIncidentAction Action,
	AIGChapterOneIncidentAction* Source)
{
	UIGRebirthNarrativeSubsystem* RebirthState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	if (!RebirthState)
	{
		return;
	}
	FIGRebirthChoiceState Choices = RebirthState->GetChoices();
	switch (Action)
	{
	case EIGChapterOneIncidentAction::TakePaperCup:
		Choices.bHasPaperCup = true;
		RebirthState->SetChoices(Choices);
		SetVisibleInteractive(Source, false);
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH01", "PaperCupTaken", "깨끗한 종이컵 하나만 챙겼다."),
			2.6f);
		ReconcileState();
		RequestReturnCheckpointAutosave();
		break;

	case EIGChapterOneIncidentAction::GiveWaterInCap:
		if (!IGStory::HasState(
				this,
				IGChapterOneIncident::Tag(
					TEXT("State.CH01.Incident.Drank"))))
		{
			return;
		}
		Choices.CatWaterState = EIGRebirthCatWaterState::BottleCap;
		Choices.BottleClosureState =
			EIGRebirthBottleClosureState::MissingCap;
		Choices.bWaitedForCat = false;
		RebirthState->SetChoices(Choices);
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH01", "CapWaterGiven", "뚜껑에 물을 받자 고양이가 코를 댔다."),
			3.1f);
		ReconcileState();
		RequestReturnCheckpointAutosave();
		break;

	case EIGChapterOneIncidentAction::GiveWaterInPaperCup:
		if (!Choices.bHasPaperCup
			|| !IGStory::HasState(
				this,
				IGChapterOneIncident::Tag(
					TEXT("State.CH01.Incident.Drank"))))
		{
			return;
		}
		Choices.CatWaterState = EIGRebirthCatWaterState::PaperCup;
		Choices.BottleClosureState =
			EIGRebirthBottleClosureState::Resealed;
		Choices.bWaitedForCat = false;
		RebirthState->SetChoices(Choices);
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH01", "CupWaterGiven", "종이컵에 물을 따르자 고양이가 다가왔다."),
			3.1f);
		ReconcileState();
		RequestReturnCheckpointAutosave();
		break;

	case EIGChapterOneIncidentAction::WaitForCat:
		if (Choices.CatWaterState != EIGRebirthCatWaterState::BottleCap
			&& Choices.CatWaterState != EIGRebirthCatWaterState::PaperCup)
		{
			return;
		}
		Choices.bWaitedForCat = true;
		if (Choices.CatWaterState == EIGRebirthCatWaterState::BottleCap)
		{
			Choices.BottleClosureState =
				EIGRebirthBottleClosureState::Resealed;
		}
		RebirthState->SetChoices(Choices);
		FinalizeCatChoice();
		break;

	case EIGChapterOneIncidentAction::ClimbToFifthFloor:
		BeginMemoryBoundary();
		break;

	case EIGChapterOneIncidentAction::None:
	default:
		break;
	}
	PushObjectiveRefresh();
}

void AIGChapterOneIncidentDirector::HandleCatPassZone(AIGZoneTrigger* Zone)
{
	if (Zone != CatPassZone)
	{
		return;
	}
	if (!IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(TEXT("State.CH01.Incident.Drank"))))
	{
		PerformDrink();
	}
	if (UIGRebirthNarrativeSubsystem* RebirthState =
			GetGameInstance()
				? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
				: nullptr)
	{
		FIGRebirthChoiceState Choices = RebirthState->GetChoices();
		if (Choices.CatWaterState == EIGRebirthCatWaterState::Unset)
		{
			Choices.CatWaterState = EIGRebirthCatWaterState::PassedBy;
			Choices.BottleClosureState =
				EIGRebirthBottleClosureState::Resealed;
		}
		Choices.bWaitedForCat = false;
		RebirthState->SetChoices(Choices);
	}
	FinalizeCatChoice();
}

void AIGChapterOneIncidentDirector::FinalizeCatChoice()
{
	const FGameplayTag ChoiceTag = IGChapterOneIncident::Tag(
		TEXT("State.CH01.Incident.CatChoiceCommitted"));
	if (IGStory::HasState(this, ChoiceTag))
	{
		return;
	}
	IGStory::AddState(this, ChoiceTag);
	IGStory::AddState(
		this,
		IGChapterOneIncident::Tag(
			TEXT("State.CH01.Incident.ThirdGustOccurred")));
	if (Neighborhood)
	{
		Neighborhood->PlayAuthoredReturnIncident(
			ToWorld(FVector(1215.0f, -420.0f, 4.0f)),
			ToWorld(FVector(650.0f, -395.0f, 4.0f)));
	}
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGCH01",
			"ThirdGustCatRun",
			"배수 체인이 먼저 울렸다. 고양이가 현관 안으로 뛰어들었다."),
		4.2f);
	ReconcileState();
	RequestReturnCheckpointAutosave();
	PushObjectiveRefresh();
}

bool AIGChapterOneIncidentDirector::RegisterLobbyReturn()
{
	if (bMemoryBoundaryCompleted)
	{
		return true;
	}
	if (!IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(TEXT("State.CH01.Incident.Drank"))))
	{
		PerformDrink();
	}
	if (!IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.CatChoiceCommitted"))))
	{
		if (UIGRebirthNarrativeSubsystem* RebirthState =
				GetGameInstance()
					? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
					: nullptr)
		{
			FIGRebirthChoiceState Choices = RebirthState->GetChoices();
			if (Choices.CatWaterState == EIGRebirthCatWaterState::Unset)
			{
				Choices.CatWaterState = EIGRebirthCatWaterState::PassedBy;
				Choices.BottleClosureState =
					EIGRebirthBottleClosureState::Resealed;
			}
			Choices.bWaitedForCat = false;
			RebirthState->SetChoices(Choices);
		}
		FinalizeCatChoice();
	}

	const FGameplayTag EnteredLobbyTag = IGChapterOneIncident::Tag(
		TEXT("State.CH01.Incident.EnteredLobby"));
	if (!IGStory::HasState(this, EnteredLobbyTag))
	{
		IGStory::AddState(this, EnteredLobbyTag);
		TWeakObjectPtr<AIGChapterOneIncidentDirector> WeakThis(this);
		FTimerDelegate RoofRattleDelegate;
		RoofRattleDelegate.BindLambda([WeakThis]()
		{
			AIGChapterOneIncidentDirector* Director = WeakThis.Get();
			if (!Director)
			{
				return;
			}
			IGAudio::SpawnOneShotAt(
				Director,
				UIGToneSequenceSoundWave::CreateLockedRattle(Director),
				Director->GetListenerLocation(),
				0.20f,
				0.86f,
				60.0f,
				650.0f);
			AIGHorrorHUD::PushThought(
				Director,
				NSLOCTEXT(
					"IGCH01",
					"RoofDoorShouldBeLocked",
					"옥상문, 잠겨 있을 텐데."),
				3.2f);
		});
		FTimerHandle RoofRattleTimer;
		GetWorldTimerManager().SetTimer(
			RoofRattleTimer,
			RoofRattleDelegate,
			0.8f,
			false);
		RequestReturnCheckpointAutosave();
	}
	PushObjectiveRefresh();
	return true;
}

bool AIGChapterOneIncidentDirector::RegisterFourthFloorReturn()
{
	if (bMemoryBoundaryCompleted)
	{
		return true;
	}
	if (!RegisterLobbyReturn())
	{
		return false;
	}

	const FGameplayTag ReachedFourthFloorTag = IGChapterOneIncident::Tag(
		TEXT("State.CH01.Incident.ReachedFourthFloor"));
	if (IGStory::HasState(this, ReachedFourthFloorTag))
	{
		return true;
	}
	IGStory::AddState(this, ReachedFourthFloorTag);
	StartFourthFloorCueIfNeeded();
	RequestReturnCheckpointAutosave();
	PushObjectiveRefresh();
	return true;
}

bool AIGChapterOneIncidentDirector::RunRebirthEndToEndReturnRoute()
{
	if (!bConfigured)
	{
		return false;
	}

	PerformDrink();
	HandleAction(
		EIGChapterOneIncidentAction::TakePaperCup,
		PaperCupAction);
	HandleAction(
		EIGChapterOneIncidentAction::GiveWaterInPaperCup,
		CupWaterAction);
	HandleAction(
		EIGChapterOneIncidentAction::WaitForCat,
		WaitAction);
	if (!RegisterLobbyReturn() || !RegisterFourthFloorReturn())
	{
		return false;
	}

	GetWorldTimerManager().SetTimer(
		BoundaryTimer,
		this,
		&ThisClass::BeginRebirthEndToEndMemoryBoundary,
		0.65f,
		false);

	const UIGRebirthNarrativeSubsystem* RebirthState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	if (!RebirthState)
	{
		return false;
	}
	const FIGRebirthChoiceState Choices = RebirthState->GetChoices();
	const bool bCatAftermathMatched = ValidateCatWaterAftermath();
	return Choices.CatWaterState == EIGRebirthCatWaterState::PaperCup
		&& Choices.BottleClosureState
			== EIGRebirthBottleClosureState::Resealed
		&& Choices.bHasPaperCup
		&& Choices.bWaitedForCat
		&& bCatAftermathMatched;
}

void AIGChapterOneIncidentDirector::BeginRebirthEndToEndMemoryBoundary()
{
	if (!BeginMemoryBoundary())
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_E2E FAIL ch01_memory_boundary "
				"reached_fourth_floor=1 cue_armed=%d"),
			bFifthFloorStepArmed ? 1 : 0);
		FPlatformMisc::RequestExitWithStatus(
			false,
			1,
			TEXT("REBIRTH end-to-end CH01 transition failed"));
	}
}

void AIGChapterOneIncidentDirector::StartFourthFloorCueIfNeeded()
{
	if (bFourthFloorCueStarted || bMemoryBoundaryStarted)
	{
		return;
	}
	bFourthFloorCueStarted = true;
	bFifthFloorStepArmed = false;
	SetVisibleInteractive(FifthFloorStepAction, false);
	if (!WorldScene)
	{
		CompleteFourthFloorCue();
		return;
	}
	WorldScene->SuspendCorridorFlicker(true);
	// 이 큐는 총 두 번 점멸하므로 초당 세 번 한계에는 걸리지 않는다. 그래도
	// 점멸 감소를 켠 사람에게는 켜고 끄는 과정을 건너뛰고 결과와 소리만
	// 남긴다 — 복도가 반응했다는 사실이 전달되면 큐는 제 일을 한 것이다.
	CorridorBlinkStep = IsReducedFlickerEnabled() ? 3 : 0;
	AdvanceCorridorBlink();
}

bool AIGChapterOneIncidentDirector::IsReducedFlickerEnabled() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	return Accessibility && Accessibility->IsReducedFlickerEnabled();
}

void AIGChapterOneIncidentDirector::AdvanceCorridorBlink()
{
	if (!WorldScene)
	{
		CompleteFourthFloorCue();
		return;
	}
	switch (CorridorBlinkStep++)
	{
	case 0:
		WorldScene->SetFixtureLive(0, false, true);
		GetWorldTimerManager().SetTimer(
			CorridorBlinkTimer,
			this,
			&ThisClass::AdvanceCorridorBlink,
			0.12f,
			false);
		break;
	case 1:
		WorldScene->SetFixtureLive(0, true, true);
		GetWorldTimerManager().SetTimer(
			CorridorBlinkTimer,
			this,
			&ThisClass::AdvanceCorridorBlink,
			0.16f,
			false);
		break;
	case 2:
		WorldScene->SetFixtureLive(0, false, true);
		GetWorldTimerManager().SetTimer(
			CorridorBlinkTimer,
			this,
			&ThisClass::AdvanceCorridorBlink,
			0.12f,
			false);
		break;
	default:
		WorldScene->SetFixtureLive(0, true, true);
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateFluorescentBallastSnap(this),
			WorldScene->GetCorridorFixtureLocation(0),
			0.22f,
			1.0f,
			40.0f,
			520.0f);
		WorldScene->SuspendCorridorFlicker(false);
		CompleteFourthFloorCue();
		break;
	}
}

void AIGChapterOneIncidentDirector::CompleteFourthFloorCue()
{
	if (bMemoryBoundaryStarted || bFifthFloorStepArmed)
	{
		return;
	}
	bFifthFloorStepArmed = true;
	SetVisibleInteractive(FifthFloorStepAction, true);
	if (Neighborhood)
	{
		Neighborhood->PlayAuthoredCatCall(
			ToWorld(FVector(-277.5f, -145.0f, 1045.0f)),
			false);
	}
	PushObjectiveRefresh();
}

bool AIGChapterOneIncidentDirector::BeginMemoryBoundary()
{
	if (bMemoryBoundaryCompleted || bMemoryBoundaryStarted)
	{
		return true;
	}
	if (!IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.ReachedFourthFloor"))))
	{
		return false;
	}
	if (!bFifthFloorStepArmed)
	{
		return false;
	}

	bMemoryBoundaryStarted = true;
	SetVisibleInteractive(FifthFloorStepAction, false);
	PlaceAccidentBagAndRetireCarriedPurchase();
	IGStory::AddState(
		this,
		IGChapterOneIncident::Tag(
			TEXT("State.CH01.Incident.MemoryBoundary")));
	IGStory::AddState(
		this,
		IGChapterOneIncident::Tag(
			TEXT("State.CH01.Incident.BagPlacedAtLadder")));
	// Do not create a save inside the 1.8-second chapter cut. A load from the
	// last fourth-floor checkpoint replays this atomic hand-to-floor transfer;
	// CH02 writes the next safe checkpoint only after the old carried actor
	// and this transient proxy have both been retired.
	if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
	{
		Controller->SetIgnoreMoveInput(true);
		Controller->SetIgnoreLookInput(true);
		if (APlayerCameraManager* Camera = Controller->PlayerCameraManager)
		{
			Camera->StartCameraFade(
				0.0f,
				1.0f,
				1.8f,
				FLinearColor::Black,
				false,
				true);
		}
	}
	GetWorldTimerManager().SetTimer(
		BoundaryTimer,
		this,
		&ThisClass::CompleteMemoryBoundary,
		1.8f,
		false);
	return true;
}

void AIGChapterOneIncidentDirector::PlaceAccidentBagAndRetireCarriedPurchase()
{
	if (AccidentBagDressing || !GetWorld())
	{
		return;
	}

	EIGRebirthPurchaseProfile PurchaseProfile =
		EIGRebirthPurchaseProfile::ProfileA500MlX2;
	EIGRebirthBottleClosureState ClosureState =
		EIGRebirthBottleClosureState::Resealed;
	UIGRebirthNarrativeSubsystem* NarrativeState = nullptr;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		NarrativeState =
			GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>();
		if (NarrativeState)
		{
			const FIGRebirthChoiceState Choices = NarrativeState->GetChoices();
			if (Choices.PurchaseProfile != EIGRebirthPurchaseProfile::Unset)
			{
				PurchaseProfile = Choices.PurchaseProfile;
			}
			if (Choices.BottleClosureState
				!= EIGRebirthBottleClosureState::Unset)
			{
				ClosureState = Choices.BottleClosureState;
			}
		}
	}

	AIGPlayerCharacter* Player = nullptr;
	if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
	{
		Player = Cast<AIGPlayerCharacter>(Controller->GetPawn());
	}
	if (Player)
	{
		if (AActor* Carried = Player->GetCarriedActor())
		{
			Player->ReleaseCarriedActor(Carried);
		}
	}

	// Checkpoint restoration can briefly leave the selected pickup detached
	// while it retries its camera attachment. Retire every already-acquired
	// copy of the selected profile before the ground proxy appears.
	for (TActorIterator<AIGPickupItem> It(GetWorld()); It; ++It)
	{
		AIGPickupItem* Pickup = *It;
		if (!Pickup
			|| !Pickup->WasPickedUp()
			|| Pickup->RebirthPurchaseProfileOnPickup != PurchaseProfile)
		{
			continue;
		}
		Pickup->SetActorHiddenInGame(true);
		Pickup->SetActorEnableCollision(false);
		Pickup->SetInteractionEnabled(false);
	}

	// The first-stair cut never materializes the roof on top of the fourth
	// floor. Build the transfer in CH03's detached roof-stage coordinates so
	// no visible bag teleports onto the apartment stair landing.
	const FTransform BagTransform =
		AIGItemContinuityDressing::GetCanonicalAccidentTransform(
			PurchaseProfile);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AccidentBagDressing =
		GetWorld()->SpawnActor<AIGItemContinuityDressing>(
			AIGItemContinuityDressing::StaticClass(),
			BagTransform,
			SpawnParameters);
	if (AccidentBagDressing)
	{
		AccidentBagDressing->Configure(
			EIGItemContinuityPresentation::AccidentBag,
			PurchaseProfile,
			ClosureState,
			ContinuityCubeMesh,
			ContinuityCylinderMesh,
			ContinuityBagMaterial,
			ContinuityBottleMaterial,
			ContinuityBottleMaterial,
			ContinuityCapMaterial);
		if (NarrativeState)
		{
			NarrativeState->MarkOneShotBeatPlayed(
				FName(TEXT("CH01.BagPlacedAtLadder")));
		}
	}
}

void AIGChapterOneIncidentDirector::CompleteMemoryBoundary()
{
	bMemoryBoundaryCompleted = true;
	OnMemoryBoundaryCompleted.Broadcast();
}

void AIGChapterOneIncidentDirector::RequestReturnCheckpointAutosave() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGSaveSubsystem* SaveSubsystem =
				GameInstance->GetSubsystem<UIGSaveSubsystem>())
		{
			const bool bReachedFourthFloor = IGStory::HasState(
				this,
				IGChapterOneIncident::Tag(
					TEXT("State.CH01.Incident.ReachedFourthFloor")));
			const bool bEnteredLobby = IGStory::HasState(
				this,
				IGChapterOneIncident::Tag(
					TEXT("State.CH01.Incident.EnteredLobby")));
			const bool bCatChoiceCommitted = IGStory::HasState(
				this,
				IGChapterOneIncident::Tag(
					TEXT("State.CH01.Incident.CatChoiceCommitted")));
			const bool bDrank = IGStory::HasState(
				this,
				IGChapterOneIncident::Tag(
					TEXT("State.CH01.Incident.Drank")));
			const UIGRebirthNarrativeSubsystem* RebirthState =
				GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>();
			const bool bCatContainerChosen =
				RebirthState
				&& RebirthState->GetChoices().CatWaterState
					!= EIGRebirthCatWaterState::Unset;
			const TCHAR* CheckpointName = bReachedFourthFloor
				? TEXT("Checkpoint.CH01.FourthFloor")
				: bEnteredLobby
					? TEXT("Checkpoint.CH01.Lobby")
					: bCatChoiceCommitted
						? TEXT("Checkpoint.CH01.ReturnAlley")
						: bCatContainerChosen
							? TEXT("Checkpoint.CH01.CatSpot")
							: bDrank
								? TEXT("Checkpoint.CH01.CatApproach")
								: TEXT("Checkpoint.CH01.WaterPurchased");
			SaveSubsystem->RequestAutosave(
				IGChapterOneIncident::Tag(TEXT("Chapter.CH01")),
				GetWorld() ? GetWorld()->GetOutermost()->GetFName() : NAME_None,
				IGChapterOneIncident::Tag(CheckpointName));
		}
	}
}

void AIGChapterOneIncidentDirector::PushObjectiveRefresh() const
{
	if (APlayerController* Controller =
			GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (AIGHorrorHUD* HUD = Cast<AIGHorrorHUD>(Controller->GetHUD()))
		{
			HUD->SetObjectiveProvider(
				const_cast<AIGChapterOneIncidentDirector*>(this));
		}
	}
}

FText AIGChapterOneIncidentDirector::GetObjectiveText() const
{
	if (bMemoryBoundaryStarted)
	{
		return FText::GetEmpty();
	}
	if (IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.ReachedFourthFloor"))))
	{
		return NSLOCTEXT(
			"IGCH01",
			"IncidentObjectiveUpperStair",
			"계단실 위에서 들린 울음을 따라가자");
	}
	if (IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.EnteredLobby"))))
	{
		return NSLOCTEXT(
			"IGCH01",
			"IncidentObjectiveFourthFloor",
			"계단이나 엘리베이터로 4층에 올라가자");
	}
	if (!IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(TEXT("State.CH01.Incident.Drank"))))
	{
		return NSLOCTEXT("IGCH01", "IncidentObjectiveDrink", "집으로 가는 길에 물을 마시자");
	}
	if (!IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.CatChoiceCommitted"))))
	{
		const UIGRebirthNarrativeSubsystem* RebirthState =
			GetGameInstance()
				? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
				: nullptr;
		const EIGRebirthCatWaterState CatState = RebirthState
			? RebirthState->GetChoices().CatWaterState
			: EIGRebirthCatWaterState::Unset;
		return CatState == EIGRebirthCatWaterState::Unset
			? NSLOCTEXT(
				"IGCH01",
				"IncidentObjectiveCat",
				"고양이에게 물을 주거나 그냥 지나가자")
			: NSLOCTEXT(
				"IGCH01",
				"IncidentObjectiveWait",
				"잠시 기다리거나 집으로 돌아가자");
	}
	return NSLOCTEXT(
		"IGCH01",
		"IncidentObjectiveHome",
		"고양이를 따라 건물로 돌아가자");
}

FString AIGChapterOneIncidentDirector::GetObjectiveTextAscii() const
{
	if (bMemoryBoundaryStarted)
	{
		return FString();
	}
	if (IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.ReachedFourthFloor"))))
	{
		return TEXT("FOLLOW THE CALL TOWARD THE FIFTH FLOOR");
	}
	if (IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.EnteredLobby"))))
	{
		return TEXT("REACH THE FOURTH FLOOR BY STAIRS OR LIFT");
	}
	if (!IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(TEXT("State.CH01.Incident.Drank"))))
	{
		return TEXT("DRINK AND RESEAL THE WATER");
	}
	if (!IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.CatChoiceCommitted"))))
	{
		return TEXT("HELP THE CAT OR CONTINUE HOME");
	}
	return TEXT("FOLLOW THE CAT HOME");
}

float AIGChapterOneIncidentDirector::GetObjectiveProgress() const
{
	if (bMemoryBoundaryCompleted)
	{
		return 1.0f;
	}
	if (bMemoryBoundaryStarted)
	{
		return 0.96f;
	}
	if (IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.ReachedFourthFloor"))))
	{
		return 0.90f;
	}
	if (IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.EnteredLobby"))))
	{
		return 0.82f;
	}
	if (IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(
				TEXT("State.CH01.Incident.CatChoiceCommitted"))))
	{
		return 0.72f;
	}
	if (IGStory::HasState(
			this,
			IGChapterOneIncident::Tag(TEXT("State.CH01.Incident.Drank"))))
	{
		return 0.45f;
	}
	return 0.20f;
}
