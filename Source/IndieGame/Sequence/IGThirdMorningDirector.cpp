#include "Sequence/IGThirdMorningDirector.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
#include "AssetCompilingManager.h"
#include "Audio/IGAlarmSoundWave.h"
#include "Audio/IGAmbienceSoundWave.h"
#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HighResScreenshot.h"
#include "IndieGame.h"
#include "Interaction/IGInteractable.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGZoneTrigger.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Narrative/IGItemContinuityDressing.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryHelpers.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGFlashlightComponent.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Player/IGPlayerController.h"
#include "Player/IGStressComponent.h"
#include "Save/IGSaveGame.h"
#include "Save/IGSaveSubsystem.h"
#include "ShaderCompiler.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "Validation/IGRebirthEvidenceSubsystem.h"

namespace IGThirdMorning
{
	const FVector StageOrigin(-4800.0f, 4200.0f, 0.0f);
	constexpr float StandingCapsuleCenter = 96.0f;
	// CharacterMovement keeps a small floor gap after resolving contact. Resume
	// directly at that stable center so a loaded camera never drops for a frame.
	constexpr float RestoredCapsuleCenter = StandingCapsuleCenter + 2.0f;
	constexpr float DefaultWalkSpeed = 300.0f;
	// 침수 복도의 걷기 속도는 두 곳이 쓴다. 이 상수를 넣는 SetFloodMovement와,
	// 발소리 표면이 바뀔 때마다 도는 플레이어의 ApplyContextMovementSpeed다.
	// 뒤엣것은 ReferenceWalkSpeed(300)에 물 표면 계수 0.78을 곱하므로, 둘이
	// 다르면 스프린트·앉기·청음을 누를 때마다 걷기 속도가 두 값 사이를
	// 오간다. 같은 값으로 맞춰 둔다.
	constexpr float FloodWalkSpeed = 234.0f;
	// 발소리 표면. 플레이어가 발밑을 라인 트레이스해서 이 이름을 읽는다.
	// 태그가 없으면 콘크리트로 떨어진다. 소리만 바뀌는 게 아니라
	// UIGMissingFloorAudioSubsystem이 이 표면으로 반향 공간까지 고르므로,
	// 옥상과 철계단은 태그가 없으면 실내 복도 반향을 쓰게 된다.
	const FName FootstepVinylTag(TEXT("Footstep.Vinyl"));
	const FName FootstepMetalStairTag(TEXT("Footstep.MetalStair"));
	const FName FootstepRooftopTag(TEXT("Footstep.Rooftop"));
	const FName FootstepWaterTag(TEXT("Footstep.Water"));

	void TagFootstepSurface(UStaticMeshComponent* Component, const FName Tag)
	{
		if (Component && !Component->ComponentHasTag(Tag))
		{
			Component->ComponentTags.Add(Tag);
		}
	}
	constexpr float OpeningLensDropletDelaySeconds = 1.05f;
	constexpr float OpeningLensDropletDurationSeconds = 3.0f;
	constexpr float LensDropletCaptureEarlyDelaySeconds = 0.42f;
	constexpr float LensDropletCaptureLateDelaySeconds = 1.35f;
	constexpr float LensDropletCaptureFinishRetrySeconds = 0.10f;
	constexpr int32 LensDropletCaptureMaximumFinishRetries = 20;
	constexpr float LensDropletCaptureMinimumTravelPixels = 8.0f;
	constexpr float LensDropletCaptureMaximumReducedTravelPixels = 0.5f;
	constexpr float LensDropletCaptureMinimumAlpha = 0.55f;
	constexpr float NarrativeCrossfadeSeconds = 1.6f;
	constexpr float TankRevealSilenceSeconds = 6.0f;
	constexpr float EndingBMusicDurationSeconds = 45.0f;
	constexpr float EndingBDripDelaySeconds = 42.0f;
	constexpr float EndingBFinalCardDelaySeconds = 45.2f;
	static_assert(EndingBDripDelaySeconds < EndingBMusicDurationSeconds);
	constexpr float RoofFloorZ = 240.0f;
	constexpr float RoofDoorLeafWidth = 116.0f;
	constexpr float RoofDoorFreeEdgeGap = 11.0f;
	constexpr float RoofDoorLatchedAngleDegrees = 5.441396f;
	const FVector RoofDoorClosedLocation(1662.0f, -400.0f, 356.5f);
	const FVector RoofDoorClosedFreeEdgeLocation(1662.0f, -342.0f, 356.5f);
	const FVector RoofDoorLatchedLocation(1656.5f, -400.26136f, 356.5f);
	const FRotator RoofDoorLatchedRotation(
		0.0f,
		RoofDoorLatchedAngleDegrees,
		0.0f);
	const FVector RoofDoorPulledLocation(1604.0f, -458.0f, 356.5f);
	const FRotator RoofDoorPulledRotation(0.0f, 90.0f, 0.0f);
	constexpr float RoofDoorHoldSeconds = 2.4f;
	constexpr float RoofDoorReturnSeconds = 0.18f;
	const FVector TankCenter(2500.0f, -300.0f, 0.0f);
	const FVector TankHatchOffset(-96.0f, 0.0f, 0.0f);
	const FVector TankLidOpenOffset(36.9f, 0.0f, 49.5f);
	constexpr float TankShellBottomZ = 340.0f;
	constexpr float TankShellCenterZ = 470.0f;
	constexpr float TankInternalFloorZ = 381.0f;
	constexpr float TankDeckUndersideZ = 596.0f;
	constexpr float TankWaterSurfaceZ = 561.0f;
	// The highest hood vertex is local Z=20.46 cm. At this placement it remains
	// 7.5 cm below the real water plane, while sitting high enough that the near
	// hatch lip cannot amputate the legs in the player's sightline.
	constexpr float TankBodyPlacementAdjustmentZ = -9.0f;
	// Seventy degrees turns the bent-knee axis partly across the camera instead
	// of straight into depth. The body still fits inside the 306 cm tank, but
	// head, torso, knees and both feet now connect in the hatch silhouette.
	const FVector AuthoredTankBodyPlacement(-88.0f, 0.0f, 542.0f);
	const FRotator AuthoredTankBodyRotation(0.0f, 70.0f, 0.0f);
	// The repaired seam follows the revised left forearm rather than floating at
	// the old ellipsoid shoulder. Evidence focus and visible stitches both derive
	// from these local coordinates.
	const FVector AuthoredSleeveStitchLocalBase(8.0f, -27.0f, 15.2f);
	const FVector AuthoredSleeveStitchLocalStep(1.8f, 1.2f, 0.0f);
	static_assert(
		TankDeckUndersideZ - TankInternalFloorZ == 215.0f,
		"Tank internal height must remain 2.15 m");
	static_assert(
		TankWaterSurfaceZ - TankInternalFloorZ == 180.0f,
		"Tank residual water depth must remain 1.80 m");
	static_assert(
		TankDeckUndersideZ - TankWaterSurfaceZ == 35.0f,
		"Tank water surface must remain 35 cm below the hatch underside");

	FVector GetAuthoredSleeveStitchFocusOffset()
	{
		// Aim at the middle repair stitch. The same authored transform also drives
		// the visible stitch geometry, so moving the body cannot strand its trace.
		return AuthoredTankBodyPlacement
			+ AuthoredTankBodyRotation.Quaternion().RotateVector(
				AuthoredSleeveStitchLocalBase + AuthoredSleeveStitchLocalStep)
			+ FVector(0.0f, 0.0f, TankBodyPlacementAdjustmentZ);
	}

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
	const bool bEvidenceOnly =
		InAction == EIGChapterThreeAction::EvidenceCatEntered
		|| InAction == EIGChapterThreeAction::EvidenceCatExited
		|| InAction == EIGChapterThreeAction::EvidenceHosePaw
		|| InAction == EIGChapterThreeAction::EvidenceHoseImpact
		|| InAction == EIGChapterThreeAction::EvidenceBag
		|| InAction == EIGChapterThreeAction::EvidenceWetRung
		|| InAction == EIGChapterThreeAction::EvidenceHandSmear
		|| InAction == EIGChapterThreeAction::EvidenceGlasses
		|| InAction == EIGChapterThreeAction::EvidenceTankClothing
		|| InAction == EIGChapterThreeAction::EvidenceCurrentSleeve
		|| InAction == EIGChapterThreeAction::EvidenceSearchPoster;
	if (bEvidenceOnly)
	{
		// Evidence must remain hittable by the visibility interaction trace,
		// but a two-centimeter footprint or fallen bag must never snag the
		// player's movement capsule.
		PresentationMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PresentationMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		PresentationMesh->SetCollisionResponseToChannel(
			ECC_Visibility,
			ECR_Block);
	}
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
	bLensDropletCaptureMode = bCaptureMode
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("IGCaptureCH03LensDroplet"));
	bGreyboxValidationMode =
		FParse::Param(FCommandLine::Get(), TEXT("IGRebirthGreybox"));
	bReleaseValidationMode =
		bGreyboxValidationMode
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("IGRebirthReleaseValidation"));
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
			bLensDropletCaptureMode
				? &ThisClass::StartLensDropletCaptureSequence
				: &ThisClass::StartCaptureSequence,
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

	FVector SafeLocalLocation(520.0f, 0.0f, IGThirdMorning::RestoredCapsuleCenter);
	FRotator SafeRotation(0.0f, 0.0f, 0.0f);
	FRotator SafeView(-5.0f, 0.0f, 0.0f);
	if (bRestoreRoof)
	{
		ApplyRoofDoorState(EIGRoofDoorState::LatchedGap);
		RevealUpwardRoute();
		SetDocumentAvailable(P3PhotoNote, true);
		SetDocumentAvailable(ManagementDbNote, true);
		SetDocumentAvailable(PreservationNoticeNote, true);
		SetDocumentAvailable(PoliceChecklistNote, true);
		SetFloodMovement(false);
		if (!bEndingFinished)
		{
			StartRoofBed(true);
		}
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
				+ IGThirdMorning::RestoredCapsuleCenter);
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
			IGThirdMorning::RestoredCapsuleCenter);
		SafeRotation = FRotator(0.0f, -142.0f, 0.0f);
		SafeView = FRotator(-9.0f, -142.0f, 0.0f);
	}
	else
	{
		SetPhase(EIGThirdMorningPhase::FloodedCorridor);
		SetFloodMovement(true);
		StartWaterBed(true);
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
		if (!bP4Completed)
		{
			GetWorldTimerManager().SetTimer(
				P4Timer,
				this,
				&ThisClass::PollP4PressureAndHint,
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
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGSaveSubsystem* SaveSubsystem =
				GameInstance->GetSubsystem<UIGSaveSubsystem>())
		{
			SaveSubsystem->OnSaveCompleted.RemoveDynamic(
				this,
				&ThisClass::HandleReleaseValidationSaveCompleted);
			SaveSubsystem->OnLoadCompleted.RemoveDynamic(
				this,
				&ThisClass::HandleReleaseValidationLoadCompleted);
		}
	}
	if (!ReleaseValidationSlotName.IsEmpty())
	{
		UGameplayStatics::DeleteGameInSlot(ReleaseValidationSlotName, 0);
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
	State.bApartmentFridgeInspected = bFridgeInspected;
	State.P3.bDirectInletClosed = bP3DirectClosed;
	State.P3.bReserveInletClosed = bP3ReserveClosed;
	State.P3.bPressureReleaseOpen = bP3PressureReleaseOpen;
	State.P3.bPressureZero = bP3PressureZero;
	State.P3.bCompleted = bP3Solved;
	State.P3.PressureKPa = P3PressureKPa;
	State.P3.MistakeCount = P3MistakeCount;
	State.P3.ZeroConfirmationTicks = P3ZeroConfirmationTicks;
	State.P3.HintElapsedSeconds = P3HintElapsedSeconds;
	State.P3.PressureRiseElapsedSeconds = P3PressureRiseElapsedSeconds;
	State.P3.bPressureRiseArmed = bP3PressureRiseArmed;
	State.P3.HintStage = P3HintStage;
	State.P4.StairLoopCount = StairLoopCount;
	State.P4.PressureStage = P4PressureStage;
	State.P4.PressureRiseElapsedSeconds = P4PressureRiseElapsedSeconds;
	State.P4.HintElapsedSeconds = P4HintElapsedSeconds;
	State.P4.HintStage = P4HintStage;
	State.P4.bPressureArmed = bP4PressureArmed;
	State.P4.bCompleted = bP4Completed;
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
	bFridgeInspected = State.bApartmentFridgeInspected;
	bP3DirectClosed = State.P3.bDirectInletClosed;
	bP3ReserveClosed = State.P3.bReserveInletClosed;
	bP3PressureReleaseOpen = State.P3.bPressureReleaseOpen;
	bP3PressureZero = State.P3.bPressureZero;
	bP3Solved = State.P3.bCompleted;
	P3PressureKPa = State.P3.PressureKPa;
	P3MistakeCount = State.P3.MistakeCount;
	P3ZeroConfirmationTicks = State.P3.ZeroConfirmationTicks;
	P3HintElapsedSeconds = State.P3.HintElapsedSeconds;
	P3PressureRiseElapsedSeconds = State.P3.PressureRiseElapsedSeconds;
	bP3PressureRiseArmed = State.P3.bPressureRiseArmed;
	P3HintStage = State.P3.HintStage;
	StairLoopCount = State.P4.StairLoopCount;
	P4PressureStage = State.P4.PressureStage;
	P4PressureRiseElapsedSeconds = State.P4.PressureRiseElapsedSeconds;
	P4HintElapsedSeconds = State.P4.HintElapsedSeconds;
	P4HintStage = State.P4.HintStage;
	bP4PressureArmed = State.P4.bPressureArmed;
	bP4Completed = State.P4.bCompleted;
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

	return bFridgeInspected
		|| bP3DirectClosed
		|| bP3ReserveClosed
		|| bP3PressureReleaseOpen
		|| bP3PressureZero
		|| bP3Solved
		|| P3MistakeCount > 0
		|| P3ZeroConfirmationTicks > 0
		|| StairLoopCount > 0
		|| P4PressureStage > 0
		|| P4PressureRiseElapsedSeconds > 0.0f
		|| P4HintElapsedSeconds > 0.0f
		|| P4HintStage > 0
		|| bP4PressureArmed
		|| bP4Completed
		|| bTankOpened
		|| FocusedEvidence != EIGChapterThreeAction::None
		|| !ObservedP5Sources.IsEmpty()
		|| AccidentScratchCount > 0
		|| bEndingFinished;
}

void AIGThirdMorningDirector::ApplyChapterThreeWorldState()
{
	if (bFridgeInspected)
	{
		SetVisibleInteractive(FridgeDoorAction, false);
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
	}

	const auto ApplyClosedValve = [](AIGChapterThreeAction* Action)
	{
		if (Action)
		{
			Action->SetInteractionPrompt(
				NSLOCTEXT("IGCH03", "P3RestoredValveClosed", "잠김"));
			Action->SetInteractionEnabled(false);
			if (UStaticMeshComponent* Wheel = Action->GetPresentationMesh())
			{
				Wheel->SetRelativeRotation(FRotator(0, 45, 0));
			}
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
		if (UStaticMeshComponent* Wheel =
			P3PressureReleaseAction->GetPresentationMesh())
		{
			Wheel->SetRelativeRotation(FRotator(0, 45, 0));
		}
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
			FRotator(FMath::Lerp(-55.0f, 55.0f, Alpha), 0, 0));
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
			FVector(0.026f, 0.026f, 0.40f * FlowScale));
	}
	if (bP3PressureZero && P3FloorDrainAction)
	{
		P3FloorDrainAction->SetInteractionPrompt(
			bP3Solved
				? NSLOCTEXT("IGCH03", "P3RestoredDrainOpen", "바닥 배수 열림")
				: NSLOCTEXT("IGCH03", "P3RestoredDrainReady", "0 확인 후 바닥 배수 열기"));
		P3FloorDrainAction->SetInteractionEnabled(!bP3Solved);
		if (bP3Solved)
		{
			if (UStaticMeshComponent* Wheel =
				P3FloorDrainAction->GetPresentationMesh())
			{
				Wheel->SetRelativeRotation(FRotator(0, 45, 0));
			}
		}
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
	for (AIGZoneTrigger* LoopZone : StairLoopZones)
	{
		if (LoopZone)
		{
			LoopZone->SetActorEnableCollision(false);
		}
	}
	if (!bP4Completed
		&& StairLoopCount < 3
		&& StairLoopZones.IsValidIndex(StairLoopCount))
	{
		StairLoopZones[StairLoopCount]->SetActorEnableCollision(true);
	}
	if (DownRouteWaterBarrier)
	{
		DownRouteWaterBarrier->SetVisibility(StairLoopCount >= 3);
		DownRouteWaterBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	UpdateStairSign();
	ApplyP4PresentationState();
	const bool bReachedRoof = IGStory::HasState(
		this,
		FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH03.Flood.ReachedRoof")),
			false));
	if (bReachedRoof || bTankOpened || bEndingFinished)
	{
		ApplyRoofDoorState(EIGRoofDoorState::LatchedGap);
		SetDocumentAvailable(P3PhotoNote, true);
		SetDocumentAvailable(ManagementDbNote, true);
		SetDocumentAvailable(PreservationNoticeNote, true);
		SetDocumentAvailable(PoliceChecklistNote, true);
	}

	if (bTankOpened)
	{
		if (TankLidAction)
		{
			TankLidAction->SetActorHiddenInGame(false);
			TankLidAction->SetActorLocation(ToWorld(
				IGThirdMorning::TankCenter
				+ IGThirdMorning::TankHatchOffset
				+ IGThirdMorning::TankLidOpenOffset
				+ FVector(0, 0, 610)));
			TankLidAction->SetActorRotation(FRotator(-78, 0, 0));
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

void AIGThirdMorningDirector::ApplyRoofDoorState(
	const EIGRoofDoorState NewState)
{
	if (!RoofDoorAction || !RoofDoorLeaf)
	{
		return;
	}

	RoofDoorState = NewState;
	const bool bPulledOpen = NewState == EIGRoofDoorState::PulledOpen;
	const FVector& LocalLocation = bPulledOpen
		? IGThirdMorning::RoofDoorPulledLocation
		: IGThirdMorning::RoofDoorLatchedLocation;
	const FRotator& LocalRotation = bPulledOpen
		? IGThirdMorning::RoofDoorPulledRotation
		: IGThirdMorning::RoofDoorLatchedRotation;
	const FRotator WorldRotation =
		GetActorTransform().TransformRotation(LocalRotation.Quaternion()).Rotator();

	RoofDoorAction->SetActorHiddenInGame(false);
	RoofDoorAction->SetActorEnableCollision(true);
	RoofDoorAction->SetActorLocationAndRotation(
		ToWorld(LocalLocation),
		WorldRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	RoofDoorLeaf->SetCollisionProfileName(
		UCollisionProfile::BlockAll_ProfileName);
	RoofDoorLeaf->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RoofDoorLeaf->SetCollisionResponseToAllChannels(ECR_Block);
	RoofDoorLeaf->SetGenerateOverlapEvents(false);
	RoofDoorLeaf->SetCanEverAffectNavigation(false);

	switch (NewState)
	{
	case EIGRoofDoorState::LatchedGap:
		RoofDoorAction->SetInteractionPrompt(
			NSLOCTEXT("IGCH03", "RoofDoorLatchedPrompt", "옥상 철문 당기기"));
		RoofDoorAction->SetInteractionEnabled(true);
		break;
	case EIGRoofDoorState::PulledOpen:
		RoofDoorAction->SetInteractionPrompt(
			NSLOCTEXT("IGCH03", "RoofDoorReleasePrompt", "옥상 철문 놓기"));
		RoofDoorAction->SetInteractionEnabled(true);
		break;
	case EIGRoofDoorState::ReturnToGap:
		RoofDoorAction->SetInteractionPrompt(
			NSLOCTEXT("IGCH03", "RoofDoorReturningPrompt", "문이 문턱에 걸리는 중"));
		RoofDoorAction->SetInteractionEnabled(false);
		break;
	default:
		break;
	}
}

float AIGThirdMorningDirector::MeasureRoofDoorFreeEdgeGap() const
{
	if (!RoofDoorAction)
	{
		return -1.0f;
	}

	// The story's 11 cm is the horizontal slit at the latch/free edge.  It is
	// not floor clearance under the whole leaf.  Both the authored mesh and
	// cube fallback are centred on the actor, so the same local free-edge point
	// remains authoritative in every build.
	const FVector CurrentFreeEdge =
		RoofDoorAction->GetActorTransform().TransformPosition(
			FVector(0.0f, IGThirdMorning::RoofDoorLeafWidth * 0.5f, 0.0f));
	const FVector ClosedFreeEdge =
		ToWorld(IGThirdMorning::RoofDoorClosedFreeEdgeLocation);
	const FVector ClosedDoorNormal =
		GetActorTransform().TransformVectorNoScale(FVector::ForwardVector)
		.GetSafeNormal();
	return FMath::Abs(FVector::DotProduct(
		CurrentFreeEdge - ClosedFreeEdge,
		ClosedDoorNormal));
}

void AIGThirdMorningDirector::BeginRoofDoorReturn()
{
	if (!GetWorld() || RoofDoorState != EIGRoofDoorState::PulledOpen)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RoofDoorHoldTimer);
	if (!IsRoofDoorReturnClear())
	{
		// The static proxy must never teleport into a player or cat lingering
		// in the threshold. Keep the usable open state and retry after they
		// clear the closing envelope.
		GetWorldTimerManager().SetTimer(
			RoofDoorReturnTimer,
			this,
			&ThisClass::BeginRoofDoorReturn,
			IGThirdMorning::RoofDoorReturnSeconds,
			false);
		return;
	}
	ApplyRoofDoorState(EIGRoofDoorState::ReturnToGap);
	GetWorldTimerManager().SetTimer(
		RoofDoorReturnTimer,
		this,
		&ThisClass::FinishRoofDoorReturn,
		IGThirdMorning::RoofDoorReturnSeconds,
		false);
}

bool AIGThirdMorningDirector::IsRoofDoorReturnClear() const
{
	UWorld* World = GetWorld();
	if (!World || !RoofDoorAction)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(RebirthRoofDoorReturn),
		false);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(RoofDoorAction);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	// Include the shallow panel relief and pull handle in the predicted closing
	// envelope.  The leaf itself is only 45 mm thick.
	const FVector DoorHalfExtent(8.5f, 60.0f, 117.0f);
	const FQuat DoorRotation = GetActorTransform().TransformRotation(
		IGThirdMorning::RoofDoorLatchedRotation.Quaternion());
	return !World->OverlapAnyTestByObjectType(
		ToWorld(IGThirdMorning::RoofDoorLatchedLocation),
		DoorRotation,
		ObjectQueryParams,
		FCollisionShape::MakeBox(DoorHalfExtent),
		QueryParams);
}

void AIGThirdMorningDirector::FinishRoofDoorReturn()
{
	ApplyRoofDoorState(EIGRoofDoorState::LatchedGap);
}

void AIGThirdMorningDirector::ApplyCommonDiscoveryWorldState()
{
	// The 0.8 s actual-state restore both endings share: the lid closed, the
	// rod back on the deck, the glasses still on their railing bolt, and every
	// future-dated police overlay gone. Only the memory ever held them here.
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
		TankLidAction->SetActorLocation(ToWorld(
			IGThirdMorning::TankCenter
			+ IGThirdMorning::TankHatchOffset
			+ FVector(0, 0, 610)));
		TankLidAction->SetActorRotation(FRotator::ZeroRotator);
		TankLidAction->SetInteractionEnabled(false);
		TankLidAction->SetActorEnableCollision(false);
	}
	SetInspectionRodWedged(false);
	SetDocumentAvailable(RecheckNoticeNote, false);
	SetDocumentAvailable(PreservationNoticeNote, false);
	SetDocumentAvailable(PoliceChecklistNote, false);
	SetDocumentAvailable(P3PhotoNote, false);
	SetDocumentAvailable(ManagementDbNote, false);
	for (const TPair<
		EIGChapterThreeAction,
		TObjectPtr<AIGChapterThreeAction>>& Pair : EvidenceActions)
	{
		if (AIGChapterThreeAction* Evidence = Pair.Value)
		{
			Evidence->SetActorEnableCollision(false);
			Evidence->SetInteractionEnabled(false);
		}
	}
	SetVisibleInteractive(CloseChoiceAction, false);
	SetVisibleInteractive(SupportChoiceAction, false);
}

bool AIGThirdMorningDirector::ValidateCommonDiscoveryWorldState() const
{
	const FVector ExpectedLidLocation = ToWorld(
		IGThirdMorning::TankCenter
		+ IGThirdMorning::TankHatchOffset
		+ FVector(0, 0, 610));
	const FVector ExpectedRodLocation =
		IGThirdMorning::TankCenter
		+ FVector(
			-196,
			128,
			IGThirdMorning::RoofFloorZ + 4.0f);
	const FRotator ExpectedRodRotation(90, 24, 0);
	const auto IsRetiredDocument = [](const AIGReadableNote* Note)
	{
		return IsValid(Note)
			&& Note->IsHidden()
			&& !Note->GetActorEnableCollision()
			&& !Note->IsInteractionEnabled();
	};

	bool bAllBodySilhouetteHidden = BodySilhouette.Num() > 0;
	for (const UStaticMeshComponent* Piece : BodySilhouette)
	{
		if (!IsValid(Piece) || Piece->IsVisible())
		{
			bAllBodySilhouetteHidden = false;
			break;
		}
	}
	bool bAllEvidenceRetired = EvidenceActions.Num() > 0;
	for (const TPair<
		EIGChapterThreeAction,
		TObjectPtr<AIGChapterThreeAction>>& Pair : EvidenceActions)
	{
		const AIGChapterThreeAction* Evidence = Pair.Value;
		if (!IsValid(Evidence)
			|| Evidence->GetActorEnableCollision()
			|| Evidence->IsInteractionEnabled())
		{
			bAllEvidenceRetired = false;
			break;
		}
	}

	return IsValid(TankLidAction)
		&& !TankLidAction->IsHidden()
		&& TankLidAction->GetActorLocation().Equals(ExpectedLidLocation, 0.1f)
		&& TankLidAction->GetActorRotation().Equals(FRotator::ZeroRotator, 0.1f)
		&& !TankLidAction->GetActorEnableCollision()
		&& !TankLidAction->IsInteractionEnabled()
		&& IsValid(InspectionRodVisual)
		&& InspectionRodVisual->GetRelativeLocation().Equals(
			ExpectedRodLocation,
			0.1f)
		&& InspectionRodVisual->GetRelativeRotation().Equals(
			ExpectedRodRotation,
			0.1f)
		&& IsValid(GlassesAction)
		&& !GlassesAction->IsHidden()
		&& GlassesAction->GetActorLocation().Equals(
			ToWorld(FVector(2335, -184, 662)),
			0.1f)
		&& !GlassesAction->GetActorEnableCollision()
		&& !GlassesAction->IsInteractionEnabled()
		&& IsRetiredDocument(RecheckNoticeNote)
		&& IsRetiredDocument(PreservationNoticeNote)
		&& IsRetiredDocument(PoliceChecklistNote)
		&& IsRetiredDocument(P3PhotoNote)
		&& IsRetiredDocument(ManagementDbNote)
		&& IsValid(CloseChoiceAction)
		&& CloseChoiceAction->IsHidden()
		&& !CloseChoiceAction->GetActorEnableCollision()
		&& !CloseChoiceAction->IsInteractionEnabled()
		&& IsValid(SupportChoiceAction)
		&& SupportChoiceAction->IsHidden()
		&& !SupportChoiceAction->GetActorEnableCollision()
		&& !SupportChoiceAction->IsInteractionEnabled()
		&& bAllBodySilhouetteHidden
		&& bAllEvidenceRetired;
}

void AIGThirdMorningDirector::SetInspectionRodWedged(const bool bWedged)
{
	if (!InspectionRodVisual)
	{
		return;
	}
	if (bWedged)
	{
		// Raised into the support groove, holding the remembered lid open.
		InspectionRodVisual->SetRelativeLocation(
			IGThirdMorning::TankCenter + FVector(-160, 105, 618));
		InspectionRodVisual->SetRelativeRotation(FRotator(38, 0, 0));
	}
	else
	{
		InspectionRodVisual->SetRelativeLocation(
			IGThirdMorning::TankCenter
				+ FVector(-196, 128, IGThirdMorning::RoofFloorZ + 4.0f));
		InspectionRodVisual->SetRelativeRotation(FRotator(90, 24, 0));
	}
}

void AIGThirdMorningDirector::ScheduleEndingCue(
	const float DelaySeconds,
	TFunction<void()> Fn)
{
	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);
	FTimerDelegate CueDelegate;
	CueDelegate.BindLambda([WeakThis, Fn = MoveTemp(Fn)]()
	{
		if (WeakThis.IsValid())
		{
			Fn();
		}
	});
	FTimerHandle& Handle = EndingSequenceTimers.AddDefaulted_GetRef();
	GetWorldTimerManager().SetTimer(
		Handle, CueDelegate, FMath::Max(DelaySeconds, 0.01f), false);
}

void AIGThirdMorningDirector::PlayRestoreStateSounds()
{
	// Three distances, one restore: the lid at the rim, the rod on the deck,
	// the glasses temple far out on the railing.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateDoorThud(this),
		ToWorld(
			IGThirdMorning::TankCenter
			+ IGThirdMorning::TankHatchOffset
			+ FVector(-25, 0, 640)),
		0.40f,
		0.62f,
		120.0f,
		900.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateRodWedgeSeat(this),
		ToWorld(IGThirdMorning::TankCenter
			+ FVector(-196, 128, IGThirdMorning::RoofFloorZ + 6.0f)),
		0.34f,
		0.86f,
		90.0f,
		760.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateGlassesTinyRing(this),
		ToWorld(FVector(2335, -184, 662)),
		0.30f,
		1.0f,
		60.0f,
		620.0f);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT(
			"IGCH03",
			"RestoreStateCaption",
			"[뚜껑과 지지봉이 제자리로 돌아가고, 난간의 안경이 한 번 울린다]"),
		2.2f);
}

