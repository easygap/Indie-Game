#include "Sequence/IGThirdMorningDirector.h"

#include "AssetCompilingManager.h"
#include "Audio/IGAlarmSoundWave.h"
#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "IndieGame.h"
#include "Interaction/IGInteractable.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGZoneTrigger.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryHelpers.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGFlashlightComponent.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Save/IGSaveSubsystem.h"
#include "ShaderCompiler.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

namespace IGThirdMorning
{
	const FVector StageOrigin(-4800.0f, 4200.0f, 0.0f);
	constexpr float StandingCapsuleCenter = 96.0f;
	constexpr float DefaultWalkSpeed = 300.0f;
	constexpr float FloodWalkSpeed = 225.0f;
	constexpr float RoofFloorZ = 240.0f;
	const FVector TankCenter(2500.0f, -300.0f, 0.0f);

	UMaterialInterface* LoadMaterial(const TCHAR* AssetPath)
	{
		return LoadObject<UMaterialInterface>(nullptr, AssetPath);
	}

	UStaticMesh* LoadMesh(const TCHAR* AssetPath)
	{
		return LoadObject<UStaticMesh>(nullptr, AssetPath);
	}

	EIGRebirthEvidenceId ToPersistentEvidence(
		const EIGChapterThreeAction Action)
	{
		switch (Action)
		{
		case EIGChapterThreeAction::EvidenceCatEntered:
			return EIGRebirthEvidenceId::CatEntered;
		case EIGChapterThreeAction::EvidenceCatExited:
			return EIGRebirthEvidenceId::CatExited;
		case EIGChapterThreeAction::EvidenceHosePaw:
			return EIGRebirthEvidenceId::HosePaw;
		case EIGChapterThreeAction::EvidenceHoseImpact:
			return EIGRebirthEvidenceId::HoseImpact;
		case EIGChapterThreeAction::EvidenceBag:
			return EIGRebirthEvidenceId::Bag;
		case EIGChapterThreeAction::EvidenceWetRung:
			return EIGRebirthEvidenceId::WetRung;
		case EIGChapterThreeAction::EvidenceHandSmear:
			return EIGRebirthEvidenceId::HandSmear;
		case EIGChapterThreeAction::EvidenceGlasses:
			return EIGRebirthEvidenceId::Glasses;
		case EIGChapterThreeAction::EvidenceTankClothing:
			return EIGRebirthEvidenceId::TankClothing;
		case EIGChapterThreeAction::EvidenceCurrentSleeve:
			return EIGRebirthEvidenceId::CurrentSleeve;
		case EIGChapterThreeAction::EvidenceSearchPoster:
			return EIGRebirthEvidenceId::SearchPoster;
		default:
			return EIGRebirthEvidenceId::None;
		}
	}

	EIGChapterThreeAction FromPersistentEvidence(
		const EIGRebirthEvidenceId EvidenceId)
	{
		switch (EvidenceId)
		{
		case EIGRebirthEvidenceId::CatEntered:
			return EIGChapterThreeAction::EvidenceCatEntered;
		case EIGRebirthEvidenceId::CatExited:
			return EIGChapterThreeAction::EvidenceCatExited;
		case EIGRebirthEvidenceId::HosePaw:
			return EIGChapterThreeAction::EvidenceHosePaw;
		case EIGRebirthEvidenceId::HoseImpact:
			return EIGChapterThreeAction::EvidenceHoseImpact;
		case EIGRebirthEvidenceId::Bag:
			return EIGChapterThreeAction::EvidenceBag;
		case EIGRebirthEvidenceId::WetRung:
			return EIGChapterThreeAction::EvidenceWetRung;
		case EIGRebirthEvidenceId::HandSmear:
			return EIGChapterThreeAction::EvidenceHandSmear;
		case EIGRebirthEvidenceId::Glasses:
			return EIGChapterThreeAction::EvidenceGlasses;
		case EIGRebirthEvidenceId::TankClothing:
			return EIGChapterThreeAction::EvidenceTankClothing;
		case EIGRebirthEvidenceId::CurrentSleeve:
			return EIGChapterThreeAction::EvidenceCurrentSleeve;
		case EIGRebirthEvidenceId::SearchPoster:
			return EIGChapterThreeAction::EvidenceSearchPoster;
		default:
			return EIGChapterThreeAction::None;
		}
	}

	TArray<FName> EvidenceSources(const EIGChapterThreeAction Action)
	{
		switch (Action)
		{
		case EIGChapterThreeAction::EvidenceCatEntered:
			return {FName(TEXT("P5.CatEnteredPrints"))};
		case EIGChapterThreeAction::EvidenceCatExited:
			return {FName(TEXT("P5.CatExitedPrints"))};
		case EIGChapterThreeAction::EvidenceHosePaw:
			return {FName(TEXT("P5.HosePawCompression"))};
		case EIGChapterThreeAction::EvidenceHoseImpact:
			return {
				FName(TEXT("P5.CouplingImpact")),
				FName(TEXT("P5.InnerRimFriction"))};
		case EIGChapterThreeAction::EvidenceWetRung:
			return {
				FName(TEXT("P5.UpperSlipperEnd")),
				FName(TEXT("P5.LiftedPad")),
				FName(TEXT("P5.CorrodedClips"))};
		case EIGChapterThreeAction::EvidenceHandSmear:
			return {FName(TEXT("P5.InwardHandSmear"))};
		case EIGChapterThreeAction::EvidenceCurrentSleeve:
			return {FName(TEXT("P5.CurrentSleeveThreeStitches"))};
		case EIGChapterThreeAction::EvidenceSearchPoster:
			return {FName(TEXT("P5.SearchPosterOutfit"))};
		case EIGChapterThreeAction::EvidenceGlasses:
			return {FName(TEXT("P5.Glasses"))};
		case EIGChapterThreeAction::EvidenceTankClothing:
			return {
				FName(TEXT("P5.TankSleeveThreeStitches")),
				FName(TEXT("P5.TankHeelWear")),
				FName(TEXT("P5.TankOutfit"))};
		case EIGChapterThreeAction::EvidenceBag:
		case EIGChapterThreeAction::None:
		default:
			return {};
		}
	}
}

// ---------------------------------------------------------------------------
// Physical action
// ---------------------------------------------------------------------------

AIGChapterThreeAction::AIGChapterThreeAction()
{
	PrimaryActorTick.bCanEverTick = false;

	PresentationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Presentation"));
	SetRootComponent(PresentationMesh);
	PresentationMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	PresentationMesh->SetGenerateOverlapEvents(false);
	PresentationMesh->SetCanEverAffectNavigation(false);
}

void AIGChapterThreeAction::Configure(
	AIGThirdMorningDirector* InDirector,
	const EIGChapterThreeAction InAction,
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
		FName(TEXT("Interaction.Inspect")), false);
}

void AIGChapterThreeAction::CompleteInteraction_Implementation(
	const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);
	if (Director)
	{
		Director->HandleAction(Action, this);
	}
}

// ---------------------------------------------------------------------------
// Director lifecycle
// ---------------------------------------------------------------------------

AIGThirdMorningDirector::AIGThirdMorningDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CH03Root"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Static);
}

FVector AIGThirdMorningDirector::GetStageOrigin()
{
	return IGThirdMorning::StageOrigin;
}

void AIGThirdMorningDirector::ConfigureAndStart(
	const bool bShowIntroCard,
	const bool bCaptureSequence)
{
	if (bStageBuilt || !GetWorld())
	{
		return;
	}

	bCaptureMode = bCaptureSequence;
	bGreyboxValidationMode =
		FParse::Param(FCommandLine::Get(), TEXT("IGRebirthGreybox"));
	if (bCaptureMode || bGreyboxValidationMode)
	{
		if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
		{
			RebirthState->ResetChapterThreeAttempt();
			// Deterministic documentation/validation routes exercise the full
			// tank reveal. Normal play never receives this item implicitly.
			RebirthState->SetHasMemoryFlashlight(true);
		}
	}
	BuildStage();
	bStageBuilt = true;
	if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
	{
		RebirthState->OnTruthChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleRebirthTruthChanged);
	}
	BootstrapRebirthContext();
	bRestoredChapterThreeProgress = RestoreChapterThreeState();
	ApplyChapterThreeWorldState();
	UpdateEndingAvailability();

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;
	if (Player)
	{
		const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
		const bool bOutfitWasAlreadyEquipped =
			RebirthState
			&& RebirthState->BuildSnapshot().EquippedOutfitChapters.Contains(
				FName(TEXT("CH03")));
		Player->SetRebirthOutfitEquipped(bOutfitWasAlreadyEquipped);
		Player->SetActorLocationAndRotation(
			ToWorld(FVector(35.0f, -55.0f, IGThirdMorning::StandingCapsuleCenter)),
			FRotator(0.0f, -142.0f, 0.0f),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		Player->SetCameraMotionEnabled(true);
		if (UIGFlashlightComponent* Flashlight = Player->GetFlashlight())
		{
			const bool bHasFlashlight = HasMemoryFlashlight();
			Flashlight->SetAvailable(bHasFlashlight);
			Flashlight->SetOn(bHasFlashlight);
		}
	}
	if (PlayerController)
	{
		PlayerController->SetControlRotation(FRotator(-9.0f, -142.0f, 0.0f));
		if (AIGHorrorHUD* HUD = Cast<AIGHorrorHUD>(PlayerController->GetHUD()))
		{
			HUD->SetObjectiveProvider(this);
		}
	}

	IGStory::AddState(
		this,
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH03.Flood.Started")), false));

	if (bGreyboxValidationMode)
	{
		SetPhase(EIGThirdMorningPhase::ApartmentClues);
		GetWorldTimerManager().SetTimer(
			CaptureTimer,
			this,
			&ThisClass::StartRebirthGreyboxValidation,
			1.2f,
			false);
		return;
	}

	if (bCaptureMode)
	{
		// The capture route is also a deterministic smoke test.  It does not
		// wait through the authored opening, but it uses the real stage and
		// interactions rather than a separate mock-up.
		SetPhase(EIGThirdMorningPhase::ApartmentClues);
		GetWorldTimerManager().SetTimer(
			CaptureTimer,
			this,
			&ThisClass::StartCaptureSequence,
			1.2f,
			false);
		return;
	}

	if (bEndingFinished)
	{
		ResumeRestoredEnding();
		return;
	}
	if (bRestoredChapterThreeProgress)
	{
		bAlarmStopped = true;
		TryUnlockApartmentExit();
		SetPhase(
			bTankOpened
				? EIGThirdMorningPhase::TankReveal
				: EIGThirdMorningPhase::ApartmentClues);
		ScheduleAccidentScratch();
		UpdateEndingAvailability();
		return;
	}

	if (bShowIntroCard)
	{
		AIGHorrorHUD::ShowChapterCard(
			this,
			NSLOCTEXT("IGCH03", "IntroEyebrow", "CHAPTER 03"),
			NSLOCTEXT("IGCH03", "IntroTitle", "세 번째 아침"),
			NSLOCTEXT("IGCH03", "IntroSubtitle", "물이 온다"),
			3.2f);
		GetWorldTimerManager().SetTimer(
			FlowTimer,
			this,
			&ThisClass::BeginAwakening,
			3.25f,
			false);
	}
	else
	{
		BeginAwakening();
	}

	UE_LOG(LogIndieGame, Display, TEXT("CH03 third morning stage ready."));
}

void AIGThirdMorningDirector::RestoreCheckpointAnchor(
	const FGameplayTag CheckpointTag)
{
	if (!bStageBuilt || !GetWorld() || !CheckpointTag.IsValid())
	{
		return;
	}

	const FGameplayTag FloodCheckpoint = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Checkpoint.CH03.Flood")),
		false);
	const FGameplayTag ApartmentCheckpoint = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Checkpoint.CH03.Apartment")),
		false);
	const FGameplayTag RoofCheckpoint = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Checkpoint.CH03.Roof")),
		false);
	const bool bRestoreRoof =
		CheckpointTag.MatchesTagExact(RoofCheckpoint);
	const bool bRestoreApartment =
		CheckpointTag.MatchesTagExact(ApartmentCheckpoint);
	if (!bRestoreRoof
		&& !bRestoreApartment
		&& !CheckpointTag.MatchesTagExact(FloodCheckpoint))
	{
		return;
	}

	// ConfigureAndStart may have queued the opening because an early CH03 save
	// contains truths but no P3 bits yet. The checkpoint itself is authoritative:
	// cancel that presentation and restore free exploration without replaying it.
	if (!bEndingFinished)
	{
		GetWorldTimerManager().ClearTimer(FlowTimer);
		StopAllChapterAudio();
	}
	GetWorldTimerManager().ClearTimer(AlarmTimer);
	bRestoredChapterThreeProgress = true;
	bAlarmStopped = true;

	const auto OpenPassage = [](AIGChapterThreeAction* Action)
	{
		if (Action)
		{
			Action->SetActorHiddenInGame(true);
			Action->SetActorEnableCollision(false);
			Action->SetInteractionEnabled(false);
		}
	};
	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	bool bApartmentExitTraversed =
		RebirthState
		&& RebirthState->BuildSnapshot().EquippedOutfitChapters.Contains(
			FName(TEXT("CH03")));
	if (!bRestoreApartment && !bApartmentExitTraversed && RebirthState)
	{
		// Flood/roof checkpoints can only be reached after leaving 404. Keep
		// older saves playable even when they predate the outfit ledger.
		RebirthState->MarkOutfitEquipped(FName(TEXT("CH03")));
		bApartmentExitTraversed = true;
	}
	if (bApartmentExitTraversed)
	{
		OpenPassage(ApartmentDoorAction);
		SetVisibleInteractive(CurrentSleeveAction, true);
	}
	else
	{
		SetVisibleInteractive(ApartmentDoorAction, true);
		TryUnlockApartmentExit();
	}
	if (CorridorEntryZone && !bRestoreApartment)
	{
		CorridorEntryZone->SetActorEnableCollision(false);
	}

	FVector SafeLocalLocation(520.0f, 0.0f, IGThirdMorning::StandingCapsuleCenter);
	FRotator SafeRotation(0.0f, 0.0f, 0.0f);
	FRotator SafeView(-5.0f, 0.0f, 0.0f);
	if (bRestoreRoof)
	{
		OpenPassage(RoofDoorAction);
		RevealUpwardRoute();
		SetDocumentAvailable(P3PhotoNote, true);
		SetDocumentAvailable(ManagementDbNote, true);
		SetDocumentAvailable(PreservationNoticeNote, true);
		SetDocumentAvailable(PoliceChecklistNote, true);
		SetFloodMovement(false);
		if (bEndingFinished)
		{
			SetPhase(EIGThirdMorningPhase::Ending);
		}
		else
		{
			SetPhase(
				bTankOpened
					? EIGThirdMorningPhase::TankReveal
					: EIGThirdMorningPhase::Roof);
		}
		SafeLocalLocation = FVector(
			1815.0f,
			-400.0f,
			IGThirdMorning::RoofFloorZ
				+ IGThirdMorning::StandingCapsuleCenter);
		SafeRotation = FRotator(0.0f, 0.0f, 0.0f);
		SafeView = FRotator(-5.0f, 0.0f, 0.0f);
	}
	else if (bRestoreApartment)
	{
		SetFloodMovement(false);
		SetPhase(
			bTankOpened
				? EIGThirdMorningPhase::TankReveal
				: EIGThirdMorningPhase::ApartmentClues);
		SafeLocalLocation = FVector(
			35.0f,
			-55.0f,
			IGThirdMorning::StandingCapsuleCenter);
		SafeRotation = FRotator(0.0f, -142.0f, 0.0f);
		SafeView = FRotator(-9.0f, -142.0f, 0.0f);
	}
	else
	{
		SetPhase(EIGThirdMorningPhase::FloodedCorridor);
		SetFloodMovement(true);
		StartWaterBed();
		LastPlayerMovingTime = GetWorld()->GetTimeSeconds();
		NextDelayedSplashTime = LastPlayerMovingTime + 7.0;
		GetWorldTimerManager().SetTimer(
			MotionPollTimer,
			this,
			&ThisClass::PollPlayerMotion,
			0.1f,
			true);
		if (!bP3Solved)
		{
			GetWorldTimerManager().SetTimer(
				P3HintTimer,
				this,
				&ThisClass::PollP3Hint,
				1.0f,
				true);
		}
	}

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn)
	{
		if (AIGPlayerCharacter* Player =
				Cast<AIGPlayerCharacter>(PlayerPawn))
		{
			Player->SetRebirthOutfitEquipped(bApartmentExitTraversed);
		}
		PlayerPawn->SetActorLocationAndRotation(
			ToWorld(SafeLocalLocation),
			SafeRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	if (PlayerController)
	{
		PlayerController->SetControlRotation(SafeView);
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StopCameraFade();
		}
	}

	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("CH03 checkpoint reconciled at %s (%s)."),
		*SafeLocalLocation.ToCompactString(),
		*CheckpointTag.ToString());
}

void AIGThirdMorningDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
	{
		RebirthState->OnTruthChanged.RemoveDynamic(
			this,
			&ThisClass::HandleRebirthTruthChanged);
	}
	StopAllChapterAudio();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

FVector AIGThirdMorningDirector::ToWorld(const FVector& LocalLocation) const
{
	return GetActorTransform().TransformPosition(LocalLocation);
}

void AIGThirdMorningDirector::SetPhase(const EIGThirdMorningPhase NewPhase)
{
	Phase = NewPhase;
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("CH03 phase -> %d"),
		static_cast<int32>(Phase));
}

UIGRebirthNarrativeSubsystem* AIGThirdMorningDirector::GetRebirthState() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
}

void AIGThirdMorningDirector::RegisterTruth(
	const TCHAR* TruthName,
	const FName SourceId,
	const bool bConfirmsTruth)
{
	if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
	{
		RebirthState->RegisterTruthSource(
			FGameplayTag::RequestGameplayTag(FName(TruthName), false),
			SourceId,
			bConfirmsTruth);
	}
}

void AIGThirdMorningDirector::RegisterTruth(
	const TCHAR* TruthName,
	const TCHAR* SourceId,
	const bool bConfirmsTruth)
{
	RegisterTruth(TruthName, FName(SourceId), bConfirmsTruth);
}

void AIGThirdMorningDirector::BootstrapRebirthContext()
{
	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	UIGStoryStateSubsystem* LegacyState = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UIGStoryStateSubsystem>()
		: nullptr;
	if (!RebirthState)
	{
		return;
	}

	if (LegacyState)
	{
		const auto LegacyHas = [LegacyState](const TCHAR* TagName)
		{
			return LegacyState->HasState(
				FGameplayTag::RequestGameplayTag(FName(TagName), false));
		};
		if (LegacyHas(TEXT("State.CH02.Loop.ReadDuplicateReceipt")))
		{
			RegisterTruth(
				TEXT("Truth.DeathOverlay"),
				TEXT("CH02.DuplicateReceipt"));
		}
	}

	RebirthState->MarkLocationVisited(FName(TEXT("CH03.Apartment")));
	RebirthState->RecomputeChapterThreeDebt();
}

void AIGThirdMorningDirector::CommitChapterThreeState()
{
	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState)
	{
		return;
	}

	FIGRebirthChapterThreeState State =
		RebirthState->GetChapterThreeState();
	State.P3.bDirectInletClosed = bP3DirectClosed;
	State.P3.bReserveInletClosed = bP3ReserveClosed;
	State.P3.bPressureReleaseOpen = bP3PressureReleaseOpen;
	State.P3.bPressureZero = bP3PressureZero;
	State.P3.bCompleted = bP3Solved;
	State.P3.PressureKPa = P3PressureKPa;
	State.P3.MistakeCount = P3MistakeCount;
	State.P3.ZeroConfirmationTicks = P3ZeroConfirmationTicks;
	State.P3.HintElapsedSeconds = P3HintElapsedSeconds;
	State.P3.HintStage = P3HintStage;
	State.bTankOpened = bTankOpened;
	State.FocusedEvidence =
		IGThirdMorning::ToPersistentEvidence(FocusedEvidence);
	State.ObservedP5Sources = ObservedP5Sources;
	State.AccidentScratchCount = AccidentScratchCount;
	State.bAccidentScratchTailSettled =
		bAccidentScratchTailSettled;
	State.bLookedAwayAfterFirstScratch =
		bLookedAwayAfterFirstScratch;
	State.bActedAfterSecondScratch =
		bActedAfterSecondScratch;
	if (bEndingFinished)
	{
		State.EndingChoice = bEndingASelected
			? EIGRebirthEndingChoice::EndingA
			: EIGRebirthEndingChoice::EndingB;
	}
	RebirthState->SetChapterThreeState(State);
}

void AIGThirdMorningDirector::RequestCheckpointAutosave(
	const TCHAR* CheckpointTagName) const
{
	if (bCaptureMode
		|| bGreyboxValidationMode
		|| !CheckpointTagName
		|| !GetWorld())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UIGSaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UIGSaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem)
	{
		return;
	}

	const FGameplayTag ChapterId = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Chapter.CH03")),
		false);
	const FGameplayTag CheckpointTag = FGameplayTag::RequestGameplayTag(
		FName(CheckpointTagName),
		false);
	if (ChapterId.IsValid() && CheckpointTag.IsValid())
	{
		SaveSubsystem->RequestAutosave(
			ChapterId,
			GetWorld()->GetOutermost()->GetFName(),
			CheckpointTag);
	}
}

bool AIGThirdMorningDirector::RestoreChapterThreeState()
{
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState)
	{
		return false;
	}

	const FIGRebirthChapterThreeState State =
		RebirthState->GetChapterThreeState();
	bP3DirectClosed = State.P3.bDirectInletClosed;
	bP3ReserveClosed = State.P3.bReserveInletClosed;
	bP3PressureReleaseOpen = State.P3.bPressureReleaseOpen;
	bP3PressureZero = State.P3.bPressureZero;
	bP3Solved = State.P3.bCompleted;
	P3PressureKPa = State.P3.PressureKPa;
	P3MistakeCount = State.P3.MistakeCount;
	P3ZeroConfirmationTicks = State.P3.ZeroConfirmationTicks;
	P3HintElapsedSeconds = State.P3.HintElapsedSeconds;
	P3HintStage = State.P3.HintStage;
	bTankOpened = State.bTankOpened;
	FocusedEvidence =
		IGThirdMorning::FromPersistentEvidence(State.FocusedEvidence);
	ObservedP5Sources = State.ObservedP5Sources;
	AccidentScratchCount = State.AccidentScratchCount;
	bAccidentScratchTailSettled =
		State.bAccidentScratchTailSettled;
	bLookedAwayAfterFirstScratch =
		State.bLookedAwayAfterFirstScratch;
	bActedAfterSecondScratch =
		State.bActedAfterSecondScratch;
	bEndingFinished =
		State.EndingChoice != EIGRebirthEndingChoice::None;
	bEndingASelected =
		State.EndingChoice == EIGRebirthEndingChoice::EndingA;

	return bP3DirectClosed
		|| bP3ReserveClosed
		|| bP3PressureReleaseOpen
		|| bP3PressureZero
		|| bP3Solved
		|| P3MistakeCount > 0
		|| P3ZeroConfirmationTicks > 0
		|| bTankOpened
		|| FocusedEvidence != EIGChapterThreeAction::None
		|| !ObservedP5Sources.IsEmpty()
		|| AccidentScratchCount > 0
		|| bEndingFinished;
}

void AIGThirdMorningDirector::ApplyChapterThreeWorldState()
{
	const auto ApplyClosedValve = [](AIGChapterThreeAction* Action)
	{
		if (Action)
		{
			Action->SetInteractionPrompt(
				NSLOCTEXT("IGCH03", "P3RestoredValveClosed", "잠김"));
			Action->SetInteractionEnabled(false);
		}
	};
	if (bP3DirectClosed)
	{
		ApplyClosedValve(P3DirectInletAction);
	}
	if (bP3ReserveClosed)
	{
		ApplyClosedValve(P3ReserveInletAction);
	}
	if (bP3PressureReleaseOpen && P3PressureReleaseAction)
	{
		P3PressureReleaseAction->SetInteractionPrompt(
			NSLOCTEXT("IGCH03", "P3RestoredBleedOpen", "압력 해제 열림"));
		P3PressureReleaseAction->SetInteractionEnabled(false);
	}
	if (P3PressureText)
	{
		P3PressureText->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f kPa"), P3PressureKPa)));
	}
	if (P3PressureNeedle)
	{
		const float Alpha = 1.0f - P3PressureKPa / 60.0f;
		P3PressureNeedle->SetRelativeRotation(
			FRotator(0, 0, FMath::Lerp(-55.0f, 55.0f, Alpha)));
	}
	if (P3BleedWaterVisual)
	{
		P3BleedWaterVisual->SetVisibility(
			bP3PressureReleaseOpen && !bP3PressureZero);
		const float FlowScale = FMath::Clamp(
			P3PressureKPa / 60.0f,
			0.08f,
			1.0f);
		P3BleedWaterVisual->SetRelativeScale3D(
			FVector(0.62f * FlowScale, 0.038f, 0.038f));
	}
	if (bP3PressureZero && P3FloorDrainAction)
	{
		P3FloorDrainAction->SetInteractionPrompt(
			bP3Solved
				? NSLOCTEXT("IGCH03", "P3RestoredDrainOpen", "바닥 배수 열림")
				: NSLOCTEXT("IGCH03", "P3RestoredDrainReady", "0 확인 후 바닥 배수 열기"));
		P3FloorDrainAction->SetInteractionEnabled(!bP3Solved);
	}
	if (bP3PressureReleaseOpen && !bP3PressureZero && !bP3Solved)
	{
		// The valve-opening impact is a one-shot. Restore only the deterministic
		// gauge countdown from the exact saved pressure.
		GetWorldTimerManager().SetTimer(
			P3PressureTimer,
			this,
			&ThisClass::UpdateP3PressureRelease,
			0.1f,
			true);
	}
	if (CorridorWaterVisual)
	{
		FVector WaterLocation = CorridorWaterVisual->GetRelativeLocation();
		WaterLocation.Z = bP3Solved
			? 2.0f
			: 2.0f + P3MistakeCount * 5.0f;
		CorridorWaterVisual->SetRelativeLocation(WaterLocation);
	}
	if (bP3Solved)
	{
		SetDocumentAvailable(P3RecordNote, true);
	}
	if (SearchPosterAction)
	{
		SearchPosterAction->SetActorLocation(ToWorld(
			bP3Solved
				? FVector(992.0f, -84.0f, 150.0f)
				: FVector(1178.0f, -185.0f, 188.0f)));
	}
	const bool bReachedRoof = IGStory::HasState(
		this,
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH03.Flood.ReachedRoof")),
			false));
	if (bReachedRoof || bTankOpened || bEndingFinished)
	{
		SetDocumentAvailable(P3PhotoNote, true);
		SetDocumentAvailable(ManagementDbNote, true);
		SetDocumentAvailable(PreservationNoticeNote, true);
		SetDocumentAvailable(PoliceChecklistNote, true);
	}

	if (bTankOpened)
	{
		if (TankLidAction)
		{
			TankLidAction->SetActorHiddenInGame(true);
			TankLidAction->SetInteractionEnabled(false);
			TankLidAction->SetActorEnableCollision(false);
		}
	}
	ApplyTankRevealVisibility();
	ArmFlashlightReturnReveal();
	SetVisibleInteractive(MemoryFlashlightAction, !HasMemoryFlashlight());
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	const bool bChapterThreeOutfitEquipped =
		RebirthState
		&& RebirthState->BuildSnapshot().EquippedOutfitChapters.Contains(
			FName(TEXT("CH03")));
	SetVisibleInteractive(
		CurrentSleeveAction,
		bChapterThreeOutfitEquipped);
	RefreshEvidencePrompts();

	if (bEndingFinished)
	{
		ApplyCommonDiscoveryWorldState();
	}
}