void AIGThirdMorningDirector::PlayCommonSafetyOpening()
{
	// 2024-07-31 04:00, heard only: the machines are calm, the people are
	// careful, and the hatch opens the way it always should have.
	const FVector TankTop = ToWorld(
		IGThirdMorning::TankCenter
		+ IGThirdMorning::TankHatchOffset
		+ FVector(0, 0, 620));
	const FVector LadderBase = ToWorld(FVector(1915, -300, 260));
	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);
	auto PlayAt = [WeakThis](
		UIGToneSequenceSoundWave* (*Factory)(UObject*),
		const FVector& Location,
		const float Volume,
		const FName CueId,
		const float CaptionDuration)
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->RecordCommonSafetyCue(CueId);
			IGAudio::SpawnOneShotAt(
				Director,
				Factory(Director),
				Location,
				Volume,
				1.0f,
				140.0f,
				1200.0f);
			FText Caption;
			if (CueId == FName(TEXT("Safety.GasDetector")))
			{
				Caption = NSLOCTEXT("IGCH03", "SafetyGasCaption", "[가스 측정기가 정상음을 낸다]");
			}
			else if (CueId == FName(TEXT("Safety.Ventilation")))
			{
				Caption = NSLOCTEXT("IGCH03", "SafetyVentCaption", "[환기 덕트와 송풍기가 차례로 돈다]");
			}
			else if (CueId == FName(TEXT("Safety.Harness")))
			{
				Caption = NSLOCTEXT("IGCH03", "SafetyHarnessCaption", "[안전벨트 버클을 잠근다]");
			}
			else if (CueId == FName(TEXT("Safety.TwoClimbers")))
			{
				Caption = NSLOCTEXT("IGCH03", "SafetyClimbersCaption", "[두 사람이 사다리를 오른다]");
			}
			else if (CueId == FName(TEXT("Safety.HatchOpen")))
			{
				Caption = NSLOCTEXT("IGCH03", "SafetyHatchCaption", "[점검구가 천천히 열린다]");
			}
			AIGHorrorHUD::PushAudioCaption(
				Director,
				Caption,
				CaptionDuration);
		}
	};

	ScheduleEndingCue(0.10f, [PlayAt, TankTop]()
	{
		PlayAt(
			&UIGToneSequenceSoundWave::CreateGasDetectorOk,
			TankTop,
			0.42f,
			FName(TEXT("Safety.GasDetector")),
			1.15f);
	});
	ScheduleEndingCue(1.40f, [PlayAt, TankTop]()
	{
		PlayAt(
			&UIGToneSequenceSoundWave::CreateVentDuctSpinUp,
			TankTop,
			0.40f,
			FName(TEXT("Safety.Ventilation")),
			2.30f);
	});
	ScheduleEndingCue(4.10f, [PlayAt, LadderBase]()
	{
		PlayAt(
			&UIGToneSequenceSoundWave::CreateHarnessBuckle,
			LadderBase,
			0.44f,
			FName(TEXT("Safety.Harness")),
			1.05f);
	});
	ScheduleEndingCue(5.30f, [PlayAt, LadderBase]()
	{
		PlayAt(
			&UIGToneSequenceSoundWave::CreateLadderClimbTwoPeople,
			LadderBase,
			0.42f,
			FName(TEXT("Safety.TwoClimbers")),
			2.50f);
	});
	ScheduleEndingCue(9.00f, [PlayAt, TankTop]()
	{
		PlayAt(
			&UIGToneSequenceSoundWave::CreateHatchOpenMetal,
			TankTop,
			0.46f,
			FName(TEXT("Safety.HatchOpen")),
			1.70f);
	});
	ScheduleEndingCue(10.90f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->ShowCommonDiscoveryCard();
		}
	});
}

void AIGThirdMorningDirector::RecordCommonSafetyCue(const FName CueId)
{
	if (bReleaseValidationInProgress && !CueId.IsNone())
	{
		ReleaseValidationSafetyOpeningCues.Add(CueId);
	}
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
	PlaneMesh = IGThirdMorning::LoadMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	CylinderMesh = IGThirdMorning::LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	SphereMesh = IGThirdMorning::LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	WaterBottleMesh = IGThirdMorning::LoadMesh(TEXT("/Game/Meshes/SM_WaterBottle.SM_WaterBottle"));
	BottleCapMesh = IGThirdMorning::LoadMesh(TEXT("/Game/Meshes/SM_BottleCap.SM_BottleCap"));
	LabelSleeveMesh = IGThirdMorning::LoadMesh(TEXT("/Game/Meshes/SM_LabelSleeve.SM_LabelSleeve"));
	HornRimGlassesMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_HornRimGlasses.SM_HornRimGlasses"));
	InspectionRodMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_InspectionRod.SM_InspectionRod"));
	CrackedPhoneMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_CrackedPhone.SM_CrackedPhone"));
	SubmergedHoodieMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_SubmergedHoodieCurl.SM_SubmergedHoodieCurl"));
	SubmergedPantsMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_SubmergedPantsCurl.SM_SubmergedPantsCurl"));
	SubmergedSlippersMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_SubmergedSlippersCurl.SM_SubmergedSlippersCurl"));
	RooftopWaterTankShellMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_RooftopWaterTankShell.SM_RooftopWaterTankShell"));
	TankInternalLiningMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_TankInternalLining.SM_TankInternalLining"));
	RooftopTankPipeClusterMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_RooftopTankPipeCluster.SM_RooftopTankPipeCluster"));
	TankInternalLadderMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_TankInternalLadder.SM_TankInternalLadder"));
	TankAccessGuardRailMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_TankAccessGuardRail.SM_TankAccessGuardRail"));
	TankAccessDeckMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_TankAccessDeck.SM_TankAccessDeck"));
	TankAccessLidMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_TankAccessLid.SM_TankAccessLid"));
	RooftopServiceHoseMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_RooftopServiceHose.SM_RooftopServiceHose"));
	HoseCouplingMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_HoseCoupling.SM_HoseCoupling"));
	CarrierBagCollapsedMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_CarrierBagCollapsed.SM_CarrierBagCollapsed"));
	RooftopFireDoorLeafMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_RooftopFireDoorLeaf.SM_RooftopFireDoorLeaf"));
	RooftopFireDoorFrameMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_RooftopFireDoorFrame.SM_RooftopFireDoorFrame"));
	RooftopUnlockedPadlockKeysMesh = IGThirdMorning::LoadMesh(
		TEXT(
			"/Game/Meshes/SM_RooftopUnlockedPadlockKeys."
			"SM_RooftopUnlockedPadlockKeys"));
	TankExteriorAccessStairMesh = IGThirdMorning::LoadMesh(
		TEXT(
			"/Game/Meshes/SM_TankExteriorAccessStair."
			"SM_TankExteriorAccessStair"));
	LadderFailureRungMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_LadderFailureRung.SM_LadderFailureRung"));
	LadderRungPadMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_LadderRungPadLifted.SM_LadderRungPadLifted"));
	LadderRungClipsMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_LadderRungRetainingClips.SM_LadderRungRetainingClips"));
	P3ServiceCabinetShellMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_P3ServiceCabinetShell.SM_P3ServiceCabinetShell"));
	P3ServiceManifoldMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_P3ServiceManifold.SM_P3ServiceManifold"));
	P3LargeValveWheelMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_P3ValveWheelLarge.SM_P3ValveWheelLarge"));
	P3SmallValveWheelMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_P3ValveWheelSmall.SM_P3ValveWheelSmall"));
	P3PressureGaugeMesh = IGThirdMorning::LoadMesh(
		TEXT("/Game/Meshes/SM_P3PressureGauge.SM_P3PressureGauge"));

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
	EvidenceSlipperMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_EvidenceSlipperTrail.M_EvidenceSlipperTrail"));
	EvidenceCatPawMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_EvidenceCatPawTrail.M_EvidenceCatPawTrail"));
	EvidenceHoseMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_EvidenceHoseDrag.M_EvidenceHoseDrag"));
	EvidenceHandSmearMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_EvidenceHandSmear.M_EvidenceHandSmear"));
	DecalDampWallpaperMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_DecalDampWallpaper.M_DecalDampWallpaper"));
	DecalRustFastenersMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_DecalRustFasteners.M_DecalRustFasteners"));
	DecalMineralScaleMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_DecalMineralScale.M_DecalMineralScale"));
	DecalRainGrimeMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_DecalRainGrime.M_DecalRainGrime"));
	ScreenMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_ScreenGlow.M_ScreenGlow"));
	BeddingMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_BeddingUV.M_BeddingUV"));
	WetHoodieMaterial = IGThirdMorning::LoadMaterial(
		TEXT(
			"/Game/Prototype/Materials/M_SubmergedHoodieUV."
			"M_SubmergedHoodieUV"));
	SubmergedPantsMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_SubmergedPantsUV.M_SubmergedPantsUV"));
	SubmergedSlippersMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_SubmergedSlippersUV.M_SubmergedSlippersUV"));
	SubmergedSlipperWearMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_SubmergedSlipperWearUV.M_SubmergedSlipperWearUV"));
	CarrierBagMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_CarrierBagFilm.M_CarrierBagFilm"));
	WaterTankMetalMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_WaterTankMetalUV.M_WaterTankMetalUV"));
	TankInteriorBiofilmMaterial = IGThirdMorning::LoadMaterial(
		TEXT(
			"/Game/Prototype/Materials/M_TankInteriorBiofilmUV."
			"M_TankInteriorBiofilmUV"));
	WetServiceHoseMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_WetServiceHoseUV.M_WetServiceHoseUV"));
	WetRungPadMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_WetRungPadUV.M_WetRungPadUV"));
	TankRevealWaterMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_TankWaterReveal.M_TankWaterReveal"));
	P3CabinetMetalMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_P3CabinetMetalUV.M_P3CabinetMetalUV"));
	RoofDoorMaterial = IGThirdMorning::LoadMaterial(
		TEXT("/Game/Prototype/Materials/M_SteelDoorUV.M_SteelDoorUV"));
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
	if (!WetHoodieMaterial)
	{
		WetHoodieMaterial = BeddingMaterial;
	}
	if (!SubmergedPantsMaterial)
	{
		SubmergedPantsMaterial = BeddingMaterial;
	}
	if (!SubmergedSlippersMaterial)
	{
		SubmergedSlippersMaterial = PlasticMaterial;
	}
	if (!SubmergedSlipperWearMaterial)
	{
		SubmergedSlipperWearMaterial = WetPaperMaterial;
	}
	if (!CarrierBagMaterial)
	{
		CarrierBagMaterial = GlassMaterial;
	}
	if (!WaterTankMetalMaterial)
	{
		WaterTankMetalMaterial = MetalMaterial;
	}
	if (!TankInteriorBiofilmMaterial)
	{
		TankInteriorBiofilmMaterial = WaterTankMetalMaterial;
	}
	if (!WetServiceHoseMaterial)
	{
		WetServiceHoseMaterial = PlasticMaterial;
	}
	if (!WetRungPadMaterial)
	{
		WetRungPadMaterial = PlasticMaterial;
	}
	if (!TankRevealWaterMaterial)
	{
		TankRevealWaterMaterial = WaterMaterial;
	}
	if (!P3CabinetMetalMaterial)
	{
		P3CabinetMetalMaterial = MetalMaterial;
	}
	if (!RoofDoorMaterial)
	{
		RoofDoorMaterial = MetalMaterial;
	}
	if (!EvidenceSlipperMaterial)
	{
		EvidenceSlipperMaterial = WetStepMaterial;
	}
	if (!EvidenceCatPawMaterial)
	{
		EvidenceCatPawMaterial = WetStepMaterial;
	}
	if (!EvidenceHoseMaterial)
	{
		EvidenceHoseMaterial = WetStepMaterial;
	}
	if (!EvidenceHandSmearMaterial)
	{
		EvidenceHandSmearMaterial = WetStepMaterial;
	}

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
	UMaterialInterface* InPaperMaterial,
	UStaticMesh* MeshOverride)
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
			MeshOverride ? MeshOverride : CubeMesh.Get(),
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
	// 장판 바닥이므로 프롤로그의 같은 방과 같은 발소리를 쓴다.
	IGThirdMorning::TagFootstepSurface(
		CreateBlock(FVector(0, 0, -10), FVector(700, 440, 20), RoomFloorMaterial),
		IGThirdMorning::FootstepVinylTag);
	CreateBlock(FVector(0, -220, 130), FVector(700, 20, 280), WallMaterial);
	CreateBlock(FVector(0, 220, 130), FVector(700, 20, 280), WallMaterial);
	CreateBlock(FVector(-350, 0, 130), FVector(20, 440, 280), WallMaterial);
	CreateBlock(FVector(0, 0, 270), FVector(700, 440, 20), WallMaterial);
	if (PlaneMesh && DecalDampWallpaperMaterial)
	{
		// A single tide line anchors the flooded room in familiar vinyl
		// wallpaper. It stays below eye level and never reads as decoration.
		CreateBlock(
			FVector(-48.0f, -209.4f, 82.0f),
			FVector(220.0f, 92.0f, 1.0f),
			DecalDampWallpaperMaterial,
			false,
			FRotator(0.0f, 0.0f, 90.0f),
			PlaneMesh);
	}
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
	// The wet patch belongs on the underside of the slab at Z 260; at Z 266 it
	// was inside the concrete.
	CreateBlock(FVector(45, -20, 258.5f), FVector(18, 18, 3), WetStepMaterial, false);

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
	// Hung on the wall face at Y 210; at Y 214 the glass was inside the wall.
	CreateBlock(
		FVector(275, 208, 155),
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

	// One-room kitchenette on the north wall: a real 404 has a sink, and
	// ending B's epilogue returns here for its single drop of water.
	CreateBlock(FVector(-55, 185, 42), FVector(85, 66, 84), PlasticMaterial);
	CreateBlock(FVector(-55, 185, 82.5f), FVector(48, 40, 5), DarkConcreteMaterial, false);
	// Mixer tap: the column stands on the worktop insert at Z 85 and the spout
	// runs out of its head over the basin. Previously the column started two
	// centimetres above the deck and the spout crossed the counter east-west
	// nine centimetres clear of the column, connected to nothing.
	CreateBlock(
		FVector(-55, 205, 98),
		FVector(5, 5, 26),
		MetalMaterial,
		false,
		FRotator::ZeroRotator,
		CylinderMesh);
	CreateBlock(
		FVector(-55, 194, 108),
		FVector(4, 4, 22),
		MetalMaterial,
		false,
		FRotator(0, 0, 90),
		CylinderMesh);
	// The empty glass cup under the closed tap. It is here in the flood
	// morning too: the same cup, before and after.
	CreateBlock(
		FVector(-30, 188, 91),
		FVector(8, 8, 13),
		GlassMaterial,
		false,
		FRotator::ZeroRotator,
		CylinderMesh);

	// Spring-morning dressing, revealed only by ending B's epilogue: the new
	// tenant's wall clock and a daylight key that has no place in the flood.
	EpilogueClockText = NewObject<UTextRenderComponent>(
		this, TEXT("CH03_EpilogueClock"));
	EpilogueClockText->SetupAttachment(SceneRoot);
	EpilogueClockText->SetRelativeLocation(FVector(-55, 207, 162));
	EpilogueClockText->SetRelativeRotation(FRotator(0, -90, 0));
	EpilogueClockText->SetHorizontalAlignment(EHTA_Center);
	EpilogueClockText->SetWorldSize(20.0f);
	EpilogueClockText->SetTextRenderColor(FColor(46, 66, 58));
	EpilogueClockText->SetText(FText::FromString(TEXT("4:43")));
	EpilogueClockText->SetVisibility(false);
	EpilogueClockText->RegisterComponent();
	EpilogueSpringLight = CreatePointLight(
		FVector(40, 60, 216),
		0.0f,
		760.0f,
		FLinearColor(0.93f, 0.97f, 1.0f),
		true);
	if (EpilogueSpringLight)
	{
		EpilogueSpringLight->SetVisibility(false);
	}
}