void AIGThirdMorningDirector::ApplyCommonDiscoveryWorldState()
{
	for (UStaticMeshComponent* Piece : BodySilhouette)
	{
		if (Piece)
		{
			Piece->SetVisibility(false);
		}
	}
	if (TankLidAction)
	{
		TankLidAction->SetActorHiddenInGame(false);
		TankLidAction->SetActorLocation(
			ToWorld(IGThirdMorning::TankCenter + FVector(0, 0, 610)));
		TankLidAction->SetActorRotation(FRotator::ZeroRotator);
		TankLidAction->SetInteractionEnabled(false);
		TankLidAction->SetActorEnableCollision(false);
	}
	SetVisibleInteractive(CloseChoiceAction, false);
	SetVisibleInteractive(SupportChoiceAction, false);
}

void AIGThirdMorningDirector::ResumeRestoredEnding()
{
	StopAllChapterAudio();
	SetPhase(EIGThirdMorningPhase::Ending);
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState)
	{
		return;
	}

	const FIGRebirthChapterThreeState State =
		RebirthState->GetChapterThreeState();
	if (!State.bCommonDiscoveryCommitted)
	{
		// A pre-card save must still show the shared physical outcome exactly
		// once. Branch-selection impacts and rings are not replayed.
		ShowCommonDiscoveryCard();
		return;
	}

	ApplyCommonDiscoveryWorldState();
	if (!bEndingASelected)
	{
		FinishEndingB();
		return;
	}

	if (RebirthState->WasOneShotBeatPlayed(
		FName(TEXT("Ending.A.StrongCuePlayed"))))
	{
		PresentEndingControls(
			NSLOCTEXT("IGCH03", "EndingARestoredTitle", "내일 또"),
			NSLOCTEXT(
				"IGCH03",
				"EndingARestoredSubtitle",
				"2024년 7월 26일 금요일, 오전 4시 44분."));
	}
	else
	{
		// The common card was committed but the branch cue was not. Resume
		// after the card and let its guarded cue run once.
		FinishEndingAAfterDiscovery();
	}
}

void AIGThirdMorningDirector::HandleRebirthTruthChanged(
	FGameplayTag TruthTag,
	FName SourceId,
	bool bConfirmed)
{
	(void)TruthTag;
	(void)SourceId;
	(void)bConfirmed;
	if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
	{
		RebirthState->RecomputeChapterThreeDebt();
	}
	UpdateEndingAvailability();
}

// ---------------------------------------------------------------------------
// Stage assembly
// ---------------------------------------------------------------------------

void AIGThirdMorningDirector::BuildStage()
{
	CubeMesh = IGThirdMorning::LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CylinderMesh = IGThirdMorning::LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	SphereMesh = IGThirdMorning::LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	WaterBottleMesh = IGThirdMorning::LoadMesh(TEXT("/Game/Meshes/SM_WaterBottle.SM_WaterBottle"));
	BottleCapMesh = IGThirdMorning::LoadMesh(TEXT("/Game/Meshes/SM_BottleCap.SM_BottleCap"));
	LabelSleeveMesh = IGThirdMorning::LoadMesh(TEXT("/Game/Meshes/SM_LabelSleeve.SM_LabelSleeve"));

	ConcreteMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_Concrete.M_Concrete"));
	DarkConcreteMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_ConcreteDark.M_ConcreteDark"));
	RoomFloorMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_RoomFloor.M_RoomFloor"));
	WallMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_RoomWall.M_RoomWall"));
	MetalMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_MetalUV.M_MetalUV"));
	PlasticMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_PlasticDark.M_PlasticDark"));
	PaperMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_PaperOld.M_PaperOld"));
	WetPaperMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_PaperWet.M_PaperWet"));
	WaterMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_WaterBlue.M_WaterBlue"));
	WetStepMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_WetStep.M_WetStep"));
	ScreenMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_ScreenGlow.M_ScreenGlow"));
	BeddingMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_BeddingUV.M_BeddingUV"));
	GlassMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_Glass.M_Glass"));
	BottleCapMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_SnackBlue.M_SnackBlue"));
	WaterLabelMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_LabelWater.M_LabelWater"));
	FridgeBodyMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_FridgeBody.M_FridgeBody"));
	FridgeInteriorMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_FridgeInterior.M_FridgeInterior"));
	EmergencyMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_Alarm.M_Alarm"));

	// Missing cooked materials degrade to the engine default but never prevent
	// the chapter from being playable.
	UMaterialInterface* DefaultMaterial =
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	auto FallBack = [DefaultMaterial](TObjectPtr<UMaterialInterface>& Material)
	{
		if (!Material)
		{
			Material = DefaultMaterial;
		}
	};
	FallBack(ConcreteMaterial);
	FallBack(DarkConcreteMaterial);
	FallBack(RoomFloorMaterial);
	FallBack(WallMaterial);
	FallBack(MetalMaterial);
	FallBack(PlasticMaterial);
	FallBack(PaperMaterial);
	FallBack(WetPaperMaterial);
	FallBack(WaterMaterial);
	FallBack(WetStepMaterial);
	FallBack(ScreenMaterial);
	FallBack(BeddingMaterial);
	FallBack(GlassMaterial);
	FallBack(BottleCapMaterial);
	FallBack(WaterLabelMaterial);
	FallBack(FridgeBodyMaterial);
	FallBack(FridgeInteriorMaterial);
	FallBack(EmergencyMaterial);

	BuildApartment();
	BuildFloodedCorridor();
	BuildP3ServiceCabinet();
	BuildLoopingStairwell();
	BuildFifthFloorAndRoof();
	BuildWaterTank();
	BuildP5AccidentEvidence();
	SpawnClueDocuments();
	SpawnTriggers();
}

UStaticMeshComponent* AIGThirdMorningDirector::CreateBlock(
	const FVector& LocalCenter,
	const FVector& SizeCentimeters,
	UMaterialInterface* Material,
	const bool bCollision,
	const FRotator& Rotation,
	UStaticMesh* MeshOverride,
	const bool bMovable)
{
	UStaticMesh* Mesh = MeshOverride ? MeshOverride : CubeMesh.Get();
	if (!Mesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("CH03_Block_%03d"), Geometry.Num()));
	Component->SetupAttachment(SceneRoot);
	Component->SetStaticMesh(Mesh);
	Component->SetMaterial(0, Material);
	Component->SetRelativeLocation(LocalCenter);
	Component->SetRelativeRotation(Rotation);
	Component->SetRelativeScale3D(SizeCentimeters / 100.0f);
	// Most of the chapter shell is static. The few authored feedback parts
	// request Movable before registration so pressure, bleed and water-level
	// transforms remain valid in a game world.
	Component->SetMobility(
		bMovable
			? EComponentMobility::Movable
			: EComponentMobility::Static);
	Component->SetCollisionProfileName(
		bCollision
			? UCollisionProfile::BlockAll_ProfileName
			: UCollisionProfile::NoCollision_ProfileName);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(SizeCentimeters.GetMin() > 2.0f);
	Component->RegisterComponent();
	Geometry.Add(Component);
	return Component;
}

UPointLightComponent* AIGThirdMorningDirector::CreatePointLight(
	const FVector& LocalLocation,
	const float Intensity,
	const float Radius,
	const FLinearColor& Color,
	const bool bCastShadows)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(
		this,
		*FString::Printf(TEXT("CH03_Light_%02d"), Lights.Num()));
	Light->SetupAttachment(SceneRoot);
	Light->SetRelativeLocation(LocalLocation);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(Radius);
	Light->SetLightColor(Color);
	Light->SetCastShadows(bCastShadows);
	Light->SetSourceRadius(8.0f);
	Light->SetSoftSourceRadius(18.0f);
	Light->RegisterComponent();
	Lights.Add(Light);
	return Light;
}

AIGChapterThreeAction* AIGThirdMorningDirector::SpawnAction(
	const EIGChapterThreeAction Action,
	const FVector& LocalLocation,
	const FVector& SizeCentimeters,
	UMaterialInterface* Material,
	const FText& Prompt,
	const float HoldSeconds,
	const FRotator& Rotation,
	UStaticMesh* MeshOverride)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIGChapterThreeAction* Target = GetWorld()->SpawnActor<AIGChapterThreeAction>(
		AIGChapterThreeAction::StaticClass(),
		FTransform(Rotation, ToWorld(LocalLocation)),
		Parameters);
	if (Target)
	{
		Target->Configure(
			this,
			Action,
			MeshOverride ? MeshOverride : CubeMesh.Get(),
			Material,
			SizeCentimeters,
			Prompt,
			HoldSeconds);
	}
	return Target;
}

AIGReadableNote* AIGThirdMorningDirector::SpawnNote(
	const FVector& LocalLocation,
	const FRotator& Rotation,
	const FVector& PaperSize,
	const FText& Prompt,
	const FText& Title,
	TArray<FText> BodyLines,
	UMaterialInterface* InPaperMaterial)
{
	if (!GetWorld() || !CubeMesh)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIGReadableNote* Note = GetWorld()->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(Rotation, ToWorld(LocalLocation)),
		Parameters);
	if (Note)
	{
		Note->ConfigurePrototypeVisuals(
			CubeMesh,
			InPaperMaterial ? InPaperMaterial : PaperMaterial.Get(),
			PaperSize);
		Note->SetInteractionPrompt(Prompt);
		Note->SetNoteText(Title, MoveTemp(BodyLines));
		Note->OnReadStateChanged.AddUniqueDynamic(
			this, &ThisClass::HandleClueRead);
	}
	return Note;
}

AIGZoneTrigger* AIGThirdMorningDirector::SpawnZone(
	const FVector& LocalLocation,
	const FVector& HalfExtent)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	const FTransform ZoneTransform(
		FRotator::ZeroRotator,
		ToWorld(LocalLocation));
	AIGZoneTrigger* Zone = GetWorld()->SpawnActorDeferred<AIGZoneTrigger>(
		AIGZoneTrigger::StaticClass(),
		ZoneTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Zone)
	{
		Zone->SetZoneExtent(HalfExtent);
		Zone->FinishSpawning(ZoneTransform);
	}
	return Zone;
}

void AIGThirdMorningDirector::BuildApartment()
{
	// 6.8 m x 4.4 m one-room, scaled like the established 404 set.
	CreateBlock(FVector(0, 0, -10), FVector(700, 440, 20), RoomFloorMaterial);
	CreateBlock(FVector(0, -220, 130), FVector(700, 20, 280), WallMaterial);
	CreateBlock(FVector(0, 220, 130), FVector(700, 20, 280), WallMaterial);
	CreateBlock(FVector(-350, 0, 130), FVector(20, 440, 280), WallMaterial);
	CreateBlock(FVector(0, 0, 270), FVector(700, 440, 20), WallMaterial);
	// East wall leaves a 110 cm Korean apartment doorway.
	CreateBlock(FVector(350, -165, 130), FVector(20, 110, 280), WallMaterial);
	CreateBlock(FVector(350, 165, 130), FVector(20, 110, 280), WallMaterial);
	CreateBlock(FVector(350, 0, 250), FVector(20, 110, 40), WallMaterial);

	// Bed and tired possessions: the same person, but a wetter room.
	CreateBlock(FVector(-105, -125, 18), FVector(205, 105, 36), PlasticMaterial);
	CreateBlock(FVector(-105, -125, 39), FVector(194, 96, 10), BeddingMaterial);
	CreateBlock(FVector(-185, -125, 57), FVector(48, 82, 28), BeddingMaterial);
	CreateBlock(FVector(-225, -190, 35), FVector(58, 45, 70), PlasticMaterial);
	CreateBlock(FVector(-210, -190, 75), FVector(25, 18, 12), PlasticMaterial);
	CreateBlock(FVector(-200, -190, 80), FVector(22, 3, 6), ScreenMaterial, false);

	// A single ceiling leak and the stain beneath it.
	CreateBlock(FVector(45, -20, 0.8f), FVector(82, 54, 1.5f), WetStepMaterial, false);
	CreateBlock(FVector(45, -20, 266), FVector(18, 18, 3), WetStepMaterial, false);

	// Fridge carcass.  The contents are pre-built and merely revealed when the
	// door moves; no bottle allocations occur during the scare.  The pale
	// enamel cavity and familiar PET silhouette keep this domestic rather than
	// reading as a shelf of debug cylinders.
	CreateBlock(
		FVector(-286, 115, 92),
		FVector(105, 126, 184),
		FridgeBodyMaterial);
	CreateBlock(
		FVector(-239, 115, 92),
		FVector(4, 112, 172),
		FridgeInteriorMaterial,
		false);
	CreateBlock(
		FVector(-232, 60, 92),
		FVector(18, 6, 172),
		FridgeInteriorMaterial,
		false);
	CreateBlock(
		FVector(-232, 170, 92),
		FVector(18, 6, 172),
		FridgeInteriorMaterial,
		false);

	// A layered enamel frame and dark rubber gasket keep the front edge from
	// reading as a freestanding display shelf once the interaction door hides.
	CreateBlock(
		FVector(-219, 61, 94),
		FVector(8, 8, 176),
		FridgeBodyMaterial,
		false);
	CreateBlock(
		FVector(-219, 169, 94),
		FVector(8, 8, 176),
		FridgeBodyMaterial,
		false);
	CreateBlock(
		FVector(-219, 115, 8),
		FVector(8, 116, 8),
		FridgeBodyMaterial,
		false);
	CreateBlock(
		FVector(-219, 115, 180),
		FVector(8, 116, 8),
		FridgeBodyMaterial,
		false);
	CreateBlock(
		FVector(-213, 66, 93),
		FVector(3, 4, 156),
		PlasticMaterial,
		false);
	CreateBlock(
		FVector(-213, 164, 93),
		FVector(3, 4, 156),
		PlasticMaterial,
		false);
	CreateBlock(
		FVector(-213, 115, 17),
		FVector(3, 96, 4),
		PlasticMaterial,
		false);
	CreateBlock(
		FVector(-213, 115, 169),
		FVector(3, 96, 4),
		PlasticMaterial,
		false);

	// The opened door is pre-assembled and revealed with the bottles.  Its
	// liner, seal, bins and edge handle make this read as a domestic fridge
	// without allocating or moving components during the scare.
	auto HideUntilFridgeOpened = [this](UStaticMeshComponent* Part)
	{
		if (Part)
		{
			Part->SetVisibility(false);
			FridgeContents.Add(Part);
		}
	};
	HideUntilFridgeOpened(CreateBlock(
		FVector(-158, 174, 94),
		FVector(116, 8, 180),
		FridgeBodyMaterial,
		false));
	HideUntilFridgeOpened(CreateBlock(
		FVector(-158, 168, 94),
		FVector(104, 3, 166),
		FridgeInteriorMaterial,
		false));
	HideUntilFridgeOpened(CreateBlock(
		FVector(-158, 165, 14),
		FVector(104, 3, 5),
		PlasticMaterial,
		false));
	HideUntilFridgeOpened(CreateBlock(
		FVector(-158, 165, 174),
		FVector(104, 3, 5),
		PlasticMaterial,
		false));
	HideUntilFridgeOpened(CreateBlock(
		FVector(-207, 165, 94),
		FVector(5, 3, 156),
		PlasticMaterial,
		false));
	HideUntilFridgeOpened(CreateBlock(
		FVector(-109, 165, 94),
		FVector(5, 3, 156),
		PlasticMaterial,
		false));
	for (const float BinZ : {46.0f, 94.0f, 142.0f})
	{
		HideUntilFridgeOpened(CreateBlock(
			FVector(-158, 156, BinZ),
			FVector(92, 20, 5),
			FridgeInteriorMaterial,
			false));
		HideUntilFridgeOpened(CreateBlock(
			FVector(-158, 147, BinZ + 12.0f),
			FVector(92, 4, 24),
			FridgeInteriorMaterial,
			false));
	}
	HideUntilFridgeOpened(CreateBlock(
		FVector(-102, 172, 104),
		FVector(5, 10, 100),
		MetalMaterial,
		false));

	auto CreateHiddenWaterBottle = [this](const FVector& BottleLocation)
	{
		UStaticMeshComponent* BottleBody = CreateBlock(
			BottleLocation,
			FVector(100.0f),
			GlassMaterial,
			false,
			FRotator::ZeroRotator,
			WaterBottleMesh ? WaterBottleMesh.Get() : CylinderMesh.Get());
		UStaticMeshComponent* BottleCap = CreateBlock(
			BottleLocation + FVector(0, 0, 20.1f),
			FVector(100.0f),
			BottleCapMaterial,
			false,
			FRotator::ZeroRotator,
			BottleCapMesh ? BottleCapMesh.Get() : CylinderMesh.Get());
		UStaticMeshComponent* BottleLabel = CreateBlock(
			BottleLocation + FVector(0, 0, 5.0f),
			// CreateBlock registers static geometry after applying this authored
			// sleeve scale, so no static transform changes at runtime.
			FVector(336.0f, 336.0f, 860.0f),
			WaterLabelMaterial,
			false,
			FRotator(0, -90, 0),
			LabelSleeveMesh ? LabelSleeveMesh.Get() : CylinderMesh.Get());
		for (UStaticMeshComponent* BottlePart : {BottleBody, BottleCap, BottleLabel})
		{
			if (BottlePart)
			{
				BottlePart->SetVisibility(false);
				FridgeContents.Add(BottlePart);
			}
		}
	};

	// The door bins are part of the reveal too. Leaving them empty made the
	// supposedly impossible "full fridge" look like an ordinary half-stocked
	// appliance from the documentation camera.
	for (int32 BinShelf = 0; BinShelf < 3; ++BinShelf)
	{
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			CreateHiddenWaterBottle(FVector(
				-195.0f + Slot * 25.0f,
				154.0f,
				47.5f + BinShelf * 48.0f));
		}
	}

	for (int32 Shelf = 0; Shelf < 4; ++Shelf)
	{
		CreateBlock(
			FVector(-224, 115, 38.0f + Shelf * 38.0f),
			FVector(48, 108, 2),
			MetalMaterial,
			false);
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Column = 0; Column < 8; ++Column)
			{
				const FVector BottleLocation(
					-241.0f + Row * 16.5f,
					67.5f + Column * 13.5f + (Row % 2) * 1.5f,
					39.5f + Shelf * 38.0f);
				CreateHiddenWaterBottle(BottleLocation);
			}
		}
	}
	FridgeInteriorLightPanel = CreateBlock(
		FVector(-215, 115, 173),
		FVector(3, 72, 5),
		FridgeInteriorMaterial,
		false);
	if (FridgeInteriorLightPanel)
	{
		FridgeInteriorLightPanel->SetVisibility(false);
	}
	FridgeInteriorLight = CreatePointLight(
		FVector(-202, 115, 144),
		165.0f,
		210.0f,
		FLinearColor(0.70f, 0.79f, 0.90f),
		false);
	if (FridgeInteriorLight)
	{
		FridgeInteriorLight->SetVisibility(false);
	}
	FridgeDoorAction = SpawnAction(
		EIGChapterThreeAction::OpenFridge,
		FVector(-218, 115, 94),
		FVector(8, 120, 180),
		FridgeBodyMaterial,
		NSLOCTEXT("IGCH03", "OpenFullFridge", "냉장고 열기"));

	// The same flashlight that could be taken in CH02 remains on the low
	// shoe cabinet. It is a real recovery route, not an automatic CH03 grant.
	CreateBlock(
		FVector(275, 145, 42),
		FVector(105, 58, 84),
		PlasticMaterial);
	// A real mirror-height comparison point lets the player inspect their own
	// repaired left sleeve before reaching the body. It is hidden until the
	// CH03 outfit is actually equipped at the apartment threshold.
	CreateBlock(
		FVector(275, 214, 155),
		FVector(78, 4, 98),
		PlasticMaterial,
		false);
	CurrentSleeveAction = SpawnAction(
		EIGChapterThreeAction::EvidenceCurrentSleeve,
		FVector(275, 211, 155),
		FVector(70, 3, 90),
		GlassMaterial,
		NSLOCTEXT(
			"IGCH03",
			"CurrentSleevePrompt",
			"거울에 왼쪽 소매 수선 확인하기"));
	if (CurrentSleeveAction)
	{
		EvidenceActions.Add(
			EIGChapterThreeAction::EvidenceCurrentSleeve,
			CurrentSleeveAction);
		SetVisibleInteractive(CurrentSleeveAction, false);
	}
	MemoryFlashlightAction = SpawnAction(
		EIGChapterThreeAction::RecoverFlashlight,
		FVector(275, 145, 91),
		FVector(6, 6, 19),
		PlasticMaterial,
		NSLOCTEXT("IGCH03", "RecoverFlashlightPrompt", "현관 손전등 챙기기"),
		0.25f,
		FRotator(90, 0, 0),
		CylinderMesh);
	SetVisibleInteractive(MemoryFlashlightAction, !HasMemoryFlashlight());

	// Desk under the window: planner note is placed later so it can own text.
	CreateBlock(FVector(80, 150, 38), FVector(145, 62, 8), PlasticMaterial);
	CreateBlock(FVector(25, 150, 18), FVector(8, 54, 36), PlasticMaterial);
	CreateBlock(FVector(135, 150, 18), FVector(8, 54, 36), PlasticMaterial);

	ApartmentDoorAction = SpawnAction(
		EIGChapterThreeAction::OpenApartmentDoor,
		FVector(348, 0, 112),
		FVector(8, 106, 224),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "ApartmentDoorLockedPrompt", "현관문 확인하기"));

	CreatePointLight(
		FVector(-30, 0, 225),
		465.0f,
		430.0f,
		FLinearColor(0.47f, 0.56f, 0.68f),
		true);
	CreatePointLight(
		FVector(65, 145, 138),
		95.0f,
		280.0f,
		FLinearColor(0.62f, 0.42f, 0.28f),
		false);
}

void AIGThirdMorningDirector::BuildFloodedCorridor()
{
	// The wet 4F corridor starts at the apartment threshold and ends at the
	// stairwell.  Water is visual/non-colliding; the dry floor carries physics.
	CreateBlock(FVector(675, 0, -10), FVector(650, 440, 20), ConcreteMaterial);
	CreateBlock(FVector(675, -220, 130), FVector(650, 20, 280), DarkConcreteMaterial);
	CreateBlock(FVector(675, 220, 130), FVector(650, 20, 280), DarkConcreteMaterial);
	CreateBlock(FVector(675, 0, 270), FVector(650, 440, 20), DarkConcreteMaterial);
	CorridorWaterVisual = CreateBlock(
		FVector(675, 0, 2),
		FVector(640, 430, 3),
		WaterMaterial,
		false,
		FRotator::ZeroRotator,
		nullptr,
		true);

	// Dead elevator: black COP, shut steel leaves, water bleeding from the sill.
	CreateBlock(FVector(690, 210, 112), FVector(210, 12, 224), MetalMaterial);
	CreateBlock(FVector(690, 201, 110), FVector(4, 3, 205), DarkConcreteMaterial, false);
	ElevatorAction = SpawnAction(
		EIGChapterThreeAction::InspectElevator,
		FVector(812, 204, 116),
		FVector(18, 5, 36),
		PlasticMaterial,
		NSLOCTEXT("IGCH03", "DeadLiftPrompt", "엘리베이터 호출하기"));
	CreateBlock(FVector(690, 188, 2.5f), FVector(170, 32, 3), WetStepMaterial, false);

	for (int32 LightIndex = 0; LightIndex < 3; ++LightIndex)
	{
		const float X = 455.0f + LightIndex * 230.0f;
		CreateBlock(
			FVector(X, 0, 250),
			FVector(68, 20, 4),
			ScreenMaterial,
			false);
		CreatePointLight(
			FVector(X, 0, 235),
			145.0f,
			260.0f,
			FLinearColor(0.18f, 0.48f, 0.33f),
			true);
	}
}