void AIGThirdMorningDirector::BuildFloodedCorridor()
{
	// The wet 4F corridor starts at the apartment threshold and ends at the
	// stairwell.  Water is visual/non-colliding; the dry floor carries physics.
	//
	// 발소리는 그 물리 바닥에서 정해진다. 물판은 충돌이 없으므로 발밑
	// 라인 트레이스가 통과해 이 슬래브를 때리고, 태그가 없으면 콘크리트로
	// 읽힌다 — 무릎까지 물이 찬 복도에서 마른 복도 소리가 났다. 물 발소리는
	// 이미 네 음으로 저작돼 있는데(`CreateFootstep`의 Water) 5층 별관
	// 급수 비트에서만 닿고 있었다. 이 바닥은 침수 복도 전용이므로 여기에
	// 태그를 건다.
	if (UStaticMeshComponent* FloodFloor = CreateBlock(
			FVector(675, 0, -10), FVector(650, 440, 20), ConcreteMaterial))
	{
		FloodFloor->ComponentTags.AddUnique(IGThirdMorning::FootstepWaterTag);
	}
	CreateBlock(FVector(675, -220, 130), FVector(650, 20, 280), DarkConcreteMaterial);
	CreateBlock(FVector(675, 220, 130), FVector(650, 20, 280), DarkConcreteMaterial);
	CreateBlock(FVector(675, 0, 270), FVector(650, 440, 20), DarkConcreteMaterial);
	CorridorWaterVisual = CreateBlock(
		FVector(675, 0, 2),
		FVector(640, 430, 3),
		TankRevealWaterMaterial,
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
		// Surface-mounted batten, flush to the soffit at Z 260. At Z 250 the
		// fitting hung eight centimetres below the ceiling on nothing.
		CreateBlock(
			FVector(X, 0, 258),
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
	const bool bHasAuthoredP3Cluster =
		P3ServiceCabinetShellMesh
		&& P3ServiceManifoldMesh
		&& P3LargeValveWheelMesh
		&& P3SmallValveWheelMesh
		&& P3PressureGaugeMesh;
	if (bHasAuthoredP3Cluster)
	{
		CreateBlock(
			FVector(675, -196, 132),
			FVector(100, 100, 100),
			P3CabinetMetalMaterial,
			false,
			FRotator::ZeroRotator,
			P3ServiceCabinetShellMesh);
		CreateBlock(
			FVector(675, -196, 132),
			FVector(100, 100, 100),
			WaterTankMetalMaterial,
			false,
			FRotator::ZeroRotator,
			P3ServiceManifoldMesh);
	}
	else
	{
		// Release-safe blockout keeps the exact cabinet envelope and interaction
		// coordinates until every authored member has been baked.
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
	}

	// Exactly two black recesses mark the absent latch screws. They are not
	// interaction targets and never multiply with puzzle or save state.
	CreateBlock(
		FVector(812, -176.8f, 130),
		FVector(1.2f, 1.2f, 0.4f),
		PlasticMaterial,
		false,
		FRotator(0, 0, -90),
		CylinderMesh);
	CreateBlock(
		FVector(812, -176.8f, 122),
		FVector(1.2f, 1.2f, 0.4f),
		PlasticMaterial,
		false,
		FRotator(0, 0, -90),
		CylinderMesh);

	UStaticMesh* const LargeValveMesh =
		bHasAuthoredP3Cluster ? P3LargeValveWheelMesh.Get() : CylinderMesh.Get();
	UStaticMesh* const SmallValveMesh =
		bHasAuthoredP3Cluster ? P3SmallValveWheelMesh.Get() : CylinderMesh.Get();
	const FVector LargeValveSize =
		bHasAuthoredP3Cluster ? FVector(100, 100, 100) : FVector(18, 18, 4);
	const FVector SmallValveSize =
		bHasAuthoredP3Cluster ? FVector(100, 100, 100) : FVector(12, 12, 3);
	const FRotator ValveRotation(0, 0, -90);

	P3DirectInletAction = SpawnAction(
		EIGChapterThreeAction::P3CloseDirectInlet,
		FVector(600, -178, 164),
		LargeValveSize,
		EmergencyMaterial,
		NSLOCTEXT("IGCH03", "P3DirectPrompt", "직결 급수 잠그기"),
		0.45f,
		ValveRotation,
		LargeValveMesh);
	P3ReserveInletAction = SpawnAction(
		EIGChapterThreeAction::P3CloseReserveInlet,
		FVector(650, -178, 164),
		LargeValveSize,
		BottleCapMaterial,
		NSLOCTEXT("IGCH03", "P3ReservePrompt", "예비조 잠그기"),
		0.45f,
		ValveRotation,
		LargeValveMesh);
	P3PressureReleaseAction = SpawnAction(
		EIGChapterThreeAction::P3OpenPressureRelease,
		FVector(705, -178, 112),
		SmallValveSize,
		PlasticMaterial,
		NSLOCTEXT("IGCH03", "P3BleedPrompt", "압력 해제 열기"),
		0.55f,
		ValveRotation,
		SmallValveMesh);
	P3FloorDrainAction = SpawnAction(
		EIGChapterThreeAction::P3OpenFloorDrain,
		FVector(760, -178, 112),
		LargeValveSize,
		MetalMaterial,
		NSLOCTEXT("IGCH03", "P3DrainPrompt", "바닥 배수 열기"),
		0.75f,
		ValveRotation,
		LargeValveMesh);

	if (!bHasAuthoredP3Cluster)
	{
		// Fallback risers, clipped to the cabinet's back panel at Y -202. The
		// authored cluster stands off that panel on its own manifold; without
		// it these were left hanging ten centimetres clear of everything.
		CreateBlock(FVector(600, -200, 150), FVector(4, 4, 74), MetalMaterial, false);
		CreateBlock(FVector(650, -200, 150), FVector(4, 4, 74), MetalMaterial, false);
		CreateBlock(FVector(705, -200.5f, 95), FVector(3, 3, 52), MetalMaterial, false);
		CreateBlock(FVector(760, -200.5f, 95), FVector(3, 3, 52), MetalMaterial, false);
	}
	P3BleedTubeVisual = CreateBlock(
		FVector(705, -174, 73),
		FVector(4, 4, 46),
		GlassMaterial,
		false,
		FRotator::ZeroRotator,
		CylinderMesh);
	P3BleedWaterVisual = CreateBlock(
		FVector(705, -174, 73),
		FVector(2.6f, 2.6f, 40),
		WaterMaterial,
		false,
		FRotator::ZeroRotator,
		CylinderMesh,
		true);
	if (P3BleedWaterVisual)
	{
		P3BleedWaterVisual->SetVisibility(false);
	}

	P3PressureNeedle = CreateBlock(
		FVector(755, -174.8f, 178),
		FVector(0.8f, 0.5f, 7),
		EmergencyMaterial,
		false,
		FRotator(-55, 0, 0),
		nullptr,
		true);
	CreateBlock(
		FVector(755, -178, 178),
		bHasAuthoredP3Cluster ? FVector(100, 100, 100) : FVector(16, 16, 4),
		P3CabinetMetalMaterial,
		false,
		FRotator(0, 0, -90),
		bHasAuthoredP3Cluster ? P3PressureGaugeMesh.Get() : CylinderMesh.Get());
	CreateBlock(
		FVector(755, -175.8f, 178),
		FVector(14, 14, 0.35f),
		PaperMaterial,
		false,
		FRotator(0, 0, -90),
		CylinderMesh);

	// Blank physical plates keep the cabinet believable. Korean control names
	// remain in the HUD prompt because the fallback 3D font has no CJK glyphs.
	for (const FVector& PlateLocation : {
		FVector(600, -174.0f, 190),
		FVector(650, -174.0f, 190),
		FVector(705, -174.0f, 138),
		FVector(760, -174.0f, 138)})
	{
		CreateBlock(
			PlateLocation,
			FVector(20, 0.8f, 6),
			PaperMaterial,
			false);
	}

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

}

void AIGThirdMorningDirector::BuildLoopingStairwell()
{
	// Entry landing.
	CreateBlock(FVector(1085, 0, -10), FVector(200, 260, 20), ConcreteMaterial);
	// Keep the original fire wall, but divide it around a real 150 x 210 cm
	// opening. A single full-width collision block here used to make the
	// corridor and stair landing look connected while being impassable.
	CreateBlock(FVector(975, -227.5f, 130), FVector(20, 305, 280), DarkConcreteMaterial);
	CreateBlock(FVector(975, 227.5f, 130), FVector(20, 305, 280), DarkConcreteMaterial);
	CreateBlock(FVector(975, 0, 245), FVector(20, 150, 70), DarkConcreteMaterial);
	CreateBlock(FVector(1210, 180, 20), FVector(20, 500, 520), DarkConcreteMaterial);
	// Enclose the upward flight all the way to the unfinished fifth-floor
	// landing. The former shell stopped after the first tread, exposing the
	// sky as a flat blue rectangle and making an ordinary stairwell read like
	// a missing level rather than a continuous route through the building.
	CreateBlock(FVector(975, -235, 195), FVector(20, 330, 390), DarkConcreteMaterial);
	// Stop the right wall before the fifth-floor landing. The upper flight runs
	// along -Y, but the player turns +X at Y=-420; extending this wall to the
	// landing made the enclosure look complete while silently sealing the turn.
	constexpr float UpperFlightRightWallCenterY = -200.0f;
	constexpr float UpperFlightRightWallDepth = 260.0f;
	constexpr float LandingRouteY = -420.0f;
	constexpr float PlayerCapsuleRadius = 34.0f;
	constexpr float RequiredWallClearance = 20.0f;
	static_assert(
		UpperFlightRightWallCenterY - UpperFlightRightWallDepth * 0.5f
			>= LandingRouteY + PlayerCapsuleRadius + RequiredWallClearance,
		"The upper-flight wall must leave a capsule-safe turn at the landing.");
	CreateBlock(
		FVector(1210, UpperFlightRightWallCenterY, 195),
		FVector(20, UpperFlightRightWallDepth, 390),
		DarkConcreteMaterial);
	CreateBlock(FVector(1092.5f, -235, 400), FVector(255, 330, 20), DarkConcreteMaterial);
	if (PlaneMesh && DecalRainGrimeMaterial)
	{
		CreateBlock(
			FVector(986.0f, 118.0f, 132.0f),
			FVector(150.0f, 175.0f, 1.0f),
			DecalRainGrimeMaterial,
			false,
			FRotator(90.0f, 0.0f, 0.0f),
			PlaneMesh);
	}

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
		TankRevealWaterMaterial,
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
	// Sits on the cap of the up-route bulkhead below it. At Z 252, Y -30 the
	// fitting hung fourteen centimetres above that cap with most of its depth
	// off the end of it.
	CreateBlock(
		FVector(1110, -42, 238),
		FVector(62, 18, 4),
		ScreenMaterial,
		false);
	P4LandingLight = CreatePointLight(
		FVector(1110, -30, 232),
		1180.0f,
		560.0f,
		FLinearColor(0.34f, 0.46f, 0.39f),
		false);

	// A fixed wall bulkhead and handrail reveal the stair pitch without
	// removing the dark landing beyond it.  These are static scene components;
	// there is no per-frame animation or spawning as the loop changes.
	CreateBlock(
		FVector(1197.5f, -205, 226),
		FVector(5, 42, 22),
		ScreenMaterial,
		false);
	CreatePointLight(
		FVector(1168, -205, 220),
		880.0f,
		470.0f,
		FLinearColor(0.30f, 0.39f, 0.34f),
		false);
	// The next practical is visible before the player reaches the turn. It
	// establishes a real upper landing and preserves a dark pocket behind it.
	CreateBlock(
		FVector(1085, -365, 387.5f),
		FVector(58, 20, 5),
		ScreenMaterial,
		false);
	CreatePointLight(
		FVector(1085, -350, 360),
		680.0f,
		430.0f,
		FLinearColor(0.28f, 0.39f, 0.36f),
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
	P4ReceiptFragment = CreateBlock(
		FVector(1090, -250, 190),
		FVector(24, 13, 1.2f),
		WetPaperMaterial,
		false,
		FRotator(0, 0, 12),
		nullptr,
		true);
	if (P4ReceiptFragment)
	{
		P4ReceiptFragment->SetVisibility(false);
	}
}

void AIGThirdMorningDirector::BuildFifthFloorAndRoof()
{
	constexpr float LandingRouteY = -420.0f;
	constexpr float DoorLowerInnerY = -480.0f;
	constexpr float DoorUpperInnerY = -320.0f;
	constexpr float RequiredCapsuleHalfWidth = 34.0f + 20.0f;
	constexpr float HangingVinylWallY = -322.0f;
	// Half-thickness plus the farthest 5.75-degree roll of the 210 cm sheet.
	constexpr float HangingVinylMaximumRouteReach = 12.0f;
	static_assert(
		LandingRouteY - DoorLowerInnerY >= RequiredCapsuleHalfWidth
			&& DoorUpperInnerY - LandingRouteY >= RequiredCapsuleHalfWidth,
		"The fifth-floor landing must preserve the 160 cm doorway route.");
	static_assert(
		HangingVinylWallY - HangingVinylMaximumRouteReach
			>= LandingRouteY + RequiredCapsuleHalfWidth,
		"The moving vinyl must not visually seal the landing route.");

	// Unfinished fifth-floor landing.
	// The slab overlaps the first roof riser by one centimeter so no invisible
	// fall-through strip exists between the two pieces.
	CreateBlock(
		FVector(1265, -451.5f, 170),
		FVector(470, 147, 20),
		ConcreteMaterial);
	// Extend only the doorway side of the landing. Extending the whole slab
	// north would cover the last upper-flight treads and turn them into an
	// invisible 30 cm step.
	CreateBlock(
		FVector(1355, -340.0f, 170),
		FVector(290, 76, 20),
		ConcreteMaterial);
	// The stair opens into a real enclosed landing, not straight into the sky.
	// The landing-side wall starts east of the stair aperture and meets the
	// north roof-door jamb at its inner edge. This preserves the authored
	// 160 cm doorway instead of narrowing it with an overlapping wall. The
	// extended slab, west wall and soffit close every sightline visible from
	// 4F while retaining the unfinished concrete character of the space.
	CreateBlock(
		FVector(1235, -525, 290),
		FVector(530, 18, 220),
		DarkConcreteMaterial);
	CreateBlock(
		FVector(1020, -451.5f, 290),
		FVector(20, 147, 220),
		DarkConcreteMaterial);
	CreateBlock(
		FVector(1355, -311, 290),
		FVector(290, 18, 220),
		DarkConcreteMaterial);
	CreateBlock(
		FVector(1235, -451.5f, 410),
		FVector(530, 147, 20),
		DarkConcreteMaterial);
	CreateBlock(
		FVector(1355, -340.0f, 410),
		FVector(290, 76, 20),
		DarkConcreteMaterial);
	// A weak maintenance fixture makes the four risers and the latched fire
	// door read as a navigable route, not a black collision wall.  It stays
	// shadowless and local so the fifth floor remains the story's quiet,
	// low-pressure transition rather than becoming a safe room.
	CreateBlock(
		FVector(1415, -420, 398.5f),
		FVector(48, 14, 3),
		ScreenMaterial,
		false);
	CreatePointLight(
		FVector(1415, -420, 386),
		1850.0f,
		520.0f,
		FLinearColor(0.24f, 0.34f, 0.29f),
		false);
	// A dim reflected pool on the roof side separates the four risers from the
	// fire-door leaf. It carries no visible fixture and stays below the landing
	// practical, reading as spill from outdoors rather than a second safe light.
	CreatePointLight(
		FVector(1570, -420, 305),
		540.0f,
		360.0f,
		FLinearColor(0.17f, 0.25f, 0.32f),
		false);
	// A 160 x 210 cm unfinished doorway connects the landing to the roof
	// risers. The side piers preserve the load-bearing wall silhouette.
	CreateBlock(FVector(1450, -502.5f, 205), FVector(18, 45, 410), DarkConcreteMaterial);
	CreateBlock(FVector(1450, -297.5f, 205), FVector(18, 45, 410), DarkConcreteMaterial);
	CreateBlock(FVector(1450, -400, 400), FVector(18, 160, 20), DarkConcreteMaterial);
	for (int32 BeamIndex = 0; BeamIndex < 4; ++BeamIndex)
	{
		CreateBlock(
			FVector(1100 + BeamIndex * 120, -510, 325),
			FVector(16, 16, 310),
			MetalMaterial);
	}
	// Keep the construction vinyl on the landing wall instead of laying a
	// bright, non-colliding plane across the playable doorway.  The dedicated
	// film material catches the flashlight at a grazing angle while the entire
	// 160 cm route remains visually legible.
	HangingVinyl = CreateBlock(
		FVector(1325, HangingVinylWallY, 300),
		FVector(150, 3, 210),
		CarrierBagMaterial,
		false,
		FRotator(0, 0, -4),
		nullptr,
		true);
	if (HangingVinyl && GetWorld())
	{
		// One 20 Hz transform is enough for slow, irregular movement and avoids
		// enabling an otherwise unnecessary per-frame actor tick.
		GetWorldTimerManager().SetTimer(
			HangingVinylTimer,
			this,
			&ThisClass::UpdateHangingVinylSway,
			0.05f,
			true);
	}

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

	const bool bHasAuthoredRoofDoor =
		RooftopFireDoorLeafMesh && RooftopFireDoorFrameMesh;
	if (bHasAuthoredRoofDoor)
	{
		CreateBlock(
			IGThirdMorning::RoofDoorClosedLocation,
			FVector(100.0f, 100.0f, 100.0f),
			RoofDoorMaterial,
			false,
			FRotator::ZeroRotator,
			RooftopFireDoorFrameMesh);
	}
	else
	{
		// Primitive fallback preserves the same 120 x 234 cm clear opening.
		for (const float JambY : {-464.0f, -336.0f})
		{
			CreateBlock(
				FVector(1669.0f, JambY, 357.0f),
				FVector(14.0f, 8.0f, 234.0f),
				RoofDoorMaterial,
				false);
		}
		CreateBlock(
			FVector(1669.0f, -400.0f, 478.0f),
			FVector(14.0f, 136.0f, 8.0f),
			RoofDoorMaterial,
			false);
		CreateBlock(
			FVector(1669.0f, -400.0f, 240.5f),
			FVector(14.0f, 136.0f, 1.2f),
			RoofDoorMaterial,
			false);
	}

	// Thin hidden stop volumes keep the frame opening authoritative without a
	// convex hull filling the visible authored frame.  Their inner X face is
	// the closed plane used by the 11 cm free-edge measurement.
	for (const float JambY : {-464.0f, -336.0f})
	{
		// physics-audit: intentional hidden collision proxy inside the frame
		if (UStaticMeshComponent* JambCollision = CreateBlock(
			FVector(1664.0f, JambY, 357.0f),
			FVector(4.0f, 8.0f, 234.0f),
			MetalMaterial,
			true))
		{
			JambCollision->SetVisibility(false, true);
			JambCollision->SetHiddenInGame(true, true);
		}
	}

	RoofDoorAction = SpawnAction(
		EIGChapterThreeAction::OpenRoofDoor,
		IGThirdMorning::RoofDoorLatchedLocation,
		bHasAuthoredRoofDoor
			? FVector(100.0f, 100.0f, 100.0f)
			: FVector(4.5f, 116.0f, 230.0f),
		RoofDoorMaterial,
		NSLOCTEXT("IGCH03", "RoofDoorPrompt", "옥상 철문 당기기"),
		0.0f,
		IGThirdMorning::RoofDoorLatchedRotation,
		bHasAuthoredRoofDoor ? RooftopFireDoorLeafMesh.Get() : nullptr);
	if (RoofDoorAction)
	{
		RoofDoorLeaf = RoofDoorAction->GetPresentationMesh();
		if (RoofDoorLeaf)
		{
			RoofDoorLeaf->SetMobility(EComponentMobility::Movable);
		}
		ApplyRoofDoorState(EIGRoofDoorState::LatchedGap);
	}
	const bool bHasAuthoredPadlockKeys =
		RooftopUnlockedPadlockKeysMesh != nullptr;
	const FVector PadlockKeysLocation(1659.5f, -336.0f, 353.5f);
	KeysAction = SpawnAction(
		EIGChapterThreeAction::InspectKeys,
		PadlockKeysLocation,
		bHasAuthoredPadlockKeys
			? FVector(100.0f, 100.0f, 100.0f)
			: FVector(2.8f, 5.0f, 6.2f),
		MetalMaterial,
		NSLOCTEXT(
			"IGCH03",
			"KeysPrompt",
			"열린 자물쇠와 꽂힌 열쇠 확인하기"),
		0.0f,
		FRotator::ZeroRotator,
		bHasAuthoredPadlockKeys
			? RooftopUnlockedPadlockKeysMesh.Get()
			: nullptr);
	if (!bHasAuthoredPadlockKeys)
	{
		// The fallback keeps the same unlocked read: one open shackle, one
		// inserted stem and three hanging blades.  It never becomes a solid
		// nineteen-centimetre key-shaped cube.
		CreateBlock(
			PadlockKeysLocation + FVector(0.0f, -1.65f, 4.8f),
			FVector(0.8f, 0.8f, 4.0f),
			MetalMaterial,
			false,
			FRotator::ZeroRotator,
			CylinderMesh);
		CreateBlock(
			PadlockKeysLocation + FVector(0.0f, 1.65f, 6.45f),
			FVector(0.8f, 0.8f, 1.2f),
			MetalMaterial,
			false,
			FRotator::ZeroRotator,
			CylinderMesh);
		CreateBlock(
			PadlockKeysLocation + FVector(0.0f, 0.0f, 7.0f),
			FVector(0.8f, 4.1f, 0.8f),
			MetalMaterial,
			false);
		CreateBlock(
			PadlockKeysLocation + FVector(0.0f, 0.0f, -3.6f),
			FVector(0.24f, 0.82f, 2.2f),
			MetalMaterial,
			false);
		const FVector FallbackKeySizes[] = {
			FVector(0.22f, 0.72f, 5.0f),
			FVector(0.22f, 0.90f, 4.0f),
			FVector(0.22f, 0.64f, 3.0f)};
		for (int32 KeyIndex = 0; KeyIndex < 3; ++KeyIndex)
		{
			CreateBlock(
				PadlockKeysLocation + FVector(
					0.0f,
					-1.15f + KeyIndex * 1.15f,
					-9.15f + KeyIndex * 0.50f),
				FallbackKeySizes[KeyIndex],
				MetalMaterial,
				false,
				FRotator(0.0f, 0.0f, -5.0f + KeyIndex * 6.0f));
		}
	}

	// Wide roof: the absence of traffic and animals is legible because there
	// is room for the player to wait and hear nothing.
	IGThirdMorning::TagFootstepSurface(
		CreateBlock(
			FVector(2360, -400, IGThirdMorning::RoofFloorZ - 10),
			FVector(1450, 1180, 20),
			ConcreteMaterial),
		IGThirdMorning::FootstepRooftopTag);
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

	// Distant Korean rooftop landmark: a muted church cross fixed to an actual
	// neighbouring roof silhouette. The old marker stood beside the capture
	// camera and appeared as three unrelated floating light bars.
	CreateBlock(
		FVector(1580, -1900, 430), FVector(360, 160, 380),
		DarkConcreteMaterial, false);
	CreateBlock(
		FVector(1580, -1900, 690), FVector(6, 6, 130),
		EmergencyMaterial, false);
	CreateBlock(
		FVector(1580, -1900, 720), FVector(62, 6, 6),
		EmergencyMaterial, false);

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
		FVector(2650, -560, 548),
		FVector(86, 24, 12),
		MetalMaterial,
		false,
		FRotator(0, -45, 0));
	CreateBlock(
		FVector(2650, -560, 541),
		FVector(60, 16, 3),
		ScreenMaterial,
		false,
		FRotator(0, -45, 0));
	CreatePointLight(
		FVector(2645, -555, 530),
		4600.0f,
		1620.0f,
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
		2050.0f,
		920.0f,
		FLinearColor(0.16f, 0.30f, 0.46f),
		false);
}

void AIGThirdMorningDirector::UpdateHangingVinylSway()
{
	if (!HangingVinyl || !GetWorld())
	{
		return;
	}

	const float Seconds = GetWorld()->GetTimeSeconds();
	const float SlowDrift = FMath::Sin(Seconds * 0.83f) * 1.4f;
	const float UnevenFlutter = FMath::Sin(Seconds * 1.73f + 0.8f) * 0.35f;
	HangingVinyl->SetRelativeRotation(
		FRotator(0.0f, 0.0f, -4.0f + SlowDrift + UnevenFlutter));
}

void AIGThirdMorningDirector::BuildWaterTank()
{
	const FVector Tank = IGThirdMorning::TankCenter;
	UMaterialInterface* const TankMetal = WaterTankMetalMaterial
		? WaterTankMetalMaterial.Get()
		: MetalMaterial.Get();

	// Concrete plinth and the authored hollow shell. Sixteen invisible panels
	// retain the proven collision envelope without leaking greybox geometry into
	// the final silhouette.
	CreateBlock(Tank + FVector(0, 0, 50 + IGThirdMorning::RoofFloorZ),
		FVector(360, 360, 100), ConcreteMaterial);
	const bool bHasAuthoredTankShell =
		RooftopWaterTankShellMesh != nullptr;
	if (bHasAuthoredTankShell)
	{
		CreateBlock(
			Tank + FVector(0, 0, IGThirdMorning::TankShellCenterZ),
			FVector(100.0f),
			TankMetal,
			false,
			FRotator::ZeroRotator,
			RooftopWaterTankShellMesh);
	}
	for (int32 PanelIndex = 0; PanelIndex < 16; ++PanelIndex)
	{
		const float AngleDegrees = PanelIndex * 22.5f;
		const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		const FVector Radial(
			FMath::Cos(AngleRadians) * 145.0f,
			FMath::Sin(AngleRadians) * 145.0f,
			0.0f);
		UStaticMeshComponent* const CollisionPanel = CreateBlock(
			Tank + Radial + FVector(0, 0, 470),
			FVector(58, 12, 260),
			TankMetal,
			true,
			FRotator(0, AngleDegrees + 90.0f, 0));
		if (bHasAuthoredTankShell && CollisionPanel)
		{
			CollisionPanel->SetVisibility(false, true);
			CollisionPanel->SetHiddenInGame(true, true);
		}
	}
	if (TankInternalLiningMesh)
	{
		// The wet lining is visual-only and sits behind the canonical collision
		// panels. Disabling its shadow prevents a second shell from darkening the
		// reveal while its PBR channels still respond to the flashlight and Lumen.
		if (UStaticMeshComponent* const InteriorLining = CreateBlock(
			Tank,
			FVector(100.0f),
			TankInteriorBiofilmMaterial,
			false,
			FRotator::ZeroRotator,
			TankInternalLiningMesh))
		{
			InteriorLining->SetCastShadow(false);
		}
	}
	if (PlaneMesh && DecalRustFastenersMaterial)
	{
		// Two surviving screws and one empty hole repeat the same maintenance
		// failure language established before the accident.
		CreateBlock(
			Tank + FVector(-151.2f, 0.0f, 520.0f),
			FVector(112.0f, 74.0f, 1.0f),
			DecalRustFastenersMaterial,
			false,
			FRotator(90.0f, 0.0f, 0.0f),
			PlaneMesh);
	}
	if (PlaneMesh && DecalMineralScaleMaterial)
	{
		// Mineral residue streaks down the inner wall immediately below the hatch.
		// Keeping the masked plane vertical preserves the maintenance clue without
		// projecting a bright ring across the water or the body silhouette.
		CreateBlock(
			Tank + FVector(-151.0f, 45.0f, 580.0f),
			FVector(58.0f, 58.0f, 1.0f),
			DecalMineralScaleMaterial,
			false,
			FRotator(90.0f, 0.0f, 0.0f),
			PlaneMesh);
	}

	// The authored shell owns its three continuous hoops. Development checkouts
	// without baked assets retain the segmented, non-colliding fallback.
	if (!bHasAuthoredTankShell)
	{
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
					TankMetal,
					false,
					FRotator(0, AngleDegrees + 90.0f, 0));
			}
		}
	}

	// Real-scale 89/76 mm plumbing replaces the oversized engine cylinders.
	// The static valve reuses the same 18 cm authored wheel as P3.
	const bool bHasAuthoredTankPlumbing =
		RooftopTankPipeClusterMesh && P3LargeValveWheelMesh;
	if (bHasAuthoredTankPlumbing)
	{
		CreateBlock(
			Tank,
			FVector(100.0f),
			TankMetal,
			false,
			FRotator::ZeroRotator,
			RooftopTankPipeClusterMesh);
		CreateBlock(
			Tank + FVector(181, -128, 302),
			FVector(100.0f),
			TankMetal,
			false,
			FRotator(90, 0, 0),
			P3LargeValveWheelMesh);
	}
	else
	{
		CreateBlock(
			Tank + FVector(128, -128, 392),
			FVector(8.9f, 8.9f, 300),
			TankMetal,
			false,
			FRotator::ZeroRotator,
			CylinderMesh);
		CreateBlock(
			Tank + FVector(154.5f, -128, 302),
			FVector(7.6f, 7.6f, 53),
			TankMetal,
			false,
			FRotator(90, 0, 0),
			CylinderMesh);
		for (const float ClampZ : {340.0f, 438.0f})
		{
			CreateBlock(
				Tank + FVector(128, -128, ClampZ),
				FVector(11.6f, 11.6f, 2.4f),
				DarkConcreteMaterial,
				false,
				FRotator::ZeroRotator,
				CylinderMesh);
		}
		CreateBlock(
			Tank + FVector(181, -128, 302),
			FVector(18, 18, 3),
			TankMetal,
			false,
			FRotator(90, 0, 0),
			CylinderMesh);
	}

	// The final reveal names a real internal ladder, so it must exist in the
	// same tank geometry before the water plane is drawn over it. Seven rungs
	// descend from the hatch to the raised inner floor without offering a new
	// climb interaction or collision trap.
	if (TankInternalLadderMesh)
	{
		CreateBlock(
			Tank,
			FVector(100.0f),
			TankMetal,
			false,
			FRotator::ZeroRotator,
			TankInternalLadderMesh);
	}
	else
	{
		for (const float RailY : {-23.0f, 23.0f})
		{
			CreateBlock(
				Tank + FVector(-125, RailY, 488),
				FVector(3.4f, 3.4f, 204),
				TankMetal,
				false);
			for (const float BracketZ : {405.0f, 575.0f})
			{
				CreateBlock(
					Tank + FVector(-135, RailY, BracketZ),
					FVector(20, 3.4f, 3.4f),
					TankMetal,
					false);
			}
		}
		for (int32 InnerRungIndex = 0; InnerRungIndex < 7; ++InnerRungIndex)
		{
			CreateBlock(
				Tank + FVector(-125, 0, 400 + InnerRungIndex * 30.0f),
				FVector(2.5f, 46, 2.5f),
				TankMetal,
				false);
		}
	}

	// A small service lamp on the pipe side lifts the lower shell and bands,
	// while the opposite side remains available for the flashlight scare.
	CreateBlock(
		Tank + FVector(105, -105, 570),
		FVector(30, 12, 22),
		TankMetal,
		false,
		FRotator(0, 45, 0));
	CreateBlock(
		Tank + FVector(110, -110, 570),
		FVector(22, 2, 14),
		ScreenMaterial,
		false,
		FRotator(0, 45, 0));
	CreatePointLight(
		Tank + FVector(123, -123, 560),
		1580.0f,
		760.0f,
		FLinearColor(0.34f, 0.45f, 0.58f),
		false);

	TankWaterSurface = CreateBlock(
		Tank + FVector(0, 0, IGThirdMorning::TankWaterSurfaceZ),
		PlaneMesh ? FVector(270, 270, 1.0f) : FVector(270, 270, 3.0f),
		TankRevealWaterMaterial,
		false,
		FRotator::ZeroRotator,
		PlaneMesh.Get());
	if (TankWaterSurface)
	{
		// One horizontal sheet avoids the doubled opacity and sorting seams of
		// a translucent cube. It must never cast a black slab over the body.
		TankWaterSurface->SetCastShadow(false);
		TankWaterSurface->SetTranslucentSortPriority(2);
	}
	TankRevealKeyLight = CreatePointLight(
		Tank + FVector(-90, 0, 572),
		0.0f,
		270.0f,
		FLinearColor(0.36f, 0.42f, 0.48f),
		false);
	TankRevealRimLight = CreatePointLight(
		// The black slides return beside the knees in the compact foetal pose. A
		// small cool grazing light reveals sole, strap and ankle without flattening
		// the torso or turning the whole tank into a lit display case.
		Tank + FVector(-70, 82, 552),
		0.0f,
		250.0f,
		FLinearColor(0.22f, 0.27f, 0.33f),
		false);
	if (TankRevealKeyLight)
	{
		TankRevealKeyLight->SetVisibility(false);
		TankRevealKeyLight->SetSourceRadius(50.0f);
		TankRevealKeyLight->SetSoftSourceRadius(90.0f);
		TankRevealKeyLight->SetSpecularScale(0.12f);
	}
	if (TankRevealRimLight)
	{
		TankRevealRimLight->SetVisibility(false);
		TankRevealRimLight->SetSourceRadius(24.0f);
		TankRevealRimLight->SetSoftSourceRadius(54.0f);
		TankRevealRimLight->SetSpecularScale(0.08f);
	}

	// The authored 45-degree stair and the proven 18-step collision route share
	// the same origin, 20 cm run and 20 cm rise. The visual mesh omits tread 16;
	// the separate failure cluster supplies that upper-second evidence tread.
	const bool bHasAuthoredExteriorStair =
		TankExteriorAccessStairMesh && LadderFailureRungMesh
		&& LadderRungPadMesh && LadderRungClipsMesh;
	if (bHasAuthoredExteriorStair)
	{
		CreateBlock(
			FVector(1915, -300, IGThirdMorning::RoofFloorZ),
			FVector(100.0f),
			TankMetal,
			false,
			FRotator::ZeroRotator,
			TankExteriorAccessStairMesh);
	}
	for (int32 StepIndex = 0; StepIndex < 18; ++StepIndex)
	{
		const float TopZ = IGThirdMorning::RoofFloorZ + 20.0f * (StepIndex + 1);
		const float Height = TopZ - IGThirdMorning::RoofFloorZ;
		UStaticMeshComponent* const StairCollision = CreateBlock(
			FVector(1915.0f + StepIndex * 20.0f, -300, IGThirdMorning::RoofFloorZ + Height * 0.5f),
			FVector(22, 105, Height),
			TankMetal);
		// 저작 계단이 있으면 이 블록은 숨지만 충돌은 그대로 남는다.
		// 발소리 판정은 Visibility 채널 트레이스라 숨긴 뒤에도 이 태그를 읽는다.
		IGThirdMorning::TagFootstepSurface(
			StairCollision,
			IGThirdMorning::FootstepMetalStairTag);
		if (bHasAuthoredExteriorStair && StairCollision)
		{
			StairCollision->SetVisibility(false, true);
			StairCollision->SetHiddenInGame(true, true);
		}
	}
	if (bHasAuthoredExteriorStair)
	{
		CreateBlock(
			FVector(2235, -300, 578.5f),
			FVector(100.0f),
			TankMetal,
			false,
			FRotator::ZeroRotator,
			LadderFailureRungMesh);
	}
	else
	{
		// Development checkouts without baked assets keep the same diagonal
		// travel line. Two primitive rails replace the old unrelated vertical
		// rung wall, so even the fallback preserves physical coherence.
		for (const float Side : {-1.0f, 1.0f})
		{
			const FVector RailStart(1915, -300 + Side * 56.0f, 335);
			const FVector RailEnd(2270, -300 + Side * 105.0f, 695);
			const FVector RailDirection = (RailEnd - RailStart).GetSafeNormal();
			CreateBlock(
				(RailStart + RailEnd) * 0.5f,
				FVector(4.2f, 4.2f, FVector::Distance(RailStart, RailEnd)),
				TankMetal,
				false,
				FQuat::FindBetweenNormals(FVector::UpVector, RailDirection).Rotator(),
				CylinderMesh);
		}
	}
	// Start the top platform where the last tread ends. The previous overlap
	// buried that tread under a 40 cm lip even though every authored rise was
	// intended to stay at 20 cm.
	IGThirdMorning::TagFootstepSurface(
		CreateBlock(FVector(2308.5f, -300, 610), FVector(87, 220, 20), TankMetal),
		IGThirdMorning::FootstepMetalStairTag);

	// A compact two-sided guardrail makes the top landing physically legible.
	// The approach edge remains open, while invisible side volumes preserve the
	// safety boundary without depending on generated complex collision.
	if (TankAccessGuardRailMesh)
	{
		CreateBlock(
			FVector(2335, -300, 620),
			FVector(100.0f),
			TankMetal,
			false,
			FRotator::ZeroRotator,
			TankAccessGuardRailMesh);
	}
	else
	{
		for (const float RailY : {-405.0f, -195.0f})
		{
			CreateBlock(
				FVector(2304.5f, RailY, 695),
				FVector(69, 4.2f, 4.2f),
				TankMetal,
				false);
			CreateBlock(
				FVector(2304.5f, RailY, 658),
				FVector(69, 4.2f, 4.2f),
				TankMetal,
				false);
			for (const float PostX : {2270.0f, 2339.0f})
			{
				CreateBlock(
					FVector(PostX, RailY, 657.5f),
					FVector(4.2f, 4.2f, 75),
					TankMetal,
					false);
			}
		}
		CreateBlock(
			FVector(2337, -193, 677),
			FVector(10, 1.6f, 12),
			TankMetal,
			false);
		CreateBlock(
			FVector(2335, -191.5f, 672),
			FVector(1.2f, 13, 1.2f),
			TankMetal,
			false);
		CreateBlock(
			FVector(2335, -191.5f, 680),
			FVector(1.2f, 13, 1.2f),
			TankMetal,
			false);
		CreateBlock(
			FVector(2335, -185, 676),
			FVector(1.2f, 1.2f, 8),
			TankMetal,
			false);
	}
	for (const float RailY : {-405.0f, -195.0f})
	{
		if (UStaticMeshComponent* RailCollision = CreateBlock(
			FVector(2304.5f, RailY, 657.5f),
			FVector(69, 6, 75),
			TankMetal,
			true))
		{
			RailCollision->SetVisibility(false, true);
			RailCollision->SetHiddenInGame(true, true);
		}
	}

	const bool bHasGlassesMesh = HornRimGlassesMesh != nullptr;
	GlassesAction = SpawnAction(
		EIGChapterThreeAction::EvidenceGlasses,
		FVector(2335, -184, 662),
		bHasGlassesMesh ? FVector(100.0f) : FVector(16, 5, 3),
		PlasticMaterial,
		NSLOCTEXT(
			"IGCH03",
			"GlassesPrompt",
			"난간 U볼트의 젖은 안경 확인하기"),
		0.0f,
		FRotator(0.0f, -8.0f, 82.0f),
		bHasGlassesMesh ? HornRimGlassesMesh.Get() : nullptr);
	if (GlassesAction)
	{
		EvidenceActions.Add(EIGChapterThreeAction::EvidenceGlasses, GlassesAction);
	}
	if (!bHasGlassesMesh)
	{
		// Two narrow temple lines preserve the hanging read without duplicating
		// the action's single fallback frame proxy.
		CreateBlock(
			FVector(2333, -184, 668),
			FVector(2, 2, 12),
			PlasticMaterial,
			false,
			FRotator(0, -8, 8));
		CreateBlock(
			FVector(2337, -184, 668),
			FVector(2, 2, 12),
			PlasticMaterial,
			false,
			FRotator(0, 8, -8));
	}

	// A person can lift the service hatch, not the tank's entire three-metre
	// roof. The authored annulus leaves a 104 cm opening; four plates preserve
	// the same opening in an unbaked development checkout.
	if (TankAccessDeckMesh)
	{
		CreateBlock(
			Tank + FVector(0, 0, 600),
			FVector(100.0f),
			TankMetal,
			false,
			FRotator::ZeroRotator,
			TankAccessDeckMesh);
	}
	else
	{
		CreateBlock(Tank + FVector(-151, 0, 600), FVector(6, 308, 8), TankMetal, false);
		CreateBlock(Tank + FVector(55, 0, 600), FVector(198, 308, 8), TankMetal, false);
		CreateBlock(Tank + FVector(-96, -103, 600), FVector(104, 102, 8), TankMetal, false);
		CreateBlock(Tank + FVector(-96, 103, 600), FVector(104, 102, 8), TankMetal, false);
	}

	TankLidAction = SpawnAction(
		EIGChapterThreeAction::OpenTank,
		Tank + IGThirdMorning::TankHatchOffset + FVector(0, 0, 610),
		TankAccessLidMesh ? FVector(100.0f) : FVector(108, 108, 6),
		TankMetal,
		NSLOCTEXT("IGCH03", "OpenTankPrompt", "물탱크 뚜껑 열기"),
		1.2f,
		FRotator::ZeroRotator,
		TankAccessLidMesh ? TankAccessLidMesh.Get() : CylinderMesh.Get());

	CloseChoiceAction = SpawnAction(
		EIGChapterThreeAction::CloseTank,
		Tank + FVector(-155, -82, 625),
		FVector(24, 10, 8),
		TankMetal,
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

	// The loose inspection rod itself, resting on the deck by the tank base
	// where a maintenance hand once left it. Choosing B moves it up into the
	// support groove; the shared actual-state restore returns it here.
	InspectionRodVisual = CreateBlock(
		Tank + FVector(-196, 128, IGThirdMorning::RoofFloorZ + 4.0f),
		InspectionRodMesh ? FVector(100.0f) : FVector(7, 7, 118),
		TankMetal,
		false,
		FRotator(90, 24, 0),
		InspectionRodMesh ? InspectionRodMesh.Get() : CylinderMesh.Get(),
		true);

	// Settling ripples live in the single translucent water material. Separate
	// opaque ring segments used to float above the surface, hide refraction,
	// and read like a targeting reticle under the flashlight.

	// The reveal prefers three authored static groups sharing one local origin.
	// The compact pose sits directly below the real 104 cm service hatch. This
	// placement is part of the physical sightline: the player never sees a body
	// through an impossible missing tank roof.
	const FVector& AuthoredBodyPlacement =
		IGThirdMorning::AuthoredTankBodyPlacement;
	const FRotator& AuthoredBodyRotation =
		IGThirdMorning::AuthoredTankBodyRotation;
	const FQuat AuthoredBodyRotationQuat = AuthoredBodyRotation.Quaternion();
	auto RotateAuthoredBodyOffset =
		[AuthoredBodyPlacement, AuthoredBodyRotationQuat](const FVector& LocalOffset)
	{
		return AuthoredBodyPlacement
			+ AuthoredBodyRotationQuat.RotateVector(LocalOffset);
	};
	auto RotateAuthoredBodyRotation =
		[AuthoredBodyRotationQuat](const FRotator& LocalRotation)
	{
		return (AuthoredBodyRotationQuat * LocalRotation.Quaternion()).Rotator();
	};
	auto AddBodyPiece =
		[this, &Tank](
			const FVector& Offset,
			const FVector& Size,
			UMaterialInterface* Material,
			UStaticMesh* Mesh,
			const FRotator& Rotation = FRotator::ZeroRotator)
	{
		if (UStaticMeshComponent* Piece = CreateBlock(
			Tank + Offset + FVector(
				0,
				0,
				IGThirdMorning::TankBodyPlacementAdjustmentZ),
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

	const bool bHasAuthoredBody =
		SubmergedHoodieMesh &&
		SubmergedPantsMesh &&
		SubmergedSlippersMesh;
	bUsesAuthoredTankBody = bHasAuthoredBody;
	if (bHasAuthoredBody)
	{
		// The revised hood crown peaks below local Z=21, leaving more than 14 cm
		// of real water above the body. All three anatomical clothing groups share
		// the hatch-aligned origin and therefore cannot drift apart.
		AddBodyPiece(
			AuthoredBodyPlacement,
			FVector(100.0f),
			WetHoodieMaterial,
			SubmergedHoodieMesh,
			AuthoredBodyRotation);
		AddBodyPiece(
			AuthoredBodyPlacement,
			FVector(100.0f),
			SubmergedPantsMaterial,
			SubmergedPantsMesh,
			AuthoredBodyRotation);
		AddBodyPiece(
			AuthoredBodyPlacement,
			FVector(100.0f),
			SubmergedSlippersMaterial,
			SubmergedSlippersMesh,
			AuthoredBodyRotation);
	}
	else
	{
		// Development machines without baked assets retain the proven greybox.
		auto AddLimb =
			[this, &AddBodyPiece](
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
		WetHoodieMaterial,
		SphereMesh,
		FRotator(0, 16, -3));
	AddBodyPiece(
		FVector(-34, -3, 551),
		FVector(52, 43, 27),
		SubmergedPantsMaterial,
		SphereMesh,
		FRotator(0, 12, 0));
	AddBodyPiece(
		FVector(62, 15, 556),
		FVector(31, 29, 29),
		WetHoodieMaterial,
		SphereMesh,
		FRotator(0, 9, 0));
	AddBodyPiece(
		FVector(66, 18, 563),
		FVector(36, 34, 17),
		WetHoodieMaterial,
		SphereMesh,
		FRotator(0, 14, 8));
	AddBodyPiece(
		FVector(50, 11, 553),
		FVector(18, 19, 18),
		WetHoodieMaterial,
		SphereMesh);

	// Arms folded in toward the chest.
	AddLimb(FVector(30, -10, 554), FVector(7, -38, 551), 13.5f, WetHoodieMaterial);
	AddLimb(FVector(7, -38, 551), FVector(-20, -24, 548), 11.5f, WetHoodieMaterial);
	AddBodyPiece(
		FVector(-23, -22, 548),
		FVector(13, 10, 8),
		WetHoodieMaterial,
		SphereMesh,
		FRotator(0, -12, 0));
	AddLimb(FVector(31, 20, 554), FVector(4, 44, 551), 13.5f, WetHoodieMaterial);
	AddLimb(FVector(4, 44, 551), FVector(-25, 31, 548), 11.5f, WetHoodieMaterial);
	AddBodyPiece(
		FVector(-28, 29, 548),
		FVector(13, 10, 8),
		WetHoodieMaterial,
		SphereMesh,
		FRotator(0, 10, 0));

	// Bent legs give the same uneasy foetal posture glimpsed in room 403.
	AddLimb(FVector(-30, -8, 551), FVector(-63, -42, 549), 20.0f, SubmergedPantsMaterial);
	AddLimb(FVector(-63, -42, 549), FVector(-107, -21, 547), 16.5f, SubmergedPantsMaterial);
	AddBodyPiece(
		FVector(-115, -17, 547),
		FVector(28, 16, 12),
		SubmergedSlippersMaterial,
		SphereMesh,
		FRotator(0, -19, 0));
	AddLimb(FVector(-33, 8, 551), FVector(-62, 38, 550), 20.0f, SubmergedPantsMaterial);
	AddLimb(FVector(-62, 38, 550), FVector(-102, 27, 547), 16.5f, SubmergedPantsMaterial);
	AddBodyPiece(
		FVector(-111, 25, 547),
		FVector(28, 16, 12),
		SubmergedSlippersMaterial,
		SphereMesh,
		FRotator(0, 11, 0));
	}

	// Three black repair stitches on the left sleeve and the worn outer heel
	// are separate identity source details, not one generic "same clothes" prop.
	const FVector StitchBase = bHasAuthoredBody
		? RotateAuthoredBodyOffset(
			IGThirdMorning::AuthoredSleeveStitchLocalBase)
		: FVector(-15.0f, -30.0f, 552.5f);
	const FVector StitchStep = bHasAuthoredBody
		? AuthoredBodyRotationQuat.RotateVector(
			IGThirdMorning::AuthoredSleeveStitchLocalStep)
		: FVector(4.0f, 0.0f, 0.0f);
	const FRotator StitchRotation = bHasAuthoredBody
		? RotateAuthoredBodyRotation(FRotator(0, 0, 18))
		: FRotator(0, 0, 18);
	for (int32 StitchIndex = 0; StitchIndex < 3; ++StitchIndex)
	{
		AddBodyPiece(
			StitchBase + StitchStep * StitchIndex,
			FVector(0.8f, 3.2f, 0.5f),
			SubmergedSlippersMaterial,
			CubeMesh,
			StitchRotation);
	}
	const FVector HeelWearLocation = bHasAuthoredBody
		? RotateAuthoredBodyOffset(FVector(31.5f, 6.0f, -6.0f))
		: FVector(-120, -19, 548.5f);
	const FRotator HeelWearRotation = bHasAuthoredBody
		? RotateAuthoredBodyRotation(FRotator(0, 2, 0))
		: FRotator(0, -19, 0);
	AddBodyPiece(
		HeelWearLocation,
		FVector(7, 11, 3),
		SubmergedSlipperWearMaterial,
		CubeMesh,
		HeelWearRotation);
	const FVector StripeBase = bHasAuthoredBody
		? RotateAuthoredBodyOffset(FVector(46.0f, 6.0f, 0.8f))
		: FVector(-110.0f, -17.0f, 554.5f);
	const FVector StripeStep = bHasAuthoredBody
		? AuthoredBodyRotationQuat.RotateVector(FVector(2.4f, 0.0f, 0.0f))
		: FVector(3.4f, 0.0f, 0.0f);
	for (int32 StripeIndex = 0; StripeIndex < 3; ++StripeIndex)
	{
		AddBodyPiece(
			StripeBase + StripeStep * StripeIndex,
			FVector(1.6f, 10.0f, 1.0f),
			SubmergedSlipperWearMaterial,
			CubeMesh,
			HeelWearRotation);
	}
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
	const bool bHasCarrierBagMesh = CarrierBagCollapsedMesh != nullptr;
	const FVector CarrierBagMeshScalePercent(
		BagSize.X / 12.0f * 100.0f,
		BagSize.Y / 18.0f * 100.0f,
		BagSize.Z / 20.0f * 100.0f);

	// The sagging roof door catches at the threshold and leaves an authored
	// 11 cm horizontal slit at its free edge; the underside stays near the floor.
	// Paw marks cross it in both directions; human access still requires the
	// existing pull interaction.
	const bool bHasCatPawPlane =
		PlaneMesh && EvidenceCatPawMaterial != WetStepMaterial;
	const bool bHasHosePlane =
		PlaneMesh && EvidenceHoseMaterial != WetStepMaterial;
	const bool bHasSlipperPlane =
		PlaneMesh && EvidenceSlipperMaterial != WetStepMaterial;
	const bool bHasHandSmearPlane =
		PlaneMesh && EvidenceHandSmearMaterial != WetStepMaterial;
	const bool bHasRungFailureCluster =
		TankExteriorAccessStairMesh && LadderFailureRungMesh
		&& LadderRungPadMesh && LadderRungClipsMesh;
	if (!bHasCatPawPlane)
	{
		CreateBlock(
			FVector(1666, -400, 244.2f),
			FVector(18, 122, 4.0f),
			WetStepMaterial,
			false);
	}
	for (int32 Step = 0; Step < (bHasCatPawPlane ? 0 : 3); ++Step)
	{
		CreateBlock(
			FVector(1698.0f + Step * 24.0f, -430.0f + Step * 5.0f, 242.5f),
			FVector(18, 24, 1.0f),
			EvidenceCatPawMaterial,
			false,
			FRotator(0, 12, 0),
			bHasCatPawPlane ? PlaneMesh.Get() : nullptr);
		CreateBlock(
			FVector(1770.0f - Step * 24.0f, -360.0f - Step * 5.0f, 242.7f),
			FVector(18, 24, 1.0f),
			EvidenceCatPawMaterial,
			false,
			FRotator(0, 192, 0),
			bHasCatPawPlane ? PlaneMesh.Get() : nullptr);
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
		FVector(28, 34, bHasCatPawPlane ? 1.0f : 2.0f),
		EvidenceCatPawMaterial,
		NSLOCTEXT("IGCH03", "EvidenceCatEntered", "들어온 앞발자국 확인하기"),
		FRotator(0, 12, 0),
		bHasCatPawPlane ? PlaneMesh.Get() : nullptr);
	AddEvidence(
		EIGChapterThreeAction::EvidenceCatExited,
		FVector(1750, -360, 245),
		FVector(28, 34, bHasCatPawPlane ? 1.0f : 2.0f),
		EvidenceCatPawMaterial,
		NSLOCTEXT("IGCH03", "EvidenceCatExited", "나간 앞발자국 확인하기"),
		FRotator(0, 192, 0),
		bHasCatPawPlane ? PlaneMesh.Get() : nullptr);

	// One continuous 42 mm service hose replaces the five disconnected 14 cm
	// greybox cylinders. The lower mesh deformation carries the paw compression
	// and the separate galvanized coupling carries the upper impact source.
	if (RooftopServiceHoseMesh)
	{
		CreateBlock(
			Tank,
			FVector(100.0f),
			WetServiceHoseMaterial,
			false,
			FRotator::ZeroRotator,
			RooftopServiceHoseMesh);
	}
	else
	{
		for (int32 Segment = 0; Segment < 5; ++Segment)
		{
			CreateBlock(
				Tank + FVector(
					180.0f - Segment * 55.0f,
					-180.0f + Segment * 18.0f,
					248.0f + Segment * 65.0f),
				FVector(4.2f, 4.2f, 82.0f),
				WetServiceHoseMaterial,
				false,
				FRotator(0, 18, -38),
				CylinderMesh);
		}
	}
	const FVector CouplingDirection =
		(FVector(-118, -72, 622) - FVector(-91, -86, 590)).GetSafeNormal();
	const FRotator CouplingRotation = FQuat::FindBetweenNormals(
		FVector::UpVector,
		CouplingDirection).Rotator();
	CreateBlock(
		Tank + FVector(-121, -70, 627),
		HoseCouplingMesh ? FVector(100.0f) : FVector(7.0f, 7.0f, 12.0f),
		WaterTankMetalMaterial,
		false,
		CouplingRotation,
		HoseCouplingMesh ? HoseCouplingMesh.Get() : CylinderMesh.Get());
	AddEvidence(
		EIGChapterThreeAction::EvidenceHosePaw,
		Tank + FVector(165, -175, 250),
		FVector(42, 32, bHasHosePlane ? 1.0f : 5.0f),
		EvidenceHoseMaterial,
		NSLOCTEXT("IGCH03", "EvidenceHosePaw", "호스의 눌린 앞발 자국 확인하기"),
		FRotator(0, 18, 0),
		bHasHosePlane ? PlaneMesh.Get() : nullptr);
	AddEvidence(
		EIGChapterThreeAction::EvidenceHoseImpact,
		Tank + FVector(-118, -72, 632),
		FVector(44, 28, bHasHosePlane ? 1.0f : 9.0f),
		EvidenceHoseMaterial,
		NSLOCTEXT(
			"IGCH03",
			"EvidenceHoseImpact",
			"젖은 안쪽 마찰 자국과 커플링 물자국 대조하기"),
		FRotator(0, -18, 0),
		bHasHosePlane ? PlaneMesh.Get() : CylinderMesh.Get());

	AddEvidence(
		EIGChapterThreeAction::EvidenceBag,
		PurchaseBagLocation,
		bHasCarrierBagMesh ? CarrierBagMeshScalePercent : BagSize,
		CarrierBagMaterial,
		NSLOCTEXT("IGCH03", "EvidenceBag", "사다리 아래 편의점 봉지 확인하기"),
		PurchaseBagRotation,
		bHasCarrierBagMesh ? CarrierBagCollapsedMesh.Get() : nullptr);

	// The final scene reproduces the exact purchase branch instead of using a
	// generic bag token. The first bottle is the one Ji-woon drank from: its
	// water is visibly lower and its cap is absent only on cap+leave.
	if (!bHasCarrierBagMesh)
	{
		const float BagWallThickness = 0.55f;
		for (const float Side : {-1.0f, 1.0f})
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
				CarrierBagMaterial,
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
				CarrierBagMaterial,
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
				CarrierBagMaterial,
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
			CarrierBagMaterial,
			false,
			PurchaseBagRotation);
	}

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
		FVector(2235, -300, 582.0f),
		FVector(8.0f, 52.0f, bHasSlipperPlane ? 1.0f : 3.0f),
		EvidenceSlipperMaterial,
		NSLOCTEXT(
			"IGCH03",
			"EvidenceWetRung",
			"상단 자국·들뜬 패드·녹슨 클립을 함께 확인하기"),
		FRotator(0, 12, 0),
		bHasSlipperPlane ? PlaneMesh.Get() : nullptr);
	// RungFailureCluster is one causal object, not a wet-rung token: the last
	// slipper mark ends here, the rubber pad has lifted inward, and both
	// retaining clips show the same corrosion visible in the 04:03 photo.
	for (int32 PrintIndex = 0; PrintIndex < (bHasSlipperPlane ? 0 : 3); ++PrintIndex)
	{
		CreateBlock(
			FVector(
				2195.0f + PrintIndex * 20.0f,
				-323.0f + PrintIndex * 8.0f,
				540.6f + PrintIndex * 20.0f),
			FVector(20, 30, 1.0f),
			EvidenceSlipperMaterial,
			false,
			FRotator(0, 12, 0),
			bHasSlipperPlane ? PlaneMesh.Get() : nullptr);
	}
	if (bHasRungFailureCluster)
	{
		CreateBlock(
			FVector(2235, -300, 578.5f),
			FVector(100.0f),
			WetRungPadMaterial,
			false,
			FRotator::ZeroRotator,
			LadderRungPadMesh);
		CreateBlock(
			FVector(2235, -300, 578.5f),
			FVector(100.0f),
			WaterTankMetalMaterial,
			false,
			FRotator::ZeroRotator,
			LadderRungClipsMesh);
	}
	else
	{
		CreateBlock(
			FVector(2235.0f, -300.0f, 580.7f),
			FVector(8.5f, 54.0f, 0.3f),
			WetRungPadMaterial,
			false,
			FRotator(6.0f, 0.0f, 0.0f));
	}
	// Corrosion is a small source detail on otherwise galvanized clips. Two
	// thin screw-head inserts avoid tinting the entire clip orange.
	for (const float ClipY : {-329.0f, -271.0f})
	{
		CreateBlock(
			FVector(2235.0f, ClipY, 581.55f),
			FVector(1.8f, 1.8f, 0.6f),
			EmergencyMaterial,
			false,
			FRotator::ZeroRotator,
			CylinderMesh);
	}
	AddEvidence(
		EIGChapterThreeAction::EvidenceHandSmear,
		Tank + FVector(-149, -28, 574),
		FVector(34, 24, bHasHandSmearPlane ? 1.0f : 3.0f),
		EvidenceHandSmearMaterial,
		NSLOCTEXT(
			"IGCH03",
			"EvidenceHandSmear",
			"안쪽으로 이어진 손바닥 쓸림 확인하기"),
		FRotator(90, 0, 0),
		bHasHandSmearPlane ? PlaneMesh.Get() : nullptr);

	const FVector ClothingEvidenceOffset = bUsesAuthoredTankBody
		? IGThirdMorning::GetAuthoredSleeveStitchFocusOffset()
		: FVector(-11.0f, -30.0f, 536.5f);
	AIGChapterThreeAction* ClothingEvidence = AddEvidence(
		EIGChapterThreeAction::EvidenceTankClothing,
		Tank + ClothingEvidenceOffset,
		FVector(34, 28, 18),
		BeddingMaterial,
		NSLOCTEXT("IGCH03", "EvidenceTankClothing", "물속 후드의 수선 자국 대조하기"),
		FRotator(0, 16, 0));
	if (ClothingEvidence && ClothingEvidence->GetPresentationMesh())
	{
		// This is a query-only focus volume around the actual sleeve stitches,
		// not another visible clothing proxy floating above the authored body.
		ClothingEvidence->GetPresentationMesh()->SetVisibility(false);
		ClothingEvidence->GetPresentationMesh()->SetCastShadow(false);
	}
	SetVisibleInteractive(ClothingEvidence, false);

	// InspectionRodVisual already exists on the deck. Ending B moves that same
	// component into the support groove; never spawn a second explanatory prop.
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
		CrackedPhoneMesh ? FVector(100.0f) : FVector(16, 8, 1.4f),
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
		PlasticMaterial,
		CrackedPhoneMesh.Get());
	// Two hairline screen cracks are enough to identify the object before the
	// reading panel opens, without making a bespoke phone UI asset.
	CreateBlock(
		FVector(121, 150, CrackedPhoneMesh ? 45.44f : 45.25f),
		FVector(7, 0.5f, 0.25f),
		ScreenMaterial,
		false,
		FRotator(0, -17, 0));
	CreateBlock(
		FVector(124, 150, CrackedPhoneMesh ? 45.47f : 45.28f),
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
	// The box is screwed to the south jamb of that door; at X 1604 it stood
	// 67 cm above the roof deck with nothing behind or beneath it.
	CreateBlock(
		FVector(1660, -478, 326),
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
	// Matching enclosure on the north jamb of the same door.
	CreateBlock(
		FVector(1660, -322, 326),
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
	// New CH03 attempts alone reach BeginAwakening. Keep an explicit guard so
	// save restoration and deterministic validation can never replay the cue.
	if (!bCaptureMode && !bGreyboxValidationMode && !bRestoredChapterThreeProgress)
	{
		GetWorldTimerManager().SetTimer(
			LensDropletTimer,
			this,
			&ThisClass::ShowOpeningLensDroplet,
			IGThirdMorning::OpeningLensDropletDelaySeconds,
			false);
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

void AIGThirdMorningDirector::ShowOpeningLensDroplet()
{
	if ((bCaptureMode && !bLensDropletCaptureMode)
		|| bGreyboxValidationMode
		|| bRestoredChapterThreeProgress)
	{
		return;
	}
	AIGHorrorHUD::PushLensDroplet(
		this,
		IGThirdMorning::OpeningLensDropletDurationSeconds);
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
				NSLOCTEXT(
					"IGCH03",
					"KeysThought",
					"자물쇠가 열려 있다. 관리 열쇠도 그대로 꽂혀 있다."),
				4.2f);
			IGStory::AddState(
				this,
				FGameplayTag::RequestGameplayTag(
					FName(TEXT("State.CH03.Flood.FoundKeys")), false));
		}
		break;

	case EIGChapterThreeAction::OpenRoofDoor:
		if (RoofDoorState == EIGRoofDoorState::PulledOpen)
		{
			BeginRoofDoorReturn();
			break;
		}
		if (RoofDoorState == EIGRoofDoorState::ReturnToGap)
		{
			break;
		}
		ApplyRoofDoorState(EIGRoofDoorState::PulledOpen);
		if (Phase < EIGThirdMorningPhase::Roof)
		{
			EnterRoofSilence();
		}
		if (GetWorld())
		{
			GetWorldTimerManager().SetTimer(
				RoofDoorHoldTimer,
				this,
				&ThisClass::BeginRoofDoorReturn,
				IGThirdMorning::RoofDoorHoldSeconds,
				false);
		}
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
				if (UStaticMeshComponent* Wheel = Source->GetPresentationMesh())
				{
					Wheel->SetRelativeRotation(FRotator(0, 45, 0));
				}
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
				if (UStaticMeshComponent* Wheel = Source->GetPresentationMesh())
				{
					Wheel->SetRelativeRotation(FRotator(0, 45, 0));
				}
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
			&& !bActedAfterSecondScratch
			&& GetWorld()
			&& AccidentScratchGateStartSeconds >= 0.0
			&& GetWorld()->GetTimeSeconds()
				- AccidentScratchGateStartSeconds >= 4.0)
		{
			bActedAfterSecondScratch = true;
			// Evidence re-selection can return before its normal commit path.
			// Persist this physical gate at the action boundary so a quit in
			// the following timer tick never asks for the action twice.
			CommitChapterThreeState();
			RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
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
		if (UStaticMeshComponent* Wheel =
			P3PressureReleaseAction->GetPresentationMesh())
		{
			Wheel->SetRelativeRotation(FRotator(0, 45, 0));
		}
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
			FRotator(FMath::Lerp(-55.0f, 55.0f, Alpha), 0, 0));
	}
	if (P3BleedWaterVisual)
	{
		const float FlowScale = FMath::Clamp(
			P3PressureKPa / 60.0f,
			0.08f,
			1.0f);
		P3BleedWaterVisual->SetRelativeScale3D(
			FVector(0.026f, 0.026f, 0.40f * FlowScale));
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

void AIGThirdMorningDirector::AdvanceP3TimedPressure()
{
	if (bP3Solved || P3MistakeCount >= 3)
	{
		return;
	}
	++P3MistakeCount;
	if (CorridorWaterVisual)
	{
		FVector WaterLocation = CorridorWaterVisual->GetRelativeLocation();
		WaterLocation.Z = 2.0f + P3MistakeCount * 5.0f;
		CorridorWaterVisual->SetRelativeLocation(WaterLocation);
	}

	if (P3MistakeCount == 1)
	{
		PlayMetalEcho(FVector(675, -190, 115), 0.76f);
	}
	else if (P3MistakeCount == 2)
	{
		PlayDelayedSplash();
		if (APlayerController* Controller =
				GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (AIGPlayerCharacter* Player =
					Cast<AIGPlayerCharacter>(Controller->GetPawn()))
			{
				if (UIGFlashlightComponent* Flashlight = Player->GetFlashlight())
				{
					Flashlight->AddImpulse(FRotator(-0.6f, 0.9f, 0.0f));
				}
			}
		}
	}
	else
	{
		PlayMetalEcho(FVector(620, -245, 126), 0.64f);
	}

	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
}

void AIGThirdMorningDirector::CompleteP3()
{
	if (bP3Solved)
	{
		return;
	}
	bP3Solved = true;
	bP3PressureRiseArmed = false;
	P3PressureRiseElapsedSeconds = 0.0f;
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
		if (UStaticMeshComponent* Wheel =
			P3FloorDrainAction->GetPresentationMesh())
		{
			Wheel->SetRelativeRotation(FRotator(0, 45, 0));
		}
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
		PlayMetalEcho(FVector(2335, -184, 662), 1.0f);
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
	if (TryAutoConnectEvidence(EvidenceAction))
	{
		FocusedEvidence = EIGChapterThreeAction::None;
		CommitChapterThreeState();
		RequestCheckpointAutosave(EvidenceCheckpoint);
		RefreshEvidencePrompts();
		return;
	}

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

bool AIGThirdMorningDirector::IsEvidenceActionObserved(
	const EIGChapterThreeAction EvidenceAction) const
{
	const TArray<FName> Sources =
		IGThirdMorning::EvidenceSources(EvidenceAction);
	if (Sources.IsEmpty())
	{
		return false;
	}
	for (const FName SourceId : Sources)
	{
		if (!ObservedP5Sources.Contains(SourceId))
		{
			return false;
		}
	}
	return true;
}

bool AIGThirdMorningDirector::TryAutoConnectEvidence(
	const EIGChapterThreeAction EvidenceAction)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	if (!Accessibility
		|| !Accessibility->UsesAutomaticEvidenceConnections())
	{
		return false;
	}

	// Automatic connection never invents evidence. It only removes the second
	// selection/hold after both physical nodes (or all three identity nodes)
	// have actually been inspected by the player.
	const EIGChapterThreeAction CandidateActions[] =
	{
		EIGChapterThreeAction::EvidenceCatEntered,
		EIGChapterThreeAction::EvidenceCatExited,
		EIGChapterThreeAction::EvidenceHosePaw,
		EIGChapterThreeAction::EvidenceHoseImpact,
		EIGChapterThreeAction::EvidenceBag,
		EIGChapterThreeAction::EvidenceWetRung,
		EIGChapterThreeAction::EvidenceHandSmear,
		EIGChapterThreeAction::EvidenceCurrentSleeve,
		EIGChapterThreeAction::EvidenceSearchPoster,
		EIGChapterThreeAction::EvidenceGlasses,
		EIGChapterThreeAction::EvidenceTankClothing
	};
	for (const EIGChapterThreeAction Candidate : CandidateActions)
	{
		if (Candidate != EvidenceAction
			&& IsEvidenceActionObserved(Candidate)
			&& IsEvidencePairValid(Candidate, EvidenceAction)
			&& ResolveEvidencePair(Candidate, EvidenceAction))
		{
			return true;
		}
	}
	return false;
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
		if (TankPressureComponent)
		{
			TankPressureComponent->FadeOut(
				IGThirdMorning::NarrativeCrossfadeSeconds,
				0.0f);
			TankPressureComponent = nullptr;
		}
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
			IGThirdMorning::TankRevealSilenceSeconds,
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
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT(
			"IGCH03",
			"TankScratchCaption",
			"[물탱크 안쪽을 긁는 소리]"),
		2.2f);
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
	StopRoofBed(IGThirdMorning::NarrativeCrossfadeSeconds);
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
	if (!bP4Completed
		&& !GetWorldTimerManager().IsTimerActive(P4Timer))
	{
		GetWorldTimerManager().SetTimer(
			P4Timer,
			this,
			&ThisClass::PollP4PressureAndHint,
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
	const bool bNearCabinet = Pawn
		&& FVector::DistSquared(
			Pawn->GetActorLocation(),
			ToWorld(FVector(675, -190, 132)))
			<= FMath::Square(420.0f);
	if (bNearCabinet && !bP3PressureRiseArmed)
	{
		bP3PressureRiseArmed = true;
		CommitChapterThreeState();
	}
	if (AIGReadableNote::GetOpenNote())
	{
		// Reading a clue owns the player's attention. Both pressure and hint
		// clocks resume from the exact persisted values after the note closes.
		return;
	}
	const UIGAccessibilitySubsystem* Accessibility = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			Accessibility =
				GameInstance->GetSubsystem<UIGAccessibilitySubsystem>();
		}
	}
	if (bP3PressureRiseArmed)
	{
		P3PressureRiseElapsedSeconds += 1.0f;
		const float PressureRiseInterval = Accessibility
			? Accessibility->GetPressureRiseIntervalSeconds()
			: 55.0f;
		if (P3PressureRiseElapsedSeconds >= PressureRiseInterval)
		{
			P3PressureRiseElapsedSeconds = FMath::Max(
				0.0f,
				P3PressureRiseElapsedSeconds - PressureRiseInterval);
			AdvanceP3TimedPressure();
		}
		CommitChapterThreeState();
	}
	if (!bNearCabinet)
	{
		// Automatic hints count only actual cabinet dwell. Pressure remains
		// armed while the player retreats into the corridor or stairwell.
		return;
	}
	if (Accessibility && !Accessibility->ShouldAutoShowHints())
	{
		// Silent mode preserves the exact persisted dwell time. A later manual
		// request can reveal the next rung without a hidden clock advancing.
		return;
	}

	P3HintElapsedSeconds += 1.0f;
	// Keep the in-memory narrative snapshot exact between authored thresholds.
	CommitChapterThreeState();
	const FVector HintThresholds = Accessibility
		? Accessibility->GetP3HintThresholds()
		: FVector(90.0f, 150.0f, 210.0f);
	const float Thresholds[] =
	{
		HintThresholds.X,
		HintThresholds.Y,
		HintThresholds.Z
	};
	if (P3HintStage >= UE_ARRAY_COUNT(Thresholds)
		|| P3HintElapsedSeconds < Thresholds[P3HintStage])
	{
		return;
	}

	PresentNextP3Hint();
}

void AIGThirdMorningDirector::PollP4PressureAndHint()
{
	if (bP4Completed || Phase >= EIGThirdMorningPhase::FifthFloor)
	{
		StopP4Clock();
		return;
	}
	APlayerController* Controller =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const bool bNearLanding = Pawn
		&& FVector::DistSquared(
			Pawn->GetActorLocation(),
			ToWorld(FVector(1085.0f, -90.0f, 145.0f)))
			<= FMath::Square(360.0f);
	if (bNearLanding && !bP4PressureArmed)
	{
		bP4PressureArmed = true;
		CommitChapterThreeState();
	}
	if (AIGReadableNote::GetOpenNote())
	{
		return;
	}

	const UIGAccessibilitySubsystem* Accessibility = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			Accessibility =
				GameInstance->GetSubsystem<UIGAccessibilitySubsystem>();
		}
	}
	if (bP4PressureArmed && P4PressureStage < 3)
	{
		P4PressureRiseElapsedSeconds += 1.0f;
		const float PressureRiseInterval = Accessibility
			? Accessibility->GetPressureRiseIntervalSeconds()
			: 55.0f;
		if (P4PressureRiseElapsedSeconds >= PressureRiseInterval)
		{
			P4PressureRiseElapsedSeconds = FMath::Max(
				0.0f,
				P4PressureRiseElapsedSeconds - PressureRiseInterval);
			AdvanceP4Pressure();
		}
		CommitChapterThreeState();
	}
	if (!bNearLanding
		|| (Accessibility && !Accessibility->ShouldAutoShowHints()))
	{
		return;
	}

	P4HintElapsedSeconds += 1.0f;
	CommitChapterThreeState();
	const FVector HintThresholds = Accessibility
		? Accessibility->GetP4HintThresholds()
		: FVector(45.0f, 100.0f, 150.0f);
	const float Thresholds[] =
	{
		HintThresholds.X,
		HintThresholds.Y,
		HintThresholds.Z
	};
	if (P4HintStage >= UE_ARRAY_COUNT(Thresholds)
		|| P4HintElapsedSeconds < Thresholds[P4HintStage])
	{
		return;
	}
	PresentNextP4Hint();
}