void AIGThirdMorningDirector::BuildP3ServiceCabinet()
{
	// A single open cabinet keeps every P3 state readable in one view.
	// The broken latch and the two missing screws are physical continuity
	// clues, not a lock the player has to solve.
	CreateBlock(
		FVector(675, -208, 132),
		FVector(270, 12, 196),
		DarkConcreteMaterial,
		false);
	CreateBlock(FVector(675, -196, 224), FVector(270, 18, 10), MetalMaterial, false);
	CreateBlock(FVector(675, -196, 40), FVector(270, 18, 10), MetalMaterial, false);
	CreateBlock(FVector(545, -196, 132), FVector(10, 18, 194), MetalMaterial, false);
	CreateBlock(FVector(805, -196, 132), FVector(10, 18, 194), MetalMaterial, false);
	CreateBlock(
		FVector(810, -186, 126),
		FVector(8, 32, 42),
		MetalMaterial,
		false,
		FRotator(0, 0, -12));
	CreateBlock(FVector(812, -178, 154), FVector(8, 8, 4), WetStepMaterial, false);
	CreateBlock(FVector(812, -178, 98), FVector(8, 8, 4), WetStepMaterial, false);

	P3DirectInletAction = SpawnAction(
		EIGChapterThreeAction::P3CloseDirectInlet,
		FVector(600, -178, 164),
		FVector(34, 34, 8),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "P3DirectPrompt", "직결 급수 잠그기"),
		0.45f,
		FRotator(90, 0, 0),
		CylinderMesh);
	P3ReserveInletAction = SpawnAction(
		EIGChapterThreeAction::P3CloseReserveInlet,
		FVector(650, -178, 164),
		FVector(34, 34, 8),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "P3ReservePrompt", "예비조 잠그기"),
		0.45f,
		FRotator(90, 0, 0),
		CylinderMesh);
	P3PressureReleaseAction = SpawnAction(
		EIGChapterThreeAction::P3OpenPressureRelease,
		FVector(705, -178, 112),
		FVector(24, 24, 7),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "P3BleedPrompt", "압력 해제 열기"),
		0.55f,
		FRotator(90, 0, 0),
		CylinderMesh);
	P3FloorDrainAction = SpawnAction(
		EIGChapterThreeAction::P3OpenFloorDrain,
		FVector(760, -178, 112),
		FVector(38, 38, 8),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "P3DrainPrompt", "바닥 배수 열기"),
		0.75f,
		FRotator(90, 0, 0),
		CylinderMesh);

	CreateBlock(FVector(600, -190, 150), FVector(12, 12, 74), MetalMaterial, false);
	CreateBlock(FVector(650, -190, 150), FVector(12, 12, 74), MetalMaterial, false);
	CreateBlock(FVector(705, -190, 95), FVector(8, 8, 52), MetalMaterial, false);
	CreateBlock(FVector(760, -190, 95), FVector(16, 16, 52), MetalMaterial, false);
	P3BleedTubeVisual = CreateBlock(
		FVector(705, -190, 62),
		FVector(70, 7, 7),
		GlassMaterial,
		false);
	P3BleedWaterVisual = CreateBlock(
		FVector(705, -189, 62),
		FVector(62, 3.8f, 3.8f),
		WaterMaterial,
		false,
		FRotator::ZeroRotator,
		nullptr,
		true);
	if (P3BleedWaterVisual)
	{
		P3BleedWaterVisual->SetVisibility(false);
	}

	P3PressureNeedle = CreateBlock(
		FVector(755, -178, 178),
		FVector(4, 30, 4),
		EmergencyMaterial,
		false,
		FRotator(0, 0, -55),
		nullptr,
		true);
	CreateBlock(
		FVector(755, -188, 178),
		FVector(54, 8, 54),
		GlassMaterial,
		false,
		FRotator(90, 0, 0),
		CylinderMesh);

	P3PressureText = NewObject<UTextRenderComponent>(
		this,
		TEXT("CH03_P3PressureText"));
	P3PressureText->SetupAttachment(SceneRoot);
	P3PressureText->SetRelativeLocation(FVector(755, -171, 145));
	P3PressureText->SetRelativeRotation(FRotator(0, 90, 0));
	P3PressureText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	P3PressureText->SetWorldSize(12.0f);
	P3PressureText->SetTextRenderColor(FColor(188, 204, 190));
	P3PressureText->SetText(FText::FromString(TEXT("60 kPa")));
	P3PressureText->RegisterComponent();

	UTextRenderComponent* Labels = NewObject<UTextRenderComponent>(
		this,
		TEXT("CH03_P3Labels"));
	Labels->SetupAttachment(SceneRoot);
	Labels->SetRelativeLocation(FVector(680, -171, 202));
	Labels->SetRelativeRotation(FRotator(0, 90, 0));
	Labels->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	Labels->SetWorldSize(10.0f);
	Labels->SetTextRenderColor(FColor(162, 176, 164));
	Labels->SetText(FText::FromString(
		TEXT("DIRECT  RESERVE\nPRESSURE RELEASE  FLOOR DRAIN")));
	Labels->RegisterComponent();
}

void AIGThirdMorningDirector::BuildLoopingStairwell()
{
	// Entry landing.
	CreateBlock(FVector(1085, 0, -10), FVector(200, 260, 20), ConcreteMaterial);
	CreateBlock(FVector(975, 0, 130), FVector(20, 760, 280), DarkConcreteMaterial);
	CreateBlock(FVector(1210, 180, 20), FVector(20, 500, 520), DarkConcreteMaterial);

	// Down flight.  Every tread has a constant bottom so collision remains
	// stable and there are no thin physics slivers.
	for (int32 StepIndex = 0; StepIndex < 12; ++StepIndex)
	{
		const float TopZ = -15.0f * StepIndex;
		const float Height = 260.0f + TopZ;
		CreateBlock(
			FVector(1085, 45.0f + StepIndex * 29.0f, TopZ - Height * 0.5f),
			FVector(200, 30, Height),
			ConcreteMaterial);
	}
	CreateBlock(FVector(1265, 382, -190), FVector(560, 130, 20), ConcreteMaterial);
	CreateBlock(FVector(1265, 447, -55), FVector(560, 20, 290), DarkConcreteMaterial);
	CreateBlock(FVector(1265, 317, -55), FVector(560, 20, 290), DarkConcreteMaterial);
	CreateBlock(FVector(1545, 382, -55), FVector(20, 150, 290), DarkConcreteMaterial);

	// The third loop makes the lower route physically unreadable: a waist-high
	// sheet of water occupies the first tread while the opposite gate opens.
	DownRouteWaterBarrier = CreateBlock(
		FVector(1085, 48, 78),
		FVector(205, 18, 156),
		WaterMaterial,
		true);
	if (DownRouteWaterBarrier)
	{
		DownRouteWaterBarrier->SetVisibility(false);
		DownRouteWaterBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Up flight, initially hidden behind a maintenance gate.
	for (int32 StepIndex = 0; StepIndex < 12; ++StepIndex)
	{
		const float TopZ = 15.0f * (StepIndex + 1);
		CreateBlock(
			FVector(1085, -45.0f - StepIndex * 29.0f, TopZ * 0.5f),
			FVector(200, 30, TopZ),
			ConcreteMaterial);
	}
	UpRouteGate = CreateBlock(
		FVector(1085, -42, 118),
		FVector(205, 16, 236),
		MetalMaterial,
		true);
	if (UpRouteGate)
	{
		// REBIRTH permits the player to follow the water upward immediately.
		// The lower loop remains optional evidence instead of a three-pass key.
		UpRouteGate->SetVisibility(false);
		UpRouteGate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	CreateBlock(
		FVector(1110, -30, 252),
		FVector(62, 18, 4),
		ScreenMaterial,
		false);
	CreatePointLight(
		FVector(1110, -30, 232),
		1180.0f,
		560.0f,
		FLinearColor(0.34f, 0.46f, 0.39f),
		false);

	// A fixed wall bulkhead and handrail reveal the stair pitch without
	// removing the dark landing beyond it.  These are static scene components;
	// there is no per-frame animation or spawning as the loop changes.
	CreateBlock(
		FVector(1188, -205, 226),
		FVector(5, 42, 22),
		ScreenMaterial,
		false);
	CreatePointLight(
		FVector(1168, -205, 220),
		880.0f,
		470.0f,
		FLinearColor(0.30f, 0.39f, 0.34f),
		false);
	CreateBlock(
		FVector(1174, -205, 181),
		FVector(8, 390, 8),
		MetalMaterial,
		false,
		FRotator(0, 0, -27));
	for (int32 RailPostIndex = 0; RailPostIndex < 4; ++RailPostIndex)
	{
		const float PostY = -58.0f - RailPostIndex * 102.0f;
		const float StepRise = 15.0f * (1.0f + RailPostIndex * 3.5f);
		CreateBlock(
			FVector(1174, PostY, StepRise + 68.0f),
			FVector(8, 8, 136),
			MetalMaterial,
			false);
	}

	// A real apartment fire-stair sign: painted steel, a large floor number,
	// and a route arrow. The fallback 3D font has no CJK glyphs, so the
	// direction is communicated by geometry instead of an implausible English
	// "ROOF EXIT" label in an old Korean villa.
	CreateBlock(
		FVector(986, 0, 145),
		FVector(4, 180, 110),
		DarkConcreteMaterial,
		false);
	CreateBlock(
		FVector(989, 0, 145),
		FVector(1.5f, 176, 106),
		MetalMaterial,
		false);
	StairSignText = NewObject<UTextRenderComponent>(this, TEXT("CH03_StairSignText"));
	StairSignText->SetupAttachment(SceneRoot);
	StairSignText->SetRelativeLocation(FVector(991, 28, 157));
	StairSignText->SetRelativeRotation(FRotator::ZeroRotator);
	StairSignText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	StairSignText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	StairSignText->SetWorldSize(46.0f);
	StairSignText->SetTextRenderColor(FColor(188, 194, 184));
	StairSignText->SetText(FText::FromString(TEXT("4F")));
	StairSignText->RegisterComponent();

	StairRoofText = NewObject<UTextRenderComponent>(this, TEXT("CH03_StairRoofText"));
	StairRoofText->SetupAttachment(SceneRoot);
	StairRoofText->SetRelativeLocation(FVector(991, -23, 112));
	StairRoofText->SetRelativeRotation(FRotator::ZeroRotator);
	StairRoofText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	StairRoofText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	StairRoofText->SetWorldSize(14.0f);
	StairRoofText->SetTextRenderColor(FColor(180, 186, 176));
	StairRoofText->SetText(FText::GetEmpty());
	StairRoofText->SetVisibility(false);
	StairRoofText->RegisterComponent();

	// Three strokes form a familiar upward arrow. The old five-block pyramid
	// read as a cross at this camera angle.
	const struct
	{
		FVector Center;
		FVector Size;
		FRotator Rotation;
	} ArrowBlocks[] = {
		{FVector(991, 49, 118), FVector(2.8f, 6, 42), FRotator::ZeroRotator},
		{FVector(991, 41.5f, 132), FVector(2.8f, 22, 4), FRotator(0, 0, -42)},
		{FVector(991, 56.5f, 132), FVector(2.8f, 22, 4), FRotator(0, 0, 42)}
	};
	for (const auto& BlockData : ArrowBlocks)
	{
		if (UStaticMeshComponent* Stroke = CreateBlock(
			BlockData.Center,
			BlockData.Size,
			PaperMaterial,
			false,
			BlockData.Rotation))
		{
			Stroke->SetVisibility(false);
			StairSignParts.Add(Stroke);
		}
	}

	// A thin paint run appears on the second impossible return.
	if (UStaticMeshComponent* Drip = CreateBlock(
			FVector(991, 28, 116),
			FVector(2, 2, 20),
			WetPaperMaterial,
			false);
		Drip)
	{
		Drip->SetVisibility(false);
		StairLatinSignParts.Add(Drip);
	}
}

void AIGThirdMorningDirector::BuildFifthFloorAndRoof()
{
	// Unfinished fifth-floor landing.
	CreateBlock(FVector(1240, -400, 170), FVector(420, 250, 20), ConcreteMaterial);
	CreateBlock(FVector(1450, -400, 205), FVector(18, 250, 410), DarkConcreteMaterial);
	for (int32 BeamIndex = 0; BeamIndex < 4; ++BeamIndex)
	{
		CreateBlock(
			FVector(1100 + BeamIndex * 120, -510, 325),
			FVector(16, 16, 310),
			MetalMaterial);
	}
	// Hanging vinyl: a thin non-colliding sheet, motionless air made suspect
	// by its skewed pose.
	CreateBlock(
		FVector(1390, -335, 300),
		FVector(3, 155, 210),
		WetPaperMaterial,
		false,
		FRotator(0, 0, 7));

	// The search flyer is found on the 4F route, never as a convenient document
	// cache on the fifth floor. Before P3 it hangs wet across the stair rail;
	// after a successful drain it settles against the landing wall.
	SearchPosterAction = SpawnAction(
		EIGChapterThreeAction::EvidenceSearchPoster,
		FVector(1178.0f, -185.0f, 188.0f),
		FVector(3, 66, 92),
		PaperMaterial,
		NSLOCTEXT(
			"IGCH03",
			"SearchPosterPrompt",
			"실종 전단의 복장과 안경 확인하기"));
	if (SearchPosterAction)
	{
		EvidenceActions.Add(
			EIGChapterThreeAction::EvidenceSearchPoster,
			SearchPosterAction.Get());
	}

	// Four small risers connect the landing to the roof slab.
	for (int32 StepIndex = 0; StepIndex < 4; ++StepIndex)
	{
		const float TopZ = 195.0f + StepIndex * 15.0f;
		CreateBlock(
			FVector(1510.0f + StepIndex * 36.0f, -400, 180.0f + (TopZ - 180.0f) * 0.5f),
			FVector(38, 180, TopZ - 180.0f),
			ConcreteMaterial);
	}

	RoofDoorAction = SpawnAction(
		EIGChapterThreeAction::OpenRoofDoor,
		FVector(1662, -400, 366),
		FVector(12, 116, 230),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "RoofDoorPrompt", "옥상 철문 열기"));
	KeysAction = SpawnAction(
		EIGChapterThreeAction::InspectKeys,
		FVector(1625, -342, 330),
		FVector(7, 18, 8),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "KeysPrompt", "꽂힌 열쇠뭉치 확인하기"));

	// Wide roof: the absence of traffic and animals is legible because there
	// is room for the player to wait and hear nothing.
	CreateBlock(
		FVector(2360, -400, IGThirdMorning::RoofFloorZ - 10),
		FVector(1450, 1180, 20),
		ConcreteMaterial);
	CreateBlock(FVector(2360, -990, 290), FVector(1450, 20, 100), DarkConcreteMaterial);
	CreateBlock(FVector(2360, 190, 290), FVector(1450, 20, 100), DarkConcreteMaterial);
	CreateBlock(FVector(3085, -400, 290), FVector(20, 1180, 100), DarkConcreteMaterial);
	CreateBlock(FVector(1690, -820, 290), FVector(20, 340, 100), DarkConcreteMaterial);
	CreateBlock(FVector(1690, 20, 290), FVector(20, 340, 100), DarkConcreteMaterial);

	// Expansion joints, a grated drain and a code-like parapet rail give the
	// roof a readable scale even when the predawn sky remains almost black.
	CreateBlock(
		FVector(2380, -690, 241.5f),
		FVector(1160, 5, 3),
		DarkConcreteMaterial,
		false);
	CreateBlock(
		FVector(2070, -385, 241.5f),
		FVector(5, 610, 3),
		DarkConcreteMaterial,
		false);
	CreateBlock(
		FVector(2860, -510, 243),
		FVector(120, 24, 5),
		MetalMaterial,
		false);
	for (int32 DrainSlotIndex = 0; DrainSlotIndex < 5; ++DrainSlotIndex)
	{
		CreateBlock(
			FVector(2816.0f + DrainSlotIndex * 22.0f, -510, 246),
			FVector(10, 27, 2),
			DarkConcreteMaterial,
			false);
	}
	for (int32 RailPostIndex = 0; RailPostIndex < 7; ++RailPostIndex)
	{
		CreateBlock(
			FVector(1750.0f + RailPostIndex * 210.0f, 174, 390),
			FVector(9, 9, 110),
			MetalMaterial,
			false);
	}
	CreateBlock(
		FVector(2380, 174, 365),
		FVector(1350, 9, 9),
		MetalMaterial,
		false);
	CreateBlock(
		FVector(2380, 174, 435),
		FVector(1350, 9, 9),
		MetalMaterial,
		false);

	// Broken salt line across the threshold.
	CreateBlock(FVector(1690, -455, 242), FVector(3, 58, 2), PaperMaterial, false);
	CreateBlock(FVector(1690, -345, 242), FVector(3, 45, 2), PaperMaterial, false);

	// One wet, dry leaf where no tree exists.
	CreateBlock(
		FVector(1905, -250, 242),
		FVector(18, 6, 1),
		WetPaperMaterial,
		false,
		FRotator(0, 28, 11));

	// Distant Korean rooftop landmark: a muted red church cross.
	CreateBlock(FVector(3020, 120, 520), FVector(12, 12, 220), ScreenMaterial, false);
	CreateBlock(FVector(3020, 120, 555), FVector(85, 12, 12), ScreenMaterial, false);

	// A caged maintenance lamp and the stale red emergency lamp separate the
	// tank, service stair and parapet without flattening the predawn darkness.
	CreateBlock(
		FVector(2145, -815, 470),
		FVector(24, 10, 34),
		MetalMaterial,
		false);
	CreateBlock(
		FVector(2145, -808, 470),
		FVector(16, 4, 22),
		EmergencyMaterial,
		false);

	// The cool roof fill now has a visible maintenance mast instead of an
	// unexplained floating highlight.
	CreateBlock(
		FVector(2680, -590, 395),
		FVector(10, 10, 310),
		MetalMaterial,
		false);
	CreateBlock(
		FVector(2655, -568, 548),
		FVector(68, 12, 10),
		MetalMaterial,
		false,
		FRotator(0, -28, 0));
	CreateBlock(
		FVector(2630, -546, 542),
		FVector(42, 24, 8),
		ScreenMaterial,
		false,
		FRotator(0, -28, 0));
	CreatePointLight(
		FVector(2630, -546, 530),
		4200.0f,
		1480.0f,
		FLinearColor(0.36f, 0.47f, 0.68f),
		false);
	CreatePointLight(
		FVector(2160, -770, 490),
		940.0f,
		790.0f,
		FLinearColor(0.62f, 0.065f, 0.035f),
		true);
	CreatePointLight(
		FVector(2350, -120, 520),
		780.0f,
		620.0f,
		FLinearColor(0.16f, 0.30f, 0.46f),
		false);
}

void AIGThirdMorningDirector::BuildWaterTank()
{
	const FVector Tank = IGThirdMorning::TankCenter;

	// Concrete plinth and a sixteen-panel cylindrical shell.  Panels are
	// cheaper than a masked custom mesh and leave the open top genuinely open.
	CreateBlock(Tank + FVector(0, 0, 50 + IGThirdMorning::RoofFloorZ),
		FVector(360, 360, 100), ConcreteMaterial);
	for (int32 PanelIndex = 0; PanelIndex < 16; ++PanelIndex)
	{
		const float AngleDegrees = PanelIndex * 22.5f;
		const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		const FVector Radial(
			FMath::Cos(AngleRadians) * 145.0f,
			FMath::Sin(AngleRadians) * 145.0f,
			0.0f);
		CreateBlock(
			Tank + Radial + FVector(0, 0, 470),
			FVector(58, 12, 260),
			MetalMaterial,
			true,
			FRotator(0, AngleDegrees + 90.0f, 0));
	}

	// Three proud reinforcement rings catch long highlights across several
	// facets, visually joining the sixteen panels into an industrial tank.
	for (const float BandZ : {352.0f, 470.0f, 588.0f})
	{
		for (int32 SegmentIndex = 0; SegmentIndex < 16; ++SegmentIndex)
		{
			const float AngleDegrees = SegmentIndex * 22.5f;
			const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
			const FVector Radial(
				FMath::Cos(AngleRadians) * 153.0f,
				FMath::Sin(AngleRadians) * 153.0f,
				0.0f);
			CreateBlock(
				Tank + Radial + FVector(0, 0, BandZ),
				FVector(61, 20, 12),
				MetalMaterial,
				false,
				FRotator(0, AngleDegrees + 90.0f, 0));
		}
	}

	// Visible inlet pipe, clamps and valve connect the tank to the building.
	// Cylinders are static and non-colliding, so this detail adds no gameplay
	// physics or per-frame work.
	CreateBlock(
		Tank + FVector(128, -128, 390),
		FVector(18, 18, 300),
		MetalMaterial,
		false,
		FRotator::ZeroRotator,
		CylinderMesh);
	CreateBlock(
		Tank + FVector(248, -128, 254),
		FVector(16, 16, 240),
		MetalMaterial,
		false,
		FRotator(90, 0, 0),
		CylinderMesh);
	for (const float ClampZ : {330.0f, 450.0f})
	{
		CreateBlock(
			Tank + FVector(128, -128, ClampZ),
			FVector(26, 26, 8),
			DarkConcreteMaterial,
			false,
			FRotator::ZeroRotator,
			CylinderMesh);
	}
	CreateBlock(
		Tank + FVector(128, -151, 410),
		FVector(46, 46, 8),
		MetalMaterial,
		false,
		FRotator(0, 0, 90),
		CylinderMesh);

	// A small service lamp on the pipe side lifts the lower shell and bands,
	// while the opposite side remains available for the flashlight scare.
	CreateBlock(
		Tank + FVector(145, -151, 570),
		FVector(34, 18, 24),
		MetalMaterial,
		false);
	CreateBlock(
		Tank + FVector(158, -164, 570),
		FVector(18, 6, 14),
		ScreenMaterial,
		false);
	CreatePointLight(
		Tank + FVector(170, -176, 560),
		1050.0f,
		660.0f,
		FLinearColor(0.34f, 0.45f, 0.58f),
		false);

	TankWaterSurface = CreateBlock(
		Tank + FVector(0, 0, 542),
		FVector(270, 270, 3),
		WaterMaterial,
		false);
	CreatePointLight(
		Tank + FVector(-38, -42, 586),
		390.0f,
		330.0f,
		FLinearColor(0.18f, 0.40f, 0.58f),
		false);
	CreatePointLight(
		Tank + FVector(72, 68, 576),
		90.0f,
		235.0f,
		FLinearColor(0.34f, 0.045f, 0.028f),
		false);

	// Service stair disguised with ladder rails/rungs.  The 20 cm risers are
	// within CharacterMovement step height, avoiding bespoke ladder physics.
	for (int32 StepIndex = 0; StepIndex < 18; ++StepIndex)
	{
		const float TopZ = IGThirdMorning::RoofFloorZ + 20.0f * (StepIndex + 1);
		const float Height = TopZ - IGThirdMorning::RoofFloorZ;
		CreateBlock(
			FVector(1915.0f + StepIndex * 20.0f, -300, IGThirdMorning::RoofFloorZ + Height * 0.5f),
			FVector(22, 105, Height),
			MetalMaterial);
	}
	CreateBlock(FVector(2265, -365, 455), FVector(12, 12, 430), MetalMaterial);
	CreateBlock(FVector(2265, -235, 455), FVector(12, 12, 430), MetalMaterial);
	for (int32 RungIndex = 0; RungIndex < 10; ++RungIndex)
	{
		CreateBlock(
			FVector(2265, -300, 285.0f + RungIndex * 34.0f),
			FVector(10, 136, 7),
			MetalMaterial);
	}
	CreateBlock(FVector(2325, -300, 610), FVector(150, 220, 20), MetalMaterial);

	GlassesAction = SpawnAction(
		EIGChapterThreeAction::EvidenceGlasses,
		FVector(1880, -250, 247),
		FVector(16, 5, 3),
		PlasticMaterial,
		NSLOCTEXT("IGCH03", "GlassesPrompt", "젖은 안경 확인하기"));
	if (GlassesAction)
	{
		EvidenceActions.Add(EIGChapterThreeAction::EvidenceGlasses, GlassesAction);
	}
	// Two temple arms make the block immediately read as black horn-rimmed glasses.
	CreateBlock(FVector(1882, -242, 249), FVector(20, 2, 2), PlasticMaterial, false);
	CreateBlock(FVector(1882, -258, 249), FVector(20, 2, 2), PlasticMaterial, false);

	TankLidAction = SpawnAction(
		EIGChapterThreeAction::OpenTank,
		Tank + FVector(-25, 0, 655),
		FVector(310, 310, 8),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "OpenTankPrompt", "물탱크 뚜껑 열기"),
		1.2f,
		FRotator(0, 0, 58),
		CylinderMesh);

	CloseChoiceAction = SpawnAction(
		EIGChapterThreeAction::CloseTank,
		Tank + FVector(-155, -82, 625),
		FVector(24, 10, 8),
		MetalMaterial,
		NSLOCTEXT("IGCH03", "CloseTankPrompt", "뚜껑을 닫는다"),
		0.8f);
	SupportChoiceAction = SpawnAction(
		EIGChapterThreeAction::SupportLid,
		Tank + FVector(-155, 105, 625),
		FVector(44, 28, 8),
		DarkConcreteMaterial,
		NSLOCTEXT(
			"IGCH03",
			"SupportLidPrompt",
			"점검봉을 끼운다"),
		1.2f);
	SetVisibleInteractive(CloseChoiceAction, false);
	SetVisibleInteractive(SupportChoiceAction, false);

	// Concentric broken highlights make the otherwise still surface read as
	// water rather than a blue floor.  They are deliberately sparse: the only
	// motion after the lid lifts is the player's own light and these ripples.
	for (int32 RingIndex = 0; RingIndex < 3; ++RingIndex)
	{
		const float RadiusX = 62.0f + RingIndex * 32.0f;
		const float RadiusY = 40.0f + RingIndex * 24.0f;
		const int32 SegmentCount = 12;
		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			// Missing segments stop the pattern looking like a UI reticle.
			if ((SegmentIndex + RingIndex * 2) % 5 == 0)
			{
				continue;
			}
			const float Angle = 2.0f * PI
				* static_cast<float>(SegmentIndex)
				/ static_cast<float>(SegmentCount);
			CreateBlock(
				Tank + FVector(
					-4.0f + FMath::Cos(Angle) * RadiusX,
					2.0f + FMath::Sin(Angle) * RadiusY,
					544.0f + RingIndex * 0.25f),
				FVector(24.0f + RingIndex * 3.0f, 1.8f, 0.7f),
				WetStepMaterial,
				false,
				FRotator(0, FMath::RadiansToDegrees(Angle) + 90.0f, 0));
		}
	}

	// A curled, back-facing human silhouette assembled from rounded primitives.
	// It stays non-graphic, but shoulders, elbows, knees and wet clothing make
	// it unmistakably human instead of a rectangular placeholder.
	auto AddBodyPiece =
		[this, &Tank](
			const FVector& Offset,
			const FVector& Size,
			UMaterialInterface* Material,
			UStaticMesh* Mesh,
			const FRotator& Rotation = FRotator::ZeroRotator)
	{
		if (UStaticMeshComponent* Piece = CreateBlock(
			Tank + Offset + FVector(0, 0, -10.0f),
			Size,
			Material,
			false,
			Rotation,
			Mesh))
		{
			Piece->SetVisibility(false);
			BodySilhouette.Add(Piece);
		}
	};
	auto AddLimb =
		[this, &Tank, &AddBodyPiece](
			const FVector& Start,
			const FVector& End,
			const float Diameter,
			UMaterialInterface* Material)
	{
		const FVector Delta = End - Start;
		const FRotator Rotation = FQuat::FindBetweenNormals(
			FVector::UpVector,
			Delta.GetSafeNormal()).Rotator();
		AddBodyPiece(
			(Start + End) * 0.5f,
			FVector(Diameter, Diameter, Delta.Size()),
			Material,
			CylinderMesh,
			Rotation);
		// A rounded joint hides the cylinder end and keeps bent limbs reading
		// as one submerged human silhouette instead of detached oval pieces.
		AddBodyPiece(
			End,
			FVector(Diameter * 1.08f),
			Material,
			SphereMesh);
	};

	AddBodyPiece(
		FVector(8, 4, 553),
		FVector(92, 46, 25),
		WetStepMaterial,
		SphereMesh,
		FRotator(0, 16, -3));
	AddBodyPiece(
		FVector(-34, -3, 551),
		FVector(52, 43, 27),
		BeddingMaterial,
		SphereMesh,
		FRotator(0, 12, 0));
	AddBodyPiece(
		FVector(62, 15, 556),
		FVector(31, 29, 29),
		WetStepMaterial,
		SphereMesh,
		FRotator(0, 9, 0));
	AddBodyPiece(
		FVector(66, 18, 563),
		FVector(36, 34, 17),
		PlasticMaterial,
		SphereMesh,
		FRotator(0, 14, 8));
	AddBodyPiece(
		FVector(50, 11, 553),
		FVector(18, 19, 18),
		WetStepMaterial,
		SphereMesh);

	// Arms folded in toward the chest.
	AddLimb(FVector(30, -10, 554), FVector(7, -38, 551), 13.5f, WetStepMaterial);
	AddLimb(FVector(7, -38, 551), FVector(-20, -24, 548), 11.5f, WetStepMaterial);
	AddBodyPiece(
		FVector(-23, -22, 548),
		FVector(13, 10, 8),
		WetStepMaterial,
		SphereMesh,
		FRotator(0, -12, 0));
	AddLimb(FVector(31, 20, 554), FVector(4, 44, 551), 13.5f, WetStepMaterial);
	AddLimb(FVector(4, 44, 551), FVector(-25, 31, 548), 11.5f, WetStepMaterial);
	AddBodyPiece(
		FVector(-28, 29, 548),
		FVector(13, 10, 8),
		WetStepMaterial,
		SphereMesh,
		FRotator(0, 10, 0));

	// Bent legs give the same uneasy foetal posture glimpsed in room 403.
	AddLimb(FVector(-30, -8, 551), FVector(-63, -42, 549), 20.0f, BeddingMaterial);
	AddLimb(FVector(-63, -42, 549), FVector(-107, -21, 547), 16.5f, BeddingMaterial);
	AddBodyPiece(
		FVector(-115, -17, 547),
		FVector(28, 16, 12),
		PlasticMaterial,
		SphereMesh,
		FRotator(0, -19, 0));
	AddLimb(FVector(-33, 8, 551), FVector(-62, 38, 550), 20.0f, BeddingMaterial);
	AddLimb(FVector(-62, 38, 550), FVector(-102, 27, 547), 16.5f, BeddingMaterial);
	AddBodyPiece(
		FVector(-111, 25, 547),
		FVector(28, 16, 12),
		PlasticMaterial,
		SphereMesh,
		FRotator(0, 11, 0));
	// Three black repair stitches on the left sleeve and the worn outer heel
	// are separate identity source details, not one generic "same clothes" prop.
	for (int32 StitchIndex = 0; StitchIndex < 3; ++StitchIndex)
	{
		AddBodyPiece(
			FVector(-15.0f + StitchIndex * 4.0f, -30.0f, 552.5f),
			FVector(1.2f, 7.0f, 1.2f),
			DarkConcreteMaterial,
			CubeMesh,
			FRotator(0, 0, 18));
	}
	AddBodyPiece(
		FVector(-120, -19, 548.5f),
		FVector(7, 11, 3),
		WetPaperMaterial,
		CubeMesh,
		FRotator(0, -19, 0));
}

void AIGThirdMorningDirector::BuildP5AccidentEvidence()
{
	const FVector Tank = IGThirdMorning::TankCenter;
	FIGRebirthChoiceState PurchaseChoices;
	if (const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
	{
		PurchaseChoices = RebirthState->GetChoices();
	}
	if (PurchaseChoices.PurchaseProfile == EIGRebirthPurchaseProfile::Unset)
	{
		// Direct CH03 development launches retain a deterministic physical
		// proxy without mutating the player's authored branch.
		PurchaseChoices.PurchaseProfile =
			EIGRebirthPurchaseProfile::ProfileA500MlX2;
	}

	int32 BottleCount = 2;
	float NominalCapacityMl = 500.0f;
	FVector BottleProfileScale(1.0f, 1.0f, 1.0f);
	FVector BagSize(12.0f, 18.0f, 20.0f);
	float BagHandleHeight = 8.0f;
	FRotator PurchaseBagRotation(0.0f, 20.0f, -8.0f);
	switch (PurchaseChoices.PurchaseProfile)
	{
	case EIGRebirthPurchaseProfile::ProfileB1LX1:
		BottleCount = 1;
		NominalCapacityMl = 1000.0f;
		BottleProfileScale = FVector(1.12f, 1.12f, 1.35f);
		BagSize = FVector(13.0f, 12.0f, 28.0f);
		BagHandleHeight = 10.0f;
		break;
	case EIGRebirthPurchaseProfile::ProfileC2LX2:
		BottleCount = 2;
		NominalCapacityMl = 2000.0f;
		BottleProfileScale = FVector(1.34f, 1.34f, 1.72f);
		BagSize = FVector(16.0f, 25.0f, 31.0f);
		BagHandleHeight = 15.0f;
		// Two full 2 L bottles cannot stand neatly after the fall. The whole
		// purchase assembly lies on its side, preserving every child offset.
		PurchaseBagRotation = FRotator(78.0f, 20.0f, -8.0f);
		break;
	case EIGRebirthPurchaseProfile::ProfileA500MlX2:
	default:
		break;
	}
	const FVector BottlePhysicalSize =
		FVector(6.0f, 6.0f, 20.3f) * BottleProfileScale;
	const FVector BottleMeshScalePercent =
		FVector(100.0f) * BottleProfileScale;
	const FVector CapMeshScalePercent =
		FVector(100.0f) * BottleProfileScale;
	const FVector LabelMeshScalePercent =
		FVector(336.0f, 336.0f, 860.0f) * BottleProfileScale;
	const FQuat PurchaseBagQuat = PurchaseBagRotation.Quaternion();
	const float BagHalfHeight =
		FMath::Abs(PurchaseBagQuat.GetAxisX().Z) * BagSize.X * 0.5f
		+ FMath::Abs(PurchaseBagQuat.GetAxisY().Z) * BagSize.Y * 0.5f
		+ FMath::Abs(PurchaseBagQuat.GetAxisZ().Z) * BagSize.Z * 0.5f;
	const FVector PurchaseBagLocation(
		1885.0f,
		-350.0f,
		IGThirdMorning::RoofFloorZ + BagHalfHeight + 0.5f);
	const FTransform PurchaseBagTransform(
		PurchaseBagRotation,
		PurchaseBagLocation);

	// The sagging roof door leaves an authored 11 cm gap above the threshold.
	// Paw marks cross it in both directions; human access still requires the
	// existing pull interaction.
	CreateBlock(FVector(1666, -400, 248), FVector(18, 122, 4), WetStepMaterial, false);
	for (int32 Step = 0; Step < 3; ++Step)
	{
		CreateBlock(
			FVector(1698.0f + Step * 24.0f, -430.0f + Step * 5.0f, 242.5f),
			FVector(12, 8, 1.2f),
			WetStepMaterial,
			false,
			FRotator(0, 12, 0));
		CreateBlock(
			FVector(1770.0f - Step * 24.0f, -360.0f - Step * 5.0f, 242.7f),
			FVector(12, 8, 1.2f),
			WetStepMaterial,
			false,
			FRotator(0, 192, 0));
	}

	auto AddEvidence = [this](
		const EIGChapterThreeAction Action,
		const FVector& Location,
		const FVector& Size,
		UMaterialInterface* Material,
		const FText& Prompt,
		const FRotator& Rotation = FRotator::ZeroRotator,
		UStaticMesh* Mesh = nullptr)
	{
		AIGChapterThreeAction* Evidence = SpawnAction(
			Action,
			Location,
			Size,
			Material,
			Prompt,
			0.0f,
			Rotation,
			Mesh);
		if (Evidence)
		{
			EvidenceActions.Add(Action, Evidence);
		}
		return Evidence;
	};

	AddEvidence(
		EIGChapterThreeAction::EvidenceCatEntered,
		FVector(1718, -430, 245),
		FVector(20, 14, 2),
		WetStepMaterial,
		NSLOCTEXT("IGCH03", "EvidenceCatEntered", "들어온 앞발자국 확인하기"),
		FRotator(0, 12, 0));
	AddEvidence(
		EIGChapterThreeAction::EvidenceCatExited,
		FVector(1750, -360, 245),
		FVector(20, 14, 2),
		WetStepMaterial,
		NSLOCTEXT("IGCH03", "EvidenceCatExited", "나간 앞발자국 확인하기"),
		FRotator(0, 192, 0));

	// The hose ends on the support deck, never inside the tank. A flattened
	// section carries the paw compression and the coupling carries the impact.
	for (int32 Segment = 0; Segment < 5; ++Segment)
	{
		CreateBlock(
			Tank + FVector(180.0f - Segment * 55.0f, -180.0f + Segment * 18.0f, 248.0f + Segment * 65.0f),
			FVector(14, 14, 82),
			PlasticMaterial,
			false,
			FRotator(0, 18, -38),
			CylinderMesh);
	}
	AddEvidence(
		EIGChapterThreeAction::EvidenceHosePaw,
		Tank + FVector(165, -175, 250),
		FVector(26, 18, 5),
		WetStepMaterial,
		NSLOCTEXT("IGCH03", "EvidenceHosePaw", "호스의 눌린 앞발 자국 확인하기"),
		FRotator(0, 18, 0));
	AddEvidence(
		EIGChapterThreeAction::EvidenceHoseImpact,
		Tank + FVector(-118, -72, 632),
		FVector(22, 12, 9),
		WetStepMaterial,
		NSLOCTEXT(
			"IGCH03",
			"EvidenceHoseImpact",
			"젖은 안쪽 마찰 자국과 커플링 물자국 대조하기"),
		FRotator(0, -18, 0),
		CylinderMesh);

	AddEvidence(
		EIGChapterThreeAction::EvidenceBag,
		PurchaseBagLocation,
		BagSize,
		GlassMaterial,
		NSLOCTEXT("IGCH03", "EvidenceBag", "사다리 아래 편의점 봉지 확인하기"),
		PurchaseBagRotation);

	// The final scene reproduces the exact purchase branch instead of using a
	// generic bag token. The first bottle is the one Ji-woon drank from: its
	// water is visibly lower and its cap is absent only on cap+leave.
	const float BagWallThickness = 0.55f;
	for (const float Side :
		{-1.0f, 1.0f})
	{
		CreateBlock(
			PurchaseBagTransform.TransformPosition(
				FVector(
					Side * BagSize.X * 0.48f,
					0.0f,
					0.0f)),
			FVector(
				BagWallThickness,
				BagSize.Y,
				BagSize.Z),
			GlassMaterial,
			false,
			PurchaseBagRotation);
		CreateBlock(
			PurchaseBagTransform.TransformPosition(
				FVector(
					0.0f,
					Side * BagSize.Y * 0.48f,
					0.0f)),
			FVector(
				BagSize.X,
				BagWallThickness,
				BagSize.Z),
			GlassMaterial,
			false,
			PurchaseBagRotation);
	}
	const float HandleHalfWidth = BagSize.Y * 0.29f;
	for (const float HandleSide : {-1.0f, 1.0f})
	{
		CreateBlock(
			PurchaseBagTransform.TransformPosition(
				FVector(
					0.0f,
					HandleSide * HandleHalfWidth,
					BagSize.Z * 0.5f + BagHandleHeight * 0.5f)),
			FVector(0.9f, 0.8f, BagHandleHeight),
			PaperMaterial,
			false,
			PurchaseBagRotation);
	}
	CreateBlock(
		PurchaseBagTransform.TransformPosition(
			FVector(
				0.0f,
				0.0f,
				BagSize.Z * 0.5f + BagHandleHeight)),
		FVector(0.9f, HandleHalfWidth * 2.0f, 0.8f),
		PaperMaterial,
		false,
		PurchaseBagRotation);

	// The visual loss is derived from an absolute amount, not copied between
	// bottle sizes. Ji-woon drinks about 90 mL; the cap and paper-cup routes
	// use roughly another 35 mL and 65 mL respectively.
	float ConsumedMilliliters = 90.0f;
	switch (PurchaseChoices.CatWaterState)
	{
	case EIGRebirthCatWaterState::BottleCap:
		ConsumedMilliliters += 35.0f;
		break;
	case EIGRebirthCatWaterState::PaperCup:
		ConsumedMilliliters += 65.0f;
		break;
	case EIGRebirthCatWaterState::PassedBy:
	case EIGRebirthCatWaterState::Unset:
	default:
		break;
	}
	const float DrunkBottleFill = FMath::Clamp(
		0.94f - ConsumedMilliliters / NominalCapacityMl,
		0.08f,
		0.94f);
	for (int32 BottleIndex = 0; BottleIndex < BottleCount; ++BottleIndex)
	{
		const float Lateral =
			(BottleIndex - (BottleCount - 1) * 0.5f)
			* (BottlePhysicalSize.Y + 1.5f);
		const FTransform BottleTransform(
			PurchaseBagTransform.TransformRotation(
				FRotator(0.0f, BottleIndex * 7.0f, 0.0f).Quaternion()),
			PurchaseBagTransform.TransformPosition(
				FVector(
					0.0f,
					Lateral,
					-BagSize.Z * 0.5f + 2.5f)));
		const bool bHasBottleMesh = WaterBottleMesh != nullptr;
		const FVector BottleBodySize = bHasBottleMesh
			? BottleMeshScalePercent
			: BottlePhysicalSize;
		const FVector BottleBodyLocation = bHasBottleMesh
			? BottleTransform.GetLocation()
			: BottleTransform.TransformPosition(
				FVector(0.0f, 0.0f, BottlePhysicalSize.Z * 0.5f));
		CreateBlock(
			BottleBodyLocation,
			BottleBodySize,
			GlassMaterial,
			false,
			BottleTransform.Rotator(),
			bHasBottleMesh ? WaterBottleMesh.Get() : CylinderMesh.Get());

		const float FillFraction =
			BottleIndex == 0 ? DrunkBottleFill : 0.94f;
		const float FillHeight =
			FMath::Max(2.0f, BottlePhysicalSize.Z * FillFraction);
		CreateBlock(
			BottleTransform.TransformPosition(
				FVector(0.0f, 0.0f, FillHeight * 0.5f)),
			FVector(
				BottlePhysicalSize.X * 0.70f,
				BottlePhysicalSize.Y * 0.70f,
				FillHeight),
			WaterMaterial,
			false,
			BottleTransform.Rotator(),
			CylinderMesh);
		const bool bHasLabelMesh = LabelSleeveMesh != nullptr;
		const FVector LabelVisualSize = bHasLabelMesh
			? LabelMeshScalePercent
			: FVector(
				BottlePhysicalSize.X * 1.03f,
				BottlePhysicalSize.Y * 1.03f,
				4.6f * BottleProfileScale.Z);
		CreateBlock(
			BottleTransform.TransformPosition(
				FVector(
					0.0f,
					0.0f,
					5.0f * BottleProfileScale.Z)),
			LabelVisualSize,
			WaterLabelMaterial,
			false,
			BottleTransform.TransformRotation(
				FRotator(0.0f, -90.0f, 0.0f).Quaternion()).Rotator(),
			bHasLabelMesh ? LabelSleeveMesh.Get() : CylinderMesh.Get());

		if (BottleIndex == 0)
		{
			// Both fragments stay at the neck after the first opening. Their
			// centre gap is readable even when the cap was later screwed back.
			const float RingZ = 18.55f * BottleProfileScale.Z;
			for (const float FragmentSide : {-1.0f, 1.0f})
			{
				CreateBlock(
					BottleTransform.TransformPosition(
						FVector(
							FragmentSide * BottlePhysicalSize.X * 0.24f,
							0.0f,
							RingZ)),
					FVector(
						BottlePhysicalSize.X * 0.34f,
						BottlePhysicalSize.Y * 0.70f,
						0.75f * BottleProfileScale.Z),
					BottleCapMaterial,
					false,
					BottleTransform.TransformRotation(
						FRotator(0.0f, FragmentSide * 6.0f, 0.0f).Quaternion())
						.Rotator());
			}
		}

		const bool bMissingThisCap =
			BottleIndex == 0
			&& PurchaseChoices.BottleClosureState
				== EIGRebirthBottleClosureState::MissingCap;
		if (!bMissingThisCap)
		{
			const bool bHasCapMesh = BottleCapMesh != nullptr;
			const FVector CapVisualSize = bHasCapMesh
				? CapMeshScalePercent
				: FVector(
					BottlePhysicalSize.X * 0.58f,
					BottlePhysicalSize.Y * 0.58f,
					2.3f * BottleProfileScale.Z);
			CreateBlock(
				BottleTransform.TransformPosition(
					FVector(
						0.0f,
						0.0f,
						20.1f * BottleProfileScale.Z)),
				CapVisualSize,
				BottleCapMaterial,
				false,
				BottleTransform.Rotator(),
				bHasCapMesh ? BottleCapMesh.Get() : CylinderMesh.Get());
		}
	}

	AddEvidence(
		EIGChapterThreeAction::EvidenceWetRung,
		FVector(2260, -300, 556),
		FVector(20, 58, 10),
		WetStepMaterial,
		NSLOCTEXT(
			"IGCH03",
			"EvidenceWetRung",
			"상단 자국·들뜬 패드·녹슨 클립을 함께 확인하기"));
	// RungFailureCluster is one causal object, not a wet-rung token: the last
	// slipper mark ends here, the rubber pad has lifted inward, and both
	// retaining clips show the same corrosion visible in the 04:03 photo.
	for (int32 PrintIndex = 0; PrintIndex < 3; ++PrintIndex)
	{
		CreateBlock(
			FVector(
				2198.0f + PrintIndex * 27.0f,
				-323.0f + PrintIndex * 8.0f,
				514.0f + PrintIndex * 20.0f),
			FVector(16, 9, 1.2f),
			WetStepMaterial,
			false,
			FRotator(0, 12, 0));
	}
	CreateBlock(
		FVector(2256.0f, -300.0f, 562.0f),
		FVector(18, 54, 3.2f),
		PlasticMaterial,
		false,
		FRotator(0, 0, -8));
	for (const float ClipY : {-326.0f, -274.0f})
	{
		CreateBlock(
			FVector(2263.0f, ClipY, 559.0f),
			FVector(5, 7, 5),
			EmergencyMaterial,
			false,
			FRotator(0, 0, 18));
	}
	AddEvidence(
		EIGChapterThreeAction::EvidenceHandSmear,
		Tank + FVector(-135, 10, 626),
		FVector(34, 14, 3),
		WetStepMaterial,
		NSLOCTEXT(
			"IGCH03",
			"EvidenceHandSmear",
			"안쪽으로 이어진 손바닥 쓸림 확인하기"),
		FRotator(0, 82, 0));

	AIGChapterThreeAction* ClothingEvidence = AddEvidence(
		EIGChapterThreeAction::EvidenceTankClothing,
		Tank + FVector(20, 5, 562),
		FVector(55, 28, 9),
		BeddingMaterial,
		NSLOCTEXT("IGCH03", "EvidenceTankClothing", "물속 후드의 수선 자국 대조하기"),
		FRotator(0, 16, 0));
	SetVisibleInteractive(ClothingEvidence, false);

	// The support rod is present before the ending prompt so choice B never
	// materializes an unexplained tool.
	CreateBlock(
		Tank + FVector(-190, 108, 612),
		FVector(10, 10, 118),
		MetalMaterial,
		false,
		FRotator(0, 72, 68),
		CylinderMesh);
}

void AIGThirdMorningDirector::SpawnClueDocuments()
{
	PlannerNote = SpawnNote(
		FVector(80, 150, 44),
		FRotator::ZeroRotator,
		FVector(30, 22, 1.2f),
		NSLOCTEXT("IGCH03", "PlannerPrompt", "수험 플래너 확인하기"),
		NSLOCTEXT("IGCH03", "PlannerTitle", "7월 기상·근무 플래너"),
		{
			NSLOCTEXT("IGCH03", "PlannerL1", "평일  05:10 기상  ·  05:30 캠프 집결"),
			NSLOCTEXT("IGCH03", "PlannerL2", "7/24(수)  모의고사 채점"),
			NSLOCTEXT("IGCH03", "PlannerL3", "7/25(목)  생수 · 골목 고양이 밥"),
			NSLOCTEXT("IGCH03", "PlannerL4", "7/26(금)  __________________")
		},
		PaperMaterial);

	MotherPhoneNote = SpawnNote(
		FVector(122, 150, 44.4f),
		FRotator(0, 0, -8),
		FVector(16, 8, 1.4f),
		NSLOCTEXT("IGCH03", "MotherPhonePrompt", "금 간 휴대폰 확인하기"),
		NSLOCTEXT("IGCH03", "MotherPhoneTitle", "엄마"),
		{
			NSLOCTEXT(
				"IGCH03",
				"MotherPhoneL1",
				"엄마  7/22  쌀 보냈다. 물 많이 마시고 다녀라."),
			NSLOCTEXT(
				"IGCH03",
				"MotherPhoneL2",
				"엄마  7/25  주말에 내려오니?"),
			NSLOCTEXT(
				"IGCH03",
				"MotherPhoneL3",
				"읽지 않음  7/26 07:02  지운아"),
			FText::GetEmpty(),
			NSLOCTEXT(
				"IGCH03",
				"MotherPhoneApproval0431",
				"카드 알림  7/26 04:31  체크카드 원승인"),
			NSLOCTEXT(
				"IGCH03",
				"MotherPhoneApproval0444",
				"거래 기록  7/26 04:44  동일 승인 중복")
		},
		PlasticMaterial);
	// Two hairline screen cracks are enough to identify the object before the
	// reading panel opens, without making a bespoke phone UI asset.
	CreateBlock(
		FVector(121, 150, 45.25f),
		FVector(7, 0.5f, 0.25f),
		ScreenMaterial,
		false,
		FRotator(0, -17, 0));
	CreateBlock(
		FVector(124, 150, 45.28f),
		FVector(5, 0.5f, 0.25f),
		ScreenMaterial,
		false,
		FRotator(0, 23, 0));

	OfferingNote = SpawnNote(
		FVector(1265, -485, 184),
		FRotator::ZeroRotator,
		FVector(25, 18, 1.2f),
		NSLOCTEXT("IGCH03", "OfferingPrompt", "엎어진 정화수 옆 쪽지 읽기"),
		NSLOCTEXT("IGCH03", "OfferingTitle", "삐뚤한 글씨"),
		{
			NSLOCTEXT("IGCH03", "OfferingL1", "목마른 사람은"),
			NSLOCTEXT("IGCH03", "OfferingL2", "이 물을 드시오."),
			NSLOCTEXT("IGCH03", "OfferingL3", "— 401호")
		},
		WetPaperMaterial);
	// Overturned bowl and its spilled water.
	CreateBlock(
		FVector(1228, -485, 190),
		FVector(24, 24, 10),
		MetalMaterial,
		false,
		FRotator(20, 0, 18),
		CylinderMesh);
	CreateBlock(FVector(1215, -470, 182), FVector(55, 38, 2), WaterMaterial, false);

	EstimateNote = SpawnNote(
		FVector(1608, -486, 326),
		FRotator(90, 0, 0),
		FVector(31, 22, 1.2f),
		NSLOCTEXT("IGCH03", "EstimatePrompt", "철문 옆 원본 인계표 읽기"),
		NSLOCTEXT("IGCH03", "EstimateTitle", "저수조 현장 인계표 원본"),
		{
			NSLOCTEXT("IGCH03", "EstimateL1", "미르워터텍  ·  2024년 7월 26일(금)"),
			NSLOCTEXT("IGCH03", "EstimateL2", "03:58  작업 중단 / 점검구 임시 하강"),
			NSLOCTEXT("IGCH03", "EstimateL3", "내부 확인·계측값             [공란]"),
			NSLOCTEXT("IGCH03", "EstimateL4", "현장 인계  강만식            [서명]")
		},
		WetPaperMaterial);
	// The original is kept in a shallow IP65 record box outside the locked
	// roof door, so it cannot contain information created by the later search.
	CreateBlock(
		FVector(1604, -486, 326),
		FVector(38, 30, 5),
		DarkConcreteMaterial,
		false,
		FRotator(90, 0, 0));

	P3RecordNote = SpawnNote(
		FVector(620, -171, 72),
		FRotator(90, 0, 0),
		FVector(42, 28, 1.2f),
		NSLOCTEXT("IGCH03", "P3RecordPrompt", "빈 작업 종료 기록 확인하기"),
		NSLOCTEXT("IGCH03", "P3RecordTitle", "작업 종료 기록"),
		{
			NSLOCTEXT("IGCH03", "P3RecordL1", "직결 차단 시각   __:__"),
			NSLOCTEXT("IGCH03", "P3RecordL2", "예비조 차단 시각 __:__"),
			NSLOCTEXT("IGCH03", "P3RecordL3", "압력 해제 시각   __:__"),
			NSLOCTEXT("IGCH03", "P3RecordL4", "압력 0 확인      __ kPa"),
			NSLOCTEXT("IGCH03", "P3RecordL5", "배수량           __ L")
		},
		WetPaperMaterial);
	P3PhotoNote = SpawnNote(
		FVector(1815, -760, 246),
		FRotator::ZeroRotator,
		FVector(42, 28, 1.2f),
		NSLOCTEXT("IGCH03", "P3PhotoPrompt", "04:03 전송 기록 확인하기"),
		NSLOCTEXT("IGCH03", "P3PhotoTitle", "건물주 업무 대화"),
		{
			NSLOCTEXT("IGCH03", "P3PhotoL1", "04:01  수위와 부식 부위 사진 보내 주세요."),
			NSLOCTEXT("IGCH03", "P3PhotoL2", "04:03  강만식  [사진 2장 전송]"),
			NSLOCTEXT("IGCH03", "P3PhotoL3", "촬영 위치  옥상 예비 저수조 개방 점검구")
		},
		WetPaperMaterial);
	ManagementDbNote = SpawnNote(
		FVector(1870, -760, 246),
		FRotator::ZeroRotator,
		FVector(42, 28, 1.2f),
		NSLOCTEXT("IGCH03", "ManagementDbPrompt", "관리 시스템 입력 이력 확인하기"),
		NSLOCTEXT("IGCH03", "ManagementDbTitle", "작업 완료 기록 · 수정 이력"),
		{
			NSLOCTEXT("IGCH03", "ManagementDbL1", "표시 작업시각  07/26 04:10:00"),
			NSLOCTEXT("IGCH03", "ManagementDbL2", "실제 입력시각  07/26 06:20:42"),
			NSLOCTEXT("IGCH03", "ManagementDbL3", "입력 계정      관리인-01"),
			NSLOCTEXT("IGCH03", "ManagementDbL4", "내부 확인값    없음 / 사진 첨부 없음"),
			NSLOCTEXT("IGCH03", "ManagementDbL5", "※ 작업시각 수기 지정 · 사후 입력")
		},
		WetPaperMaterial);
	RecheckNoticeNote = SpawnNote(
		FVector(1608, -314, 326),
		FRotator(90, 0, 0),
		FVector(31, 22, 1.2f),
		NSLOCTEXT("IGCH03", "RecheckNoticePrompt", "업체 재방문 안내 읽기"),
		NSLOCTEXT("IGCH03", "RecheckNoticeTitle", "저수조 설비 재확인 안내"),
		{
			NSLOCTEXT("IGCH03", "RecheckNoticeL1", "작성  2024년 7월 29일"),
			NSLOCTEXT("IGCH03", "RecheckNoticeL2", "미르워터텍 설비 재확인  7월 31일 04:00"),
			NSLOCTEXT("IGCH03", "RecheckNoticeL3", "확인 완료 전 옥상 출입 금지")
		},
		WetPaperMaterial);
	CreateBlock(
		FVector(1604, -314, 326),
		FVector(38, 30, 5),
		DarkConcreteMaterial,
		false,
		FRotator(90, 0, 0));
	PreservationNoticeNote = SpawnNote(
		FVector(1925, -760, 246),
		FRotator::ZeroRotator,
		FVector(42, 28, 1.2f),
		NSLOCTEXT("IGCH03", "PreservationPrompt", "경찰 보존 통지 확인하기"),
		NSLOCTEXT("IGCH03", "PreservationTitle", "현장 보존·개방 입회 통지"),
		{
			NSLOCTEXT("IGCH03", "PreservationL1", "작성  2024년 7월 30일 23:18"),
			NSLOCTEXT("IGCH03", "PreservationL2", "7월 31일 03:40  관계자 입회"),
			NSLOCTEXT("IGCH03", "PreservationL3", "04:00  경찰·소방 합동 개방"),
			NSLOCTEXT("IGCH03", "PreservationL4", "봉인·보존선 안쪽 출입 금지")
		},
		WetPaperMaterial);
	PoliceChecklistNote = SpawnNote(
		FVector(1980, -760, 246),
		FRotator::ZeroRotator,
		FVector(42, 28, 1.2f),
		NSLOCTEXT("IGCH03", "PoliceChecklistPrompt", "수사철 둘째 탭 확인하기"),
		NSLOCTEXT("IGCH03", "PoliceChecklistTitle", "현장 확인 체크리스트"),
		{
			NSLOCTEXT("IGCH03", "PoliceChecklistL1", "04:03 개방 점검구 사진 확보"),
			NSLOCTEXT("IGCH03", "PoliceChecklistL2", "내부 계측·확인자 기록 없음"),
			NSLOCTEXT("IGCH03", "PoliceChecklistL3", "06:20 사후 완료 입력 확인"),
			NSLOCTEXT("IGCH03", "PoliceChecklistL4", "원본 인계표와 입력 이력 대조 필요")
		},
		WetPaperMaterial);
	// A damp investigation folder waits beyond the roof door in the future
	// overlap. It is independent of the service-cabinet puzzle.
	CreateBlock(
		FVector(1897.5f, -760, 243.5f),
		FVector(220, 42, 3),
		DarkConcreteMaterial,
		false);
	for (AIGReadableNote* Note : {
		P3RecordNote.Get(),
		P3PhotoNote.Get(),
		ManagementDbNote.Get(),
		PreservationNoticeNote.Get(),
		PoliceChecklistNote.Get()})
	{
		SetDocumentAvailable(Note, false);
	}
}

void AIGThirdMorningDirector::SpawnTriggers()
{
	CorridorEntryZone = SpawnZone(FVector(425, 0, 110), FVector(42, 92, 115));
	if (CorridorEntryZone)
	{
		CorridorEntryZone->OnZoneTriggered.AddUniqueDynamic(
			this, &ThisClass::HandleCorridorEntered);
	}

	for (int32 LoopIndex = 0; LoopIndex < 3; ++LoopIndex)
	{
		AIGZoneTrigger* LoopZone = SpawnZone(
			FVector(1465, 382, -84),
			FVector(48, 48, 95));
		if (LoopZone)
		{
			LoopZone->OnZoneTriggered.AddUniqueDynamic(
				this, &ThisClass::HandleStairLoop);
			LoopZone->SetActorEnableCollision(LoopIndex == 0);
			StairLoopZones.Add(LoopZone);
		}
	}

	FifthFloorZone = SpawnZone(FVector(1190, -385, 276), FVector(95, 62, 100));
	if (FifthFloorZone)
	{
		FifthFloorZone->OnZoneTriggered.AddUniqueDynamic(
			this, &ThisClass::HandleFifthFloorEntered);
	}

	LadderZone = SpawnZone(FVector(2075, -300, 405), FVector(65, 90, 155));
	if (LadderZone)
	{
		LadderZone->OnZoneTriggered.AddUniqueDynamic(
			this, &ThisClass::HandleLadderEntered);
	}
}

// ---------------------------------------------------------------------------
// Story flow and interactions
// ---------------------------------------------------------------------------

void AIGThirdMorningDirector::BeginAwakening()
{
	if (!GetWorld())
	{
		return;
	}

	SetPhase(EIGThirdMorningPhase::Awakening);
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StopCameraFade();
			Camera->StartCameraFade(
				1.0f, 0.0f, 1.2f, FLinearColor::Black, false, false);
		}
	}

	UIGAlarmSoundWave* Alarm = NewObject<UIGAlarmSoundWave>(this, TEXT("CH03SelfStoppingAlarm"));
	AlarmComponent = IGAudio::SpawnOneShotAt(
		this,
		Alarm,
		ToWorld(FVector(-205, -190, 82)),
		0.82f,
		0.983f,
		70.0f,
		500.0f);

	GetWorldTimerManager().SetTimer(
		AlarmTimer,
		this,
		&ThisClass::StopAlarmByItself,
		4.0f,
		false);
}