void AIGThirdMorningDirector::AdvanceP4Pressure()
{
	if (bP4Completed || P4PressureStage >= 3)
	{
		return;
	}
	++P4PressureStage;
	ApplyP4PresentationState();
	if (P4PressureStage == 2)
	{
		PlayDelayedSplash();
		if (APlayerController* Controller =
				GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (AIGPlayerCharacter* Player =
					Cast<AIGPlayerCharacter>(Controller->GetPawn()))
			{
				if (UIGFlashlightComponent* Flashlight = Player->GetFlashlight())
				{
					Flashlight->AddImpulse(FRotator(-0.45f, 0.72f, 0.0f));
				}
			}
		}
	}
	else if (P4PressureStage == 3)
	{
		PlayMetalEcho(FVector(1110, -250, 240), 0.71f);
		AIGHorrorHUD::PushAudioCaption(
			this,
			NSLOCTEXT(
				"IGCH03",
				"P4UpperMetalCaption",
				"[계단 위쪽에서 금속이 울린다]"),
			2.4f);
	}
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
}

void AIGThirdMorningDirector::PresentNextP4Hint()
{
	if (bP4Completed || P4HintStage >= 3)
	{
		return;
	}
	const int32 HintToPresent = P4HintStage;
	if (HintToPresent == 0)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH03",
				"P4HintFlowRelation",
				"물은… 위에서 내려오고 있어."),
			4.2f);
	}
	else if (HintToPresent == 1)
	{
		BeginP4ReceiptHint();
		PlayDelayedSplash();
	}
	else
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH03",
				"P4HintRouteAnswer",
				"물때 아래 화살표가 위를 가리킨다. 흐름이 시작된 쪽이야."),
			5.0f);
	}
	P4HintStage = FMath::Min(P4HintStage + 1, 3);
	UpdateStairSign();
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
}