void AIGThirdMorningDirector::StopAlarmByItself()
{
	if (bAlarmStopped)
	{
		return;
	}
	bAlarmStopped = true;
	TryUnlockApartmentExit();
	if (AlarmComponent)
	{
		AlarmComponent->Stop();
		AlarmComponent = nullptr;
	}

	// Let the absence land. Capture/smoke mode skips the wait, while the
	// playable route holds four full seconds before naming what happened.
	if (bCaptureMode)
	{
		SetPhase(EIGThirdMorningPhase::ApartmentClues);
		return;
	}
	GetWorldTimerManager().SetTimer(
		FlowTimer,
		this,
		&ThisClass::EndOpeningSilence,
		4.0f,
		false);
}

void AIGThirdMorningDirector::EndOpeningSilence()
{
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGCH03", "AlarmStoppedThought", "…저절로 꺼졌다."),
		3.2f);
	SetPhase(EIGThirdMorningPhase::ApartmentClues);
}

void AIGThirdMorningDirector::HandleAction(
	const EIGChapterThreeAction Action,
	AIGChapterThreeAction* Source)
{
	switch (Action)
	{
	case EIGChapterThreeAction::OpenFridge:
		if (bFridgeInspected)
		{
			return;
		}
		bFridgeInspected = true;
		if (Source)
		{
			Source->SetActorHiddenInGame(true);
			Source->SetActorEnableCollision(false);
			Source->SetInteractionEnabled(false);
		}
		for (UStaticMeshComponent* Bottle : FridgeContents)
		{
			if (Bottle)
			{
				Bottle->SetVisibility(true);
			}
		}
		if (FridgeInteriorLightPanel)
		{
			FridgeInteriorLightPanel->SetVisibility(true);
		}
		if (FridgeInteriorLight)
		{
			FridgeInteriorLight->SetVisibility(true);
		}
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH03", "FullFridgeThought", "누가… 새벽샘물로 가득 채워 놨어."),
			4.0f);
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH03.Flood.FridgeFull")), false));
		TryUnlockApartmentExit();
		CommitChapterThreeState();
		RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Apartment"));
		break;

	case EIGChapterThreeAction::RecoverFlashlight:
		if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
		{
			RebirthState->SetHasMemoryFlashlight(true);
		}
		if (APlayerController* PlayerController =
				GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (AIGPlayerCharacter* Player =
					Cast<AIGPlayerCharacter>(PlayerController->GetPawn()))
			{
				if (UIGFlashlightComponent* Flashlight = Player->GetFlashlight())
				{
					Flashlight->SetAvailable(true);
					Flashlight->SetOn(true);
				}
			}
		}
		SetVisibleInteractive(Source, false);
		ApplyTankRevealVisibility();
		ArmFlashlightReturnReveal();
		AIGHorrorHUD::PushThought(
			this,
			bTankOpened
				? NSLOCTEXT(
					"IGCH03",
					"FlashlightRecoveredAfterTank",
					"이제 물탱크 안을 제대로 확인할 수 있어.")
				: NSLOCTEXT(
					"IGCH03",
					"FlashlightRecovered",
					"이번에는 챙겨 가자."),
			3.4f);
		CommitChapterThreeState();
		RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Apartment"));
		break;

	case EIGChapterThreeAction::OpenApartmentDoor:
		if (!bAlarmStopped)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGCH03", "WaitAlarmThought", "알람이… 내 손도 안 댔는데."),
				2.8f);
			return;
		}
		if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
		{
			const bool bFirstOutfitPresentation =
				RebirthState->MarkOutfitEquipped(FName(TEXT("CH03")));
			if (APlayerController* PlayerController =
					GetWorld()->GetFirstPlayerController())
			{
				if (AIGPlayerCharacter* Player =
						Cast<AIGPlayerCharacter>(PlayerController->GetPawn()))
				{
					Player->SetRebirthOutfitEquipped(
						true,
						bFirstOutfitPresentation);
				}
			}
			if (bFirstOutfitPresentation)
			{
				AIGHorrorHUD::PushThought(
					this,
					NSLOCTEXT(
						"IGCH03",
						"OutfitRepairThought",
						"문고리를 잡자 왼쪽 소매의 검은 실 세 땀이 눈에 걸린다."),
					3.2f);
			}
		}
		SetVisibleInteractive(CurrentSleeveAction, true);
		if (Source)
		{
			Source->SetActorHiddenInGame(true);
			Source->SetActorEnableCollision(false);
			Source->SetInteractionEnabled(false);
		}
		break;

	case EIGChapterThreeAction::InspectElevator:
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH03", "DeadLiftThought", "불도, 호출음도 없다. 문틈에서 물만 나온다."),
			4.0f);
		PlayDelayedSplash();
		break;

	case EIGChapterThreeAction::InspectKeys:
		if (!bKeysInspected)
		{
			bKeysInspected = true;
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGCH03", "KeysThought", "관리사무소 열쇠다. 그날부터 꽂혀 있었던 건가."),
				4.2f);
			IGStory::AddState(
				this,
				FGameplayTag::RequestGameplayTag(
					FName(TEXT("State.CH03.Flood.FoundKeys")), false));
			if (RoofDoorAction)
			{
				RoofDoorAction->SetInteractionPrompt(
					NSLOCTEXT("IGCH03", "RoofDoorUnlockedPrompt", "열쇠로 옥상 철문 열기"));
			}
		}
		break;

	case EIGChapterThreeAction::OpenRoofDoor:
		if (Source)
		{
			Source->SetActorHiddenInGame(true);
			Source->SetActorEnableCollision(false);
			Source->SetInteractionEnabled(false);
		}
		EnterRoofSilence();
		break;

	case EIGChapterThreeAction::P3CloseDirectInlet:
		if (!bP3DirectClosed)
		{
			bP3DirectClosed = true;
			if (Source)
			{
				Source->SetInteractionPrompt(
					NSLOCTEXT("IGCH03", "P3DirectClosed", "직결 급수 잠김"));
				Source->SetInteractionEnabled(false);
			}
			PlayMetalEcho(FVector(600, -178, 164), 0.92f);
			CommitChapterThreeState();
			RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
		}
		break;

	case EIGChapterThreeAction::P3CloseReserveInlet:
		if (!bP3ReserveClosed)
		{
			bP3ReserveClosed = true;
			if (Source)
			{
				Source->SetInteractionPrompt(
					NSLOCTEXT("IGCH03", "P3ReserveClosed", "예비조 잠김"));
				Source->SetInteractionEnabled(false);
			}
			PlayMetalEcho(FVector(650, -178, 164), 0.88f);
			CommitChapterThreeState();
			RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
		}
		break;

	case EIGChapterThreeAction::P3OpenPressureRelease:
		if (bP3DirectClosed && bP3ReserveClosed)
		{
			BeginP3PressureRelease();
		}
		else
		{
			HandleP3Mistake();
		}
		break;

	case EIGChapterThreeAction::P3OpenFloorDrain:
		if (bP3DirectClosed
			&& bP3ReserveClosed
			&& bP3PressureReleaseOpen
			&& bP3PressureZero)
		{
			CompleteP3();
		}
		else
		{
			HandleP3Mistake();
		}
		break;

	case EIGChapterThreeAction::EvidenceCatEntered:
	case EIGChapterThreeAction::EvidenceCatExited:
	case EIGChapterThreeAction::EvidenceHosePaw:
	case EIGChapterThreeAction::EvidenceHoseImpact:
	case EIGChapterThreeAction::EvidenceBag:
	case EIGChapterThreeAction::EvidenceWetRung:
	case EIGChapterThreeAction::EvidenceHandSmear:
	case EIGChapterThreeAction::EvidenceGlasses:
	case EIGChapterThreeAction::EvidenceTankClothing:
	case EIGChapterThreeAction::EvidenceCurrentSleeve:
	case EIGChapterThreeAction::EvidenceSearchPoster:
		if (AccidentScratchCount == 2
			&& GetWorld()
			&& AccidentScratchGateStartSeconds >= 0.0
			&& GetWorld()->GetTimeSeconds()
				- AccidentScratchGateStartSeconds >= 4.0)
		{
			bActedAfterSecondScratch = true;
		}
		if (bTankOpened && HasMemoryFlashlight())
		{
			ScheduleAccidentScratch();
		}
		FocusOrCompareEvidence(Action);
		break;

	case EIGChapterThreeAction::OpenTank:
		RevealTank();
		break;

	case EIGChapterThreeAction::CloseTank:
		FinishEndingA();
		break;

	case EIGChapterThreeAction::SupportLid:
		BeginEndingB();
		break;

	case EIGChapterThreeAction::None:
	default:
		break;
	}
}

void AIGThirdMorningDirector::TryUnlockApartmentExit()
{
	if (ApartmentDoorAction && bAlarmStopped)
	{
		ApartmentDoorAction->SetInteractionPrompt(
			NSLOCTEXT("IGCH03", "ApartmentDoorReadyPrompt", "현관문 열기"));
	}
}

void AIGThirdMorningDirector::BeginP3PressureRelease()
{
	if (bP3Solved || bP3PressureReleaseOpen)
	{
		return;
	}

	bP3PressureReleaseOpen = true;
	P3ZeroConfirmationTicks = 0;
	if (P3PressureReleaseAction)
	{
		P3PressureReleaseAction->SetInteractionPrompt(
			NSLOCTEXT("IGCH03", "P3BleedOpen", "압력 해제 열림"));
		P3PressureReleaseAction->SetInteractionEnabled(false);
	}
	if (P3BleedWaterVisual)
	{
		P3BleedWaterVisual->SetVisibility(true);
	}
	PlayMetalEcho(FVector(705, -178, 112), 0.84f);
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
	GetWorldTimerManager().SetTimer(
		P3PressureTimer,
		this,
		&ThisClass::UpdateP3PressureRelease,
		0.1f,
		true);
}

void AIGThirdMorningDirector::UpdateP3PressureRelease()
{
	if (!bP3PressureReleaseOpen || bP3PressureZero)
	{
		GetWorldTimerManager().ClearTimer(P3PressureTimer);
		return;
	}

	if (P3PressureKPa > 0.0f)
	{
		P3PressureKPa = FMath::Max(0.0f, P3PressureKPa - 1.0f);
	}
	if (P3PressureText)
	{
		P3PressureText->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f kPa"), P3PressureKPa)));
	}
	if (P3PressureNeedle)
	{
		const float Alpha = 1.0f - P3PressureKPa / 60.0f;
		P3PressureNeedle->SetRelativeRotation(
			FRotator(0, 0, FMath::Lerp(-55.0f, 55.0f, Alpha)));
	}
	if (P3BleedWaterVisual)
	{
		const float FlowScale = FMath::Clamp(
			P3PressureKPa / 60.0f,
			0.08f,
			1.0f);
		P3BleedWaterVisual->SetRelativeScale3D(
			FVector(0.62f * FlowScale, 0.038f, 0.038f));
	}

	if (P3PressureKPa <= 0.0f)
	{
		P3ZeroConfirmationTicks =
			FMath::Min(P3ZeroConfirmationTicks + 1, 2);
		PlayMetalEcho(
			FVector(755, -178, 178),
			P3ZeroConfirmationTicks == 1 ? 1.12f : 1.18f);
		RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
		if (P3ZeroConfirmationTicks >= 2)
		{
			bP3PressureZero = true;
			GetWorldTimerManager().ClearTimer(P3PressureTimer);
			if (P3BleedWaterVisual)
			{
				P3BleedWaterVisual->SetVisibility(false);
			}
			if (P3FloorDrainAction)
			{
				P3FloorDrainAction->SetInteractionPrompt(
					NSLOCTEXT("IGCH03", "P3DrainReady", "0 확인 후 바닥 배수 열기"));
			}
		}
	}
	CommitChapterThreeState();
	if (FMath::IsNearlyZero(FMath::Fmod(P3PressureKPa, 15.0f)))
	{
		// Preserve bounded, deterministic in-flight points without issuing a
		// disk request on every 0.1 second pressure update.
		RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
	}
}

void AIGThirdMorningDirector::HandleP3Mistake()
{
	P3MistakeCount = FMath::Min(P3MistakeCount + 1, 3);
	if (CorridorWaterVisual)
	{
		FVector WaterLocation = CorridorWaterVisual->GetRelativeLocation();
		WaterLocation.Z = 2.0f + P3MistakeCount * 5.0f;
		CorridorWaterVisual->SetRelativeLocation(WaterLocation);
	}
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
	PlayDelayedSplash();
	PlayMetalEcho(FVector(675, -190, 115), 0.70f);
	if (APlayerController* Controller =
			GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (AIGPlayerCharacter* Player =
				Cast<AIGPlayerCharacter>(Controller->GetPawn()))
		{
			if (UIGFlashlightComponent* Flashlight = Player->GetFlashlight())
			{
				Flashlight->TriggerBrownOut(1.2f);
			}
		}
	}
	AIGHorrorHUD::PushThought(
		this,
		bP3DirectClosed && bP3ReserveClosed
			? NSLOCTEXT("IGCH03", "P3WaitForZero", "아직 압력이 남아 있어.")
			: NSLOCTEXT("IGCH03", "P3CloseInflows", "들어오는 물부터 모두 막아야 해."),
		2.8f);
}

void AIGThirdMorningDirector::CompleteP3()
{
	if (bP3Solved)
	{
		return;
	}
	bP3Solved = true;
	StopP3HintClock();
	if (SearchPosterAction)
	{
		SearchPosterAction->SetActorLocation(
			ToWorld(FVector(992.0f, -84.0f, 150.0f)));
	}
	if (P3FloorDrainAction)
	{
		P3FloorDrainAction->SetInteractionPrompt(
			NSLOCTEXT("IGCH03", "P3DrainOpen", "바닥 배수 열림"));
		P3FloorDrainAction->SetInteractionEnabled(false);
	}
	if (CorridorWaterVisual)
	{
		FVector WaterLocation = CorridorWaterVisual->GetRelativeLocation();
		WaterLocation.Z = 2.0f;
		CorridorWaterVisual->SetRelativeLocation(WaterLocation);
	}
	// Solving P3 exposes only the empty service record. The later 04:03 photo
	// and 06:20 false-completion log belong to the roof investigation folder,
	// so the document-only route never depends on operating these valves.
	SetDocumentAvailable(P3RecordNote, true);
	if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
	{
		RebirthState->MarkPuzzleResolved(FName(TEXT("P3")));
	}
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGCH03",
			"P3SolvedThought",
			"끝낸 작업이면 이 수치들이 남아 있어야 해."),
		3.8f);
}

void AIGThirdMorningDirector::FocusOrCompareEvidence(
	const EIGChapterThreeAction EvidenceAction)
{
	if (EvidenceAction == EIGChapterThreeAction::EvidenceBag)
	{
		PresentPurchaseEvidenceContinuity();
	}

	if (EvidenceAction == EIGChapterThreeAction::EvidenceGlasses
		&& !bGlassesInspected)
	{
		bGlassesInspected = true;
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH03.Flood.FoundGlasses")), false));
		PlayMetalEcho(FVector(2265, -300, 360), 1.0f);
	}
	if (EvidenceAction == EIGChapterThreeAction::EvidenceCurrentSleeve)
	{
		if (APlayerController* Controller =
				GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (AIGPlayerCharacter* Player =
					Cast<AIGPlayerCharacter>(Controller->GetPawn()))
			{
				Player->SetRebirthOutfitEquipped(true, true);
			}
		}
	}

	const bool bComparisonWasArmed =
		FocusedEvidence != EIGChapterThreeAction::None
		&& FocusedEvidence != EvidenceAction
		&& IsEvidencePairValid(FocusedEvidence, EvidenceAction);
	RegisterEvidenceObservation(EvidenceAction);
	const TCHAR* EvidenceCheckpoint =
		Phase >= EIGThirdMorningPhase::Roof
			? TEXT("Checkpoint.CH03.Roof")
			: Phase <= EIGThirdMorningPhase::ApartmentClues
				? TEXT("Checkpoint.CH03.Apartment")
				: TEXT("Checkpoint.CH03.Flood");

	if (FocusedEvidence == EIGChapterThreeAction::None)
	{
		FocusedEvidence = EvidenceAction;
		CommitChapterThreeState();
		RequestCheckpointAutosave(EvidenceCheckpoint);
		RefreshEvidencePrompts();
		return;
	}
	if (FocusedEvidence == EvidenceAction)
	{
		return;
	}

	const EIGChapterThreeAction Previous = FocusedEvidence;
	if (bComparisonWasArmed
		&& ResolveEvidencePair(Previous, EvidenceAction))
	{
		FocusedEvidence = EIGChapterThreeAction::None;
	}
	else
	{
		// There is no wrong-answer bark. The most recent physical clue simply
		// becomes the new comparison anchor.
		FocusedEvidence = EvidenceAction;
	}
	CommitChapterThreeState();
	RequestCheckpointAutosave(EvidenceCheckpoint);
	RefreshEvidencePrompts();
}

void AIGThirdMorningDirector::RegisterEvidenceObservation(
	const EIGChapterThreeAction EvidenceAction)
{
	const bool bFirstSearchPosterObservation =
		EvidenceAction == EIGChapterThreeAction::EvidenceSearchPoster
		&& !ObservedP5Sources.Contains(
			FName(TEXT("P5.SearchPosterOutfit")));
	for (const FName SourceId : IGThirdMorning::EvidenceSources(EvidenceAction))
	{
		ObservedP5Sources.AddUnique(SourceId);
	}
	if (bFirstSearchPosterObservation)
	{
		RegisterTruth(
			TEXT("Truth.WasSearched"),
			TEXT("CH03.SearchPoster"));
	}
}

void AIGThirdMorningDirector::PresentPurchaseEvidenceContinuity()
{
	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState
		|| !RebirthState->MarkOneShotBeatPlayed(
			FName(TEXT("P5.PurchaseChoiceContinuity"))))
	{
		return;
	}

	const FIGRebirthChoiceState Choices = RebirthState->GetChoices();
	FText ProductText;
	switch (Choices.PurchaseProfile)
	{
	case EIGRebirthPurchaseProfile::ProfileB1LX1:
		ProductText = NSLOCTEXT("IGCH03", "P5BagProfileB", "1L 한 병");
		break;
	case EIGRebirthPurchaseProfile::ProfileC2LX2:
		ProductText = NSLOCTEXT("IGCH03", "P5BagProfileC", "2L 두 병");
		break;
	case EIGRebirthPurchaseProfile::ProfileA500MlX2:
	case EIGRebirthPurchaseProfile::Unset:
	default:
		ProductText = NSLOCTEXT("IGCH03", "P5BagProfileA", "500mL 두 병");
		break;
	}

	FText ClosureText;
	if (Choices.BottleClosureState
		== EIGRebirthBottleClosureState::MissingCap)
	{
		ClosureText = NSLOCTEXT(
			"IGCH03",
			"P5BagMissingCap",
			"마신 병의 수위가 낮고 뚜껑은 없다. 기다리지 않고 두고 온 그대로다.");
	}
	else if (Choices.CatWaterState == EIGRebirthCatWaterState::PaperCup)
	{
		ClosureText = NSLOCTEXT(
			"IGCH03",
			"P5BagPaperCup",
			"마신 병의 수위가 더 낮지만 뚜껑은 다시 잠겨 있다. 종이컵에 따라 줬던 그대로다.");
	}
	else if (Choices.bWaitedForCat)
	{
		ClosureText = NSLOCTEXT(
			"IGCH03",
			"P5BagWaited",
			"마신 병의 수위가 낮고 뚜껑은 다시 잠겨 있다. 다 마실 때까지 기다렸던 그대로다.");
	}
	else
	{
		ClosureText = NSLOCTEXT(
			"IGCH03",
			"P5BagResealed",
			"마신 병의 수위가 낮고 뚜껑은 다시 잠겨 있다. 내가 놓은 상태 그대로다.");
	}

	AIGHorrorHUD::PushThought(
		this,
		FText::Format(
			NSLOCTEXT(
				"IGCH03",
				"P5BagChoiceContinuity",
				"{0}. {1}"),
			ProductText,
			ClosureText),
		5.4f);
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
}

void AIGThirdMorningDirector::RefreshEvidencePrompts()
{
	for (const TPair<EIGChapterThreeAction, TObjectPtr<AIGChapterThreeAction>>& Pair
		: EvidenceActions)
	{
		AIGChapterThreeAction* Evidence = Pair.Value;
		if (!Evidence || Evidence->IsHidden())
		{
			continue;
		}
		if (FocusedEvidence == EIGChapterThreeAction::None)
		{
			Evidence->SetInteractionPrompt(
				NSLOCTEXT("IGCH03", "EvidenceInspectGeneric", "흔적 확인하기"));
			Evidence->SetHoldSeconds(0.0f);
		}
		else if (Pair.Key == FocusedEvidence)
		{
			Evidence->SetInteractionPrompt(
				NSLOCTEXT("IGCH03", "EvidenceFocused", "기억한 흔적"));
			Evidence->SetHoldSeconds(0.0f);
		}
		else if (IsEvidencePairValid(FocusedEvidence, Pair.Key))
		{
			Evidence->SetInteractionPrompt(
				NSLOCTEXT("IGCH03", "EvidenceCompare", "앞의 흔적과 대조한다"));
			Evidence->SetHoldSeconds(0.8f);
		}
		else
		{
			Evidence->SetInteractionPrompt(
				NSLOCTEXT("IGCH03", "EvidenceInspectGeneric", "흔적 확인하기"));
			Evidence->SetHoldSeconds(0.0f);
		}
	}
}