void AIGThirdMorningDirector::ApplyP4PresentationState()
{
	if (P4LandingLight)
	{
		const float Intensities[] = {1180.0f, 900.0f, 690.0f, 520.0f};
		P4LandingLight->SetIntensity(
			Intensities[FMath::Clamp(P4PressureStage, 0, 3)]);
	}
	if (P4ReceiptFragment
		&& !GetWorldTimerManager().IsTimerActive(P4ReceiptHintTimer))
	{
		P4ReceiptFragment->SetVisibility(P4HintStage >= 2);
		if (P4HintStage >= 2)
		{
			P4ReceiptFragment->SetRelativeLocation(FVector(1060, -25, 9));
			P4ReceiptFragment->SetRelativeRotation(FRotator(0, 0, -18));
		}
	}
	UpdateStairSign();
}

void AIGThirdMorningDirector::StopP4Clock()
{
	GetWorldTimerManager().ClearTimer(P4Timer);
}

void AIGThirdMorningDirector::BeginP4ReceiptHint()
{
	if (!P4ReceiptFragment || !GetWorld())
	{
		return;
	}
	P4ReceiptHintAlpha = 0.0f;
	P4ReceiptFragment->SetRelativeLocation(FVector(1090, -250, 190));
	P4ReceiptFragment->SetRelativeRotation(FRotator(0, 0, 12));
	P4ReceiptFragment->SetVisibility(true);
	GetWorldTimerManager().SetTimer(
		P4ReceiptHintTimer,
		this,
		&ThisClass::UpdateP4ReceiptHint,
		0.05f,
		true);
}

void AIGThirdMorningDirector::UpdateP4ReceiptHint()
{
	if (!P4ReceiptFragment)
	{
		GetWorldTimerManager().ClearTimer(P4ReceiptHintTimer);
		return;
	}
	P4ReceiptHintAlpha = FMath::Min(1.0f, P4ReceiptHintAlpha + 0.05f / 1.35f);
	const FVector Start(1090, -250, 190);
	const FVector End(1060, -25, 9);
	FVector Location = FMath::Lerp(Start, End, P4ReceiptHintAlpha);
	Location.Z += FMath::Sin(P4ReceiptHintAlpha * PI * 3.0f) * 5.0f;
	P4ReceiptFragment->SetRelativeLocation(Location);
	P4ReceiptFragment->SetRelativeRotation(FRotator(
		0,
		0,
		FMath::Lerp(12.0f, -18.0f, P4ReceiptHintAlpha)));
	if (P4ReceiptHintAlpha >= 1.0f)
	{
		GetWorldTimerManager().ClearTimer(P4ReceiptHintTimer);
	}
}

bool AIGThirdMorningDirector::RequestManualHint()
{
	if (Phase < EIGThirdMorningPhase::FloodedCorridor
		|| Phase >= EIGThirdMorningPhase::Roof
		|| AIGReadableNote::GetOpenNote())
	{
		return false;
	}
	const APlayerController* Controller =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return false;
	}
	const bool bNearP4 = FVector::DistSquared(
		Pawn->GetActorLocation(),
		ToWorld(FVector(1085.0f, -90.0f, 145.0f)))
		<= FMath::Square(360.0f);
	if (!bP4Completed && bNearP4)
	{
		PresentNextP4Hint();
		return true;
	}
	const bool bNearP3 = FVector::DistSquared(
		Pawn->GetActorLocation(),
		ToWorld(FVector(675, -190, 132)))
		<= FMath::Square(420.0f);
	if (!bP3Solved && bNearP3)
	{
		PresentNextP3Hint();
		return true;
	}
	return false;
}

void AIGThirdMorningDirector::PresentNextP3Hint()
{
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
			bP3PressureReleaseOpen
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
	P3HintStage = FMath::Min(P3HintStage + 1, 3);
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
	bP4PressureArmed = true;
	UIGRebirthEvidenceSubsystem::RecordPuzzleFourObservation(
		this,
		StairLoopCount,
		StairLoopCount >= 3);
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

	ApplyP4PresentationState();
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
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
}

void AIGThirdMorningDirector::UpdateStairSign()
{
	if (StairSignText)
	{
		StairSignText->SetVisibility(true);
		StairSignText->SetText(FText::FromString(TEXT("4F")));
		const int32 PressureDim = P4PressureStage * 18;
		const FColor BaseColor = StairLoopCount >= 2
			? FColor(152, 164, 154)
			: FColor(188, 194, 184);
		StairSignText->SetTextRenderColor(FColor(
			FMath::Max(72, static_cast<int32>(BaseColor.R) - PressureDim),
			FMath::Max(76, static_cast<int32>(BaseColor.G) - PressureDim),
			FMath::Max(72, static_cast<int32>(BaseColor.B) - PressureDim)));
	}
	if (StairRoofText)
	{
		StairRoofText->SetVisibility(StairLoopCount >= 3 || P4HintStage >= 3);
	}
	for (UStaticMeshComponent* Drip : StairLatinSignParts)
	{
		if (Drip)
		{
			Drip->SetVisibility(StairLoopCount == 2 || P4HintStage >= 2);
		}
	}
	for (UStaticMeshComponent* ArrowStroke : StairSignParts)
	{
		if (ArrowStroke)
		{
			ArrowStroke->SetVisibility(StairLoopCount >= 3 || P4HintStage >= 3);
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
	if (bP4Completed)
	{
		return;
	}
	UIGRebirthEvidenceSubsystem::RecordPuzzleFourObservation(
		this,
		StairLoopCount,
		true);
	bP4Completed = true;
	bP4PressureArmed = false;
	P4PressureRiseElapsedSeconds = 0.0f;
	P4PressureStage = FMath::Max(0, P4PressureStage - 1);
	StopP4Clock();
	ApplyP4PresentationState();
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Flood"));
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
	StopP4Clock();
	SetDocumentAvailable(P3PhotoNote, false);
	SetDocumentAvailable(ManagementDbNote, false);
	SetDocumentAvailable(PreservationNoticeNote, false);
	SetDocumentAvailable(PoliceChecklistNote, false);
	SetFloodMovement(false);
	if (WaterBedComponent)
	{
		WaterBedComponent->FadeOut(
			IGThirdMorning::NarrativeCrossfadeSeconds,
			0.0f);
		WaterBedComponent = nullptr;
	}
	StartRoofBed();
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
		&& !bActedAfterSecondScratch
		&& GetWorld()
		&& AccidentScratchGateStartSeconds >= 0.0
		&& GetWorld()->GetTimeSeconds()
			- AccidentScratchGateStartSeconds >= 4.0)
	{
		bActedAfterSecondScratch = true;
		CommitChapterThreeState();
		RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
		ScheduleAccidentScratch();
	}
	// My rung, then one wetter answer from below.
	PlayMetalEcho(FVector(2075, -300, 420), 1.0f);
	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);
	FTimerDelegate EchoDelegate;
	EchoDelegate.BindLambda([WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->PlayMetalEcho(FVector(1975, -300, 320), 0.92f);
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
		// The one-person service lid remains visible on the far hinge. Unlike the
		// former three-metre disc, it clears the opening without cutting through
		// the body silhouette and gives the support rod a readable physical job.
		TankLidAction->SetActorHiddenInGame(false);
		TankLidAction->SetActorLocation(ToWorld(
			IGThirdMorning::TankCenter
			+ IGThirdMorning::TankHatchOffset
			+ IGThirdMorning::TankLidOpenOffset
			+ FVector(0, 0, 610)));
		TankLidAction->SetActorRotation(FRotator(-78, 0, 0));
		TankLidAction->SetInteractionEnabled(false);
		TankLidAction->SetActorEnableCollision(false);
	}
	ApplyTankRevealVisibility();
	StartTankRevealSilence();
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
	StartTankRevealSilence();
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
	if (TankRevealKeyLight)
	{
		TankRevealKeyLight->SetVisibility(bCanIdentifyBody);
		TankRevealKeyLight->SetIntensity(bCanIdentifyBody ? 1600.0f : 0.0f);
	}
	if (TankRevealRimLight)
	{
		TankRevealRimLight->SetVisibility(bCanIdentifyBody);
		TankRevealRimLight->SetIntensity(bCanIdentifyBody ? 360.0f : 0.0f);
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
	UIGRebirthEvidenceSubsystem::RecordEndingEvent(
		this,
		FName(TEXT("Choice")));
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
	SetPhase(EIGThirdMorningPhase::Ending);

	// The player's own hand closes the lid; the same instant, every remembered
	// overlay resolves to the actual post-accident state and stays visible for
	// 0.8 seconds before the dark takes it.
	PlayTankSlam();
	ApplyCommonDiscoveryWorldState();
	UIGRebirthEvidenceSubsystem::RecordEndingEvent(
		this,
		FName(TEXT("ActualStateRestored")));
	PlayRestoreStateSounds();

	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);
	ScheduleEndingCue(0.80f, [WeakThis]()
	{
		AIGThirdMorningDirector* Director = WeakThis.Get();
		if (!Director || !Director->GetWorld())
		{
			return;
		}
		if (APlayerController* PlayerController =
			Director->GetWorld()->GetFirstPlayerController())
		{
			if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
			{
				Camera->StartCameraFade(
					0.0f, 1.0f, 0.7f, FLinearColor::Black, false, true);
			}
		}
	});
	ScheduleEndingCue(1.60f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->StopAllChapterAudio();
			Director->PlayCommonSafetyOpening();
		}
	});
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
	UIGRebirthEvidenceSubsystem::RecordEndingEvent(
		this,
		FName(TEXT("Choice")));
	CommitChapterThreeState();
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
	SetPhase(EIGThirdMorningPhase::Ending);
	SetVisibleInteractive(CloseChoiceAction, false);
	SetVisibleInteractive(SupportChoiceAction, false);

	// The rod seats in its groove; the railing answers with one tiny ring
	// from the glasses. Then the player may watch the open water for two
	// full seconds before the roof's colour and sound leave like memory.
	SetInspectionRodWedged(true);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateRodWedgeSeat(this),
		ToWorld(IGThirdMorning::TankCenter + FVector(-160, 105, 618)),
		0.50f,
		1.0f,
		90.0f,
		760.0f);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT(
			"IGCH03",
			"RodWedgeCaption",
			"[철제 지지봉이 홈에 걸린다]"),
		2.1f);
	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);
	ScheduleEndingCue(0.45f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			IGAudio::SpawnOneShotAt(
				Director,
				UIGToneSequenceSoundWave::CreateGlassesTinyRing(Director),
				Director->ToWorld(FVector(2335, -184, 662)),
				0.34f,
				1.0f,
				60.0f,
				620.0f);
		}
	});

	// 2.0 s undisturbed look at the lid and the water.
	ScheduleEndingCue(2.00f, [WeakThis]()
	{
		AIGThirdMorningDirector* Director = WeakThis.Get();
		if (!Director || !Director->GetWorld())
		{
			return;
		}
		if (APlayerController* PlayerController =
			Director->GetWorld()->GetFirstPlayerController())
		{
			if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
			{
				// Colour drains first; this is not yet black.
				Camera->StartCameraFade(
					0.0f,
					0.55f,
					2.2f,
					FLinearColor(0.015f, 0.12f, 0.14f),
					false,
					true);
			}
		}
	});
	ScheduleEndingCue(3.60f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			// The roof's sound leaves before its light does.
			Director->StopAllChapterAudio();
		}
	});
	// The remembered scene resolves to the actual state while still visible.
	ScheduleEndingCue(4.20f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->ApplyCommonDiscoveryWorldState();
			UIGRebirthEvidenceSubsystem::RecordEndingEvent(
				Director,
				FName(TEXT("ActualStateRestored")));
			Director->PlayRestoreStateSounds();
		}
	});
	ScheduleEndingCue(5.00f, [WeakThis]()
	{
		AIGThirdMorningDirector* Director = WeakThis.Get();
		if (!Director || !Director->GetWorld())
		{
			return;
		}
		if (APlayerController* PlayerController =
			Director->GetWorld()->GetFirstPlayerController())
		{
			if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
			{
				Camera->StartCameraFade(
					0.55f, 1.0f, 0.6f, FLinearColor::Black, false, true);
			}
		}
	});
	ScheduleEndingCue(5.80f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->PlayCommonSafetyOpening();
		}
	});
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
	if (bFirstCommit)
	{
		UIGRebirthEvidenceSubsystem::RecordEndingEvent(
			this,
			FName(TEXT("Found0731")));
	}
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

	// The card holds for two seconds, then the dark returns (§9).
	AIGHorrorHUD::ShowChapterCard(
		this,
		NSLOCTEXT("IGCH03", "DiscoveryDate", "2024년 7월 31일 04:00"),
		NSLOCTEXT("IGCH03", "DiscoveryTitle", "옥상 예비 저수조 합동 확인 중 실종자 한지운 발견"),
		FText::GetEmpty(),
		2.0f);
	UIGRebirthEvidenceSubsystem::RecordEndingEvent(
		this,
		FName(TEXT("CommonDiscoveryCard")));

	if (bEndingASelected)
	{
		GetWorldTimerManager().SetTimer(
			FlowTimer,
			this,
			&ThisClass::FinishEndingAAfterDiscovery,
			2.1f,
			false);
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			FlowTimer,
			this,
			&ThisClass::FinishEndingB,
			2.1f,
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
	// Persist the guard before emitting any part of the cue. Otherwise a
	// quit after the final controls would replay the blackout cues on load.
	RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
	UIGRebirthEvidenceSubsystem::RecordEndingEvent(
		this,
		FName(TEXT("BranchCoda")));
	PlayEndingABlackoutCues();
}