bool AIGThirdMorningDirector::IsEvidencePairValid(
	const EIGChapterThreeAction First,
	const EIGChapterThreeAction Second) const
{
	const auto IsPair = [First, Second](
		const EIGChapterThreeAction A,
		const EIGChapterThreeAction B)
	{
		return (First == A && Second == B) || (First == B && Second == A);
	};

	const bool bDirectPair = IsPair(
			EIGChapterThreeAction::EvidenceCatEntered,
			EIGChapterThreeAction::EvidenceCatExited)
		|| IsPair(
			EIGChapterThreeAction::EvidenceHosePaw,
			EIGChapterThreeAction::EvidenceHoseImpact)
		|| IsPair(
			EIGChapterThreeAction::EvidenceWetRung,
			EIGChapterThreeAction::EvidenceHandSmear)
		|| IsPair(
			EIGChapterThreeAction::EvidenceCurrentSleeve,
			EIGChapterThreeAction::EvidenceTankClothing);
	if (bDirectPair)
	{
		return true;
	}

	const bool bPosterTriangulationPair = IsPair(
			EIGChapterThreeAction::EvidenceSearchPoster,
			EIGChapterThreeAction::EvidenceGlasses)
		|| IsPair(
			EIGChapterThreeAction::EvidenceSearchPoster,
			EIGChapterThreeAction::EvidenceTankClothing)
		|| IsPair(
			EIGChapterThreeAction::EvidenceGlasses,
			EIGChapterThreeAction::EvidenceTankClothing);
	TArray<FName> CandidateSources = ObservedP5Sources;
	for (const FName SourceId : IGThirdMorning::EvidenceSources(First))
	{
		CandidateSources.AddUnique(SourceId);
	}
	for (const FName SourceId : IGThirdMorning::EvidenceSources(Second))
	{
		CandidateSources.AddUnique(SourceId);
	}
	return bPosterTriangulationPair
		&& CandidateSources.Contains(
			FName(TEXT("P5.SearchPosterOutfit")))
		&& CandidateSources.Contains(FName(TEXT("P5.Glasses")))
		&& CandidateSources.Contains(
			FName(TEXT("P5.TankOutfit")));
}

bool AIGThirdMorningDirector::ResolveEvidencePair(
	const EIGChapterThreeAction First,
	const EIGChapterThreeAction Second)
{
	if (!IsEvidencePairValid(First, Second))
	{
		return false;
	}

	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState)
	{
		return false;
	}

	const auto IsPair = [First, Second](
		const EIGChapterThreeAction A,
		const EIGChapterThreeAction B)
	{
		return (First == A && Second == B) || (First == B && Second == A);
	};
	const auto HasTruth = [RebirthState](const TCHAR* TruthName)
	{
		return RebirthState->HasTruth(FGameplayTag::RequestGameplayTag(
			FName(TruthName),
			false));
	};

	const TCHAR* TruthName = nullptr;
	if (IsPair(
			EIGChapterThreeAction::EvidenceCatEntered,
			EIGChapterThreeAction::EvidenceCatExited))
	{
		TruthName = TEXT("Truth.CatSafe");
	}
	else if (IsPair(
				EIGChapterThreeAction::EvidenceHosePaw,
				EIGChapterThreeAction::EvidenceHoseImpact))
	{
		TruthName = TEXT("Truth.HoseCause");
	}
	else if (IsPair(
				EIGChapterThreeAction::EvidenceWetRung,
				EIGChapterThreeAction::EvidenceHandSmear))
	{
		TruthName = TEXT("Truth.Fall");
	}
	else
	{
		TruthName = TEXT("Truth.Identity");
	}

	const bool bWasConfirmed = HasTruth(TruthName);
	TArray<FName> SourcesToCommit =
		IGThirdMorning::EvidenceSources(First);
	for (const FName SourceId : IGThirdMorning::EvidenceSources(Second))
	{
		SourcesToCommit.AddUnique(SourceId);
	}
	if (FCString::Strcmp(TruthName, TEXT("Truth.Identity")) == 0
		&& !IsPair(
			EIGChapterThreeAction::EvidenceCurrentSleeve,
			EIGChapterThreeAction::EvidenceTankClothing))
	{
		for (const FName RequiredSource : {
			FName(TEXT("P5.SearchPosterOutfit")),
			FName(TEXT("P5.Glasses")),
			FName(TEXT("P5.TankOutfit"))})
		{
			if (ObservedP5Sources.Contains(RequiredSource))
			{
				SourcesToCommit.AddUnique(RequiredSource);
			}
		}
	}
	for (const FName SourceId : SourcesToCommit)
	{
		RegisterTruth(TruthName, SourceId, false);
	}

	const bool bConfirmedNow = HasTruth(TruthName);
	if (!bWasConfirmed && bConfirmedNow)
	{
		if (FCString::Strcmp(TruthName, TEXT("Truth.CatSafe")) == 0)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGCH03", "P5CatSafe", "고양이는 나갔어."),
				4.0f);
		}
		else if (FCString::Strcmp(TruthName, TEXT("Truth.HoseCause")) == 0)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGCH03", "P5HoseCause", "물을 친 건… 호스였고."),
				4.0f);
		}
		else if (FCString::Strcmp(TruthName, TEXT("Truth.Fall")) == 0)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGCH03",
					"P5Fall",
					"자국, 들뜬 패드, 녹슨 클립. 여기서 미끄러져 안쪽으로 쓸렸어."),
				4.4f);
		}
		else
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGCH03",
					"P5Identity",
					"복장과 수선 자국이 이어져. 저 안에 있던 게, 나였어."),
				4.4f);
		}

		if (GetAccidentTruthCount() == 4)
		{
			RebirthState->MarkPuzzleResolved(FName(TEXT("P5")));
		}
		CommitChapterThreeState();
		ScheduleAccidentScratch();
		UpdateEndingAvailability();
	}
	return bConfirmedNow;
}

int32 AIGThirdMorningDirector::GetAccidentTruthCount() const
{
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState)
	{
		return 0;
	}
	int32 Count = 0;
	for (const TCHAR* TruthName : {
		TEXT("Truth.CatSafe"),
		TEXT("Truth.HoseCause"),
		TEXT("Truth.Fall"),
		TEXT("Truth.Identity")})
	{
		Count += RebirthState->HasTruth(
			FGameplayTag::RequestGameplayTag(FName(TruthName), false))
			? 1
			: 0;
	}
	return Count;
}

void AIGThirdMorningDirector::UpdateEndingAvailability()
{
	if (bEndingFinished)
	{
		SetVisibleInteractive(CloseChoiceAction, false);
		SetVisibleInteractive(SupportChoiceAction, false);
		return;
	}
	const bool bReady = IsEndingChoiceReady();
	SetVisibleInteractive(CloseChoiceAction, bReady);
	SetVisibleInteractive(SupportChoiceAction, bReady);
	if (bReady)
	{
		SetPhase(EIGThirdMorningPhase::Choice);
	}
}

bool AIGThirdMorningDirector::IsEndingChoiceReady() const
{
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	return bTankOpened
		&& AccidentScratchCount == 3
		&& bAccidentScratchTailSettled
		&& RebirthState
		&& RebirthState->CanConverge(
			EIGRebirthConvergencePoint::C5FinalChoice);
}

void AIGThirdMorningDirector::ScheduleAccidentScratch()
{
	if (!bTankOpened
		|| !HasMemoryFlashlight()
		|| GetWorldTimerManager().IsTimerActive(AccidentScratchTimer))
	{
		return;
	}
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (RebirthState
		&& RebirthState->WasOneShotBeatPlayed(
			FName(TEXT("CH03.TankOpenedWithoutFlashlight")))
		&& !RebirthState->WasOneShotBeatPlayed(
			FName(TEXT("CH03.TankRevealedAfterFlashlight"))))
	{
		// Restoring at 404 must not consume the first scratch. The rooftop
		// return reveal owns that beat after the player crosses its zone.
		return;
	}
	const int32 TruthCount = GetAccidentTruthCount();
	const bool bShouldPlay =
		AccidentScratchCount == 0
		|| (AccidentScratchCount == 1 && TruthCount >= 1)
		|| (AccidentScratchCount == 2 && TruthCount >= 3);
	if (!bShouldPlay || AccidentScratchCount >= 3)
	{
		return;
	}
	if (AccidentScratchCount == 0)
	{
		GetWorldTimerManager().SetTimer(
			AccidentScratchTimer,
			this,
			&ThisClass::PlayAccidentScratch,
			2.0f,
			false);
		return;
	}

	if (AccidentScratchGateStartSeconds < 0.0)
	{
		AccidentScratchGateStartSeconds =
			GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
	if (AccidentScratchCount == 2)
	{
		bScratchGatePlayerWasMoving = false;
		if (APlayerController* Controller =
				GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (const APawn* Pawn = Controller->GetPawn())
			{
				bScratchGatePlayerWasMoving =
					Pawn->GetVelocity().Size2D() >= 12.0f;
			}
		}
	}
	GetWorldTimerManager().SetTimer(
		AccidentScratchTimer,
		this,
		&ThisClass::EvaluateAccidentScratchGate,
		0.15f,
		true);
}

void AIGThirdMorningDirector::EvaluateAccidentScratchGate()
{
	if (!GetWorld()
		|| !bTankOpened
		|| AccidentScratchCount < 1
		|| AccidentScratchCount >= 3)
	{
		GetWorldTimerManager().ClearTimer(AccidentScratchTimer);
		return;
	}

	const double Elapsed =
		GetWorld()->GetTimeSeconds() - AccidentScratchGateStartSeconds;
	bool bLatchChanged = false;
	if (AccidentScratchCount == 1 && !bLookedAwayAfterFirstScratch)
	{
		if (APlayerController* Controller =
				GetWorld()->GetFirstPlayerController())
		{
			FVector ViewLocation = FVector::ZeroVector;
			FRotator ViewRotation = FRotator::ZeroRotator;
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
			const FVector ToTank = (
				ToWorld(IGThirdMorning::TankCenter + FVector(0, 0, 430))
				- ViewLocation).GetSafeNormal();
			const float TankAttention =
				FVector::DotProduct(ViewRotation.Vector(), ToTank);
			if (TankAttention < 0.25f)
			{
				bLookedAwayAfterFirstScratch = true;
				bLatchChanged = true;
			}
		}
	}
	else if (AccidentScratchCount == 2
		&& !bActedAfterSecondScratch)
	{
		if (APlayerController* Controller =
				GetWorld()->GetFirstPlayerController())
		{
			if (const APawn* Pawn = Controller->GetPawn())
			{
				const bool bMoving =
					Pawn->GetVelocity().Size2D() >= 12.0f;
				if (Elapsed >= 4.0
					&& bMoving
					&& !bScratchGatePlayerWasMoving)
				{
					bActedAfterSecondScratch = true;
					bLatchChanged = true;
				}
				bScratchGatePlayerWasMoving = bMoving;
			}
		}
	}

	if (bLatchChanged)
	{
		CommitChapterThreeState();
		RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
	}

	const bool bGateSatisfied =
		(AccidentScratchCount == 1
			&& Elapsed >= 3.0
			&& bLookedAwayAfterFirstScratch)
		|| (AccidentScratchCount == 2
			&& Elapsed >= 4.0
			&& bActedAfterSecondScratch);
	if (bGateSatisfied)
	{
		PlayAccidentScratch();
	}
}

void AIGThirdMorningDirector::PlayAccidentScratch()
{
	if (AccidentScratchCount >= 3)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(AccidentScratchTimer);
	AccidentScratchGateStartSeconds = -1.0;
	const float Height = 410.0f + AccidentScratchCount * 68.0f;
	PlayMetalEcho(
		IGThirdMorning::TankCenter + FVector(-42, 18, Height),
		0.88f - AccidentScratchCount * 0.07f);
	++AccidentScratchCount;
	if (AccidentScratchCount == 1)
	{
		bLookedAwayAfterFirstScratch = false;
		AccidentScratchGateStartSeconds =
			GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
	else if (AccidentScratchCount == 2)
	{
		bActedAfterSecondScratch = false;
		AccidentScratchGateStartSeconds =
			GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
	else if (AccidentScratchCount == 3)
	{
		bAccidentScratchTailSettled = false;
		GetWorldTimerManager().SetTimer(
			AccidentScratchTailTimer,
			this,
			&ThisClass::SettleAccidentScratchTail,
			0.6f,
			false);
	}
	if (AccidentScratchCount < 3)
	{
		ScheduleAccidentScratch();
	}
	CommitChapterThreeState();
	// Every audible scratch is a committed one-shot boundary. The stable-save
	// normalizer advances the 0.6-second third-scratch tail, so no scratch is
	// replayed when the player quits between evidence comparisons.
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
	UpdateEndingAvailability();
}

void AIGThirdMorningDirector::SettleAccidentScratchTail()
{
	if (AccidentScratchCount != 3)
	{
		return;
	}
	bAccidentScratchTailSettled = true;
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
	UpdateEndingAvailability();
}

void AIGThirdMorningDirector::HandleClueRead(
	AIGReadableNote* Note,
	const bool bOpened)
{
	if (!bOpened || !Note)
	{
		return;
	}

	if (Note == PlannerNote && !bPlannerRead)
	{
		bPlannerRead = true;
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH03", "PlannerThought", "내 알람은… 5시 10분이었는데."),
			4.0f);
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH03.Flood.ReadPlanner")), false));
		RegisterTruth(TEXT("Truth.Alarm0510"), TEXT("CH03.Planner0510"));
		TryUnlockApartmentExit();
	}
	else if (Note == MotherPhoneNote && !bMotherPhoneRead)
	{
		bMotherPhoneRead = true;
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH03.Flood.ReadMotherPhone")), false));
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH03",
				"MotherPhoneThought",
				"답장을… 언제부터 못 했지."),
			4.0f);
		RegisterTruth(TEXT("Truth.WasSearched"), TEXT("CH03.MotherMessages"));
		// The phone stayed on the desk in both payment branches. Its card log
		// is the physical CH03 fallback promised when P2 was skipped: the
		// player can see 04:31 and 04:44 together before this truth is granted.
		RegisterTruth(
			TEXT("Truth.DeathOverlay"),
			TEXT("CH03.PhoneApprovalHistory"));
		TryUnlockApartmentExit();
	}
	else if (Note == OfferingNote)
	{
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH03.Flood.ReadOffering")), false));
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH03", "OfferingThought", "물그릇이 비었어. 할머니가 두고 갔던 건데."),
			3.8f);
	}
	else if (Note == EstimateNote && !bEstimateRead)
	{
		bEstimateRead = true;
		IGStory::AddState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH03.Flood.ReadEstimate")), false));
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH03",
				"EstimateThought",
				"03시 58분. 멈추고 닫았다는 원본은 여기 남아 있어."),
			4.6f);
		RegisterTruth(
			TEXT("Truth.Negligence"),
			TEXT("Handover.Closed0358"),
			false);
		UpdateEndingAvailability();
	}
	else if (Note == RecheckNoticeNote && !bRecheckNoticeRead)
	{
		bRecheckNoticeRead = true;
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH03",
				"RecheckNoticeThought",
				"7월 31일 새벽, 경찰과 소방이 다시 열기로 했어."),
			4.8f);
		RegisterTruth(
			TEXT("Truth.RecheckScheduled0731"),
			TEXT("CH03.VendorRevisitNotice"));
		UpdateEndingAvailability();
	}
	else if (Note == PreservationNoticeNote && !bPreservationNoticeRead)
	{
		bPreservationNoticeRead = true;
		RegisterTruth(
			TEXT("Truth.RecheckScheduled0731"),
			TEXT("CH03.JointInspectionNotice"));
		UpdateEndingAvailability();
	}
	else if (Note == PoliceChecklistNote && !bPoliceChecklistRead)
	{
		bPoliceChecklistRead = true;
		RegisterTruth(
			TEXT("Truth.Negligence"),
			TEXT("PoliceChecklist.PhotoAndAudit"),
			false);
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH03",
				"PoliceChecklistThought",
				"사진은 있었고, 계측은 없었고, 완료 입력은 사고 뒤였다."),
			4.8f);
		UpdateEndingAvailability();
	}
	else if (Note == P3RecordNote && !bP3RecordRead)
	{
		bP3RecordRead = true;
		RegisterTruth(
			TEXT("Truth.Negligence"),
			TEXT("P3.EmptyMeasurements"),
			false);
	}
	else if (Note == P3PhotoNote && !bP3PhotoRead)
	{
		bP3PhotoRead = true;
		RegisterTruth(
			TEXT("Truth.Negligence"),
			TEXT("ManagementApp.Photo0403"),
			false);
	}
	else if (Note == ManagementDbNote && !bManagementDbRead)
	{
		bManagementDbRead = true;
		RegisterTruth(
			TEXT("Truth.Negligence"),
			TEXT("ManagementDb.FalseCompletion0620"),
			false);
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH03",
				"ManagementDbThought",
				"끝났다고 적은 건… 사고 뒤 두 시간이나 지나서였어."),
			4.5f);
	}
	const bool bApartmentDocument =
		Note == PlannerNote || Note == MotherPhoneNote;
	RequestCheckpointAutosave(
		bApartmentDocument
			? TEXT("Checkpoint.CH03.Apartment")
			: Phase >= EIGThirdMorningPhase::Roof
				? TEXT("Checkpoint.CH03.Roof")
				: TEXT("Checkpoint.CH03.Flood"));
}

void AIGThirdMorningDirector::HandleCorridorEntered(AIGZoneTrigger* Zone)
{
	SetPhase(EIGThirdMorningPhase::FloodedCorridor);
	SetFloodMovement(true);
	StartWaterBed();
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGCH03", "FloodedHallThought", "이게… 내가 본 건물 맞아?"),
		3.6f);

	LastPlayerMovingTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	NextDelayedSplashTime = LastPlayerMovingTime + 7.0;
	GetWorldTimerManager().SetTimer(
		MotionPollTimer,
		this,
		&ThisClass::PollPlayerMotion,
		0.1f,
		true);
	if (!bP3Solved
		&& !GetWorldTimerManager().IsTimerActive(P3HintTimer))
	{
		GetWorldTimerManager().SetTimer(
			P3HintTimer,
			this,
			&ThisClass::PollP3Hint,
			1.0f,
			true);
	}
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
}

void AIGThirdMorningDirector::PollP3Hint()
{
	if (bP3Solved || Phase >= EIGThirdMorningPhase::Roof)
	{
		StopP3HintClock();
		return;
	}
	APlayerController* Controller =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn
		|| FVector::DistSquared(
			Pawn->GetActorLocation(),
			ToWorld(FVector(675, -190, 132)))
			> FMath::Square(420.0f))
	{
		// Hint time is cabinet dwell time. Exploring the stairwell pauses it,
		// and walking back into range resumes from the persisted value.
		return;
	}

	P3HintElapsedSeconds += 1.0f;
	// Keep the in-memory narrative snapshot exact even between authored hint
	// thresholds. Disk autosaves stay bounded, but a manual save or checkpoint
	// now captures the real cabinet dwell time instead of the previous stage.
	CommitChapterThreeState();
	const float Thresholds[] = {90.0f, 150.0f, 210.0f};
	if (P3HintStage >= UE_ARRAY_COUNT(Thresholds)
		|| P3HintElapsedSeconds < Thresholds[P3HintStage])
	{
		return;
	}

	if (P3HintStage == 0)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
			"IGCH03",
			"P3HintInflows",
			"직결과 예비조. 이름은 달라도 같은 작업관으로 들어오는 두 유입이야."),
			4.6f);
	}
	else if (P3HintStage == 1)
	{
		P3HintHighlightedAction =
			!bP3DirectClosed
				? P3DirectInletAction
				: !bP3ReserveClosed
					? P3ReserveInletAction
					: !bP3PressureReleaseOpen
						? P3PressureReleaseAction
						: !bP3PressureZero
							? P3PressureReleaseAction
							: P3FloorDrainAction;
		if (P3HintHighlightedAction
			&& P3HintHighlightedAction->GetPresentationMesh())
		{
			P3HintHighlightedAction->GetPresentationMesh()->SetMaterial(
				0,
				EmergencyMaterial);
			GetWorldTimerManager().SetTimer(
				P3HintVisualTimer,
				this,
				&ThisClass::RestoreP3HintHighlight,
				3.0f,
				false);
		}
		PlayMetalEcho(
			P3PressureReleaseOpen
				? FVector(755, -178, 178)
				: FVector(705, -178, 112),
			1.04f);
	}
	else
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
			"IGCH03",
			"P3HintFullOrder",
			"두 유입 차단, 압력 해제, 0에서 두 번 확인, 마지막으로 바닥 배수."),
			5.2f);
	}
	++P3HintStage;
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
}

void AIGThirdMorningDirector::RestoreP3HintHighlight()
{
	if (P3HintHighlightedAction
		&& P3HintHighlightedAction->GetPresentationMesh())
	{
		P3HintHighlightedAction->GetPresentationMesh()->SetMaterial(
			0,
			MetalMaterial);
	}
	P3HintHighlightedAction = nullptr;
}

void AIGThirdMorningDirector::StopP3HintClock()
{
	GetWorldTimerManager().ClearTimer(P3HintTimer);
	GetWorldTimerManager().ClearTimer(P3HintVisualTimer);
	RestoreP3HintHighlight();
}

void AIGThirdMorningDirector::HandleStairLoop(AIGZoneTrigger* Zone)
{
	const int32 ZoneIndex = StairLoopZones.IndexOfByKey(Zone);
	if (ZoneIndex == INDEX_NONE || ZoneIndex != StairLoopCount)
	{
		return;
	}

	++StairLoopCount;
	SetPhase(EIGThirdMorningPhase::StairLoops);
	IGStory::AddState(
		this,
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH03.Flood.StairsLooped")), false));

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			Pawn->SetActorLocation(
				ToWorld(FVector(1085, -5, IGThirdMorning::StandingCapsuleCenter)),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			PlayerController->SetControlRotation(FRotator(-5, 90, 0));
		}
	}

	UpdateStairSign();
	PlayMetalEcho(FVector(985, 24, 145), 1.0f - StairLoopCount * 0.045f);
	AIGHorrorHUD::PushThought(
		this,
		StairLoopCount < 3
			? NSLOCTEXT("IGCH03", "LoopAgainThought", "분명 내려왔는데… 또 4층이야.")
			: NSLOCTEXT("IGCH03", "UpOnlyThought", "…위로 갈 수밖에 없다."),
		3.8f);

	if (StairLoopCount < 3 && StairLoopZones.IsValidIndex(StairLoopCount))
	{
		StairLoopZones[StairLoopCount]->SetActorEnableCollision(true);
	}
	else
	{
		RevealUpwardRoute();
	}
}

void AIGThirdMorningDirector::UpdateStairSign()
{
	if (StairSignText)
	{
		StairSignText->SetVisibility(true);
		StairSignText->SetText(FText::FromString(TEXT("4F")));
		StairSignText->SetTextRenderColor(
			StairLoopCount >= 2
				? FColor(152, 164, 154)
				: FColor(188, 194, 184));
	}
	if (StairRoofText)
	{
		StairRoofText->SetVisibility(StairLoopCount >= 3);
	}
	for (UStaticMeshComponent* Drip : StairLatinSignParts)
	{
		if (Drip)
		{
			Drip->SetVisibility(StairLoopCount == 2);
		}
	}
	for (UStaticMeshComponent* ArrowStroke : StairSignParts)
	{
		if (ArrowStroke)
		{
			ArrowStroke->SetVisibility(StairLoopCount >= 3);
		}
	}
}

void AIGThirdMorningDirector::RevealUpwardRoute()
{
	SetPhase(EIGThirdMorningPhase::UpwardOnly);
	if (UpRouteGate)
	{
		UpRouteGate->SetVisibility(false);
		UpRouteGate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (DownRouteWaterBarrier)
	{
		DownRouteWaterBarrier->SetVisibility(true);
		DownRouteWaterBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AIGThirdMorningDirector::HandleFifthFloorEntered(AIGZoneTrigger* Zone)
{
	SetPhase(EIGThirdMorningPhase::FifthFloor);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGCH03", "OfferingArrivalThought", "물그릇이 비었어. 할머니가 두고 갔던 건데."),
		4.0f);
}

void AIGThirdMorningDirector::EnterRoofSilence()
{
	SetPhase(EIGThirdMorningPhase::Roof);
	StopP3HintClock();
	SetDocumentAvailable(P3PhotoNote, false);
	SetDocumentAvailable(ManagementDbNote, false);
	SetDocumentAvailable(PreservationNoticeNote, false);
	SetDocumentAvailable(PoliceChecklistNote, false);
	SetFloodMovement(false);
	if (WaterBedComponent)
	{
		WaterBedComponent->FadeOut(1.1f, 0.0f);
		WaterBedComponent = nullptr;
	}
	GetWorldTimerManager().ClearTimer(MotionPollTimer);

	IGStory::AddState(
		this,
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH03.Flood.ReachedRoof")), false));
	if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
	{
		RebirthState->MarkLocationVisited(FName(TEXT("CH03.Roof")));
		if (!bP3Solved)
		{
			RebirthState->MarkPuzzleSkipped(FName(TEXT("P3")));
		}
	}
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));

	// Only the low-pass tail of the familiar delivery chain survives the door.
	PlayMetalEcho(FVector(1660, -400, 285), 0.78f);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGCH03",
			"RoofSilenceThought",
			"그날도 문은 열려 있었다. 위에서 고양이가 긁는 줄 알았는데… 지금은 아무것도 없다."),
		6.0f);
	GetWorldTimerManager().SetTimer(
		RoofFolderRevealTimer,
		this,
		&ThisClass::RevealRoofInvestigationFolder,
		2.2f,
		false);
}

void AIGThirdMorningDirector::RevealRoofInvestigationFolder()
{
	SetDocumentAvailable(P3PhotoNote, true);
	SetDocumentAvailable(ManagementDbNote, true);
	SetDocumentAvailable(PreservationNoticeNote, true);
	SetDocumentAvailable(PoliceChecklistNote, true);
}