void AIGThirdMorningDirector::PlayEndingABlackoutCues()
{
	// The chosen memory restarts in the dark, at its own fixed clock:
	// 0.75 s store chime, 1.30 s the 04:44 alarm, 1.90 s the cat exactly as
	// the player left it — cut mid-motion — and 2.55 s one phone vibration
	// far away in an empty room, dying before its third bar.
	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);

	ScheduleEndingCue(0.75f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			IGAudio::SpawnOneShotAt(
				Director,
				UIGToneSequenceSoundWave::CreateDoorChime(Director),
				Director->ToWorld(IGThirdMorning::TankCenter),
				0.17f,
				0.90f,
				80.0f,
				900.0f);
			AIGHorrorHUD::PushAudioCaption(
				Director,
				NSLOCTEXT("IGCH03", "EndingAChimeCaption", "[편의점 문 차임]"),
				1.2f);
		}
	});
	ScheduleEndingCue(1.30f, [WeakThis]()
	{
		AIGThirdMorningDirector* Director = WeakThis.Get();
		if (!Director)
		{
			return;
		}
		UIGAlarmSoundWave* LoopAlarm =
			NewObject<UIGAlarmSoundWave>(Director, TEXT("CH03EndingLoopAlarm"));
		UAudioComponent* AlarmBurst = IGAudio::SpawnOneShotAt(
			Director,
			LoopAlarm,
			Director->ToWorld(FVector(-205, -190, 82)),
			0.30f,
			0.983f,
			80.0f,
			750.0f);
		if (AlarmBurst)
		{
			// One pattern's worth of alarm, then it is taken away again.
			Director->ScheduleEndingCue(0.62f, [WeakAlarm =
				TWeakObjectPtr<UAudioComponent>(AlarmBurst)]()
			{
				if (UAudioComponent* Component = WeakAlarm.Get())
				{
					Component->Stop();
				}
			});
		}
		AIGHorrorHUD::PushAudioCaption(
			Director,
			NSLOCTEXT("IGCH03", "EndingAAlarmCaption", "[04:44 알람]"),
			1.1f);
	});
	ScheduleEndingCue(1.90f, [WeakThis]()
	{
		AIGThirdMorningDirector* Director = WeakThis.Get();
		if (!Director)
		{
			return;
		}
		const UIGRebirthNarrativeSubsystem* RebirthState =
			Director->GetRebirthState();
		const EIGRebirthCatWaterState CatState = RebirthState
			? RebirthState->BuildSnapshot().Choices.CatWaterState
			: EIGRebirthCatWaterState::Unset;
		UIGToneSequenceSoundWave* CatCue = nullptr;
		switch (CatState)
		{
		case EIGRebirthCatWaterState::BottleCap:
			CatCue = UIGToneSequenceSoundWave::CreateCatLickWaterPlastic(
				Director, true);
			break;
		case EIGRebirthCatWaterState::PaperCup:
			CatCue = UIGToneSequenceSoundWave::CreateCatLickWaterPaper(Director);
			break;
		default:
			CatCue = UIGToneSequenceSoundWave::CreateCatPawTrot(
				Director, 2, true);
			break;
		}
		IGAudio::SpawnOneShotAt(
			Director,
			CatCue,
			Director->ToWorld(FVector(660, -240, 20)),
			0.30f,
			1.0f,
			90.0f,
			700.0f);
		AIGHorrorHUD::PushAudioCaption(
			Director,
			CatState == EIGRebirthCatWaterState::BottleCap
				|| CatState == EIGRebirthCatWaterState::PaperCup
				? NSLOCTEXT("IGCH03", "EndingACatDrinksCaption", "[고양이가 물을 핥는다]")
				: NSLOCTEXT("IGCH03", "EndingACatLeavesCaption", "[고양이 발소리가 멀어진다]"),
			1.2f);
	});
	ScheduleEndingCue(2.55f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			IGAudio::SpawnOneShotAt(
				Director,
				UIGToneSequenceSoundWave::CreatePhoneVibrationUnfinished(Director),
				Director->ToWorld(FVector(-200, -190, 82)),
				0.26f,
				1.0f,
				70.0f,
				640.0f);
			AIGHorrorHUD::PushAudioCaption(
				Director,
				NSLOCTEXT("IGCH03", "EndingAPhoneCaption", "[빈방에서 휴대폰이 진동한다]"),
				2.0f);
		}
	});
	ScheduleEndingCue(5.60f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->ShowEndingAFinalCard();
		}
	});
}

void AIGThirdMorningDirector::ShowEndingAFinalCard()
{
	PresentEndingControls(
		NSLOCTEXT("IGCH03", "EndingATitle", "내일 또"),
		NSLOCTEXT(
			"IGCH03",
			"EndingASubtitle",
			"2024년 7월 26일 금요일, 오전 4시 44분."));
}

void AIGThirdMorningDirector::FinishEndingB()
{
	// Resume rule: a load after the montage guard replays only the final
	// card; a load between the common card and the guard re-enters the coda.
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (RebirthState
		&& RebirthState->WasOneShotBeatPlayed(
			FName(TEXT("Ending.B.StrongCuePlayed"))))
	{
		ShowEndingBFinalCard();
		return;
	}
	ShowEndingBFamilyCard();
}

void AIGThirdMorningDirector::ShowEndingBFamilyCard()
{
	AIGHorrorHUD::ShowChapterCard(
		this,
		FText::GetEmpty(),
		NSLOCTEXT("IGCH03", "EndingBFamilyLine", "가족에게 돌아갔다."),
		FText::GetEmpty(),
		3.0f);
	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);
	ScheduleEndingCue(3.40f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->StartEndingBMontage();
		}
	});
}

void AIGThirdMorningDirector::StartEndingBMontage()
{
	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (RebirthState
		&& RebirthState->MarkOneShotBeatPlayed(
			FName(TEXT("Ending.B.StrongCuePlayed"))))
	{
		// Persist the guard before the first montage sample, so a quit during
		// the goodbye never plays the tape twice.
		CommitChapterThreeState();
		RequestCheckpointAutosave(TEXT("Checkpoint.CH03.Roof"));
		UIGRebirthEvidenceSubsystem::RecordEndingEvent(
			this,
			FName(TEXT("BranchCoda")));
	}

	// The screen stays black to the end; only the sounds pack the room.
	UGameplayStatics::PlaySound2D(
		this,
		UIGToneSequenceSoundWave::CreateEndingBMontage(this),
		0.9f);

	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);
	const struct
	{
		float Delay;
		const TCHAR* Korean;
		float Duration;
	} MontageCaptions[] = {
		{0.01f, TEXT("[현관 잠금음과 빗장]"), 1.4f},
		{1.50f, TEXT("[서랍을 여는 소리]"), 1.2f},
		{2.80f, TEXT("[책을 상자에 넣는다]"), 1.8f},
		{5.20f, TEXT("[박스 테이프를 뜯는다]"), 1.4f},
		{6.90f, TEXT("[작업 조끼 지퍼]"), 1.0f},
		{7.90f, TEXT("[금 간 휴대폰이 진동한다]"), 1.2f},
		{9.10f, TEXT("[옥상문을 연다]"), 1.4f},
		{10.60f, TEXT("[콘크리트 위에 물그릇을 놓는다]"), 0.8f},
		{11.50f, TEXT("[물그릇에 물을 붓는다]"), 1.9f},
		{13.90f, TEXT("[천을 접고 한 번 숨을 쉰다]"), 2.0f}
	};
	for (const auto& Caption : MontageCaptions)
	{
		const FText CaptionText = FText::FromString(Caption.Korean);
		const float Duration = Caption.Duration;
		ScheduleEndingCue(Caption.Delay, [WeakThis, CaptionText, Duration]()
		{
			if (AIGThirdMorningDirector* Director = WeakThis.Get())
			{
				AIGHorrorHUD::PushAudioCaption(Director, CaptionText, Duration);
			}
		});
	}
	ScheduleEndingCue(16.40f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->StartEndingBEpilogue();
		}
	});
}

void AIGThirdMorningDirector::StartEndingBEpilogue()
{
	// Next spring, the same kitchen, a tenant we never see. The room is lit
	// like an ordinary morning for the first time in the whole chapter.
	if (EpilogueClockText)
	{
		EpilogueClockText->SetText(FText::FromString(TEXT("4:43")));
		EpilogueClockText->SetVisibility(true);
	}
	if (EpilogueSpringLight)
	{
		EpilogueSpringLight->SetVisibility(true);
		EpilogueSpringLight->SetIntensity(2400.0f);
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World
		? World->GetFirstPlayerController()
		: nullptr;
	if (PlayerController)
	{
		// One static shot of the sink and the empty cup; the view is a still
		// image in everything but name, per the approved static proxy.
		FActorSpawnParameters CameraParams;
		CameraParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector CameraLocation = ToWorld(FVector(58, 62, 110));
		const FRotator CameraRotation =
			FRotationMatrix::MakeFromX(
				ToWorld(FVector(-42, 186, 92)) - CameraLocation).Rotator();
		EpilogueCamera = World->SpawnActor<AActor>(
			ACameraActor::StaticClass(),
			CameraLocation,
			CameraRotation,
			CameraParams);
		if (EpilogueCamera)
		{
			PlayerController->SetViewTargetWithBlend(EpilogueCamera, 0.0f);
		}
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StartCameraFade(
				1.0f, 0.0f, 1.4f, FLinearColor::Black, false, true);
		}
	}

	EndingBMusicComponent = UGameplayStatics::SpawnSound2D(
		this,
		UIGToneSequenceSoundWave::CreateEndingBReturnHomeBed(this),
		0.001f);
	if (EndingBMusicComponent)
	{
		EndingBMusicComponent->FadeIn(
			IGThirdMorning::NarrativeCrossfadeSeconds,
			0.8f,
			0.0f);
	}

	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);
	ScheduleEndingCue(IGThirdMorning::EndingBDripDelaySeconds, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->PlayEpilogueDripAndClock();
		}
	});
	ScheduleEndingCue(IGThirdMorning::EndingBMusicDurationSeconds, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->StartEndingBLifeBed();
		}
	});
	ScheduleEndingCue(IGThirdMorning::EndingBFinalCardDelaySeconds, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->ShowEndingBFinalCard();
		}
	});
}

void AIGThirdMorningDirector::StartEndingBLifeBed()
{
	if (EndingBMusicComponent)
	{
		// M5 has completed its authored 45 seconds; discard only its protected
		// silent tail before the ordinary spring room takes over.
		EndingBMusicComponent->Stop();
		EndingBMusicComponent = nullptr;
	}
	if (EndingBLifeBedComponent)
	{
		return;
	}
	EndingBLifeBedComponent = UGameplayStatics::SpawnSound2D(
		this,
		UIGToneSequenceSoundWave::CreateSpringMorningBed(this),
		0.001f);
	if (EndingBLifeBedComponent)
	{
		EndingBLifeBedComponent->FadeIn(
			IGThirdMorning::NarrativeCrossfadeSeconds,
			0.8f,
			0.0f);
	}
}

void AIGThirdMorningDirector::PlayEpilogueDripAndClock()
{
	// The clock turns 4:44 and the shut tap lets go of exactly one drop.
	if (EpilogueClockText)
	{
		EpilogueClockText->SetText(FText::FromString(TEXT("4:44")));
	}
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateGlassCupDrip(this),
		ToWorld(FVector(-30, 188, 96)),
		0.7f,
		1.0f,
		120.0f,
		700.0f);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT(
			"IGCH03",
			"EpilogueDripCaption",
			"[잠긴 수도에서 물 한 방울]"),
		2.1f);
}

void AIGThirdMorningDirector::ShowEndingBFinalCard()
{
	if (!EndingBMusicComponent && !EndingBLifeBedComponent)
	{
		// A save restored after the one-shot montage guard resumes at the card.
		// It never replays M5, but it should not return to a dead audio world.
		StartEndingBLifeBed();
	}
	PresentEndingControls(
		NSLOCTEXT("IGCH03", "EndingBTitle", "돌려보내다"),
		NSLOCTEXT(
			"IGCH03",
			"EndingBSubtitle",
			"찾는 사람이 있었다."));
	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);
	ScheduleEndingCue(4.60f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			Director->PlayEndingBCatCoda();
		}
	});
}

void AIGThirdMorningDirector::PlayEndingBCatCoda()
{
	// After the card: the cat, alive either way. Water-givers hear the same
	// two far-off laps; everyone else hears paws approach the bowl and one
	// short mewl. The survival reads identically.
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	const EIGRebirthCatWaterState CatState = RebirthState
		? RebirthState->BuildSnapshot().Choices.CatWaterState
		: EIGRebirthCatWaterState::Unset;
	const bool bGaveWater =
		CatState == EIGRebirthCatWaterState::BottleCap
		|| CatState == EIGRebirthCatWaterState::PaperCup;
	if (bGaveWater)
	{
		UGameplayStatics::PlaySound2D(
			this,
			UIGToneSequenceSoundWave::CreateCatLickWaterPlastic(this, false),
			0.22f);
		AIGHorrorHUD::PushAudioCaption(
			this,
			NSLOCTEXT("IGCH03", "EndingBCatDrinkCaption", "[멀리서 고양이가 물을 핥는다]"),
			2.2f);
		return;
	}
	UGameplayStatics::PlaySound2D(
		this,
		UIGToneSequenceSoundWave::CreateCatPawTrot(this, 4, false),
		0.26f);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT("IGCH03", "EndingBCatApproachCaption", "[고양이 발소리가 물그릇으로 다가온다]"),
		1.6f);
	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);
	ScheduleEndingCue(0.95f, [WeakThis]()
	{
		if (AIGThirdMorningDirector* Director = WeakThis.Get())
		{
			UGameplayStatics::PlaySound2D(
				Director,
				UIGToneSequenceSoundWave::CreateCatShortMewl(Director),
				0.20f);
			AIGHorrorHUD::PushAudioCaption(
				Director,
				NSLOCTEXT("IGCH03", "EndingBCatMewlCaption", "[고양이가 짧게 운다]"),
				1.2f);
		}
	});
}

void AIGThirdMorningDirector::PresentEndingControls(
	const FText& EndingTitle,
	const FText& EndingSubtitle)
{
	const AIGPlayerController* PlayerController = Cast<AIGPlayerController>(
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
	const bool bUsingGamepad =
		PlayerController && PlayerController->IsUsingGamepadForHud();
	AIGHorrorHUD::ShowChapterCard(
		this,
		NSLOCTEXT("IGCH03", "EndingEyebrow", "ENDING"),
		EndingTitle,
		FText::Format(
			bUsingGamepad
				? NSLOCTEXT(
					"IGCH03",
					"EndingControlsGamepad",
					"{0}\nL3  이 아침 다시 시작  ·  R3  처음으로")
				: NSLOCTEXT(
					"IGCH03",
					"EndingControlsKeyboard",
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

void AIGThirdMorningDirector::StartWaterBed(const bool bRestoreImmediately)
{
	if (WaterBedComponent)
	{
		return;
	}

	UIGToneSequenceSoundWave* WaterBed =
		UIGToneSequenceSoundWave::CreateFloodedCorridorWaterBed(this);
	WaterBedComponent = IGAudio::SpawnOneShotAt(
		this,
		WaterBed,
		ToWorld(FVector(690, 0, 5)),
		bRestoreImmediately ? 0.42f : 0.001f,
		1.0f,
		260.0f,
		1450.0f);
	if (WaterBedComponent)
	{
		WaterBedComponent->SetLowPassFilterEnabled(true);
		WaterBedComponent->SetLowPassFilterFrequency(2100.0f);
		if (!bRestoreImmediately)
		{
			WaterBedComponent->FadeIn(
				IGThirdMorning::NarrativeCrossfadeSeconds,
				0.42f,
				0.0f);
		}
	}
	if (GetWorld())
	{
		NextWaterBedPulseTime =
			GetWorld()->GetTimeSeconds()
			+ IGThirdMorning::NarrativeCrossfadeSeconds;
	}
}

void AIGThirdMorningDirector::StartRoofBed(const bool bRestoreImmediately)
{
	if (!RoofBedComponent)
	{
		UIGAmbienceSoundWave* RoofBed = NewObject<UIGAmbienceSoundWave>(
			this,
			TEXT("CH03RoofWindRopeWave"));
		RoofBed->Configure(EIGAmbienceMode::RoofWindRope, 0xB00400Fu);
		RoofBedComponent = IGAudio::SpawnOneShotAt(
			this,
			RoofBed,
			ToWorld(IGThirdMorning::TankCenter + FVector(-420.0f, 0.0f, 500.0f)),
			bRestoreImmediately ? 0.38f : 0.001f,
			1.0f,
			680.0f,
			1500.0f);
		if (RoofBedComponent && !bRestoreImmediately)
		{
			RoofBedComponent->FadeIn(
				IGThirdMorning::NarrativeCrossfadeSeconds,
				0.38f,
				0.0f);
		}
	}

	if (!TankPressureComponent && !IsEndingChoiceReady())
	{
		UIGAmbienceSoundWave* TankPressure = NewObject<UIGAmbienceSoundWave>(
			this,
			TEXT("CH03RoofTankPressureWave"));
		TankPressure->Configure(EIGAmbienceMode::RoofTankPressure, 0x7A4C005u);
		TankPressureComponent = IGAudio::SpawnOneShotAt(
			this,
			TankPressure,
			ToWorld(IGThirdMorning::TankCenter + FVector(0.0f, 0.0f, 520.0f)),
			bRestoreImmediately ? 0.30f : 0.001f,
			1.0f,
			260.0f,
			1050.0f);
		if (TankPressureComponent && !bRestoreImmediately)
		{
			TankPressureComponent->FadeIn(
				IGThirdMorning::NarrativeCrossfadeSeconds,
				0.30f,
				0.0f);
		}
	}
}

void AIGThirdMorningDirector::StopRoofBed(const float FadeSeconds)
{
	const auto StopComponent = [FadeSeconds](TObjectPtr<UAudioComponent>& Component)
	{
		if (!Component)
		{
			return;
		}
		UAudioComponent* Audio = Component;
		Component = nullptr;
		if (FadeSeconds <= KINDA_SMALL_NUMBER)
		{
			Audio->Stop();
		}
		else
		{
			Audio->FadeOut(FadeSeconds, 0.0f);
		}
	};
	StopComponent(RoofBedComponent);
	StopComponent(TankPressureComponent);
}

void AIGThirdMorningDirector::StartTankRevealSilence()
{
	StopAllChapterAudio();
	if (UWorld* World = GetWorld())
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		AIGPlayerCharacter* Player = PlayerController
			? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
			: nullptr;
		if (UIGStressComponent* Stress = Player ? Player->GetStress() : nullptr)
		{
			Stress->SuppressHeartbeat(
				IGThirdMorning::TankRevealSilenceSeconds,
				true);
		}
		World->GetTimerManager().SetTimer(
			TankRevealBedRestoreTimer,
			this,
			&ThisClass::RestoreRoofBedAfterTankSilence,
			IGThirdMorning::TankRevealSilenceSeconds,
			false);
	}
}

void AIGThirdMorningDirector::RestoreRoofBedAfterTankSilence()
{
	if (Phase >= EIGThirdMorningPhase::Roof
		&& Phase < EIGThirdMorningPhase::Ending)
	{
		StartRoofBed();
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
	const float GroundSpeed = Pawn->GetVelocity().Size2D();
	if (GroundSpeed > 12.0f)
	{
		if (WaterBedComponent && Now >= NextWaterBedPulseTime)
		{
			// Reuse the looping bed instead of allocating a procedural wave per
			// footfall. A short gain swell lets its 52 Hz layer answer cadence.
			WaterBedComponent->SetVolumeMultiplier(0.50f);
			WaterBedComponent->AdjustVolume(0.30f, 0.42f);
			NextWaterBedPulseTime = Now + FMath::Clamp(
				74.0 / FMath::Max(static_cast<double>(GroundSpeed), 1.0),
				0.32,
				0.72);
		}
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
	const FVector CueLocation =
		Pawn->GetActorLocation() - Pawn->GetActorForwardVector() * 115.0f;
	IGAudio::SpawnOneShotAt(
		this,
		Splash,
		CueLocation,
		0.46f,
		0.92f,
		45.0f,
		720.0f);
	AIGHorrorHUD::PushFearDirection(this, CueLocation, 0.9f);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT(
			"IGCH03",
			"RearSplashCaption",
			"[뒤쪽에서 물이 튀는 소리]"),
		2.0f);
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
	const FVector WorldLocation = ToWorld(LocalLocation);
	IGAudio::SpawnOneShotAt(
		this,
		Ring,
		WorldLocation,
		0.52f,
		PitchMultiplier,
		65.0f,
		1050.0f);
	AIGHorrorHUD::PushFearDirection(this, WorldLocation, 1.1f);
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
	const FVector WorldLocation = ToWorld(
		IGThirdMorning::TankCenter
		+ IGThirdMorning::TankHatchOffset
		+ FVector(0, 0, 620));
	IGAudio::SpawnOneShotAt(
		this,
		Slam,
		WorldLocation,
		0.86f,
		1.0f,
		140.0f,
		1700.0f);
	AIGHorrorHUD::PushFearDirection(this, WorldLocation, 1.4f);
}

void AIGThirdMorningDirector::StopAllChapterAudio()
{
	GetWorldTimerManager().ClearTimer(TankRevealBedRestoreTimer);
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
	if (RoofBedComponent)
	{
		RoofBedComponent->Stop();
		RoofBedComponent = nullptr;
	}
	if (TankPressureComponent)
	{
		TankPressureComponent->Stop();
		TankPressureComponent = nullptr;
	}
	if (EndingBMusicComponent)
	{
		EndingBMusicComponent->Stop();
		EndingBMusicComponent = nullptr;
	}
	if (EndingBLifeBedComponent)
	{
		EndingBLifeBedComponent->Stop();
		EndingBLifeBedComponent = nullptr;
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

	const bool bEndToEndValidation = FParse::Param(
		FCommandLine::Get(),
		TEXT("IGRebirthEndToEndValidation"));
	int32 OutfitDuplicateCount = 0;
	int32 OutfitStitchCount = 0;
	bool bOutfitSpikeValid = !bEndToEndValidation;
	if (bEndToEndValidation && RebirthState)
	{
		const TArray<FName> EquippedChapters =
			RebirthState->BuildSnapshot().EquippedOutfitChapters;
		TSet<FName> UniqueEquippedChapters;
		for (const FName ChapterId : EquippedChapters)
		{
			UniqueEquippedChapters.Add(ChapterId);
		}
		OutfitDuplicateCount =
			EquippedChapters.Num() - UniqueEquippedChapters.Num();
		const APlayerController* Controller =
			GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		const AIGPlayerCharacter* Player = Controller
			? Cast<AIGPlayerCharacter>(Controller->GetPawn())
			: nullptr;
		bOutfitSpikeValid =
			EquippedChapters.Num() == 3
			&& EquippedChapters.Contains(FName(TEXT("CH01")))
			&& EquippedChapters.Contains(FName(TEXT("CH02")))
			&& EquippedChapters.Contains(FName(TEXT("CH03")))
			&& OutfitDuplicateCount == 0
			&& Player
			&& Player->ValidateRebirthOutfitProxy(OutfitStitchCount);
	}

	const float RoofDoorGap = MeasureRoofDoorFreeEdgeGap();
	const bool bDoorGapValid = FMath::IsNearlyEqual(
		RoofDoorGap,
		IGThirdMorning::RoofDoorFreeEdgeGap,
		1.0f);
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
		&& bOutfitSpikeValid
		&& bDoorGapValid;

	if (bPassed)
	{
		if (bEndToEndValidation)
		{
			UE_LOG(
				LogIndieGame,
				Display,
				TEXT(
					"REBIRTH_SPIKE PASS s1_outfit_sleeve "
					"chapters=3 duplicates=0 stitches=3"));
		}
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
		if (bReleaseValidationMode)
		{
			StartRebirthReleaseValidation();
			return;
		}
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
				"scratches=%d tail_settled=%d choices=%d outfit=%d "
				"outfit_duplicates=%d outfit_stitches=%d roof_gap=%.1fcm"),
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
			bOutfitSpikeValid ? 1 : 0,
			OutfitDuplicateCount,
			OutfitStitchCount,
			RoofDoorGap);
	}
	FPlatformMisc::RequestExitWithStatus(
		false,
		bPassed ? 0 : 1,
		TEXT("REBIRTH greybox validation completed"));
}

void AIGThirdMorningDirector::StartRebirthReleaseValidation()
{
	if (bReleaseValidationInProgress)
	{
		return;
	}

	FString EndingValue;
	if (!FParse::Value(
			FCommandLine::Get(),
			TEXT("IGRebirthEnding="),
			EndingValue)
		|| (!EndingValue.Equals(TEXT("A"), ESearchCase::IgnoreCase)
			&& !EndingValue.Equals(TEXT("B"), ESearchCase::IgnoreCase)))
	{
		FailRebirthReleaseValidation(
			TEXT("IGRebirthEnding must be A or B"));
		return;
	}
	bReleaseValidationEndingA =
		EndingValue.Equals(TEXT("A"), ESearchCase::IgnoreCase);

	UGameInstance* GameInstance = GetGameInstance();
	UIGSaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UIGSaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem || !GetWorld())
	{
		FailRebirthReleaseValidation(TEXT("savegame_v3 subsystem unavailable"));
		return;
	}
	if (SaveSubsystem->IsBusy())
	{
		// The settled scratch tail requests its final autosave immediately
		// before the greybox verifier reaches this function. Let that real
		// asynchronous boundary flush instead of treating disk latency as a
		// release failure.
		++ReleaseValidationSaveIdleRetryCount;
		if (ReleaseValidationSaveIdleRetryCount > 100)
		{
			FailRebirthReleaseValidation(
				TEXT("savegame_v3 autosave did not become idle"));
			return;
		}
		GetWorldTimerManager().SetTimer(
			ReleaseValidationTimer,
			this,
			&ThisClass::StartRebirthReleaseValidation,
			0.10f,
			false);
		return;
	}
	ReleaseValidationSaveIdleRetryCount = 0;
	bReleaseValidationInProgress = true;

	int32 CatEnterPasses = 0;
	int32 CatExitPasses = 0;
	int32 LatchedHumanBlocks = 0;
	int32 OpenHumanPasses = 0;
	int32 ReturnHumanBlocks = 0;
	float RoofDoorGapCentimeters = -1.0f;
	if (!ValidateRebirthRoofDoor(
		CatEnterPasses,
		CatExitPasses,
		LatchedHumanBlocks,
		OpenHumanPasses,
		ReturnHumanBlocks,
		RoofDoorGapCentimeters))
	{
		FailRebirthReleaseValidation(TEXT("s2_roof_door"));
		return;
	}
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"REBIRTH_RELEASE PASS s2_roof_door states=3 gap=%.1fcm "
			"cat_enter=%d/20 cat_exit=%d/20 "
			"human_latched_block=%d/20 human_open_pass=%d/20 "
			"human_return_block=%d/20 authoritative_collision=1"),
		RoofDoorGapCentimeters,
		CatEnterPasses,
		CatExitPasses,
		LatchedHumanBlocks,
		OpenHumanPasses,
		ReturnHumanBlocks);

	int32 CollisionFloorSamples = 0;
	int32 CollisionCapsuleSegments = 0;
	if (!ValidateRebirthCollisionRoute(
		CollisionFloorSamples,
		CollisionCapsuleSegments))
	{
		FailRebirthReleaseValidation(TEXT("collision_route"));
		return;
	}
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"REBIRTH_RELEASE PASS collision_route "
			"floor_samples=%d capsule_segments=%d capsule_radius=34 "
			"capsule_half_height=96"),
		CollisionFloorSamples,
		CollisionCapsuleSegments);

	int32 GeneratedAudioSamples = 0;
	int32 GeneratedAudioBytes = 0;
	int32 NonZeroAudioSamples = 0;
	if (!ValidateRebirthAudioQueue(
		GeneratedAudioSamples,
		GeneratedAudioBytes,
		NonZeroAudioSamples))
	{
		FailRebirthReleaseValidation(TEXT("audio_queue"));
		return;
	}
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"REBIRTH_RELEASE PASS audio_queue created=1 stopped=1 "
			"generated_samples=%d generated_bytes=%d nonzero_samples=%d"),
		GeneratedAudioSamples,
		GeneratedAudioBytes,
		NonZeroAudioSamples);

	int32 ItemContinuityCases = 0;
	if (!ValidateRebirthItemContinuity(ItemContinuityCases))
	{
		FailRebirthReleaseValidation(TEXT("s5_item_continuity"));
		return;
	}
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"REBIRTH_RELEASE PASS s5_item_continuity profiles=3 closures=2 "
			"presentations=2 cases=%d duplicates=0"),
		ItemContinuityCases);
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("REBIRTH_RELEASE PASS p3_p5 core=1 truths=4 sources=15"));

	ReleaseValidationSlotName = FString::Printf(
		TEXT("RebirthReleaseValidation_%u"),
		FPlatformProcess::GetCurrentProcessId());
	if (UGameplayStatics::DoesSaveGameExist(ReleaseValidationSlotName, 0))
	{
		const bool bRemovedStaleSlot =
			UGameplayStatics::DeleteGameInSlot(ReleaseValidationSlotName, 0);
		if (!bRemovedStaleSlot
			|| UGameplayStatics::DoesSaveGameExist(
				ReleaseValidationSlotName,
				0))
		{
			FailRebirthReleaseValidation(
				TEXT("savegame_v3 stale slot cleanup failed"));
			return;
		}
	}

	SaveSubsystem->OnSaveCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleReleaseValidationSaveCompleted);
	const FGameplayTag ChapterTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Chapter.CH03")),
		false);
	const FGameplayTag CheckpointTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Checkpoint.CH03.Roof")),
		false);
	if (!ChapterTag.IsValid()
		|| !CheckpointTag.IsValid()
		|| !SaveSubsystem->RequestSave(
			ReleaseValidationSlotName,
			ChapterTag,
			NAME_None,
			CheckpointTag))
	{
		SaveSubsystem->OnSaveCompleted.RemoveDynamic(
			this,
			&ThisClass::HandleReleaseValidationSaveCompleted);
		FailRebirthReleaseValidation(TEXT("savegame_v3 save request rejected"));
	}
}

bool AIGThirdMorningDirector::ValidateRebirthRoofDoor(
	int32& OutCatEnterPasses,
	int32& OutCatExitPasses,
	int32& OutLatchedHumanBlocks,
	int32& OutOpenHumanPasses,
	int32& OutReturnHumanBlocks,
	float& OutGapCentimeters)
{
	OutCatEnterPasses = 0;
	OutCatExitPasses = 0;
	OutLatchedHumanBlocks = 0;
	OutOpenHumanPasses = 0;
	OutReturnHumanBlocks = 0;
	OutGapCentimeters = -1.0f;

	UWorld* World = GetWorld();
	if (!World || !RoofDoorAction || !RoofDoorLeaf)
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(RoofDoorHoldTimer);
	GetWorldTimerManager().ClearTimer(RoofDoorReturnTimer);
	const EIGRoofDoorState PreviousState = RoofDoorState;
	const auto HasAuthoritativeCollision = [this]()
	{
		return RoofDoorAction
			&& RoofDoorLeaf
			&& RoofDoorAction->GetActorEnableCollision()
			&& RoofDoorLeaf->GetCollisionEnabled()
				== ECollisionEnabled::QueryAndPhysics
			&& RoofDoorLeaf->GetCollisionResponseToChannel(ECC_Pawn)
				== ECR_Block;
	};

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(RebirthReleaseRoofDoor),
		false);
	if (const APlayerController* Controller =
			World->GetFirstPlayerController())
	{
		QueryParams.AddIgnoredActor(Controller->GetPawn());
	}
	const auto Sweep = [
		World,
		&QueryParams,
		this](
			const FCollisionShape& Shape,
			const FVector& LocalStart,
			const FVector& LocalEnd,
			FHitResult& OutHit)
	{
		return World->SweepSingleByChannel(
			OutHit,
			ToWorld(LocalStart),
			ToWorld(LocalEnd),
			FQuat::Identity,
			ECC_Pawn,
			Shape,
			QueryParams);
	};
	const FCollisionShape CatCapsule =
		FCollisionShape::MakeCapsule(3.5f, 3.5f);
	const FCollisionShape PlayerCapsule =
		FCollisionShape::MakeCapsule(34.0f, 96.0f);
	const float CatCenterZ = IGThirdMorning::RoofFloorZ + 5.0f;
	// The animal route bends around the free edge and through the narrow slit.
	// A straight centre-line sweep would either intersect the leaf or recreate
	// the old, physically wrong gap under the entire door.
	const TArray<FVector> CatRoute = {
		FVector(1620.0f, -400.0f, CatCenterZ),
		FVector(1645.5f, -340.5f, CatCenterZ),
		FVector(1647.0f, -339.0f, CatCenterZ),
		FVector(1653.5f, -338.5f, CatCenterZ),
		FVector(1655.5f, -339.0f, CatCenterZ),
		FVector(1705.0f, -400.0f, CatCenterZ)};
	const FVector PlayerInside(
		1620.0f,
		-400.0f,
		IGThirdMorning::RoofFloorZ
			+ IGThirdMorning::StandingCapsuleCenter
			+ 1.0f);
	const FVector PlayerOutside(
		1705.0f,
		-400.0f,
		IGThirdMorning::RoofFloorZ
			+ IGThirdMorning::StandingCapsuleCenter
			+ 1.0f);
	const auto IsCatRouteClear = [
		&Sweep,
		&CatCapsule,
		&CatRoute](const bool bReverse)
	{
		for (int32 SegmentIndex = 0;
			SegmentIndex < CatRoute.Num() - 1;
			++SegmentIndex)
		{
			const int32 StartIndex = bReverse
				? CatRoute.Num() - 1 - SegmentIndex
				: SegmentIndex;
			const int32 EndIndex = bReverse
				? StartIndex - 1
				: StartIndex + 1;
			FHitResult Hit;
			if (Sweep(
				CatCapsule,
				CatRoute[StartIndex],
				CatRoute[EndIndex],
				Hit))
			{
				return false;
			}
		}
		return true;
	};

	ApplyRoofDoorState(EIGRoofDoorState::LatchedGap);
	const bool bLatchedStateApplied =
		RoofDoorState == EIGRoofDoorState::LatchedGap
		&& HasAuthoritativeCollision();
	OutGapCentimeters = MeasureRoofDoorFreeEdgeGap();
	for (int32 Attempt = 0; Attempt < 20; ++Attempt)
	{
		if (IsCatRouteClear(false))
		{
			++OutCatEnterPasses;
		}
		if (IsCatRouteClear(true))
		{
			++OutCatExitPasses;
		}
		FHitResult HumanHit;
		if (Sweep(PlayerCapsule, PlayerInside, PlayerOutside, HumanHit)
			&& HumanHit.GetComponent() == RoofDoorLeaf.Get())
		{
			++OutLatchedHumanBlocks;
		}
	}

	ApplyRoofDoorState(EIGRoofDoorState::PulledOpen);
	const bool bPulledStateApplied =
		RoofDoorState == EIGRoofDoorState::PulledOpen
		&& HasAuthoritativeCollision();
	for (int32 Attempt = 0; Attempt < 20; ++Attempt)
	{
		FHitResult HumanHit;
		if (!Sweep(PlayerCapsule, PlayerInside, PlayerOutside, HumanHit))
		{
			++OutOpenHumanPasses;
		}
	}

	ApplyRoofDoorState(EIGRoofDoorState::ReturnToGap);
	const bool bReturnStateApplied =
		RoofDoorState == EIGRoofDoorState::ReturnToGap
		&& HasAuthoritativeCollision();
	for (int32 Attempt = 0; Attempt < 20; ++Attempt)
	{
		FHitResult HumanHit;
		if (Sweep(PlayerCapsule, PlayerInside, PlayerOutside, HumanHit)
			&& HumanHit.GetComponent() == RoofDoorLeaf.Get())
		{
			++OutReturnHumanBlocks;
		}
	}

	ApplyRoofDoorState(PreviousState);
	const bool bRestoredState = RoofDoorState == PreviousState;
	return bLatchedStateApplied
		&& bPulledStateApplied
		&& bReturnStateApplied
		&& bRestoredState
		&& FMath::IsNearlyEqual(
			OutGapCentimeters,
			IGThirdMorning::RoofDoorFreeEdgeGap,
			1.0f)
		&& OutCatEnterPasses == 20
		&& OutCatExitPasses == 20
		&& OutLatchedHumanBlocks == 20
		&& OutOpenHumanPasses == 20
		&& OutReturnHumanBlocks == 20;
}

bool AIGThirdMorningDirector::ValidateRebirthCollisionRoute(
	int32& OutFloorSamples,
	int32& OutCapsuleSegments) const
{
	OutFloorSamples = 0;
	OutCapsuleSegments = 0;
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(RebirthReleaseCollisionRoute),
		false);
	if (const APlayerController* Controller = World->GetFirstPlayerController())
	{
		QueryParams.AddIgnoredActor(Controller->GetPawn());
	}
	// The moving leaf has its own three-state cat/player sweep above. Ignore
	// only that actor here so this pass measures the fixed route, frame and
	// floor without treating the intentionally latched door as a regression.
	QueryParams.AddIgnoredActor(RoofDoorAction);

	struct FFloorSample
	{
		FVector LocalPosition;
		float SurfaceZ;
	};
	TArray<FFloorSample> FloorSamples;
	FloorSamples.Reserve(41);
	FloorSamples.Add({FVector(0.0f, 0.0f, 0.0f), 0.0f});
	FloorSamples.Add({FVector(675.0f, 0.0f, 0.0f), 0.0f});
	FloorSamples.Add({FVector(1040.0f, 0.0f, 0.0f), 0.0f});
	for (int32 StepIndex = 0; StepIndex < 12; ++StepIndex)
	{
		const float StepTopZ = 15.0f * (StepIndex + 1);
		FloorSamples.Add({
			FVector(1085.0f, -45.0f - StepIndex * 29.0f, 0.0f),
			StepTopZ});
	}
	FloorSamples.Add({FVector(1200.0f, -420.0f, 0.0f), 180.0f});
	for (int32 StepIndex = 0; StepIndex < 4; ++StepIndex)
	{
		const float StepTopZ = 195.0f + StepIndex * 15.0f;
		FloorSamples.Add({
			FVector(1510.0f + StepIndex * 36.0f, -420.0f, 0.0f),
			StepTopZ});
	}
	// Sample the first uninterrupted roof patch beyond the movable fire door.
	// The old X=1660 point sat inside the leaf while it was being held open,
	// so the vertical trace reported the door panel instead of the roof slab.
	FloorSamples.Add({
		FVector(1720.0f, -420.0f, 0.0f),
		IGThirdMorning::RoofFloorZ});
	FloorSamples.Add({
		FVector(1800.0f, -600.0f, 0.0f),
		IGThirdMorning::RoofFloorZ});
	for (int32 StepIndex = 0; StepIndex < 18; ++StepIndex)
	{
		const float StepTopZ =
			IGThirdMorning::RoofFloorZ + 20.0f * (StepIndex + 1);
		FloorSamples.Add({
			FVector(1915.0f + StepIndex * 20.0f, -300.0f, 0.0f),
			StepTopZ});
	}
	FloorSamples.Add({FVector(2310.0f, -300.0f, 0.0f), 620.0f});
	OutFloorSamples = FloorSamples.Num();
	for (const FFloorSample& Sample : FloorSamples)
	{
		FHitResult FloorHit;
		const FVector Start = ToWorld(FVector(
			Sample.LocalPosition.X,
			Sample.LocalPosition.Y,
			Sample.SurfaceZ + 150.0f));
		const FVector End = ToWorld(FVector(
			Sample.LocalPosition.X,
			Sample.LocalPosition.Y,
			Sample.SurfaceZ - 40.0f));
		const float ExpectedSurfaceZ = ToWorld(FVector(
			Sample.LocalPosition.X,
			Sample.LocalPosition.Y,
			Sample.SurfaceZ)).Z;
		if (!World->LineTraceSingleByChannel(
				FloorHit,
				Start,
				End,
				ECC_Pawn,
				QueryParams)
			|| FloorHit.ImpactNormal.Z < 0.75f
			|| !FMath::IsNearlyEqual(
				FloorHit.ImpactPoint.Z,
				ExpectedSurfaceZ,
				2.0f))
		{
			UE_LOG(
				LogIndieGame,
				Error,
				TEXT(
					"REBIRTH_RELEASE collision floor sample failed "
					"local=%s expected_z=%.1f hit=%d hit_z=%.1f normal_z=%.2f"),
				*Sample.LocalPosition.ToCompactString(),
				ExpectedSurfaceZ,
				FloorHit.bBlockingHit ? 1 : 0,
				FloorHit.ImpactPoint.Z,
				FloorHit.ImpactNormal.Z);
			return false;
		}
	}

	struct FCapsuleSegment
	{
		FVector LocalStart;
		FVector LocalEnd;
	};
	// Match AIGPlayerCharacter's actual 34 x 96 cm capsule. The one-centimeter
	// floor clearance avoids treating stable floor contact as a route blocker.
	constexpr float StandingCenter =
		IGThirdMorning::StandingCapsuleCenter + 1.0f;
	const FCapsuleSegment RouteSegments[] = {
		{
			FVector(280.0f, 0.0f, StandingCenter),
			FVector(430.0f, 0.0f, StandingCenter)
		},
		{
			FVector(430.0f, 0.0f, StandingCenter),
			FVector(1040.0f, 0.0f, StandingCenter)
		},
		{
			FVector(1085.0f, -420.0f, 180.0f + StandingCenter),
			FVector(1455.0f, -420.0f, 180.0f + StandingCenter)
		},
		{
			FVector(1590.0f, -420.0f, IGThirdMorning::RoofFloorZ + StandingCenter),
			FVector(1740.0f, -420.0f, IGThirdMorning::RoofFloorZ + StandingCenter)
		},
		{
			FVector(1740.0f, -420.0f, IGThirdMorning::RoofFloorZ + StandingCenter),
			FVector(1740.0f, -600.0f, IGThirdMorning::RoofFloorZ + StandingCenter)
		},
		{
			FVector(1740.0f, -600.0f, IGThirdMorning::RoofFloorZ + StandingCenter),
			FVector(1840.0f, -600.0f, IGThirdMorning::RoofFloorZ + StandingCenter)
		},
		{
			FVector(1840.0f, -600.0f, IGThirdMorning::RoofFloorZ + StandingCenter),
			FVector(1840.0f, -300.0f, IGThirdMorning::RoofFloorZ + StandingCenter)
		},
		{
			FVector(1840.0f, -300.0f, IGThirdMorning::RoofFloorZ + StandingCenter),
			FVector(1868.0f, -300.0f, IGThirdMorning::RoofFloorZ + StandingCenter)
		},
		{
			FVector(2280.0f, -300.0f, 620.0f + StandingCenter),
			FVector(2330.0f, -300.0f, 620.0f + StandingCenter)
		}
	};
	OutCapsuleSegments = UE_ARRAY_COUNT(RouteSegments);
	const FCollisionShape PlayerCapsule =
		FCollisionShape::MakeCapsule(34.0f, 96.0f);
	for (const FCapsuleSegment& Segment : RouteSegments)
	{
		FHitResult RouteHit;
		if (World->SweepSingleByChannel(
			RouteHit,
			ToWorld(Segment.LocalStart),
			ToWorld(Segment.LocalEnd),
			FQuat::Identity,
			ECC_Pawn,
			PlayerCapsule,
			QueryParams))
		{
			const UPrimitiveComponent* HitComponent =
				RouteHit.GetComponent();
			const FVector LocalImpact = GetActorTransform()
				.InverseTransformPosition(RouteHit.ImpactPoint);
			UE_LOG(
				LogIndieGame,
				Error,
				TEXT(
					"REBIRTH_RELEASE collision route blocked "
					"start=%s end=%s actor=%s component=%s local_impact=%s "
					"component_local=%s bounds_extent=%s time=%.3f "
					"penetrating=%d depth=%.2f"),
				*Segment.LocalStart.ToCompactString(),
				*Segment.LocalEnd.ToCompactString(),
				*GetNameSafe(RouteHit.GetActor()),
				*GetNameSafe(HitComponent),
				*LocalImpact.ToCompactString(),
				HitComponent
					? *HitComponent->GetRelativeLocation().ToCompactString()
					: TEXT("None"),
				HitComponent
					? *HitComponent->Bounds.BoxExtent.ToCompactString()
					: TEXT("None"),
				RouteHit.Time,
				RouteHit.bStartPenetrating ? 1 : 0,
				RouteHit.PenetrationDepth);
			return false;
		}
	}
	return true;
}

bool AIGThirdMorningDirector::ValidateRebirthAudioQueue(
	int32& OutGeneratedSamples,
	int32& OutGeneratedBytes,
	int32& OutNonZeroSamples)
{
	OutGeneratedSamples = 0;
	OutGeneratedBytes = 0;
	OutNonZeroSamples = 0;

	struct FAudioTrackProbe
	{
		const TCHAR* Name = TEXT("Unknown");
		UIGToneSequenceSoundWave* Tone = nullptr;
		UIGAmbienceSoundWave* Ambience = nullptr;
	};

	TArray<FAudioTrackProbe> Tracks;
	constexpr int32 ExpectedTrackCount = 19;
	Tracks.Reserve(ExpectedTrackCount);
	const auto AddAmbienceTrack =
		[this, &Tracks](
			const TCHAR* Name,
			const EIGAmbienceMode Mode,
			const uint32 Seed)
	{
		UIGAmbienceSoundWave* Wave = NewObject<UIGAmbienceSoundWave>(this);
		Wave->Configure(Mode, Seed);
		Tracks.Add(FAudioTrackProbe{Name, nullptr, Wave});
	};

	AddAmbienceTrack(TEXT("M0.RoomTone"), EIGAmbienceMode::RoomTone, 0xA000001u);
	Tracks.Add(FAudioTrackProbe{
		TEXT("M1.StoreJingle"),
		UIGToneSequenceSoundWave::CreateStoreJingle(this),
		nullptr});
	Tracks.Add(FAudioTrackProbe{
		TEXT("M1b.DegradedJingle"),
		UIGToneSequenceSoundWave::CreateStoreJingle(
			this,
			-0.18f,
			1.0f / 0.92f),
		nullptr});
	AddAmbienceTrack(TEXT("M2.DoorBeyond"), EIGAmbienceMode::DoorBeyond, 0xA200002u);
	UIGToneSequenceSoundWave* FloodedWaterBed =
		UIGToneSequenceSoundWave::CreateFloodedCorridorWaterBed(this);
	Tracks.Add(FAudioTrackProbe{
		TEXT("M3.FloodedWater"),
		FloodedWaterBed,
		nullptr});
	AddAmbienceTrack(TEXT("M4.WindRope"), EIGAmbienceMode::RoofWindRope, 0xA400004u);
	AddAmbienceTrack(TEXT("M4.TankPressure"), EIGAmbienceMode::RoofTankPressure, 0xA400005u);
	// The thinnest cue in the game still has to render real samples. A dust
	// sift that renders silence would be indistinguishable from a bug, because
	// nobody would notice the difference by ear.
	Tracks.Add(FAudioTrackProbe{
		TEXT("V1.PlasterDustFall"),
		UIGToneSequenceSoundWave::CreatePlasterDustFall(this),
		nullptr});
	// P3 decides a puzzle by ear, so both of its answers and the two ends of
	// the water's distance range have to survive the same silence-and-clipping
	// check as the score does. The hammer is the loudest cue in the game and the
	// most likely to clip, which is exactly why it is measured.
	Tracks.Add(FAudioTrackProbe{
		TEXT("P3.WallCavityHollow"),
		UIGToneSequenceSoundWave::CreateWallCavityResponse(this, true),
		nullptr});
	Tracks.Add(FAudioTrackProbe{
		TEXT("P3.WallCavitySolid"),
		UIGToneSequenceSoundWave::CreateWallCavityResponse(this, false),
		nullptr});
	Tracks.Add(FAudioTrackProbe{
		TEXT("P3.PipeWaterNear"),
		UIGToneSequenceSoundWave::CreatePipeWaterFlow(this, 0),
		nullptr});
	Tracks.Add(FAudioTrackProbe{
		TEXT("P3.PipeWaterFar"),
		UIGToneSequenceSoundWave::CreatePipeWaterFlow(this, 3),
		nullptr});
	Tracks.Add(FAudioTrackProbe{
		TEXT("P3.ValveOpen"),
		UIGToneSequenceSoundWave::CreateValveOpen(this, 0),
		nullptr});
	Tracks.Add(FAudioTrackProbe{
		TEXT("P5.HammerBreakThrough"),
		UIGToneSequenceSoundWave::CreateHammerImpact(this, 4),
		nullptr});
	// The three §21.3 rows that were still standing in for themselves. The two
	// drag beds are checked separately: a vinyl variant that renders identical
	// to the concrete one would pass every static assertion and tell the player
	// nothing about which floor he is on.
	Tracks.Add(FAudioTrackProbe{
		TEXT("P2.FrottageRub"),
		UIGToneSequenceSoundWave::CreateFrottageRub(this),
		nullptr});
	Tracks.Add(FAudioTrackProbe{
		TEXT("V1.AudibleHeartbeat"),
		UIGToneSequenceSoundWave::CreateAudibleHeartbeat(this, 0.30f),
		nullptr});
	Tracks.Add(FAudioTrackProbe{
		TEXT("Entity.DragConcrete"),
		UIGToneSequenceSoundWave::CreateEntityDragLoop(this, false),
		nullptr});
	Tracks.Add(FAudioTrackProbe{
		TEXT("Entity.DragVinyl"),
		UIGToneSequenceSoundWave::CreateEntityDragLoop(this, true),
		nullptr});
	UIGToneSequenceSoundWave* EndingBReturnHome =
		UIGToneSequenceSoundWave::CreateEndingBReturnHomeBed(this);
	Tracks.Add(FAudioTrackProbe{
		TEXT("M5.ReturnHome"),
		EndingBReturnHome,
		nullptr});

	constexpr int32 RequestedSamplesPerTrack = 4096;
	int32 InvalidTrackCount = 0;
	int32 ClippedSampleCount = 0;
	int32 PeakMagnitude = 0;
	for (const FAudioTrackProbe& Track : Tracks)
	{
		TArray<uint8> GeneratedPcm;
		int32 GeneratedSamples = 0;
		if (Track.Tone)
		{
			GeneratedSamples = Track.Tone->OnGeneratePCMAudio(
				GeneratedPcm,
				RequestedSamplesPerTrack);
		}
		else if (Track.Ambience)
		{
			GeneratedSamples = Track.Ambience->OnGeneratePCMAudio(
				GeneratedPcm,
				RequestedSamplesPerTrack);
		}

		const int32 GeneratedBytes = GeneratedPcm.Num();
		int32 TrackNonZeroSamples = 0;
		int32 TrackClippedSamples = 0;
		int32 TrackPeakMagnitude = 0;
		if (GeneratedBytes % static_cast<int32>(sizeof(int16)) == 0)
		{
			const int16* Samples =
				reinterpret_cast<const int16*>(GeneratedPcm.GetData());
			const int32 SampleCount =
				GeneratedBytes / static_cast<int32>(sizeof(int16));
			for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
			{
				const int32 Magnitude =
					FMath::Abs(static_cast<int32>(Samples[SampleIndex]));
				TrackNonZeroSamples += Magnitude > 0 ? 1 : 0;
				TrackClippedSamples += Magnitude >= 32767 ? 1 : 0;
				TrackPeakMagnitude = FMath::Max(TrackPeakMagnitude, Magnitude);
			}
		}

		OutGeneratedSamples += GeneratedSamples;
		OutGeneratedBytes += GeneratedBytes;
		OutNonZeroSamples += TrackNonZeroSamples;
		ClippedSampleCount += TrackClippedSamples;
		PeakMagnitude = FMath::Max(PeakMagnitude, TrackPeakMagnitude);

		const bool bTrackValid =
			GeneratedSamples == RequestedSamplesPerTrack
			&& GeneratedBytes
				== RequestedSamplesPerTrack * static_cast<int32>(sizeof(int16))
			&& TrackNonZeroSamples > 32
			&& TrackClippedSamples == 0;
		if (!bTrackValid)
		{
			++InvalidTrackCount;
			UE_LOG(
				LogIndieGame,
				Error,
				TEXT(
					"REBIRTH_RELEASE audio track failed name=%s samples=%d "
					"bytes=%d nonzero=%d clipped=%d peak=%d"),
				Track.Name,
				GeneratedSamples,
				GeneratedBytes,
				TrackNonZeroSamples,
				TrackClippedSamples,
				TrackPeakMagnitude);
		}
	}

	constexpr float ExpectedM5DurationSeconds = 45.05f;
	const float M5DurationSeconds = EndingBReturnHome
		? EndingBReturnHome->GetConfiguredDurationSeconds()
		: 0.0f;
	const bool bM5TimingValid =
		EndingBReturnHome
		&& !EndingBReturnHome->IsConfiguredLooping()
		&& FMath::IsNearlyEqual(
			M5DurationSeconds,
			ExpectedM5DurationSeconds,
			0.01f);
	const bool bPcmGenerated =
		Tracks.Num() == ExpectedTrackCount
		&& InvalidTrackCount == 0
		&& ClippedSampleCount == 0
		&& bM5TimingValid;

	StopAllChapterAudio();
	StartWaterBed(true);
	UAudioComponent* QueuedAudio = WaterBedComponent.Get();
	// -nosound deliberately prevents SpawnSoundAtLocation from returning a
	// device-backed component. Keep workplace/CI runs silent, but still verify
	// that the generated sound can be assigned to a component and stopped.
	if (!QueuedAudio && FParse::Param(FCommandLine::Get(), TEXT("nosound")))
	{
		QueuedAudio = NewObject<UAudioComponent>(
			this,
			TEXT("CH03ReleaseValidationSilentAudio"));
		if (QueuedAudio)
		{
			QueuedAudio->SetSound(FloodedWaterBed);
		}
	}
	const bool bCreated =
		IsValid(QueuedAudio) && IsValid(QueuedAudio->GetSound());
	if (QueuedAudio)
	{
		QueuedAudio->Stop();
	}
	const bool bStopped =
		!IsValid(QueuedAudio) || !QueuedAudio->IsPlaying();
	WaterBedComponent = nullptr;
	if (!bPcmGenerated || !bCreated || !bStopped)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_RELEASE audio queue failed pcm=%d created=%d "
				"stopped=%d tracks=%d invalid=%d samples=%d bytes=%d "
				"nonzero=%d clipped=%d peak=%d m5_duration=%.2f nosound=%d"),
			bPcmGenerated ? 1 : 0,
			bCreated ? 1 : 0,
			bStopped ? 1 : 0,
			Tracks.Num(),
			InvalidTrackCount,
			OutGeneratedSamples,
			OutGeneratedBytes,
			OutNonZeroSamples,
			ClippedSampleCount,
			PeakMagnitude,
			M5DurationSeconds,
			FParse::Param(FCommandLine::Get(), TEXT("nosound")) ? 1 : 0);
	}
	else
	{
		UE_LOG(
			LogIndieGame,
			Display,
			TEXT(
				"REBIRTH_RELEASE PASS audio_synthesis tracks=%d invalid=0 "
				"clipped=0 peak=%d m5_duration=%.2f"),
			Tracks.Num(),
			PeakMagnitude,
			M5DurationSeconds);
	}
	return bPcmGenerated && bCreated && bStopped;
}