void AIGThirdMorningDirector::HandleLadderEntered(AIGZoneTrigger* Zone)
{
	if (bEndingFinished)
	{
		return;
	}
	if (AccidentScratchCount == 2
		&& GetWorld()
		&& AccidentScratchGateStartSeconds >= 0.0
		&& GetWorld()->GetTimeSeconds()
			- AccidentScratchGateStartSeconds >= 4.0)
	{
		bActedAfterSecondScratch = true;
		CommitChapterThreeState();
		ScheduleAccidentScratch();
	}
	// My rung, then one wetter answer from below.
	PlayMetalEcho(FVector(2075, -300, 350), 1.0f);
	FTimerDelegate EchoDelegate;
	EchoDelegate.BindLambda([this]()
	{
		if (IsValid(this))
		{
			PlayMetalEcho(FVector(1970, -300, 280), 0.92f);
		}
	});
	GetWorldTimerManager().SetTimer(
		LadderEchoTimer,
		EchoDelegate,
		0.4f,
		false);
}

void AIGThirdMorningDirector::RevealTank()
{
	if (bTankOpened)
	{
		return;
	}
	bTankOpened = true;
	SetPhase(EIGThirdMorningPhase::TankReveal);
	if (TankLidAction)
	{
		// The open disc sits beyond the player's sightline.  A 3.1 m lid parked
		// on the near hinge used to cut through the body like a render artifact.
		TankLidAction->SetActorHiddenInGame(true);
		TankLidAction->SetInteractionEnabled(false);
		TankLidAction->SetActorEnableCollision(false);
	}
	ApplyTankRevealVisibility();
	StopAllChapterAudio();
	IGStory::AddState(
		this,
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH03.Flood.OpenedTank")), false));

	if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
	{
		if (!RebirthState->CanConverge(EIGRebirthConvergencePoint::C5FinalChoice))
		{
			RebirthState->SetTankOpenedEarly(true);
		}
		if (!HasMemoryFlashlight())
		{
			RebirthState->MarkOneShotBeatPlayed(
				FName(TEXT("CH03.TankOpenedWithoutFlashlight")));
		}
	}
	AIGHorrorHUD::PushThought(
		this,
		HasMemoryFlashlight()
			? NSLOCTEXT("IGCH03", "BodyRevealThought", "누구지… 저 옷은.")
			: NSLOCTEXT(
				"IGCH03",
				"DarkTankThought",
				"수면과 사다리만 보여. 현관 손전등이 필요해."),
		5.0f);
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
	ScheduleAccidentScratch();
	UpdateEndingAvailability();
}

void AIGThirdMorningDirector::ArmFlashlightReturnReveal()
{
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!GetWorld()
		|| !bTankOpened
		|| !HasMemoryFlashlight()
		|| !RebirthState
		|| !RebirthState->WasOneShotBeatPlayed(
			FName(TEXT("CH03.TankOpenedWithoutFlashlight")))
		|| RebirthState->WasOneShotBeatPlayed(
			FName(TEXT("CH03.TankRevealedAfterFlashlight")))
		|| FlashlightReturnRevealZone)
	{
		return;
	}

	FlashlightReturnRevealZone = SpawnZone(
		FVector(1770.0f, -400.0f, 340.0f),
		FVector(62.0f, 125.0f, 105.0f));
	if (FlashlightReturnRevealZone)
	{
		FlashlightReturnRevealZone->OnZoneTriggered.AddUniqueDynamic(
			this,
			&ThisClass::HandleFlashlightReturnReveal);
	}
}

void AIGThirdMorningDirector::HandleFlashlightReturnReveal(
	AIGZoneTrigger* Zone)
{
	if (Zone != FlashlightReturnRevealZone)
	{
		return;
	}
	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState)
	{
		return;
	}
	if (!RebirthState->MarkOneShotBeatPlayed(
			FName(TEXT("CH03.TankRevealedAfterFlashlight"))))
	{
		return;
	}
	FlashlightReturnRevealZone->SetActorEnableCollision(false);
	FlashlightReturnRevealZone->OnZoneTriggered.RemoveDynamic(
		this,
		&ThisClass::HandleFlashlightReturnReveal);
	StopAllChapterAudio();
	PlayMetalEcho(
		IGThirdMorning::TankCenter + FVector(0.0f, 0.0f, 545.0f),
		0.72f);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGCH03",
			"TankRevealAfterFlashlight",
			"빛이 수면 아래를 훑었다. 아까는 어둠이 가린 게 아니라, 내가 보지 못한 거였다."),
		5.6f);
	ScheduleAccidentScratch();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
}

void AIGThirdMorningDirector::SetVisibleInteractive(
	AIGChapterThreeAction* Action,
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

void AIGThirdMorningDirector::SetDocumentAvailable(
	AIGReadableNote* Note,
	const bool bAvailable)
{
	if (!Note)
	{
		return;
	}
	Note->SetActorHiddenInGame(!bAvailable);
	Note->SetActorEnableCollision(bAvailable);
	Note->SetInteractionEnabled(bAvailable);
}

bool AIGThirdMorningDirector::HasMemoryFlashlight() const
{
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	return RebirthState
		&& RebirthState->BuildSnapshot().bHasMemoryFlashlight;
}

void AIGThirdMorningDirector::ApplyTankRevealVisibility()
{
	const bool bCanIdentifyBody = bTankOpened && HasMemoryFlashlight();
	for (UStaticMeshComponent* Piece : BodySilhouette)
	{
		if (Piece)
		{
			Piece->SetVisibility(bCanIdentifyBody);
		}
	}
	if (TObjectPtr<AIGChapterThreeAction>* ClothingEvidence =
		EvidenceActions.Find(EIGChapterThreeAction::EvidenceTankClothing))
	{
		SetVisibleInteractive(ClothingEvidence->Get(), bCanIdentifyBody);
	}
	RefreshEvidencePrompts();
}

void AIGThirdMorningDirector::FinishEndingA()
{
	if (bEndingFinished || !IsEndingChoiceReady())
	{
		return;
	}
	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState
		|| !RebirthState->SelectChapterThreeEnding(
			EIGRebirthEndingChoice::EndingA))
	{
		return;
	}
	bEndingFinished = true;
	bEndingASelected = true;
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
	SetPhase(EIGThirdMorningPhase::Ending);
	SetVisibleInteractive(CloseChoiceAction, false);
	SetVisibleInteractive(SupportChoiceAction, false);
	for (UStaticMeshComponent* Piece : BodySilhouette)
	{
		if (Piece)
		{
			Piece->SetVisibility(false);
		}
	}
	if (TankLidAction)
	{
		TankLidAction->SetActorHiddenInGame(false);
		TankLidAction->SetActorLocation(
			ToWorld(IGThirdMorning::TankCenter + FVector(0, 0, 610)));
		TankLidAction->SetActorRotation(FRotator::ZeroRotator);
	}
	PlayTankSlam();

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StartCameraFade(
				0.0f, 1.0f, 2.2f, FLinearColor::Black, false, true);
		}
	}
	GetWorldTimerManager().SetTimer(
		FlowTimer,
		this,
		&ThisClass::ShowCommonDiscoveryCard,
		2.4f,
		false);
}

void AIGThirdMorningDirector::BeginEndingB()
{
	if (bEndingFinished || !IsEndingChoiceReady())
	{
		return;
	}
	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState
		|| !RebirthState->SelectChapterThreeEnding(
			EIGRebirthEndingChoice::EndingB))
	{
		return;
	}
	bEndingFinished = true;
	bEndingASelected = false;
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
	SetPhase(EIGThirdMorningPhase::Ending);
	SetVisibleInteractive(CloseChoiceAction, false);
	SetVisibleInteractive(SupportChoiceAction, false);
	if (RebirthState->MarkOneShotBeatPlayed(
		FName(TEXT("Ending.B.StrongCuePlayed"))))
	{
		RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateWaterDripMetalRing(this),
			ToWorld(IGThirdMorning::TankCenter + FVector(0, 0, 545)),
			0.52f,
			0.62f,
			80.0f,
			650.0f);
	}

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StartCameraFade(
				0.0f,
				1.0f,
				4.8f,
				FLinearColor(0.015f, 0.12f, 0.14f),
				false,
				true);
		}
	}
	GetWorldTimerManager().SetTimer(
		FlowTimer,
		this,
		&ThisClass::ShowCommonDiscoveryCard,
		5.2f,
		false);
}

void AIGThirdMorningDirector::ShowCommonDiscoveryCard()
{
	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState)
	{
		return;
	}
	ApplyCommonDiscoveryWorldState();
	const bool bFirstCommit =
		RebirthState->CommitChapterThreeCommonDiscovery();
	ensureMsgf(
		RebirthState->GetTruthSources(FGameplayTag::RequestGameplayTag(
			FName(TEXT("Truth.Found0731")),
			false)).Contains(FName(TEXT("Ending.CommonDiscoveryCard"))),
		TEXT(
			"Both CH03 endings must commit the shared discovery through "
			"Ending.CommonDiscoveryCard."));
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
	if (!bFirstCommit)
	{
		if (bEndingASelected)
		{
			PresentEndingControls(
				NSLOCTEXT("IGCH03", "EndingAResumeTitle", "내일 또"),
				NSLOCTEXT(
					"IGCH03",
					"EndingAResumeSubtitle",
					"2024년 7월 26일 금요일, 오전 4시 44분."));
		}
		else
		{
			FinishEndingB();
		}
		return;
	}

	AIGHorrorHUD::ShowChapterCard(
		this,
		NSLOCTEXT("IGCH03", "DiscoveryDate", "2024년 7월 31일 04:00"),
		NSLOCTEXT("IGCH03", "DiscoveryTitle", "옥상 예비 저수조 합동 확인 중 실종자 한지운 발견"),
		FText::GetEmpty(),
		6.0f);

	if (bEndingASelected)
	{
		GetWorldTimerManager().SetTimer(
			FlowTimer,
			this,
			&ThisClass::FinishEndingAAfterDiscovery,
			6.1f,
			false);
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			FlowTimer,
			this,
			&ThisClass::FinishEndingB,
			6.1f,
			false);
	}
}

void AIGThirdMorningDirector::FinishEndingAAfterDiscovery()
{
	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState
		|| !RebirthState->MarkOneShotBeatPlayed(
			FName(TEXT("Ending.A.StrongCuePlayed"))))
	{
		PresentEndingControls(
			NSLOCTEXT("IGCH03", "EndingAQuietResumeTitle", "내일 또"),
			NSLOCTEXT(
				"IGCH03",
				"EndingAQuietResumeSubtitle",
				"2024년 7월 26일 금요일, 오전 4시 44분."));
		return;
	}
	// Persist the guard before emitting either part of the cue. Otherwise a
	// quit after the final controls would replay the chime and alarm on load.
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
	// The loop cue belongs to Ji-woon's chosen memory, not to the common
	// physical outcome. It therefore starts only after the 07/31 discovery.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateDoorChime(this),
		ToWorld(IGThirdMorning::TankCenter),
		0.17f,
		0.90f,
		80.0f,
		900.0f);

	FTimerDelegate LoopAlarmDelegate;
	LoopAlarmDelegate.BindLambda([this]()
	{
		if (IsValid(this))
		{
			UIGAlarmSoundWave* LoopAlarm =
				NewObject<UIGAlarmSoundWave>(this, TEXT("CH03EndingLoopAlarm"));
			IGAudio::SpawnOneShotAt(
				this,
				LoopAlarm,
				ToWorld(FVector(-205, -190, 82)),
				0.30f,
				0.983f,
				80.0f,
				750.0f);
		}
	});
	FTimerHandle LoopAlarmHandle;
	GetWorldTimerManager().SetTimer(
		LoopAlarmHandle, LoopAlarmDelegate, 0.55f, false);

	FTimerDelegate FinalCardDelegate;
	FinalCardDelegate.BindLambda([this]()
	{
		if (IsValid(this))
		{
			PresentEndingControls(
				NSLOCTEXT("IGCH03", "EndingATitle", "내일 또"),
				NSLOCTEXT(
					"IGCH03",
					"EndingASubtitle",
					"2024년 7월 26일 금요일, 오전 4시 44분."));
		}
	});
	GetWorldTimerManager().SetTimer(FlowTimer, FinalCardDelegate, 1.8f, false);
}

void AIGThirdMorningDirector::FinishEndingB()
{
	PresentEndingControls(
		NSLOCTEXT("IGCH03", "EndingBTitle", "돌려보내다"),
		NSLOCTEXT(
			"IGCH03",
			"EndingBSubtitle",
			"찾는 사람이 있었다.\n가족에게 돌아갔다."));
}

void AIGThirdMorningDirector::PresentEndingControls(
	const FText& EndingTitle,
	const FText& EndingSubtitle)
{
	AIGHorrorHUD::ShowChapterCard(
		this,
		NSLOCTEXT("IGCH03", "EndingEyebrow", "ENDING"),
		EndingTitle,
		FText::Format(
			NSLOCTEXT(
				"IGCH03",
				"EndingControls",
				"{0}\nR  이 아침 다시 시작  ·  M  처음으로"),
			EndingSubtitle),
		120.0f);
	EnableEndingInput();
}

void AIGThirdMorningDirector::EnableEndingInput()
{
	APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	if (!PlayerController)
	{
		return;
	}
	EnableInput(PlayerController);
	if (InputComponent)
	{
		InputComponent->BindAction(
			TEXT("RestartChapter"),
			IE_Pressed,
			this,
			&ThisClass::RestartChapter);
		InputComponent->BindAction(
			TEXT("ReturnToMenu"),
			IE_Pressed,
			this,
			&ThisClass::ReturnToBeginning);
	}
}

void AIGThirdMorningDirector::RestartChapter()
{
	if (!GetWorld())
	{
		return;
	}
	StopAllChapterAudio();
	GetWorldTimerManager().ClearAllTimersForObject(this);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->ClearStates(false);
		}
		if (UIGRebirthNarrativeSubsystem* RebirthState =
			GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
		{
			RebirthState->ResetChapterThreeAttempt();
			const FIGRebirthNarrativeSnapshot RestartSnapshot =
				RebirthState->BuildSnapshot();
			ensureMsgf(
				RestartSnapshot.ChapterThree.EndingChoice
					== EIGRebirthEndingChoice::None
					&& !RestartSnapshot.ResolvedPuzzles.Contains(
						FName(TEXT("Ending.A")))
					&& !RestartSnapshot.ResolvedPuzzles.Contains(
						FName(TEXT("Ending.B")))
					&& !RestartSnapshot.PlayedOneShotBeats.Contains(
						FName(TEXT("Ending.A.StrongCuePlayed")))
					&& !RestartSnapshot.PlayedOneShotBeats.Contains(
						FName(TEXT("Ending.B.StrongCuePlayed"))),
				TEXT(
					"CH03 restart must atomically clear both ending branches "
					"and their strong-cue guards."));
		}
		if (UIGSaveSubsystem* SaveSubsystem =
			GameInstance->GetSubsystem<UIGSaveSubsystem>())
		{
			SaveSubsystem->ClearRotatingAutosaves();
		}
	}
	const FName LevelName(*UGameplayStatics::GetCurrentLevelName(this, true));
	UGameplayStatics::OpenLevel(this, LevelName, true, TEXT("IGChapterThree=1"));
}

void AIGThirdMorningDirector::ReturnToBeginning()
{
	if (!GetWorld())
	{
		return;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			// M means a true return to the beginning, not a CH01 load carrying
			// the flood chapter's autosaved tags.
			StoryState->ClearStates(false);
		}
		if (UIGRebirthNarrativeSubsystem* RebirthState =
			GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
		{
			RebirthState->ResetNarrative();
		}
		if (UIGSaveSubsystem* SaveSubsystem =
			GameInstance->GetSubsystem<UIGSaveSubsystem>())
		{
			SaveSubsystem->ClearRotatingAutosaves();
		}
	}
	const FName LevelName(*UGameplayStatics::GetCurrentLevelName(this, true));
	UGameplayStatics::OpenLevel(this, LevelName, true, TEXT("IGIgnoreDirectStart=1"));
}

void AIGThirdMorningDirector::SetFloodMovement(const bool bFlooded)
{
	APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;
	if (UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr)
	{
		Movement->MaxWalkSpeed = bFlooded
			? IGThirdMorning::FloodWalkSpeed
			: IGThirdMorning::DefaultWalkSpeed;
	}
}

// ---------------------------------------------------------------------------
// Sound field
// ---------------------------------------------------------------------------

void AIGThirdMorningDirector::StartWaterBed()
{
	if (WaterBedComponent)
	{
		return;
	}

	UIGToneSequenceSoundWave* WaterBed =
		NewObject<UIGToneSequenceSoundWave>(this, TEXT("CH03WaterBed"));
	TArray<FIGToneNote> Notes;
	Notes.Add({0.0f, 9.0f, 96.0f, 0.025f, 0.20f, 0.8f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.0f, 9.0f, 52.0f, 0.018f, 0.25f, 0.8f, EIGToneWaveform::Sine});
	Notes.Add({1.2f, 0.24f, 820.0f, 0.065f, 0.02f, 2.8f, EIGToneWaveform::ValueNoise});
	Notes.Add({4.7f, 0.36f, 1280.0f, 0.050f, 0.02f, 3.2f, EIGToneWaveform::Sine});
	Notes.Add({7.4f, 0.20f, 610.0f, 0.052f, 0.02f, 2.6f, EIGToneWaveform::ValueNoise});
	WaterBed->ConfigureNotes(MoveTemp(Notes), true, 9.0f);
	WaterBedComponent = IGAudio::SpawnOneShotAt(
		this,
		WaterBed,
		ToWorld(FVector(690, 0, 5)),
		0.42f,
		1.0f,
		260.0f,
		1450.0f);
	if (WaterBedComponent)
	{
		WaterBedComponent->SetLowPassFilterEnabled(true);
		WaterBedComponent->SetLowPassFilterFrequency(2100.0f);
	}
}

void AIGThirdMorningDirector::PollPlayerMotion()
{
	if (!GetWorld() || Phase < EIGThirdMorningPhase::FloodedCorridor
		|| Phase >= EIGThirdMorningPhase::Roof)
	{
		return;
	}
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Pawn->GetVelocity().Size2D() > 12.0f)
	{
		LastPlayerMovingTime = Now;
		return;
	}
	if (Now >= NextDelayedSplashTime && Now - LastPlayerMovingTime >= 0.4)
	{
		PlayDelayedSplash();
		NextDelayedSplashTime = Now + 40.0;
	}
}

void AIGThirdMorningDirector::PlayDelayedSplash()
{
	APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	UIGToneSequenceSoundWave* Splash =
		NewObject<UIGToneSequenceSoundWave>(this, TEXT("CH03DelayedSplash"));
	TArray<FIGToneNote> Notes;
	Notes.Add({0.00f, 0.14f, 420.0f, 0.13f, 0.03f, 2.2f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.00f, 0.21f, 72.0f, 0.18f, 0.02f, 3.4f, EIGToneWaveform::Sine});
	Notes.Add({0.06f, 0.26f, 980.0f, 0.045f, 0.02f, 3.1f, EIGToneWaveform::ValueNoise});
	Splash->ConfigureNotes(MoveTemp(Notes), false);
	IGAudio::SpawnOneShotAt(
		this,
		Splash,
		Pawn->GetActorLocation() - Pawn->GetActorForwardVector() * 115.0f,
		0.46f,
		0.92f,
		45.0f,
		720.0f);
}

void AIGThirdMorningDirector::PlayMetalEcho(
	const FVector& LocalLocation,
	const float PitchMultiplier)
{
	UIGToneSequenceSoundWave* Ring =
		NewObject<UIGToneSequenceSoundWave>(
			this,
			MakeUniqueObjectName(
				this,
				UIGToneSequenceSoundWave::StaticClass(),
				TEXT("CH03MetalEcho")));
	TArray<FIGToneNote> Notes;
	Notes.Add({0.00f, 0.035f, 1550.0f, 0.13f, 0.02f, 1.4f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.00f, 0.82f, 181.0f, 0.13f, 0.01f, 4.0f, EIGToneWaveform::Sine});
	Notes.Add({0.01f, 0.63f, 1180.0f, 0.060f, 0.01f, 4.4f, EIGToneWaveform::Sine});
	Ring->ConfigureNotes(MoveTemp(Notes), false);
	IGAudio::SpawnOneShotAt(
		this,
		Ring,
		ToWorld(LocalLocation),
		0.52f,
		PitchMultiplier,
		65.0f,
		1050.0f);
}

void AIGThirdMorningDirector::PlayTankSlam()
{
	UIGToneSequenceSoundWave* Slam =
		NewObject<UIGToneSequenceSoundWave>(this, TEXT("CH03TankSlam"));
	TArray<FIGToneNote> Notes;
	Notes.Add({0.0f, 0.075f, 2600.0f, 0.30f, 0.01f, 1.0f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.0f, 0.38f, 46.0f, 0.58f, 0.01f, 3.8f, EIGToneWaveform::Sine});
	Notes.Add({0.015f, 0.62f, 164.0f, 0.32f, 0.01f, 4.2f, EIGToneWaveform::Triangle});
	Notes.Add({0.02f, 0.44f, 970.0f, 0.12f, 0.01f, 4.5f, EIGToneWaveform::Sine});
	Slam->ConfigureNotes(MoveTemp(Notes), false);
	IGAudio::SpawnOneShotAt(
		this,
		Slam,
		ToWorld(IGThirdMorning::TankCenter + FVector(0, 0, 620)),
		0.86f,
		1.0f,
		140.0f,
		1700.0f);
}

void AIGThirdMorningDirector::StopAllChapterAudio()
{
	if (AlarmComponent)
	{
		AlarmComponent->Stop();
		AlarmComponent = nullptr;
	}
	if (WaterBedComponent)
	{
		WaterBedComponent->Stop();
		WaterBedComponent = nullptr;
	}
}

// ---------------------------------------------------------------------------
// Objective provider
// ---------------------------------------------------------------------------

FText AIGThirdMorningDirector::GetObjectiveText() const
{
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (bTankOpened && !HasMemoryFlashlight())
	{
		return NSLOCTEXT(
			"IGCH03",
			"ObjectiveRecoverFlashlight",
			"404호 현관의 손전등으로 물탱크 안을 확인하자");
	}
	if (bTankOpened
		&& RebirthState
		&& !RebirthState->CanConverge(
			EIGRebirthConvergencePoint::C5FinalChoice))
	{
		const auto HasTruth = [RebirthState](const TCHAR* TruthName)
		{
			return RebirthState->HasTruth(
				FGameplayTag::RequestGameplayTag(FName(TruthName), false));
		};

		// A player may reach the tank after skipping any optional clue. Route
		// the oldest narrative debt back to its physical source before asking
		// for more roof comparisons; every staircase remains traversable after
		// the third impossible loop, so this is a real recovery route.
		if (!HasTruth(TEXT("Truth.Alarm0510")))
		{
			return NSLOCTEXT(
				"IGCH03",
				"ObjectiveDebtAlarm",
				"404호 책상 플래너에서 알람 시각을 확인하자");
		}
		if (!HasTruth(TEXT("Truth.DeathOverlay")))
		{
			return NSLOCTEXT(
				"IGCH03",
				"ObjectiveDebtApproval",
				"404호 책상 휴대폰의 결제 기록을 확인하자");
		}
		if (!HasTruth(TEXT("Truth.WasSearched")))
		{
			return NSLOCTEXT(
				"IGCH03",
				"ObjectiveDebtSearch",
				"404호 엄마 메시지나 4층 수색 전단을 확인하자");
		}
		if (!HasTruth(TEXT("Truth.RecheckScheduled0731")))
		{
			return NSLOCTEXT(
				"IGCH03",
				"ObjectiveDebtRecheck",
				"옥상 철문 밖 7월 29일 업체 재방문 안내를 읽자");
		}
		if (!HasTruth(TEXT("Truth.Negligence")))
		{
			return NSLOCTEXT(
				"IGCH03",
				"ObjectiveDebtNegligence",
				"서비스함 기록과 옥상 작업 확인표를 대조하자");
		}
		if (!HasTruth(TEXT("Truth.CatSafe")))
		{
			return NSLOCTEXT(
				"IGCH03",
				"ObjectiveDebtCat",
				"옥상 바닥의 고양이 발자국 두 구간을 대조하자");
		}
		if (!HasTruth(TEXT("Truth.HoseCause")))
		{
			return NSLOCTEXT(
				"IGCH03",
				"ObjectiveDebtHose",
				"호스의 발자국과 젖은 충돌 자국을 대조하자");
		}
		if (!HasTruth(TEXT("Truth.Fall")))
		{
			return NSLOCTEXT(
				"IGCH03",
				"ObjectiveDebtFall",
				"젖은 사다리 발판과 안쪽 손자국을 대조하자");
		}
		if (!HasTruth(TEXT("Truth.Identity")))
		{
			return NSLOCTEXT(
				"IGCH03",
				"ObjectiveDebtIdentity",
				"현재 소매와 탱크 소매를 대조하거나, 전단·안경·탱크 복장을 잇자");
		}
	}

	switch (Phase)
	{
	case EIGThirdMorningPhase::Awakening:
		return NSLOCTEXT("IGCH03", "ObjectiveWake", "알람을 듣자");
	case EIGThirdMorningPhase::ApartmentClues:
		return NSLOCTEXT(
			"IGCH03",
			"ObjectiveApartment",
			"방을 더 보거나 복도로 나가자");
	case EIGThirdMorningPhase::FloodedCorridor:
		return bP3Solved
			? NSLOCTEXT("IGCH03", "ObjectiveFollowWater", "물이 온 방향을 따라가자")
			: NSLOCTEXT(
				"IGCH03",
				"ObjectiveCorridorChoice",
				"서비스함을 보거나 물이 온 방향을 찾자");
	case EIGThirdMorningPhase::StairLoops:
		return NSLOCTEXT("IGCH03", "ObjectiveLoop", "표지보다 물의 방향을 보자");
	case EIGThirdMorningPhase::UpwardOnly:
		return NSLOCTEXT("IGCH03", "ObjectiveUp", "…위로 갈 수밖에 없다");
	case EIGThirdMorningPhase::FifthFloor:
		return NSLOCTEXT("IGCH03", "ObjectiveRoof", "옥상으로");
	case EIGThirdMorningPhase::Roof:
		return NSLOCTEXT("IGCH03", "ObjectiveRoofEvidence", "사고 현장을 자유롭게 조사하자");
	case EIGThirdMorningPhase::TankReveal:
		return NSLOCTEXT("IGCH03", "ObjectiveCompare", "서로 맞는 흔적을 대조하자");
	case EIGThirdMorningPhase::Choice:
		return NSLOCTEXT("IGCH03", "ObjectiveChoice", "직접 끝을 정하자");
	case EIGThirdMorningPhase::Ending:
	default:
		return FText::GetEmpty();
	}
}