bool AIGThirdMorningDirector::ValidateRebirthItemContinuity(
	int32& OutCaseCount)
{
	OutCaseCount = 0;
	if (!GetWorld() || !CubeMesh || !CylinderMesh)
	{
		return false;
	}

	const auto CountLiveContinuityActors = [this]()
	{
		int32 Count = 0;
		for (TActorIterator<AIGItemContinuityDressing> It(GetWorld()); It; ++It)
		{
			const AIGItemContinuityDressing* Dressing = *It;
			if (IsValid(Dressing) && !Dressing->IsActorBeingDestroyed())
			{
				++Count;
			}
		}
		return Count;
	};
	if (CountLiveContinuityActors() != 0)
	{
		return false;
	}

	const EIGRebirthPurchaseProfile Profiles[] = {
		EIGRebirthPurchaseProfile::ProfileA500MlX2,
		EIGRebirthPurchaseProfile::ProfileB1LX1,
		EIGRebirthPurchaseProfile::ProfileC2LX2};
	const EIGRebirthBottleClosureState ClosureStates[] = {
		EIGRebirthBottleClosureState::MissingCap,
		EIGRebirthBottleClosureState::Resealed};
	const EIGItemContinuityPresentation Presentations[] = {
		EIGItemContinuityPresentation::AccidentBag,
		EIGItemContinuityPresentation::LobbyRecycleSack};

	for (const EIGRebirthPurchaseProfile Profile : Profiles)
	{
		for (const EIGRebirthBottleClosureState ClosureState : ClosureStates)
		{
			for (const EIGItemContinuityPresentation Presentation : Presentations)
			{
				const bool bAccidentBag =
					Presentation == EIGItemContinuityPresentation::AccidentBag;
				const FTransform TestTransform = bAccidentBag
					? AIGItemContinuityDressing::GetCanonicalAccidentTransform(Profile)
					: FTransform(
						FRotator::ZeroRotator,
						IGThirdMorning::StageOrigin
							+ FVector(0.0f, 0.0f, -10000.0f));
				FActorSpawnParameters Parameters;
				Parameters.Owner = this;
				Parameters.SpawnCollisionHandlingOverride =
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AIGItemContinuityDressing* Dressing =
					GetWorld()->SpawnActor<AIGItemContinuityDressing>(
						AIGItemContinuityDressing::StaticClass(),
						TestTransform,
						Parameters);
				if (!Dressing)
				{
					return false;
				}
				Dressing->Configure(
					Presentation,
					Profile,
					ClosureState,
					CubeMesh,
					CylinderMesh,
					CarrierBagMaterial,
					GlassMaterial,
					WaterMaterial,
					BottleCapMaterial);
				Dressing->SetActorHiddenInGame(true);

				const FName PresentationTag = bAccidentBag
					? FName(TEXT("REBIRTH.ItemContinuity.CH01AccidentBag"))
					: FName(TEXT("REBIRTH.ItemContinuity.CH02RecycleSack"));
				const bool bTransformMatches = !bAccidentBag
					|| Dressing->GetActorTransform().Equals(TestTransform, 0.01f);
				const bool bCasePassed = Dressing->MatchesContract(
					Presentation,
					Profile,
					ClosureState)
					&& Dressing->ActorHasTag(
						FName(TEXT("REBIRTH.ItemContinuity")))
					&& Dressing->ActorHasTag(PresentationTag)
					&& bTransformMatches
					&& CountLiveContinuityActors() == 1;
				const bool bDestroyed = Dressing->Destroy();
				if (!bCasePassed || !bDestroyed
					|| CountLiveContinuityActors() != 0)
				{
					return false;
				}
				++OutCaseCount;
			}
		}
	}
	return OutCaseCount == 12 && CountLiveContinuityActors() == 0;
}

void AIGThirdMorningDirector::HandleReleaseValidationSaveCompleted(
	const bool bSuccess,
	const FString SlotName)
{
	if (SlotName != ReleaseValidationSlotName)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UIGSaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UIGSaveSubsystem>()
		: nullptr;
	if (SaveSubsystem)
	{
		SaveSubsystem->OnSaveCompleted.RemoveDynamic(
			this,
			&ThisClass::HandleReleaseValidationSaveCompleted);
	}
	if (!bSuccess
		|| !SaveSubsystem
		|| !UGameplayStatics::DoesSaveGameExist(ReleaseValidationSlotName, 0))
	{
		FailRebirthReleaseValidation(TEXT("savegame_v3 disk write failed"));
		return;
	}

	UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	if (!RebirthState)
	{
		FailRebirthReleaseValidation(TEXT("savegame_v3 narrative unavailable"));
		return;
	}
	RebirthState->SetHasMemoryFlashlight(false);
	RebirthState->ResetChapterThreeAttempt();

	SaveSubsystem->OnLoadCompleted.AddUniqueDynamic(
		this,
		&ThisClass::HandleReleaseValidationLoadCompleted);
	if (!SaveSubsystem->RequestLoad(ReleaseValidationSlotName))
	{
		SaveSubsystem->OnLoadCompleted.RemoveDynamic(
			this,
			&ThisClass::HandleReleaseValidationLoadCompleted);
		FailRebirthReleaseValidation(TEXT("savegame_v3 load request rejected"));
	}
}

void AIGThirdMorningDirector::HandleReleaseValidationLoadCompleted(
	const bool bSuccess,
	const FString SlotName,
	UIGSaveGame* SaveGame)
{
	if (SlotName != ReleaseValidationSlotName)
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGSaveSubsystem* SaveSubsystem =
				GameInstance->GetSubsystem<UIGSaveSubsystem>())
		{
			SaveSubsystem->OnLoadCompleted.RemoveDynamic(
				this,
				&ThisClass::HandleReleaseValidationLoadCompleted);
		}
	}

	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	const FIGRebirthNarrativeSnapshot RestoredSnapshot = RebirthState
		? RebirthState->BuildSnapshot()
		: FIGRebirthNarrativeSnapshot();
	const FIGRebirthChapterThreeState& RestoredChapterThree =
		RestoredSnapshot.ChapterThree;
	const FGameplayTag ExpectedChapter = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Chapter.CH03")),
		false);
	const FGameplayTag ExpectedCheckpoint = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Checkpoint.CH03.Roof")),
		false);
	const bool bRestored =
		bSuccess
		&& SaveGame
		&& SaveGame->Progress.SchemaVersion
			== UIGSaveGame::CurrentSchemaVersion
		&& SaveGame->Progress.ChapterId.MatchesTagExact(ExpectedChapter)
		&& SaveGame->Progress.CheckpointTag.MatchesTagExact(
			ExpectedCheckpoint)
		&& SaveGame->Progress.MapPackageName.IsNone()
		&& SaveGame->Progress.RebirthNarrative.SchemaVersion == 3
		&& RestoredSnapshot.SchemaVersion == 3
		&& RestoredSnapshot.bHasMemoryFlashlight
		&& RestoredChapterThree.P3.bCompleted
		&& RestoredChapterThree.ObservedP5Sources.Num() == 15
		&& RestoredChapterThree.AccidentScratchCount == 3
		&& RestoredChapterThree.bAccidentScratchTailSettled
		&& GetAccidentTruthCount() == 4
		&& RebirthState
		&& RebirthState->CanConverge(
			EIGRebirthConvergencePoint::C5FinalChoice);
	const bool bDeleteSucceeded =
		UGameplayStatics::DeleteGameInSlot(ReleaseValidationSlotName, 0);
	const bool bSlotDeleted =
		!UGameplayStatics::DoesSaveGameExist(ReleaseValidationSlotName, 0);
	if (!bRestored || !bDeleteSucceeded || !bSlotDeleted)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_RELEASE savegame cleanup detail "
				"restored=%d delete_call=%d slot_deleted=%d"),
			bRestored ? 1 : 0,
			bDeleteSucceeded ? 1 : 0,
			bSlotDeleted ? 1 : 0);
		FailRebirthReleaseValidation(
			bRestored
				? TEXT("savegame_v3 cleanup mismatch")
				: TEXT("savegame_v3 restore mismatch"));
		return;
	}

	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"REBIRTH_RELEASE PASS savegame_v3 "
			"outer=3 inner=3 restored=1 delete_call=1 slot_deleted=1"));
	BeginRebirthEndingValidation();
}

void AIGThirdMorningDirector::BeginRebirthEndingValidation()
{
	ReleaseValidationSafetyOpeningCues.Reset();
	ReleaseValidationTankLidBeforeEnding = TankLidAction;
	ReleaseValidationTankLidCountBeforeEnding =
		CountOwnedChapterThreeActions(EIGChapterThreeAction::OpenTank);
	ReleaseValidationOwnedActionCountBeforeEnding =
		CountOwnedChapterThreeActions();
	if (!IsValid(ReleaseValidationTankLidBeforeEnding)
		|| ReleaseValidationTankLidCountBeforeEnding != 1)
	{
		FailRebirthReleaseValidation(
			TEXT("s4_common_prop precondition"));
		return;
	}

	if (bReleaseValidationEndingA)
	{
		FinishEndingA();
	}
	else
	{
		BeginEndingB();
	}

	if (!bEndingFinished)
	{
		FailRebirthReleaseValidation(TEXT("ending action rejected"));
		return;
	}
	// The full authored tails: A reaches its strong-cue guard about 14.8 s
	// after the choice (restore 0.8 s, fade, five opening sounds, 2 s card,
	// blackout cues); B adds the pre-black watch and the montage guard.
	GetWorldTimerManager().SetTimer(
		ReleaseValidationTimer,
		this,
		&ThisClass::FinishRebirthEndingValidation,
		bReleaseValidationEndingA ? 17.0f : 25.5f,
		false);
}

void AIGThirdMorningDirector::FinishRebirthEndingValidation()
{
	const UIGRebirthNarrativeSubsystem* RebirthState = GetRebirthState();
	const FIGRebirthNarrativeSnapshot Snapshot = RebirthState
		? RebirthState->BuildSnapshot()
		: FIGRebirthNarrativeSnapshot();
	const bool bEndingA = bReleaseValidationEndingA;
	const EIGRebirthEndingChoice ExpectedEnding = bEndingA
		? EIGRebirthEndingChoice::EndingA
		: EIGRebirthEndingChoice::EndingB;
	const FName EndingPuzzle = bEndingA
		? FName(TEXT("Ending.A"))
		: FName(TEXT("Ending.B"));
	const FName StrongCue = bEndingA
		? FName(TEXT("Ending.A.StrongCuePlayed"))
		: FName(TEXT("Ending.B.StrongCuePlayed"));
	const TArray<FName> ExpectedSafetyOpeningCues = {
		FName(TEXT("Safety.GasDetector")),
		FName(TEXT("Safety.Ventilation")),
		FName(TEXT("Safety.Harness")),
		FName(TEXT("Safety.TwoClimbers")),
		FName(TEXT("Safety.HatchOpen"))};
	const bool bSafetyOpeningPassed =
		ReleaseValidationSafetyOpeningCues == ExpectedSafetyOpeningCues;
	const int32 TankLidCountAfterEnding =
		CountOwnedChapterThreeActions(EIGChapterThreeAction::OpenTank);
	const int32 OwnedActionCountAfterEnding =
		CountOwnedChapterThreeActions();
	const bool bCommonPropPassed =
		IsValid(ReleaseValidationTankLidBeforeEnding)
		&& TankLidAction == ReleaseValidationTankLidBeforeEnding
		&& ReleaseValidationTankLidCountBeforeEnding == 1
		&& TankLidCountAfterEnding == 1
		&& OwnedActionCountAfterEnding
			== ReleaseValidationOwnedActionCountBeforeEnding
		&& ValidateCommonDiscoveryWorldState()
		&& bSafetyOpeningPassed;
	const bool bPassed =
		RebirthState
		&& Snapshot.ChapterThree.EndingChoice
			== ExpectedEnding
		&& Snapshot.ChapterThree.bCommonDiscoveryCommitted
		&& Snapshot.ResolvedPuzzles.Contains(EndingPuzzle)
		&& Snapshot.PlayedOneShotBeats.Contains(StrongCue)
		&& RebirthState->HasTruth(FGameplayTag::RequestGameplayTag(
			FName(TEXT("Truth.Found0731")),
			false))
		&& RebirthState->CanConverge(
			EIGRebirthConvergencePoint::C6AfterDiscovery)
		&& bCommonPropPassed;
	if (!bPassed)
	{
		if (!bCommonPropPassed)
		{
			UE_LOG(
				LogIndieGame,
				Error,
				TEXT(
					"REBIRTH_SPIKE FAIL s4_common_prop "
					"same_lid=%d lid_before=%d lid_after=%d "
					"actions_before=%d actions_after=%d world_state=%d "
					"safety_order=%d safety_cues=%d"),
				TankLidAction == ReleaseValidationTankLidBeforeEnding
					? 1
					: 0,
				ReleaseValidationTankLidCountBeforeEnding,
				TankLidCountAfterEnding,
				ReleaseValidationOwnedActionCountBeforeEnding,
				OwnedActionCountAfterEnding,
				ValidateCommonDiscoveryWorldState() ? 1 : 0,
				bSafetyOpeningPassed ? 1 : 0,
				ReleaseValidationSafetyOpeningCues.Num());
		}
		FailRebirthReleaseValidation(
			bEndingA ? TEXT("ending_a") : TEXT("ending_b"));
		return;
	}

	const TCHAR* EndingLabel = bEndingA ? TEXT("A") : TEXT("B");
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"REBIRTH_SPIKE PASS s4_common_prop "
			"ending=%s duplicates=0 actual_state=1 safety_cues=5"),
		EndingLabel);
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT(
			"REBIRTH_RELEASE PASS ending=%s "
			"common_discovery=1 strong_cue=1 c6=1"),
		EndingLabel);
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("REBIRTH_RELEASE PASS complete ending=%s"),
		EndingLabel);
	if (!WriteRebirthReleaseValidationResult(true, TEXT("complete")))
	{
		FailRebirthReleaseValidation(TEXT("result_evidence_write"));
		return;
	}
	bReleaseValidationInProgress = false;
	FPlatformMisc::RequestExitWithStatus(
		false,
		0,
		TEXT("REBIRTH release validation completed"));
}

int32 AIGThirdMorningDirector::CountOwnedChapterThreeActions(
	const EIGChapterThreeAction FilterAction) const
{
	TArray<AActor*> ActionActors;
	UGameplayStatics::GetAllActorsOfClass(
		this,
		AIGChapterThreeAction::StaticClass(),
		ActionActors);

	int32 Count = 0;
	for (const AActor* Actor : ActionActors)
	{
		const AIGChapterThreeAction* ActionActor =
			Cast<AIGChapterThreeAction>(Actor);
		if (ActionActor
			&& ActionActor->GetOwner() == this
			&& (FilterAction == EIGChapterThreeAction::None
				|| ActionActor->GetAction() == FilterAction))
		{
			++Count;
		}
	}
	return Count;
}

void AIGThirdMorningDirector::FailRebirthReleaseValidation(
	const TCHAR* Reason)
{
	UE_LOG(
		LogIndieGame,
		Error,
		TEXT("REBIRTH_RELEASE FAIL reason=%s"),
		Reason ? Reason : TEXT("unknown"));
	WriteRebirthReleaseValidationResult(false, Reason);
	if (!ReleaseValidationSlotName.IsEmpty())
	{
		UGameplayStatics::DeleteGameInSlot(ReleaseValidationSlotName, 0);
	}
	bReleaseValidationInProgress = false;
	FPlatformMisc::RequestExitWithStatus(
		false,
		1,
		TEXT("REBIRTH release validation failed"));
}

bool AIGThirdMorningDirector::WriteRebirthReleaseValidationResult(
	const bool bPassed,
	const TCHAR* Reason) const
{
	FString ResultPath;
	if (!FParse::Value(
			FCommandLine::Get(),
			TEXT("IGRebirthResultPath="),
			ResultPath))
	{
		return true;
	}

	ResultPath.TrimQuotesInline();
	if (ResultPath.IsEmpty())
	{
		return false;
	}

	const TCHAR* EndingLabel = bReleaseValidationEndingA
		? TEXT("A")
		: TEXT("B");
	const FString Result = bPassed
		? FString::Printf(
			TEXT(
				"REBIRTH_PACKAGED_RUNTIME PASS contract=1 ending=%s "
				"common_discovery=1 strong_cue=1 c6=1\n"),
			EndingLabel)
		: FString::Printf(
			TEXT(
				"REBIRTH_PACKAGED_RUNTIME FAIL contract=1 ending=%s "
				"reason=%s\n"),
			EndingLabel,
			Reason ? Reason : TEXT("unknown"));
	return FFileHelper::SaveStringToFile(
		Result,
		*FPaths::ConvertRelativePathToFull(ResultPath),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
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

bool AIGThirdMorningDirector::WriteCaptureReceipt(const FString& Receipt) const
{
	FString ResultPath;
	if (!FParse::Value(
			FCommandLine::Get(),
			TEXT("IGCaptureResultPath="),
			ResultPath))
	{
		return true;
	}
	ResultPath.TrimQuotesInline();
	if (ResultPath.IsEmpty())
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(
		Receipt + LINE_TERMINATOR,
		*FPaths::ConvertRelativePathToFull(ResultPath),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool AIGThirdMorningDirector::ReadLensDropletCaptureSample(
	FVector2D& OutPosition,
	FVector2D& OutSize,
	FVector2D& OutCanvasSize,
	float& OutAlpha,
	bool& bOutReducedMotion) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World
		? World->GetFirstPlayerController()
		: nullptr;
	const AIGHorrorHUD* HUD = PlayerController
		? Cast<AIGHorrorHUD>(PlayerController->GetHUD())
		: nullptr;
	double RenderTime = -1.0;
	if (!HUD || !HUD->GetLensDropletRenderSample(
		OutPosition,
		OutSize,
		OutCanvasSize,
		OutAlpha,
		bOutReducedMotion,
		RenderTime))
	{
		return false;
	}
	return World->GetTimeSeconds() - RenderTime <= 0.25;
}

void AIGThirdMorningDirector::StartLensDropletCaptureSequence()
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
			HUD->bShowHUD = true;
		}
		if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
		{
			Camera->StopCameraFade();
		}
	}

	StopAllChapterAudio();
	SetPhase(EIGThirdMorningPhase::Awakening);
	PlaceCaptureCamera(
		FVector(35.0f, -55.0f, IGThirdMorning::StandingCapsuleCenter),
		FVector(-208.0f, 115.0f, 132.0f));
	bLensDropletCaptureEarlyValid = false;
	bLensDropletCaptureLateValid = false;
	LensDropletCaptureFinishRetries = 0;
	ShowOpeningLensDroplet();
	GetWorldTimerManager().SetTimer(
		CaptureTimer,
		this,
		&ThisClass::CaptureLensDropletEarlyFrame,
		IGThirdMorning::LensDropletCaptureEarlyDelaySeconds,
		false);
}

void AIGThirdMorningDirector::CaptureLensDropletEarlyFrame()
{
	FVector2D SampleSize = FVector2D::ZeroVector;
	FVector2D CanvasSize = FVector2D::ZeroVector;
	bool bReducedMotion = false;
	bLensDropletCaptureEarlyValid = ReadLensDropletCaptureSample(
		LensDropletCaptureEarlyPosition,
		SampleSize,
		CanvasSize,
		LensDropletCaptureEarlyAlpha,
		bReducedMotion);
	LensDropletCaptureSize = SampleSize;
	LensDropletCaptureCanvasSize = CanvasSize;
	bLensDropletCaptureReducedMotion = bReducedMotion;
	const TCHAR* Mode = bReducedMotion ? TEXT("reduced") : TEXT("default");
	LensDropletCaptureEarlyPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(
			FPaths::ProjectDir(),
			FString::Printf(
				TEXT("Docs/Media/ch03-lens-droplet-%s-early.png"),
				Mode)));
	FScreenshotRequest::RequestScreenshot(
		LensDropletCaptureEarlyPath,
		true,
		false);
	GetWorldTimerManager().SetTimer(
		CaptureTimer,
		this,
		&ThisClass::CaptureLensDropletLateFrame,
		IGThirdMorning::LensDropletCaptureLateDelaySeconds,
		false);
}

void AIGThirdMorningDirector::CaptureLensDropletLateFrame()
{
	FVector2D SampleSize = FVector2D::ZeroVector;
	FVector2D CanvasSize = FVector2D::ZeroVector;
	bool bReducedMotion = false;
	bLensDropletCaptureLateValid = ReadLensDropletCaptureSample(
		LensDropletCaptureLatePosition,
		SampleSize,
		CanvasSize,
		LensDropletCaptureLateAlpha,
		bReducedMotion);
	const bool bSampleModeMatches =
		bReducedMotion == bLensDropletCaptureReducedMotion;
	const bool bSampleDimensionsMatch =
		SampleSize.Equals(LensDropletCaptureSize, 0.5f)
		&& CanvasSize.Equals(LensDropletCaptureCanvasSize, 0.5f);
	bLensDropletCaptureLateValid =
		bLensDropletCaptureLateValid
		&& bSampleModeMatches
		&& bSampleDimensionsMatch;
	const TCHAR* Mode = bLensDropletCaptureReducedMotion
		? TEXT("reduced")
		: TEXT("default");
	LensDropletCaptureLatePath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(
			FPaths::ProjectDir(),
			FString::Printf(
				TEXT("Docs/Media/ch03-lens-droplet-%s-late.png"),
				Mode)));
	FScreenshotRequest::RequestScreenshot(
		LensDropletCaptureLatePath,
		true,
		false);
	GetWorldTimerManager().SetTimer(
		CaptureTimer,
		this,
		&ThisClass::FinishLensDropletCaptureSequence,
		IGThirdMorning::LensDropletCaptureFinishRetrySeconds,
		false);
}

void AIGThirdMorningDirector::FinishLensDropletCaptureSequence()
{
	const bool bScreenshotsReady =
		FPaths::FileExists(LensDropletCaptureEarlyPath)
		&& FPaths::FileExists(LensDropletCaptureLatePath);
	if (!bScreenshotsReady
		&& LensDropletCaptureFinishRetries
			< IGThirdMorning::LensDropletCaptureMaximumFinishRetries)
	{
		++LensDropletCaptureFinishRetries;
		GetWorldTimerManager().SetTimer(
			CaptureTimer,
			this,
			&ThisClass::FinishLensDropletCaptureSequence,
			IGThirdMorning::LensDropletCaptureFinishRetrySeconds,
			false);
		return;
	}

	const float TravelX = FMath::Abs(
		LensDropletCaptureLatePosition.X - LensDropletCaptureEarlyPosition.X);
	const float TravelY = FMath::Abs(
		LensDropletCaptureLatePosition.Y - LensDropletCaptureEarlyPosition.Y);
	const float MinimumAlpha = FMath::Min(
		LensDropletCaptureEarlyAlpha,
		LensDropletCaptureLateAlpha);
	const FVector2D DropletMaximum =
		LensDropletCaptureLatePosition + LensDropletCaptureSize;
	const bool bHudSafePlacement =
		LensDropletCaptureLatePosition.X
			>= LensDropletCaptureCanvasSize.X * 0.60f
		&& DropletMaximum.X <= LensDropletCaptureCanvasSize.X * 0.90f
		&& LensDropletCaptureLatePosition.Y
			>= LensDropletCaptureCanvasSize.Y * 0.08f
		&& DropletMaximum.Y <= LensDropletCaptureCanvasSize.Y * 0.43f;
	const bool bMotionValid = bLensDropletCaptureReducedMotion
		? TravelY <= IGThirdMorning::LensDropletCaptureMaximumReducedTravelPixels
		: TravelY >= IGThirdMorning::LensDropletCaptureMinimumTravelPixels;
	const bool bPassed =
		bScreenshotsReady
		&& bLensDropletCaptureEarlyValid
		&& bLensDropletCaptureLateValid
		&& TravelX <= 0.5f
		&& MinimumAlpha >= IGThirdMorning::LensDropletCaptureMinimumAlpha
		&& bHudSafePlacement
		&& bMotionValid;
	const TCHAR* Mode = bLensDropletCaptureReducedMotion
		? TEXT("reduced")
		: TEXT("default");
	const FString Receipt = FString::Printf(
		TEXT(
			"REBIRTH_CH03_LENS_CAPTURE %s mode=%s reduced_motion=%d "
			"stills=2 early_x=%.2f early_y=%.2f late_x=%.2f late_y=%.2f "
			"travel_y=%.2f alpha_min=%.3f canvas=%.0fx%.0f hud_safe=%d"),
		bPassed ? TEXT("PASS") : TEXT("FAIL"),
		Mode,
		bLensDropletCaptureReducedMotion ? 1 : 0,
		LensDropletCaptureEarlyPosition.X,
		LensDropletCaptureEarlyPosition.Y,
		LensDropletCaptureLatePosition.X,
		LensDropletCaptureLatePosition.Y,
		TravelY,
		MinimumAlpha,
		LensDropletCaptureCanvasSize.X,
		LensDropletCaptureCanvasSize.Y,
		bHudSafePlacement ? 1 : 0);
	const bool bReceiptWritten = WriteCaptureReceipt(Receipt);
	UE_LOG(LogIndieGame, Display, TEXT("%s"), *Receipt);
	FPlatformMisc::RequestExitWithStatus(
		false,
		bPassed && bReceiptWritten ? 0 : 1,
		bPassed && bReceiptWritten
			? TEXT("CH03 lens droplet capture completed")
			: TEXT("CH03 lens droplet capture failed"));
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
		// Keep the repaired fifth-floor landing in the deterministic visual
		// regression set.  This frame must read as one continuous, enclosed
		// route: upper stair -> 160 cm doorway -> four roof risers -> fire door.
		PlaceCaptureCamera(
			FVector(
				1115,
				-420,
				180.0f + IGThirdMorning::StandingCapsuleCenter),
			IGThirdMorning::RoofDoorClosedLocation);
		BaseName = TEXT("ch03-fifth-floor-doorway");
		break;
	case 3:
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
	case 4:
		HandleAction(EIGChapterThreeAction::EvidenceGlasses, GlassesAction);
		HandleAction(EIGChapterThreeAction::OpenTank, TankLidAction);
		PlaceCaptureCamera(
			// A player-height lean over the 104 cm hatch shows the complete curled
			// chain without moving the camera inside the tank wall.
			FVector(2315, -300, 725),
			IGThirdMorning::TankCenter
				+ IGThirdMorning::AuthoredTankBodyPlacement
				+ FVector(0, 0, IGThirdMorning::TankBodyPlacementAdjustmentZ));
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

	const TWeakObjectPtr<AIGThirdMorningDirector> WeakThis(this);
	FTimerDelegate CaptureDelegate;
	CaptureDelegate.BindLambda([WeakThis, ScreenshotPath]()
	{
		AIGThirdMorningDirector* Director = WeakThis.Get();
		if (!Director)
		{
			return;
		}
		FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
		UE_LOG(LogIndieGame, Display, TEXT("CH03 capture requested: %s"), *ScreenshotPath);
		Director->GetWorldTimerManager().SetTimer(
			Director->CaptureTimer,
			Director,
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
		&& EvidenceActions.Num() == 11
		&& bUsesAuthoredTankBody;
	if (!bRebirthGreyboxPresent)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"CH03 capture smoke route is missing P3/P5 actors or the "
				"authored tank body."));
		FPlatformMisc::RequestExitWithStatus(
			false,
			1,
			TEXT("CH03 capture validation failed"));
		return;
	}
	const FString Receipt = FString::Printf(
		TEXT(
			"REBIRTH_CH03_CAPTURE PASS loops=%d roof=%d tank=%d "
			"rebirth_greybox=%d authored_body=%d visual_stills=5"),
		StairLoopCount,
		Phase >= EIGThirdMorningPhase::Roof ? 1 : 0,
		bTankOpened ? 1 : 0,
		bRebirthGreyboxPresent ? 1 : 0,
		bUsesAuthoredTankBody ? 1 : 0);
	const bool bReceiptWritten = WriteCaptureReceipt(Receipt);
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("CH03_CAPTURE COMPLETE %s"),
		*Receipt);
	FPlatformMisc::RequestExitWithStatus(
		false,
		bReceiptWritten ? 0 : 1,
		bReceiptWritten
			? TEXT("CH03 capture validation completed")
			: TEXT("CH03 capture receipt failed"));
}