FString AIGThirdMorningDirector::GetObjectiveTextAscii() const
{
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (bTankOpened && !HasMemoryFlashlight())
	{
		return TEXT("RETURN TO 404 FLASHLIGHT");
	}
	if (bTankOpened
		&& RebirthState
		&& !RebirthState->CanConverge(
			EIGRebirthConvergencePoint::C5FinalChoice))
	{
		const auto HasTruth = [RebirthState](const TCHAR* TruthName)
		{
			return RebirthState->HasTruth(
				FGameplayTag::RequestGameplayTag(FName(TruthName), false));
		};
		if (!HasTruth(TEXT("Truth.Alarm0510")))
		{
			return TEXT("RETURN TO 404 PLANNER");
		}
		if (!HasTruth(TEXT("Truth.DeathOverlay")))
		{
			return TEXT("RETURN TO 404 PHONE APPROVALS");
		}
		if (!HasTruth(TEXT("Truth.WasSearched")))
		{
			return TEXT("READ 404 MESSAGES OR 4F SEARCH POSTER");
		}
		if (!HasTruth(TEXT("Truth.RecheckScheduled0731")))
		{
			return TEXT("READ 7/29 VENDOR REVISIT NOTICE");
		}
		if (!HasTruth(TEXT("Truth.Negligence")))
		{
			return TEXT("COMPARE SERVICE AND ROOF RECORDS");
		}
		if (!HasTruth(TEXT("Truth.CatSafe")))
		{
			return TEXT("COMPARE CAT TRACKS");
		}
		if (!HasTruth(TEXT("Truth.HoseCause")))
		{
			return TEXT("COMPARE HOSE MARKS");
		}
		if (!HasTruth(TEXT("Truth.Fall")))
		{
			return TEXT("COMPARE LADDER MARKS");
		}
		if (!HasTruth(TEXT("Truth.Identity")))
		{
			return TEXT("COMPARE SLEEVES OR POSTER + GLASSES + TANK OUTFIT");
		}
	}

	switch (Phase)
	{
	case EIGThirdMorningPhase::Awakening: return TEXT("LISTEN");
	case EIGThirdMorningPhase::ApartmentClues: return TEXT("CHECK THE WATER AND TIME");
	case EIGThirdMorningPhase::FloodedCorridor: return TEXT("GO DOWNSTAIRS");
	case EIGThirdMorningPhase::StairLoops: return TEXT("TRY THE STAIRS AGAIN");
	case EIGThirdMorningPhase::UpwardOnly: return TEXT("ONLY UP");
	case EIGThirdMorningPhase::FifthFloor: return TEXT("REACH THE ROOF");
	case EIGThirdMorningPhase::Roof: return TEXT("CHECK THE WATER TANK");
	case EIGThirdMorningPhase::TankReveal:
	case EIGThirdMorningPhase::Choice: return TEXT("CHOOSE IN THE WORLD");
	case EIGThirdMorningPhase::Ending:
	default: return FString();
	}
}

float AIGThirdMorningDirector::GetObjectiveProgress() const
{
	return FMath::Clamp(
		static_cast<float>(static_cast<uint8>(Phase))
			/ static_cast<float>(static_cast<uint8>(EIGThirdMorningPhase::Ending)),
		0.0f,
		1.0f);
}

// ---------------------------------------------------------------------------
// Deterministic CH03 capture/smoke route
// ---------------------------------------------------------------------------

void AIGThirdMorningDirector::StartRebirthGreyboxValidation()
{
	// These four facts belong to CH01/CH02 and the July 31 documents. The
	// validation route seeds only prerequisites outside the P3/P5 scope, then
	// exercises the real world interaction handlers for both greyboxes.
	RegisterTruth(TEXT("Truth.Alarm0510"), TEXT("Validation.UpstreamAlarm"));
	RegisterTruth(TEXT("Truth.DeathOverlay"), TEXT("Validation.UpstreamOverlay"));
	RegisterTruth(TEXT("Truth.WasSearched"), TEXT("Validation.UpstreamSearch"));
	RegisterTruth(
		TEXT("Truth.RecheckScheduled0731"),
		TEXT("Validation.UpstreamRecheck"));

	// Exercise the document-only P3 bypass against an isolated snapshot. The
	// live route is restored before the service-box path begins.
	if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
	{
		const FIGRebirthNarrativeSnapshot BeforeDocumentRoute =
			RebirthState->BuildSnapshot();
		const FGameplayTag NegligenceTag =
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("Truth.Negligence")),
				false);
		RebirthState->RegisterTruthSource(
			NegligenceTag,
			FName(TEXT("Handover.Closed0358")),
			false);
		RebirthState->RegisterTruthSource(
			NegligenceTag,
			FName(TEXT("ManagementApp.Photo0403")),
			false);
		RebirthState->RegisterTruthSource(
			NegligenceTag,
			FName(TEXT("ManagementDb.FalseCompletion0620")),
			false);
		bGreyboxDocumentSkipRouteValid =
			RebirthState->HasTruth(NegligenceTag)
			&& !RebirthState->GetChapterThreeState().P3.bCompleted;
		RebirthState->RestoreSnapshot(BeforeDocumentRoute);
	}
	if (!bGreyboxDocumentSkipRouteValid)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_GREYBOX FAIL P3 document-only route "
				"did not prove Negligence while P3 remained skipped."));
		FinishRebirthGreyboxValidation();
		return;
	}

	bAlarmStopped = true;
	TryUnlockApartmentExit();
	HandleAction(
		EIGChapterThreeAction::OpenApartmentDoor,
		ApartmentDoorAction);
	HandleAction(
		EIGChapterThreeAction::P3CloseDirectInlet,
		P3DirectInletAction);
	HandleAction(
		EIGChapterThreeAction::P3CloseReserveInlet,
		P3ReserveInletAction);
	HandleAction(
		EIGChapterThreeAction::P3OpenPressureRelease,
		P3PressureReleaseAction);

	GetWorldTimerManager().SetTimer(
		CaptureTimer,
		this,
		&ThisClass::ContinueRebirthGreyboxValidation,
		6.3f,
		false);
}

void AIGThirdMorningDirector::ContinueRebirthGreyboxValidation()
{
	if (!bP3PressureZero || P3PressureKPa > KINDA_SMALL_NUMBER)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT("REBIRTH_GREYBOX FAIL P3 pressure did not reach zero after 6 seconds."));
		FinishRebirthGreyboxValidation();
		return;
	}

	HandleAction(
		EIGChapterThreeAction::P3OpenFloorDrain,
		P3FloorDrainAction);
	HandleClueRead(P3RecordNote, true);
	HandleClueRead(P3PhotoNote, true);
	if (const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
		RebirthState && RebirthState->HasTruth(
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("Truth.Negligence")),
				false)))
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_GREYBOX FAIL P3 empty measurements and 04:03 photo "
				"prematurely confirmed Negligence."));
		FinishRebirthGreyboxValidation();
		return;
	}
	HandleClueRead(ManagementDbNote, true);
	HandleClueRead(EstimateNote, true);

	HandleAction(EIGChapterThreeAction::OpenRoofDoor, RoofDoorAction);
	HandleAction(EIGChapterThreeAction::OpenTank, TankLidAction);

	for (const EIGChapterThreeAction EvidenceAction : {
		EIGChapterThreeAction::EvidenceCatEntered,
		EIGChapterThreeAction::EvidenceCatExited,
		EIGChapterThreeAction::EvidenceHosePaw,
		EIGChapterThreeAction::EvidenceHoseImpact,
		EIGChapterThreeAction::EvidenceBag,
		EIGChapterThreeAction::EvidenceWetRung,
		EIGChapterThreeAction::EvidenceHandSmear,
		EIGChapterThreeAction::EvidenceGlasses,
		EIGChapterThreeAction::EvidenceTankClothing,
		EIGChapterThreeAction::EvidenceCurrentSleeve,
		EIGChapterThreeAction::EvidenceSearchPoster})
	{
		RegisterEvidenceObservation(EvidenceAction);
	}
	CommitChapterThreeState();
	if (UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState())
	{
		const FIGRebirthNarrativeSnapshot ObservationOnlySave =
			RebirthState->BuildSnapshot();
		RebirthState->RestoreSnapshot(ObservationOnlySave);
	}
	if (GetAccidentTruthCount() != 0
		|| !GetRebirthState()
		|| GetRebirthState()->GetChapterThreeState()
			.ObservedP5Sources.Num() != 15)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_GREYBOX FAIL P5 observation-only save "
				"confirmed a truth or lost raw observations."));
		FinishRebirthGreyboxValidation();
		return;
	}

	FocusOrCompareEvidence(EIGChapterThreeAction::EvidenceCatEntered);
	FocusOrCompareEvidence(EIGChapterThreeAction::EvidenceCatExited);
	FocusOrCompareEvidence(EIGChapterThreeAction::EvidenceHosePaw);
	FocusOrCompareEvidence(EIGChapterThreeAction::EvidenceHoseImpact);
	FocusOrCompareEvidence(EIGChapterThreeAction::EvidenceBag);
	FocusOrCompareEvidence(EIGChapterThreeAction::EvidenceWetRung);
	if (const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
		RebirthState && RebirthState->HasTruth(
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("Truth.Fall")),
				false)))
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_GREYBOX FAIL P5 bag and wet rung "
				"confirmed Fall without inward evidence."));
		FinishRebirthGreyboxValidation();
		return;
	}
	FocusOrCompareEvidence(EIGChapterThreeAction::EvidenceHandSmear);
	FocusOrCompareEvidence(EIGChapterThreeAction::EvidenceCurrentSleeve);
	FocusOrCompareEvidence(EIGChapterThreeAction::EvidenceTankClothing);

	GreyboxScratchWaitTicks = 0;
	GetWorldTimerManager().SetTimer(
		CaptureTimer,
		this,
		&ThisClass::WaitForRebirthGreyboxScratches,
		0.25f,
		false);
}

void AIGThirdMorningDirector::WaitForRebirthGreyboxScratches()
{
	++GreyboxScratchWaitTicks;
	if (AccidentScratchCount == 1 && GetWorld())
	{
		if (APlayerController* Controller =
				GetWorld()->GetFirstPlayerController())
		{
			FVector ViewLocation = FVector::ZeroVector;
			FRotator ViewRotation = FRotator::ZeroRotator;
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
			const FVector AwayFromTank = (
				ViewLocation
				- ToWorld(
					IGThirdMorning::TankCenter
					+ FVector(0, 0, 430))).GetSafeNormal();
			Controller->SetControlRotation(AwayFromTank.Rotation());
		}
	}
	if (AccidentScratchCount == 2
		&& !bActedAfterSecondScratch
		&& GetWorld()
		&& AccidentScratchGateStartSeconds >= 0.0
		&& GetWorld()->GetTimeSeconds()
			- AccidentScratchGateStartSeconds >= 4.0)
	{
		HandleLadderEntered(LadderZone);
	}
	if (AccidentScratchCount < 3 || !bAccidentScratchTailSettled)
	{
		const bool bChoicesPrematurelyVisible =
			CloseChoiceAction
			&& SupportChoiceAction
			&& !CloseChoiceAction->IsHidden()
			&& !SupportChoiceAction->IsHidden()
			&& CloseChoiceAction->IsInteractionEnabled()
			&& SupportChoiceAction->IsInteractionEnabled();
		if (bChoicesPrematurelyVisible)
		{
			UE_LOG(
				LogIndieGame,
				Error,
				TEXT(
					"REBIRTH_GREYBOX FAIL ending choice appeared after "
					"%d/3 accident scratches (tail_settled=%d)."),
				AccidentScratchCount,
				bAccidentScratchTailSettled ? 1 : 0);
			FinishRebirthGreyboxValidation();
			return;
		}
		if (GreyboxScratchWaitTicks >= 60)
		{
			ensureMsgf(
				false,
				TEXT(
					"REBIRTH_GREYBOX accident scratch sequence timed out "
					"after %d/3 cues (tail_settled=%d)."),
				AccidentScratchCount,
				bAccidentScratchTailSettled ? 1 : 0);
			FinishRebirthGreyboxValidation();
			return;
		}
		GetWorldTimerManager().SetTimer(
			CaptureTimer,
			this,
			&ThisClass::WaitForRebirthGreyboxScratches,
			0.25f,
			false);
		return;
	}

	const bool bExactlyThreeScratches = ensureMsgf(
		AccidentScratchCount == 3 && bAccidentScratchTailSettled,
		TEXT(
			"REBIRTH_GREYBOX expected three scratches and a settled tail, "
			"got %d/3 (tail_settled=%d)."),
		AccidentScratchCount,
		bAccidentScratchTailSettled ? 1 : 0);
	if (bExactlyThreeScratches)
	{
		UE_LOG(
			LogIndieGame,
			Display,
			TEXT(
				"REBIRTH_GREYBOX accident scratches complete "
				"count=%d/3 tail_settled=1"),
			AccidentScratchCount);
	}
	else
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_GREYBOX accident scratches invalid "
				"count=%d/3 tail_settled=%d"),
			AccidentScratchCount,
			bAccidentScratchTailSettled ? 1 : 0);
	}
	UpdateEndingAvailability();
	FinishRebirthGreyboxValidation();
}

void AIGThirdMorningDirector::FinishRebirthGreyboxValidation()
{
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	const bool bConverged = RebirthState
		&& RebirthState->CanConverge(
			EIGRebirthConvergencePoint::C5FinalChoice);
	const bool bActualDiscoveryDeferred = RebirthState
		&& !RebirthState->HasTruth(FGameplayTag::RequestGameplayTag(
			FName(TEXT("Truth.Found0731")),
			false))
		&& !RebirthState->CanConverge(
			EIGRebirthConvergencePoint::C6AfterDiscovery);
	const auto IsActionActorReady = [](const AIGChapterThreeAction* Action)
	{
		const UStaticMeshComponent* Mesh =
			IsValid(Action) ? Action->GetPresentationMesh() : nullptr;
		return Mesh && Mesh->GetStaticMesh();
	};
	const bool bP3ActorsReady =
		IsActionActorReady(P3DirectInletAction.Get())
		&& IsActionActorReady(P3ReserveInletAction.Get())
		&& IsActionActorReady(P3PressureReleaseAction.Get())
		&& IsActionActorReady(P3FloorDrainAction.Get())
		&& IsValid(P3RecordNote.Get())
		&& IsValid(P3PhotoNote.Get())
		&& IsValid(ManagementDbNote.Get())
		&& IsValid(EstimateNote.Get())
		&& IsValid(RecheckNoticeNote.Get())
		&& IsValid(PreservationNoticeNote.Get())
		&& IsValid(PoliceChecklistNote.Get())
		&& IsValid(P3PressureNeedle.Get())
		&& IsValid(P3PressureText.Get())
		&& IsValid(P3BleedTubeVisual.Get())
		&& IsValid(P3BleedWaterVisual.Get())
		&& P3ZeroConfirmationTicks == 2;
	bool bAllEvidenceActorsReady = EvidenceActions.Num() == 11;
	for (const TPair<EIGChapterThreeAction, TObjectPtr<AIGChapterThreeAction>>& Pair
		: EvidenceActions)
	{
		const AIGChapterThreeAction* Evidence = Pair.Value.Get();
		if (!IsActionActorReady(Evidence)
			|| Evidence->IsHidden()
			|| !Evidence->IsInteractionEnabled())
		{
			bAllEvidenceActorsReady = false;
			break;
		}
	}
	const auto HasEvidenceSources =
		[RebirthState](
			const TCHAR* TruthName,
			const TArray<FName>& RequiredSources)
		{
			if (!RebirthState)
			{
				return false;
			}
			const TArray<FName> Sources = RebirthState->GetTruthSources(
				FGameplayTag::RequestGameplayTag(FName(TruthName), false));
			for (const FName RequiredSource : RequiredSources)
			{
				if (!Sources.Contains(RequiredSource))
				{
					return false;
				}
			}
			return true;
		};
	const bool bAllP5SourcesValid =
		HasEvidenceSources(
			TEXT("Truth.CatSafe"),
			{
				FName(TEXT("P5.CatEnteredPrints")),
				FName(TEXT("P5.CatExitedPrints"))
			})
		&& HasEvidenceSources(
			TEXT("Truth.HoseCause"),
			{
				FName(TEXT("P5.HosePawCompression")),
				FName(TEXT("P5.CouplingImpact")),
				FName(TEXT("P5.InnerRimFriction"))
			})
		&& HasEvidenceSources(
			TEXT("Truth.Fall"),
			{
				FName(TEXT("P5.UpperSlipperEnd")),
				FName(TEXT("P5.LiftedPad")),
				FName(TEXT("P5.CorrodedClips")),
				FName(TEXT("P5.InwardHandSmear"))
			})
		&& HasEvidenceSources(
			TEXT("Truth.Identity"),
			{
				FName(TEXT("P5.CurrentSleeveThreeStitches")),
				FName(TEXT("P5.TankSleeveThreeStitches")),
				FName(TEXT("P5.TankHeelWear"))
			});
	const TArray<FName> NegligenceSources = RebirthState
		? RebirthState->GetTruthSources(FGameplayTag::RequestGameplayTag(
			FName(TEXT("Truth.Negligence")),
			false))
		: TArray<FName>();
	const bool bAllNegligenceSourcesValid =
		NegligenceSources.Contains(FName(TEXT("P3.EmptyMeasurements")))
		&& NegligenceSources.Contains(FName(TEXT("ManagementApp.Photo0403")))
		&& NegligenceSources.Contains(FName(TEXT("Handover.Closed0358")))
		&& NegligenceSources.Contains(
			FName(TEXT("ManagementDb.FalseCompletion0620")))
		&& !NegligenceSources.Contains(
			FName(TEXT("P3.EmptyRecordPlusReopenPhoto")))
		&& RebirthState
		&& RebirthState->HasTruth(FGameplayTag::RequestGameplayTag(
			FName(TEXT("Truth.Negligence")),
			false));
	const bool bChoicesVisible =
		CloseChoiceAction
		&& SupportChoiceAction
		&& !CloseChoiceAction->IsHidden()
		&& !SupportChoiceAction->IsHidden()
		&& CloseChoiceAction->IsInteractionEnabled()
		&& SupportChoiceAction->IsInteractionEnabled();
	const bool bScratchSequenceComplete =
		AccidentScratchCount == 3 && bAccidentScratchTailSettled;

	float RoofDoorGap = -1.0f;
	if (RoofDoorAction && RoofDoorAction->GetPresentationMesh())
	{
		const FBoxSphereBounds DoorBounds =
			RoofDoorAction->GetPresentationMesh()->CalcBounds(
				RoofDoorAction->GetPresentationMesh()->GetComponentTransform());
		const float RoofTop = ToWorld(
			FVector(0, 0, IGThirdMorning::RoofFloorZ)).Z;
		RoofDoorGap = DoorBounds.Origin.Z - DoorBounds.BoxExtent.Z - RoofTop;
	}
	const bool bDoorGapValid = FMath::IsNearlyEqual(RoofDoorGap, 11.0f, 1.0f);
	const bool bPassed =
		bP3Solved
		&& bP3PressureZero
		&& P3MistakeCount == 0
		&& GetAccidentTruthCount() == 4
		&& bP3ActorsReady
		&& bAllEvidenceActorsReady
		&& bAllP5SourcesValid
		&& bAllNegligenceSourcesValid
		&& bGreyboxDocumentSkipRouteValid
		&& bConverged
		&& bActualDiscoveryDeferred
		&& bScratchSequenceComplete
		&& bChoicesVisible
		&& bDoorGapValid;

	if (bPassed)
	{
		UE_LOG(
			LogIndieGame,
			Display,
			TEXT(
				"REBIRTH_GREYBOX PASS p3=1 pressure=%.0f mistakes=0 "
				"p5_truths=4 evidence=%d actors=1 sources=1 negligence_sources=1 "
				"c5=1 found_deferred=1 scratches=3 tail_settled=1 "
				"choices=1 roof_gap=%.1fcm"),
			P3PressureKPa,
			EvidenceActions.Num(),
			RoofDoorGap);
	}
	else
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_GREYBOX FAIL p3=%d pressure=%.0f mistakes=%d "
				"p5_truths=%d evidence=%d p3_actors=%d evidence_actors=%d "
				"sources=%d negligence_sources=%d c5=%d found_deferred=%d "
				"scratches=%d tail_settled=%d choices=%d roof_gap=%.1fcm"),
			bP3Solved ? 1 : 0,
			P3PressureKPa,
			P3MistakeCount,
			GetAccidentTruthCount(),
			EvidenceActions.Num(),
			bP3ActorsReady ? 1 : 0,
			bAllEvidenceActorsReady ? 1 : 0,
			bAllP5SourcesValid ? 1 : 0,
			bAllNegligenceSourcesValid ? 1 : 0,
			bConverged ? 1 : 0,
			bActualDiscoveryDeferred ? 1 : 0,
			AccidentScratchCount,
			bAccidentScratchTailSettled ? 1 : 0,
			bChoicesVisible ? 1 : 0,
			RoofDoorGap);
	}
	FPlatformMisc::RequestExitWithStatus(
		false,
		bPassed ? 0 : 1,
		TEXT("REBIRTH greybox validation completed"));
}

void AIGThirdMorningDirector::StartCaptureSequence()
{
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = false;
	}
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("r.MotionBlurQuality 0"), true);
		PlayerController->ConsoleCommand(TEXT("DisableAllScreenMessages"), true);
		if (AHUD* HUD = PlayerController->GetHUD())
		{
			HUD->bShowHUD = false;
		}
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StopCameraFade();
		}
		if (AIGPlayerCharacter* Player =
			Cast<AIGPlayerCharacter>(PlayerController->GetPawn()))
		{
			if (UIGFlashlightComponent* Flashlight = Player->GetFlashlight())
			{
				Flashlight->SetOn(true);
			}
		}
	}

	// Exercise the real opening state transition instead of setting its bool.
	StopAlarmByItself();
	CaptureIndex = 0;
	CaptureNextFrame();
}

void AIGThirdMorningDirector::PlaceCaptureCamera(
	const FVector& LocalPawnLocation,
	const FVector& LocalLookAt)
{
	APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn || !PlayerController)
	{
		return;
	}
	Pawn->SetActorLocation(
		ToWorld(LocalPawnLocation),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	FVector Eye = Pawn->GetActorLocation() + FVector(0, 0, 64);
	if (const AIGPlayerCharacter* Player = Cast<AIGPlayerCharacter>(Pawn))
	{
		if (const UCameraComponent* Camera = Player->GetFirstPersonCamera())
		{
			Eye = Camera->GetComponentLocation();
		}
	}
	const FRotator LookRotation = (ToWorld(LocalLookAt) - Eye).Rotation();
	Pawn->SetActorRotation(FRotator(0, LookRotation.Yaw, 0));
	PlayerController->SetControlRotation(LookRotation);
}

void AIGThirdMorningDirector::CaptureNextFrame()
{
	if (!GetWorld())
	{
		FinishCaptureSequence();
		return;
	}
	if (AIGReadableNote* OpenNote = AIGReadableNote::GetOpenNote())
	{
		OpenNote->Close();
	}

	FString BaseName;
	switch (CaptureIndex)
	{
	case 0:
		HandleAction(EIGChapterThreeAction::OpenFridge, FridgeDoorAction);
		HandleClueRead(PlannerNote, true);
		HandleClueRead(MotherPhoneNote, true);
		HandleAction(EIGChapterThreeAction::OpenApartmentDoor, ApartmentDoorAction);
		PlaceCaptureCamera(
			FVector(10, -35, IGThirdMorning::StandingCapsuleCenter + 18.0f),
			FVector(-208, 115, 132));
		BaseName = TEXT("ch03-full-fridge");
		break;
	case 1:
		HandleCorridorEntered(CorridorEntryZone);
		for (AIGZoneTrigger* LoopZone : StairLoopZones)
		{
			HandleStairLoop(LoopZone);
		}
		// The waist-high water sheet proves the lower route is blocked in
		// play, but it would cover the changing 4F/ROOF sign in this dedicated
		// documentation frame.
		if (DownRouteWaterBarrier)
		{
			DownRouteWaterBarrier->SetVisibility(false);
		}
		PlaceCaptureCamera(
			FVector(
				1180,
				135,
				IGThirdMorning::StandingCapsuleCenter + 20.0f),
			FVector(1040, -65, 145));
		BaseName = TEXT("ch03-stair-up");
		break;
	case 2:
		HandleAction(EIGChapterThreeAction::InspectKeys, KeysAction);
		HandleAction(EIGChapterThreeAction::OpenRoofDoor, RoofDoorAction);
		HandleClueRead(EstimateNote, true);
		PlaceCaptureCamera(
			FVector(
				3020,
				100,
				IGThirdMorning::RoofFloorZ + IGThirdMorning::StandingCapsuleCenter + 155.0f),
			IGThirdMorning::TankCenter + FVector(0, 0, 470));
		BaseName = TEXT("ch03-roof-tank");
		break;
	case 3:
		HandleAction(EIGChapterThreeAction::EvidenceGlasses, GlassesAction);
		HandleAction(EIGChapterThreeAction::OpenTank, TankLidAction);
		PlaceCaptureCamera(
			FVector(2420, -220, 1020),
			IGThirdMorning::TankCenter + FVector(-8, 2, 550));
		BaseName = TEXT("ch03-tank-reveal");
		break;
	default:
		FinishCaptureSequence();
		return;
	}

	const FString ScreenshotPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(
			FPaths::ProjectDir(),
			FString::Printf(TEXT("Docs/Media/%s.png"), *BaseName)));
	++CaptureIndex;

	FTimerDelegate CaptureDelegate;
	CaptureDelegate.BindLambda([this, ScreenshotPath]()
	{
		if (!IsValid(this))
		{
			return;
		}
		FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
		UE_LOG(LogIndieGame, Display, TEXT("CH03 capture requested: %s"), *ScreenshotPath);
		GetWorldTimerManager().SetTimer(
			CaptureTimer,
			this,
			&ThisClass::CaptureNextFrame,
			4.0f,
			false);
	});
	GetWorldTimerManager().SetTimer(CaptureTimer, CaptureDelegate, 0.75f, false);
}

void AIGThirdMorningDirector::FinishCaptureSequence()
{
	const bool bRebirthGreyboxPresent =
		bTankOpened
		&& P3DirectInletAction
		&& P3PressureReleaseAction
		&& EvidenceActions.Num() == 11;
	if (!bRebirthGreyboxPresent)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT("CH03 capture smoke route is missing REBIRTH P3/P5 greybox actors."));
	}
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"CH03_CAPTURE COMPLETE loops=%d roof=%d tank=%d "
			"rebirth_greybox=%d visual_stills=4"),
		StairLoopCount,
		Phase >= EIGThirdMorningPhase::Roof ? 1 : 0,
		bTankOpened ? 1 : 0,
		bRebirthGreyboxPresent ? 1 : 0);
	FPlatformMisc::RequestExit(false);
}
