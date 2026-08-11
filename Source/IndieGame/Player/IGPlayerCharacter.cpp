#include "Player/IGPlayerCharacter.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EngineUtils.h"
#include "Entity/IGMissingFloorNightThreeDirector.h"
#include "Entity/IGMissingFloorFifthDawnDirector.h"
#include "Entity/IGNoiseSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "Interaction/IGPickupItem.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGStoryHelpers.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "Interaction/IGReadableNote.h"
#include "Player/IGFlashlightComponent.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGInteractionComponent.h"
#include "Player/IGStressComponent.h"
#include "Sequence/IGWakeUpDirector.h"
#include "Save/IGSaveSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace IGPlayerNoise
{
	/**
	 * The project's ordinary walk speed. Footstep loudness is measured against
	 * this fixed reference rather than the movement component's current cap, so
	 * a chapter that slows the player also makes them quieter — which is what
	 * moving carefully should mean.
	 */
	constexpr float ReferenceWalkSpeed = 300.0f;
	constexpr float SprintSpeed = 460.0f;
	constexpr float CrouchSpeed = 160.0f;
	constexpr float ListenSpeed = 80.0f;
	constexpr float SprintBreathThresholdSeconds = 3.5f;
	constexpr float ListenCommitSeconds = 0.8f;
	constexpr float MaximumBreathHoldSeconds = 4.0f;
	constexpr float WalkAcceleration = 1200.0f;
	constexpr float WalkBraking = 1200.0f;
	constexpr float CrouchAcceleration = 900.0f;
	constexpr float CrouchBraking = 1500.0f;
	constexpr float SprintAcceleration = 1400.0f;
	constexpr float SprintBraking = 900.0f;
	constexpr float CrouchTransitionSeconds = 0.35f;
	constexpr float CrouchTransitionSpeedScale = 0.5f;
	constexpr float KnockInputLockSeconds = 0.9f;
	constexpr float KnockSequenceResetSeconds = 1.8f;
	constexpr float KnockCameraKickDegrees = 0.4f;
	constexpr float KnockCameraReturnSeconds = 0.18f;
	/** Quietest and loudest footfall reported to the noise bus (§5.1). */
	constexpr float MinimumFootstepLoudness = 0.06f;
	constexpr float MaximumFootstepLoudness = 0.18f;
	constexpr float CrouchFootstepLoudness = 0.05f;
	constexpr float SprintFootstepLoudness = 0.50f;
	constexpr float ExhaustedSprintFootstepLoudness = 0.70f;
}

namespace IGPlayerOutfit
{
	constexpr float PresentationDurationSeconds = 1.2f;
	constexpr float PresentationPeakAlpha = 0.42f;
	const FVector RestLocation(34.0f, -18.0f, -27.0f);
	const FVector StartLocation(22.0f, -18.0f, -49.0f);
	const FVector PeakLocation(38.0f, -16.0f, -18.0f);
	const FRotator RestRotation(72.0f, -8.0f, -6.0f);
	const FRotator StartRotation(78.0f, -5.0f, -12.0f);
	const FRotator PeakRotation(48.0f, -16.0f, 12.0f);
}

AIGPlayerCharacter::AIGPlayerCharacter()
{
	// Tick only powers optional camera motion; it stays off until enabled.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Interior openings are 84-88 cm wide.  A 42 cm radius capsule had zero
	// clearance, and even 36 cm left too little tolerance beside a moving
	// Korean steel door.  UE first-person characters commonly use a ~34 cm
	// radius; keep the standing height while giving door jambs realistic
	// shoulder clearance instead of letting the capsule snag on millimetres.
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->bOrientRotationToMovement = false;
	MovementComponent->bUseControllerDesiredRotation = true;
	MovementComponent->MaxWalkSpeed = 300.0f;
	MovementComponent->MaxWalkSpeedCrouched = IGPlayerNoise::CrouchSpeed;
	MovementComponent->MaxAcceleration = IGPlayerNoise::WalkAcceleration;
	MovementComponent->BrakingDecelerationWalking = IGPlayerNoise::WalkBraking;
	MovementComponent->NavAgentProps.bCanCrouch = true;
	MovementComponent->SetCrouchedHalfHeight(48.0f);
	// CharacterMovement's stock 750,000 push force is intended for heavy
	// physics gameplay. Against a 200 g slipper or an empty bottle it launches
	// the prop down the corridor from a light brush. Scale the impulse by mass
	// and cap the continuous touch force so small dressing can still be nudged
	// without exploding or spinning indefinitely.
	MovementComponent->bEnablePhysicsInteraction = true;
	MovementComponent->bPushForceScaledToMass = true;
	MovementComponent->bScalePushForceToVelocity = true;
	MovementComponent->InitialPushForceFactor = 250.0f;
	MovementComponent->PushForceFactor = 450.0f;
	MovementComponent->TouchForceFactor = 0.6f;
	MovementComponent->MinTouchForce = -1.0f;
	MovementComponent->MaxTouchForce = 120.0f;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(CameraBaseLocation);
	FirstPersonCamera->bUsePawnControlRotation = true;
	// ~standard 35mm feel; the default 90 reads wide-angle and warps depth.
	FirstPersonCamera->SetFieldOfView(78.0f);

	// REBIRTH uses a static first-person proxy instead of introducing a
	// skeletal-arms pipeline. The three dark stitches are the unique repair
	// repeated on the 403 figure and the tank clothing.
	OutfitSleeveProxy =
		CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OutfitSleeveProxy"));
	OutfitSleeveProxy->SetupAttachment(FirstPersonCamera);
	OutfitSleeveProxy->SetCollisionProfileName(
		UCollisionProfile::NoCollision_ProfileName);
	OutfitSleeveProxy->SetGenerateOverlapEvents(false);
	OutfitSleeveProxy->SetCanEverAffectNavigation(false);
	OutfitSleeveProxy->SetCastShadow(false);
	OutfitSleeveProxy->SetRelativeLocation(IGPlayerOutfit::RestLocation);
	OutfitSleeveProxy->SetRelativeRotation(IGPlayerOutfit::RestRotation);
	OutfitSleeveProxy->SetRelativeScale3D(FVector(0.055f, 0.055f, 0.28f));
	OutfitSleeveProxy->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> AuthoredSleeveMeshFinder(
		TEXT("/Game/Meshes/SM_FirstPersonHoodieSleeve.SM_FirstPersonHoodieSleeve"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WetSleeveMaterialFinder(
		TEXT("/Game/Prototype/Materials/M_WetHoodieUV.M_WetHoodieUV"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SleeveFallbackFinder(
		TEXT("/Game/Prototype/Materials/M_BeddingUV.M_BeddingUV"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StitchMaterialFinder(
		TEXT("/Game/Prototype/Materials/M_PlasticDark.M_PlasticDark"));
	const bool bHasAuthoredSleeveMesh = AuthoredSleeveMeshFinder.Succeeded();
	if (bHasAuthoredSleeveMesh)
	{
		OutfitSleeveProxy->SetStaticMesh(AuthoredSleeveMeshFinder.Object);
		OutfitSleeveProxy->SetRelativeScale3D(FVector::OneVector);
	}
	else if (CylinderMeshFinder.Succeeded())
	{
		OutfitSleeveProxy->SetStaticMesh(CylinderMeshFinder.Object);
	}
	if (WetSleeveMaterialFinder.Succeeded())
	{
		OutfitSleeveProxy->SetMaterial(0, WetSleeveMaterialFinder.Object);
	}
	else if (SleeveFallbackFinder.Succeeded())
	{
		OutfitSleeveProxy->SetMaterial(0, SleeveFallbackFinder.Object);
	}

	for (int32 StitchIndex = 0; StitchIndex < 3; ++StitchIndex)
	{
		UStaticMeshComponent* Stitch = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("OutfitRepairStitch%d"), StitchIndex + 1));
		// The repair belongs to the cloth, not the camera. Parenting all three
		// stitches to the sleeve keeps their spacing and orientation exact
		// throughout the first-exit presentation and ordinary camera motion.
		Stitch->SetupAttachment(OutfitSleeveProxy);
		Stitch->SetCollisionProfileName(
			UCollisionProfile::NoCollision_ProfileName);
		Stitch->SetGenerateOverlapEvents(false);
		Stitch->SetCanEverAffectNavigation(false);
		Stitch->SetCastShadow(false);
		if (bHasAuthoredSleeveMesh)
		{
			// Three 12 mm black bar stitches cross the inner seam about 6 cm
			// above the cuff. Real-centimetre sleeve geometry uses unit scale.
			Stitch->SetRelativeLocation(
				FVector(1.0f, -5.75f, -7.0f + StitchIndex * 1.4f));
			Stitch->SetRelativeRotation(FRotator::ZeroRotator);
			Stitch->SetRelativeScale3D(FVector(0.012f, 0.003f, 0.0025f));
		}
		else
		{
			// Preserve the release-safe cylinder proxy and its parent-scaled
			// stitch placement until the authored mesh has been baked.
			Stitch->SetRelativeLocation(
				FVector(-10.0f, -52.0f, -12.0f + StitchIndex * 12.0f));
			Stitch->SetRelativeRotation(FRotator(0.0f, 0.0f, 18.0f));
			Stitch->SetRelativeScale3D(FVector(0.16f, 0.045f, 0.012f));
		}
		Stitch->SetHiddenInGame(true);
		if (CubeMeshFinder.Succeeded())
		{
			Stitch->SetStaticMesh(CubeMeshFinder.Object);
		}
		if (StitchMaterialFinder.Succeeded())
		{
			Stitch->SetMaterial(0, StitchMaterialFinder.Object);
		}
		OutfitStitchProxies.Add(Stitch);
	}

	InteractionComponent = CreateDefaultSubobject<UIGInteractionComponent>(TEXT("InteractionComponent"));

	// The torch hangs off the camera so it aims where you look, but it has
	// its own sway on top of that — see UIGFlashlightComponent.
	Flashlight = CreateDefaultSubobject<UIGFlashlightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(FirstPersonCamera);
	Flashlight->SetRelativeLocation(FVector(12.0f, 14.0f, -12.0f));

	StressComponent = CreateDefaultSubobject<UIGStressComponent>(TEXT("Stress"));
}

void AIGPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		AccessibilitySubsystem =
			GameInstance->GetSubsystem<UIGAccessibilitySubsystem>();
	}

	// A short tick whenever something new becomes usable is the cheapest way
	// to make aiming at objects feel responsive rather than guessy.
	if (InteractionComponent)
	{
		InteractionComponent->OnFocusChanged.AddUniqueDynamic(
			this, &AIGPlayerCharacter::HandleFocusChanged);
	}

	SetActorTickEnabled(true);
}

void AIGPlayerCharacter::OnStartCrouch(
	const float HalfHeightAdjust,
	const float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CrouchTransitionRemaining = IGPlayerNoise::CrouchTransitionSeconds;
	CrouchCameraCompensationStart =
		AppliedCrouchCameraCompensation + ScaledHalfHeightAdjust;
	CrouchCameraCompensation = CrouchCameraCompensationStart;
	if (FirstPersonCamera)
	{
		FVector CameraLocation = FirstPersonCamera->GetRelativeLocation();
		CameraLocation.Z += CrouchCameraCompensation
			- AppliedCrouchCameraCompensation;
		FirstPersonCamera->SetRelativeLocation(CameraLocation);
		AppliedCrouchCameraCompensation = CrouchCameraCompensation;
	}
	ApplyContextMovementSpeed();
}

void AIGPlayerCharacter::OnEndCrouch(
	const float HalfHeightAdjust,
	const float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CrouchTransitionRemaining = IGPlayerNoise::CrouchTransitionSeconds;
	CrouchCameraCompensationStart =
		AppliedCrouchCameraCompensation - ScaledHalfHeightAdjust;
	CrouchCameraCompensation = CrouchCameraCompensationStart;
	if (FirstPersonCamera)
	{
		FVector CameraLocation = FirstPersonCamera->GetRelativeLocation();
		CameraLocation.Z += CrouchCameraCompensation
			- AppliedCrouchCameraCompensation;
		FirstPersonCamera->SetRelativeLocation(CameraLocation);
		AppliedCrouchCameraCompensation = CrouchCameraCompensation;
	}
	ApplyContextMovementSpeed();
}

void AIGPlayerCharacter::SetRebirthOutfitEquipped(
	const bool bEquipped,
	const bool bPlayPresentation)
{
	bRebirthOutfitEquipped = bEquipped;
	if (OutfitSleeveProxy)
	{
		if (!bEquipped)
		{
			OutfitSleeveProxy->SetRelativeLocation(
				IGPlayerOutfit::RestLocation);
			OutfitSleeveProxy->SetRelativeRotation(
				IGPlayerOutfit::RestRotation);
		}
		OutfitSleeveProxy->SetHiddenInGame(!bEquipped);
	}
	for (UStaticMeshComponent* Stitch : OutfitStitchProxies)
	{
		if (Stitch)
		{
			Stitch->SetHiddenInGame(!bEquipped);
		}
	}

	if (!bEquipped)
	{
		bOutfitPresentationActive = false;
		OutfitPresentationElapsed = 0.0f;
		return;
	}

	if (bPlayPresentation && OutfitSleeveProxy)
	{
		bOutfitPresentationActive = true;
		OutfitPresentationElapsed = 0.0f;
		OutfitSleeveProxy->SetRelativeLocation(
			IGPlayerOutfit::StartLocation);
		OutfitSleeveProxy->SetRelativeRotation(
			IGPlayerOutfit::StartRotation);
		// No input or view lock: this camera-child prop moves while the player
		// remains in full control.
		SetActorTickEnabled(true);
	}
}

bool AIGPlayerCharacter::ValidateRebirthOutfitProxy(
	int32& OutStitchCount) const
{
	OutStitchCount = 0;
	if (!bRebirthOutfitEquipped
		|| !IsValid(OutfitSleeveProxy)
		|| !OutfitSleeveProxy->GetStaticMesh())
	{
		return false;
	}

	for (const UStaticMeshComponent* Stitch : OutfitStitchProxies)
	{
		if (!IsValid(Stitch)
			|| !Stitch->GetStaticMesh()
			|| Stitch->GetAttachParent() != OutfitSleeveProxy.Get())
		{
			return false;
		}
		++OutStitchCount;
	}
	return OutStitchCount == 3;
}

void AIGPlayerCharacter::HandleFocusChanged(AActor* PreviousActor, AActor* NewActor)
{
	if (!IsValid(NewActor) || !FirstPersonCamera)
	{
		return;
	}

	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateFootstep(this, 1.95f, 0.10f),
		FirstPersonCamera->GetComponentLocation(),
		0.35f,
		1.0f,
		60.0f,
		260.0f);
}

void AIGPlayerCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Feed the fear model how dark it is here. The probe traces, so it runs
	// a few times a second and the result is smoothed between samples.
	DarknessSampleTimer -= DeltaSeconds;
	if (DarknessSampleTimer <= 0.0f)
	{
		DarknessSampleTimer = 0.25f;
		CachedDarkness = FMath::FInterpTo(CachedDarkness, SampleAmbientDarkness(), 1.0f, 0.55f);
		if (StressComponent)
		{
			StressComponent->SetDarkness(CachedDarkness);
		}
	}

	UpdateContextualActions(DeltaSeconds);
	UpdateCrouchTransition(DeltaSeconds);
	UpdateFootsteps(DeltaSeconds);
	UpdateCameraMotion(DeltaSeconds);
	UpdateCarriedItem(DeltaSeconds);
	UpdateOutfitPresentation(DeltaSeconds);
}

void AIGPlayerCharacter::UpdateOutfitPresentation(const float DeltaSeconds)
{
	if (!bOutfitPresentationActive || !OutfitSleeveProxy)
	{
		return;
	}

	OutfitPresentationElapsed = FMath::Min(
		OutfitPresentationElapsed + DeltaSeconds,
		IGPlayerOutfit::PresentationDurationSeconds);
	const float Alpha = OutfitPresentationElapsed
		/ IGPlayerOutfit::PresentationDurationSeconds;

	FVector Location;
	FRotator Rotation;
	if (Alpha < IGPlayerOutfit::PresentationPeakAlpha)
	{
		const float Phase = FMath::SmoothStep(
			0.0f,
			1.0f,
			Alpha / IGPlayerOutfit::PresentationPeakAlpha);
		Location = FMath::Lerp(
			IGPlayerOutfit::StartLocation,
			IGPlayerOutfit::PeakLocation,
			Phase);
		Rotation = FMath::Lerp(
			IGPlayerOutfit::StartRotation,
			IGPlayerOutfit::PeakRotation,
			Phase);
	}
	else
	{
		const float Phase = FMath::SmoothStep(
			0.0f,
			1.0f,
			(Alpha - IGPlayerOutfit::PresentationPeakAlpha)
				/ (1.0f - IGPlayerOutfit::PresentationPeakAlpha));
		Location = FMath::Lerp(
			IGPlayerOutfit::PeakLocation,
			IGPlayerOutfit::RestLocation,
			Phase);
		Rotation = FMath::Lerp(
			IGPlayerOutfit::PeakRotation,
			IGPlayerOutfit::RestRotation,
			Phase);
	}

	OutfitSleeveProxy->SetRelativeLocation(Location);
	OutfitSleeveProxy->SetRelativeRotation(Rotation);
	if (OutfitPresentationElapsed
		>= IGPlayerOutfit::PresentationDurationSeconds)
	{
		bOutfitPresentationActive = false;
		OutfitSleeveProxy->SetRelativeLocation(
			IGPlayerOutfit::RestLocation);
		OutfitSleeveProxy->SetRelativeRotation(
			IGPlayerOutfit::RestRotation);
	}
}

float AIGPlayerCharacter::SampleAmbientDarkness() const
{
	// There is no cheap way to read scene luminance from gameplay code, so
	// darkness is inferred from the lights that can actually reach us: any
	// point/spot light within range and not occluded counts, weighted by
	// inverse-square falloff. It is approximate, and it only has to be good
	// enough to tell "lit corridor" from "dead stairwell".
	const UWorld* World = GetWorld();
	if (!World || !FirstPersonCamera)
	{
		return 0.0f;
	}

	const FVector EyeLocation = FirstPersonCamera->GetComponentLocation();

	// A working torch is enough light to keep the dark at bay — but only
	// while it is genuinely lit. IsProvidingLight() is false through a
	// brown-out, so a dying cell stops shielding the player from the dark.
	if (Flashlight && Flashlight->IsProvidingLight())
	{
		return 0.0f;
	}

	float Illumination = 0.0f;
	for (TObjectIterator<UPointLightComponent> LightIterator; LightIterator; ++LightIterator)
	{
		const UPointLightComponent* Light = *LightIterator;
		if (!IsValid(Light) || Light->GetWorld() != World || !Light->IsVisible())
		{
			continue;
		}

		const FVector LightLocation = Light->GetComponentLocation();
		const float Distance = FVector::Dist(EyeLocation, LightLocation);
		if (Distance > Light->AttenuationRadius)
		{
			continue;
		}

		// Anything solid between us and the lamp contributes nothing.
		FHitResult Occlusion;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(IGDarknessProbe), false, this);
		if (World->LineTraceSingleByChannel(
				Occlusion, EyeLocation, LightLocation, ECC_Visibility, QueryParams))
		{
			continue;
		}

		const float Falloff = 1.0f - FMath::Clamp(Distance / Light->AttenuationRadius, 0.0f, 1.0f);
		Illumination += Light->Intensity * Falloff * Falloff / 1000.0f;
	}

	// 1.0 lit is roughly "one corridor fluorescent, two metres away".
	return 1.0f - FMath::Clamp(Illumination, 0.0f, 1.0f);
}

void AIGPlayerCharacter::ToggleFlashlight()
{
	if (!Flashlight || !Flashlight->IsAvailable())
	{
		return;
	}

	const bool bNowOn = Flashlight->Toggle();
	// The click is audible either way — a dead cell still clicks.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateScannerBeep(this),
		GetActorLocation(),
		0.18f,
		bNowOn ? 2.4f : 2.0f,
		60.0f,
		320.0f);
}

void AIGPlayerCharacter::UpdateCarriedItem(const float DeltaSeconds)
{
	AActor* Carried = CarriedActor.Get();
	if (!IsValid(Carried) || !Controller || DeltaSeconds <= 0.0f)
	{
		return;
	}

	// Held objects lag behind the view instead of being welded to it: the
	// faster the look, the further the item swings before settling back.
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator RotationDelta = (ControlRotation - PreviousControlRotation).GetNormalized();
	PreviousControlRotation = ControlRotation;

	CarrySwayOffset.Yaw = FMath::Clamp(
		CarrySwayOffset.Yaw - RotationDelta.Yaw * 0.55f, -14.0f, 14.0f);
	CarrySwayOffset.Pitch = FMath::Clamp(
		CarrySwayOffset.Pitch - RotationDelta.Pitch * 0.45f, -12.0f, 12.0f);
	CarrySwayOffset = FMath::RInterpTo(
		CarrySwayOffset, FRotator::ZeroRotator, DeltaSeconds, 6.0f);

	// A slow bob keeps the item alive in hand while standing still.
	const float BobPhase = BreathTime * 2.0f * UE_PI * 0.35f;
	const FVector CarryBob(
		0.0f,
		FMath::Sin(BobPhase) * 0.35f,
		FMath::Sin(BobPhase * 1.7f) * 0.5f);

	Carried->SetActorRelativeLocation(CarriedBaseLocation + CarryBob);
	Carried->SetActorRelativeRotation(CarriedBaseRotation + CarrySwayOffset);
}

void AIGPlayerCharacter::SetCameraMotionEnabled(const bool bEnabled)
{
	if (bCameraMotionEnabled == bEnabled)
	{
		return;
	}

	bCameraMotionEnabled = bEnabled;
	// Tick stays on regardless: the fear model samples darkness even while a
	// director owns the camera. UpdateCameraMotion gates itself on the flag.
	SetActorTickEnabled(true);

	if (!bEnabled && FirstPersonCamera)
	{
		FirstPersonCamera->SetRelativeLocation(
			CameraBaseLocation
				+ FVector(0.0f, 0.0f, CrouchCameraCompensation));
		AppliedCrouchCameraCompensation = CrouchCameraCompensation;
	}
}

void AIGPlayerCharacter::ApplyContextMovementSpeed()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		return;
	}

	const float TransitionScale = CrouchTransitionRemaining > 0.0f
		? IGPlayerNoise::CrouchTransitionSpeedScale
		: 1.0f;
	MovementComponent->MaxWalkSpeedCrouched =
		IGPlayerNoise::CrouchSpeed * TransitionScale;

	if (bListening)
	{
		MovementComponent->MaxWalkSpeed =
			IGPlayerNoise::ListenSpeed * TransitionScale;
		MovementComponent->MaxAcceleration = 800.0f;
		MovementComponent->BrakingDecelerationWalking = 1600.0f;
		return;
	}
	if (bSprinting && !bIsCrouched)
	{
		MovementComponent->MaxWalkSpeed =
			IGPlayerNoise::SprintSpeed * TransitionScale;
		MovementComponent->MaxAcceleration = IGPlayerNoise::SprintAcceleration;
		MovementComponent->BrakingDecelerationWalking = IGPlayerNoise::SprintBraking;
		return;
	}
	if (bIsCrouched)
	{
		MovementComponent->MaxWalkSpeed =
			IGPlayerNoise::ReferenceWalkSpeed * TransitionScale;
		MovementComponent->MaxAcceleration = IGPlayerNoise::CrouchAcceleration;
		MovementComponent->BrakingDecelerationWalking = IGPlayerNoise::CrouchBraking;
		return;
	}

	MovementComponent->MaxWalkSpeed =
		IGPlayerNoise::ReferenceWalkSpeed * TransitionScale;
	MovementComponent->MaxAcceleration = IGPlayerNoise::WalkAcceleration;
	MovementComponent->BrakingDecelerationWalking = IGPlayerNoise::WalkBraking;
}

void AIGPlayerCharacter::RefreshSprintState()
{
	bSprinting = bSprintInputHeld
		&& !bListening
		&& !bIsCrouched
		&& CrouchTransitionRemaining <= 0.0f;
	ApplyContextMovementSpeed();
}

void AIGPlayerCharacter::UpdateCrouchTransition(const float DeltaSeconds)
{
	if (CrouchTransitionRemaining <= 0.0f || DeltaSeconds <= 0.0f)
	{
		return;
	}

	CrouchTransitionRemaining = FMath::Max(
		0.0f,
		CrouchTransitionRemaining - DeltaSeconds);
	const float Alpha = CrouchTransitionRemaining
		/ IGPlayerNoise::CrouchTransitionSeconds;
	CrouchCameraCompensation = CrouchCameraCompensationStart * Alpha;
	if (CrouchTransitionRemaining <= 0.0f)
	{
		CrouchCameraCompensation = 0.0f;
		CrouchCameraCompensationStart = 0.0f;
		RefreshSprintState();
	}
}

void AIGPlayerCharacter::UpdateContextualActions(const float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const bool bActuallySprinting = bSprinting
		&& !bIsCrouched
		&& MovementComponent
		&& MovementComponent->IsMovingOnGround()
		&& GetVelocity().Size2D() > IGPlayerNoise::ReferenceWalkSpeed + 10.0f;
	if (bActuallySprinting)
	{
		SprintActiveSeconds += DeltaSeconds;
		SprintRecoverySeconds = 0.0f;
	}
	else
	{
		SprintRecoverySeconds += DeltaSeconds;
		if (SprintRecoverySeconds >= 2.0f)
		{
			SprintActiveSeconds = FMath::Max(
				0.0f,
				SprintActiveSeconds - DeltaSeconds * 1.75f);
		}
	}

	if (bListening)
	{
		AActor* FocusedActor = InteractionComponent
			? InteractionComponent->GetFocusedActor()
			: nullptr;
		AIGMissingFloorNightThreeDirector* ListeningDirector = nullptr;
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AIGMissingFloorNightThreeDirector> It(World); It; ++It)
			{
				if (It->IsPlayerListenTarget(FocusedActor))
				{
					ListeningDirector = *It;
					break;
				}
			}
		}
		if (!ListeningDirector)
		{
			EndListen();
		}
		else if (!bListenTriggered)
		{
			ListenHeldSeconds += DeltaSeconds;
			if (ListenHeldSeconds >= IGPlayerNoise::ListenCommitSeconds)
			{
				bListenTriggered = ListeningDirector->TryPlayerListen(
					FocusedActor,
					this);
			}
		}
	}

	if (bHoldingBreath)
	{
		BreathHeldSeconds += DeltaSeconds;
		if (BreathHeldSeconds >= IGPlayerNoise::MaximumBreathHoldSeconds)
		{
			FinishHoldBreath(true);
		}
	}
}

void AIGPlayerCharacter::UpdateFootsteps(const float DeltaSeconds)
{
	// Footsteps are the player's voice in a game that hunts by sound, so the
	// cadence must not depend on the camera-bob flag the way it used to: a
	// director that never enabled camera motion would leave the player silent
	// and the one upstairs deaf.
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const float GroundSpeed = GetVelocity().Size2D();
	if (!MovementComponent
		|| !MovementComponent->IsMovingOnGround()
		|| GroundSpeed <= 20.0f)
	{
		return;
	}

	TraveledDistanceAccum += GroundSpeed * DeltaSeconds;
	const int32 StepIndex = FMath::FloorToInt32(TraveledDistanceAccum / StepDistance);
	if (StepIndex == LastStepIndex)
	{
		return;
	}
	LastStepIndex = StepIndex;

	// Absolute speed, not speed normalized against MaxWalkSpeed: the flood
	// director lowers the cap, and a wader must not sound like a stroller
	// merely because the denominator moved with them.
	const float SpeedScale = FMath::Clamp(
		GroundSpeed / IGPlayerNoise::ReferenceWalkSpeed,
		0.0f,
		IGPlayerNoise::SprintSpeed / IGPlayerNoise::ReferenceWalkSpeed);
	PlayFootstep(SpeedScale);
}

void AIGPlayerCharacter::UpdateCameraMotion(const float DeltaSeconds)
{
	if (!bCameraMotionEnabled || !FirstPersonCamera || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const float GroundSpeed = GetVelocity().Size2D();
	const bool bWalking =
		MovementComponent && MovementComponent->IsMovingOnGround() && GroundSpeed > 20.0f;
	const float SpeedScale = FMath::Clamp(
		GroundSpeed / FMath::Max(MovementComponent ? MovementComponent->MaxWalkSpeed : 300.0f, 1.0f),
		0.0f,
		1.0f);
	const bool bReducedMotion = AccessibilitySubsystem
		&& AccessibilitySubsystem->IsReducedCameraMotionEnabled();

	BreathTime += DeltaSeconds;

	FVector TargetOffset = FVector::ZeroVector;
	if (bWalking)
	{
		if (!bReducedMotion)
		{
			// One full sine cycle spans two footsteps (left/right).
			const float StepPhase =
				(TraveledDistanceAccum / StepDistance) * UE_PI;
			TargetOffset.Z +=
				-FMath::Abs(FMath::Sin(StepPhase)) * BobAmplitude * SpeedScale;
			TargetOffset.Y += FMath::Sin(StepPhase) * 0.8f * SpeedScale;
		}
	}

	if (!bReducedMotion && !bHoldingBreath)
	{
		// Slow breathing sway; more noticeable while standing still. The rate is
		// the fear model's, so the chest visibly speeds up before the player has
		// worked out why they are frightened.
		const float BreathScale = FMath::Lerp(1.0f, 0.35f, SpeedScale);
		const float BreathsPerMinute =
			StressComponent ? StressComponent->GetBreathsPerMinute() : 13.0f;
		const float BreathHz = BreathsPerMinute / 60.0f;
		const float BreathDepth = StressComponent
			? FMath::Lerp(0.55f, 1.35f, StressComponent->GetStress())
			: 0.55f;
		TargetOffset.Z +=
			FMath::Sin(BreathTime * 2.0f * UE_PI * BreathHz)
			* BreathDepth
			* BreathScale;
	}

	// Pressing Interact nudges the head forward and down, then springs back.
	if (InteractPunch > KINDA_SMALL_NUMBER)
	{
		if (!bReducedMotion)
		{
			TargetOffset.X += InteractPunch * 2.4f;
			TargetOffset.Z -= InteractPunch * 1.6f;
		}
	}
	InteractPunch = FMath::FInterpTo(InteractPunch, 0.0f, DeltaSeconds, 7.0f);

	FVector CameraLocationWithoutCrouch = FirstPersonCamera->GetRelativeLocation();
	CameraLocationWithoutCrouch.Z -= AppliedCrouchCameraCompensation;
	FVector SmoothedLocation = FMath::VInterpTo(
		CameraLocationWithoutCrouch,
		CameraBaseLocation + TargetOffset,
		DeltaSeconds,
		10.0f);
	SmoothedLocation.Z += CrouchCameraCompensation;
	FirstPersonCamera->SetRelativeLocation(SmoothedLocation);
	AppliedCrouchCameraCompensation = CrouchCameraCompensation;

	// Fear tremor rides on the camera's own rotation rather than the control
	// rotation, so it shakes the view without fighting the player's aim.
	FRotator CameraRotation = FRotator::ZeroRotator;
	if (!bReducedMotion && StressComponent)
	{
		CameraRotation = StressComponent->GetTremor();
	}
	if (!bReducedMotion && KnockCameraKick > KINDA_SMALL_NUMBER)
	{
		CameraRotation.Pitch -= KnockCameraKick;
	}
	KnockCameraKick = FMath::FInterpConstantTo(
		KnockCameraKick,
		0.0f,
		DeltaSeconds,
		IGPlayerNoise::KnockCameraKickDegrees
			/ IGPlayerNoise::KnockCameraReturnSeconds);
	if (bReducedMotion)
	{
		FirstPersonCamera->SetRelativeRotation(FRotator::ZeroRotator);
	}
	else
	{
		FirstPersonCamera->SetRelativeRotation(CameraRotation);
	}
}

void AIGPlayerCharacter::PlayFootstep(const float SpeedScale)
{
	// Deterministic per-step variation keeps the cadence from sounding looped.
	const uint32 StepHash = static_cast<uint32>(LastStepIndex) * 2654435761u;
	const float PitchVariation = 0.90f + 0.18f * ((StepHash >> 8) & 0xFF) / 255.0f;
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateFootstep(this, PitchVariation, 1.0f),
		GetActorLocation() - FVector(0.0f, 0.0f, 80.0f),
		FootstepVolume * (0.55f + 0.45f * SpeedScale),
		1.0f,
		120.0f,
		700.0f);

	// Every footfall is also a report to the building's ear. Crouching and
	// sprinting are authored bands, not a rescale of ordinary walking: this is
	// the resource decision the player makes before the pursuer hears it.
	if (UWorld* World = GetWorld())
	{
		if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
		{
			float Loudness = FMath::Lerp(
				IGPlayerNoise::MinimumFootstepLoudness,
				IGPlayerNoise::MaximumFootstepLoudness,
				FMath::Clamp(SpeedScale, 0.0f, 1.0f));
			if (bIsCrouched)
			{
				Loudness = IGPlayerNoise::CrouchFootstepLoudness;
			}
			else if (bSprinting)
			{
				const float BreathLoad = FMath::Clamp(
					(SprintActiveSeconds
						- IGPlayerNoise::SprintBreathThresholdSeconds)
					/ 3.5f,
					0.0f,
					1.0f);
				Loudness = FMath::Lerp(
					IGPlayerNoise::SprintFootstepLoudness,
					IGPlayerNoise::ExhaustedSprintFootstepLoudness,
					BreathLoad);
			}
			Noise->ReportNoise(
				GetActorLocation(),
				Loudness,
				this);
		}
	}

	// Every footfall knocks the torch: alternate the kick left/right so the
	// beam walks with the body instead of floating.
	if (Flashlight
		&& Flashlight->IsOn()
		&& (!AccessibilitySubsystem
			|| !AccessibilitySubsystem->IsReducedCameraMotionEnabled()))
	{
		const float Side = (LastStepIndex % 2 == 0) ? 1.0f : -1.0f;
		Flashlight->AddImpulse(
			FRotator(-0.5f * SpeedScale, Side * 0.9f * SpeedScale, 0.0f));
	}
	if (bSprinting && !bIsCrouched)
	{
		PlayHapticFeedback(0.12f, 0.04f);
	}
}

bool AIGPlayerCharacter::CarryActor(
	AActor* Item,
	const FVector& RelativeOffset,
	const FRotator& RelativeRotation)
{
	if (!Item || !FirstPersonCamera || CarriedActor.IsValid())
	{
		return false;
	}

	const bool bAttached = Item->AttachToComponent(
		FirstPersonCamera,
		FAttachmentTransformRules(
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::KeepWorld,
			false));
	if (!bAttached)
	{
		return false;
	}
	Item->SetActorRelativeLocation(RelativeOffset);
	Item->SetActorRelativeRotation(RelativeRotation);
	CarriedActor = Item;

	// Cache the rest pose; UpdateCarriedItem animates around it.
	CarriedBaseLocation = RelativeOffset;
	CarriedBaseRotation = RelativeRotation;
	CarrySwayOffset = FRotator::ZeroRotator;
	if (Controller)
	{
		PreviousControlRotation = Controller->GetControlRotation();
	}
	// Carrying something is reason enough to keep the camera alive.
	SetCameraMotionEnabled(true);
	return true;
}

bool AIGPlayerCharacter::ReleaseCarriedActor(AActor* ExpectedItem)
{
	if (!ExpectedItem || CarriedActor.Get() != ExpectedItem)
	{
		return false;
	}

	CarriedActor.Reset();
	CarriedBaseLocation = FVector::ZeroVector;
	CarriedBaseRotation = FRotator::ZeroRotator;
	CarrySwayOffset = FRotator::ZeroRotator;
	bHeavyBagInteractionProxyActive = false;
	HeavyBagRestLocation = FVector::ZeroVector;
	return true;
}

void AIGPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	bool bHasEnhancedMove = false;
	bool bHasEnhancedLook = false;
	bool bHasEnhancedInteract = false;
	bool bHasEnhancedFlashlight = false;
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveInputAction)
		{
			EnhancedInputComponent->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AIGPlayerCharacter::Move);
			bHasEnhancedMove = true;
		}

		if (LookInputAction)
		{
			EnhancedInputComponent->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AIGPlayerCharacter::Look);
			bHasEnhancedLook = true;
		}

		if (FlashlightInputAction)
		{
			EnhancedInputComponent->BindAction(
				FlashlightInputAction,
				ETriggerEvent::Started,
				this,
				&AIGPlayerCharacter::ToggleFlashlight);
			bHasEnhancedFlashlight = true;
		}

		if (InteractInputAction)
		{
			EnhancedInputComponent->BindAction(
				InteractInputAction,
				ETriggerEvent::Started,
				this,
				&AIGPlayerCharacter::BeginInteraction);
			EnhancedInputComponent->BindAction(
				InteractInputAction,
				ETriggerEvent::Completed,
				this,
				&AIGPlayerCharacter::EndInteraction);
			EnhancedInputComponent->BindAction(
				InteractInputAction,
				ETriggerEvent::Canceled,
				this,
				&AIGPlayerCharacter::EndInteraction);
			bHasEnhancedInteract = true;
		}
	}

	// The prototype remains playable before Blueprint input assets exist. Once an
	// Enhanced Input action is assigned, that axis/action replaces its legacy fallback.
	if (!bHasEnhancedMove)
	{
		PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ThisClass::MoveForward);
		PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ThisClass::MoveRight);
	}

	if (!bHasEnhancedLook)
	{
		PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ThisClass::Turn);
		PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &ThisClass::LookUp);
	}

	if (!bHasEnhancedInteract)
	{
		PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &ThisClass::BeginInteraction);
		PlayerInputComponent->BindAction(TEXT("Interact"), IE_Released, this, &ThisClass::EndInteraction);
	}

	if (!bHasEnhancedFlashlight)
	{
		PlayerInputComponent->BindAction(TEXT("Flashlight"), IE_Pressed, this, &ThisClass::ToggleFlashlight);
	}

	// These verbs are intentionally independent of Interact. In particular Q/B
	// must never open a door while it is also filing a timed P4 knock.
	PlayerInputComponent->BindAction(
		TEXT("Sprint"), IE_Pressed, this, &ThisClass::BeginSprint);
	PlayerInputComponent->BindAction(
		TEXT("Sprint"), IE_Released, this, &ThisClass::EndSprint);
	PlayerInputComponent->BindAction(
		TEXT("Crouch"), IE_Pressed, this, &ThisClass::BeginCrouchInput);
	PlayerInputComponent->BindAction(
		TEXT("Crouch"), IE_Released, this, &ThisClass::EndCrouchInput);
	PlayerInputComponent->BindAction(
		TEXT("Knock"), IE_Pressed, this, &ThisClass::Knock);
	PlayerInputComponent->BindAction(
		TEXT("Listen"), IE_Pressed, this, &ThisClass::BeginListen);
	PlayerInputComponent->BindAction(
		TEXT("Listen"), IE_Released, this, &ThisClass::EndListen);
	PlayerInputComponent->BindAction(
		TEXT("HoldBreath"), IE_Pressed, this, &ThisClass::BeginHoldBreath);
	PlayerInputComponent->BindAction(
		TEXT("HoldBreath"), IE_Released, this, &ThisClass::EndHoldBreath);

	// Save/load remains available even when no Blueprint input asset or front
	// end menu has been authored yet. Autosaves are the only shipped slots.
	PlayerInputComponent->BindAction(
		TEXT("LoadAutosave"),
		IE_Pressed,
		this,
		&ThisClass::LoadLatestAutosave);
}

void AIGPlayerCharacter::BeginSprint()
{
	bSprintInputHeld = true;
	RefreshSprintState();
}

void AIGPlayerCharacter::EndSprint()
{
	bSprintInputHeld = false;
	bSprinting = false;
	ApplyContextMovementSpeed();
}

void AIGPlayerCharacter::BeginCrouchInput()
{
	bCrouchInputHeld = true;
	const bool bToggleCrouch = !AccessibilitySubsystem
		|| AccessibilitySubsystem->UsesToggleCrouch();
	if (bToggleCrouch)
	{
		ToggleCrouch();
		return;
	}

	bSprinting = false;
	CrouchTransitionRemaining = IGPlayerNoise::CrouchTransitionSeconds;
	Crouch();
	SetCameraMotionEnabled(true);
	ApplyContextMovementSpeed();
}

void AIGPlayerCharacter::EndCrouchInput()
{
	bCrouchInputHeld = false;
	if (!AccessibilitySubsystem || AccessibilitySubsystem->UsesToggleCrouch())
	{
		return;
	}

	CrouchTransitionRemaining = IGPlayerNoise::CrouchTransitionSeconds;
	UnCrouch();
	SetCameraMotionEnabled(true);
	ApplyContextMovementSpeed();
}

void AIGPlayerCharacter::ToggleCrouch()
{
	bSprinting = false;
	CrouchTransitionRemaining = IGPlayerNoise::CrouchTransitionSeconds;
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
	SetCameraMotionEnabled(true);
	ApplyContextMovementSpeed();
}

void AIGPlayerCharacter::Knock()
{
	if (AIGReadableNote::GetOpenNote())
	{
		return;
	}
	const UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld
		|| CurrentWorld->GetTimeSeconds() < KnockInputLockedUntil)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AIGMissingFloorFifthDawnDirector> It(World); It; ++It)
		{
			if (It->RegisterPlayerKnock())
			{
				ApplyPlayerKnockFeedback();
				return;
			}
		}
	}
	if (!InteractionComponent)
	{
		return;
	}
	AActor* FocusedActor = InteractionComponent->GetFocusedActor();
	if (!IsValid(FocusedActor))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AIGMissingFloorNightThreeDirector> It(World); It; ++It)
		{
			if (It->TryPlayerKnock(FocusedActor, this))
			{
				ApplyPlayerKnockFeedback();
				return;
			}
		}
		const FGameplayTag DoorInteractionTag =
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("Interaction.Door")),
				false);
		if (!InteractionComponent->GetFocusedInteractionTag().MatchesTagExact(
				DoorInteractionTag))
		{
			return;
		}

		// Ordinary doors still answer the verb physically; they simply do not
		// advance a puzzle unless a chapter director owns that surface.
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateWallKnockSingle(this, 0.0f),
			FocusedActor->GetActorLocation(),
			0.82f);
		if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
		{
			Noise->ReportNoise(FocusedActor->GetActorLocation(), 0.30f, this);
		}
		ApplyPlayerKnockFeedback();
	}
}

void AIGPlayerCharacter::ApplyPlayerKnockFeedback()
{
	InteractPunch = FMath::Max(InteractPunch, 0.45f);
	KnockCameraKick = IGPlayerNoise::KnockCameraKickDegrees;
	SetCameraMotionEnabled(true);
	if (const APlayerController* PlayerController =
		Cast<APlayerController>(Controller))
	{
		if (AIGHorrorHUD* HorrorHUD =
			Cast<AIGHorrorHUD>(PlayerController->GetHUD()))
		{
			HorrorHUD->PlayFirstPersonKnock();
		}
	}
	PlayHapticFeedback(0.35f, 0.06f);
	RegisterKnockSequenceTap();
}

void AIGPlayerCharacter::RegisterKnockSequenceTap()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (LastKnockInputSeconds < 0.0
		|| Now - LastKnockInputSeconds > IGPlayerNoise::KnockSequenceResetSeconds)
	{
		KnockSequenceTapCount = 0;
	}
	LastKnockInputSeconds = Now;
	++KnockSequenceTapCount;
	if (KnockSequenceTapCount >= 3)
	{
		KnockSequenceTapCount = 0;
		KnockInputLockedUntil = Now + IGPlayerNoise::KnockInputLockSeconds;
	}
}

void AIGPlayerCharacter::PlayHapticFeedback(
	const float Intensity,
	const float DurationSeconds) const
{
	if ((AccessibilitySubsystem && !AccessibilitySubsystem->AreHapticsEnabled())
		|| Intensity <= 0.0f
		|| DurationSeconds <= 0.0f)
	{
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		PlayerController->PlayDynamicForceFeedback(
			FMath::Clamp(Intensity, 0.0f, 1.0f),
			DurationSeconds,
			false,
			false,
			true,
			true);
	}
}

void AIGPlayerCharacter::BeginListen()
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AIGMissingFloorFifthDawnDirector> It(World); It; ++It)
		{
			if (It->SetPlayerListening(true))
			{
				return;
			}
		}
	}
	if (bListening || !InteractionComponent)
	{
		return;
	}
	AActor* FocusedActor = InteractionComponent->GetFocusedActor();
	if (!IsValid(FocusedActor) || !GetWorld())
	{
		return;
	}

	for (TActorIterator<AIGMissingFloorNightThreeDirector> It(GetWorld()); It; ++It)
	{
		if (!It->IsPlayerListenTarget(FocusedActor))
		{
			continue;
		}
		bSprinting = false;
		bListening = true;
		bListenTriggered = false;
		ListenHeldSeconds = 0.0f;
		ApplyContextMovementSpeed();
		return;
	}
}

void AIGPlayerCharacter::EndListen()
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AIGMissingFloorFifthDawnDirector> It(World); It; ++It)
		{
			if (It->SetPlayerListening(false))
			{
				return;
			}
		}
	}
	if (!bListening)
	{
		return;
	}
	bListening = false;
	bListenTriggered = false;
	ListenHeldSeconds = 0.0f;
	RefreshSprintState();
}

void AIGPlayerCharacter::BeginHoldBreath()
{
	if (bHoldingBreath)
	{
		return;
	}
	bHoldingBreath = true;
	BreathHeldSeconds = 0.0f;
}

void AIGPlayerCharacter::EndHoldBreath()
{
	FinishHoldBreath(false);
}

void AIGPlayerCharacter::FinishHoldBreath(const bool bForcedRelease)
{
	if (!bHoldingBreath)
	{
		return;
	}
	const float HeldSeconds = BreathHeldSeconds;
	bHoldingBreath = false;
	BreathHeldSeconds = 0.0f;

	// The recoil is camera-first so accessibility can suppress it. A forced
	// four-second release also becomes a small real sound for the pursuer.
	if (HeldSeconds >= 1.0f)
	{
		BreathTime += 0.35f;
		InteractPunch = FMath::Max(InteractPunch, bForcedRelease ? 0.34f : 0.20f);
	}
	if (bForcedRelease)
	{
		if (UWorld* World = GetWorld())
		{
			if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
			{
				Noise->ReportNoise(GetActorLocation(), 0.08f, this);
			}
		}
	}
}

void AIGPlayerCharacter::LoadLatestAutosave()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		if (const UIGMissingFloorNarrativeSubsystem* Narrative =
			GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>();
			Narrative && Narrative->IsHourSealed())
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGSave",
					"MissingFloorLoadLocked",
					"지금은 되돌릴 때가 아니다."),
				2.2f);
			return;
		}
	}
	UIGSaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UIGSaveSubsystem>()
		: nullptr;
	const bool bLoadStarted =
		SaveSubsystem && SaveSubsystem->RequestLoadLatestAutosave();
	AIGHorrorHUD::PushThought(
		this,
		bLoadStarted
			? NSLOCTEXT(
				"IGSave",
				"LoadingLatestAutosave",
				"최근 자동 저장을 불러옵니다.")
			: NSLOCTEXT(
				"IGSave",
				"NoAutosaveAvailable",
				"불러올 자동 저장이 없습니다."),
		2.2f);
}

void AIGPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}

	const FVector2D MovementInput = Value.Get<FVector2D>();
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementInput.Y);
	AddMovementInput(RightDirection, MovementInput.X);
}

void AIGPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();
	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void AIGPlayerCharacter::MoveForward(const float Value)
{
	if (!Controller || FMath::IsNearlyZero(Value))
	{
		return;
	}

	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
}

void AIGPlayerCharacter::MoveRight(const float Value)
{
	if (!Controller || FMath::IsNearlyZero(Value))
	{
		return;
	}

	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
}

void AIGPlayerCharacter::Turn(const float Value)
{
	AddControllerYawInput(Value);
}

void AIGPlayerCharacter::LookUp(const float Value)
{
	AddControllerPitchInput(Value);
}

void AIGPlayerCharacter::BeginInteraction()
{
	// While a note is open the interact key means "put it down", wherever the
	// player happens to be looking. Without this you can walk away from the
	// wall you took the note off and then have no way to close it.
	if (AIGReadableNote* OpenNote = AIGReadableNote::GetOpenNote())
	{
		OpenNote->Close();
		return;
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AIGMissingFloorFifthDawnDirector> It(World); It; ++It)
		{
			if (It->SetPlayerListening(true))
			{
				bInteractionRedirectedToInterludeListen = true;
				return;
			}
		}
	}

	if (InteractionComponent)
	{
		AActor* FocusedActor = InteractionComponent->GetFocusedActor();
		if (IsValid(FocusedActor) && GetWorld())
		{
			for (TActorIterator<AIGMissingFloorNightThreeDirector> It(GetWorld()); It; ++It)
			{
				// A knock surface advertises Q/B in the HUD; E/A must be inert so
				// one press cannot also count as a door interaction.
				if (It->IsPlayerKnockTarget(FocusedActor))
				{
					return;
				}
				if (It->IsPlayerListenTarget(FocusedActor))
				{
					bInteractionRedirectedToListen = true;
					BeginListen();
					return;
				}
			}
		}

		// Reaching out reads as a small forward dip of the head.
		if (FocusedActor)
		{
			InteractPunch = 1.0f;
			SetCameraMotionEnabled(true);

			const AIGPickupItem* CarriedPickup =
				Cast<AIGPickupItem>(CarriedActor.Get());
			const bool bProfileCCommitted =
				CarriedPickup
				&& CarriedPickup->RebirthPurchaseProfileOnPickup
					== EIGRebirthPurchaseProfile::ProfileC2LX2
				&& IGStory::HasState(
					this,
					FGameplayTag::RequestGameplayTag(
						FName(TEXT("State.CH01.Morning.WaterPurchased")),
						false));
			if (bProfileCCommitted && !bHeavyBagInteractionProxyActive)
			{
				// Static-proxy version of setting the 4 kg bag down before
				// using both hands. Input stays live; only the carried prop and
				// concrete-contact foley change state.
				bHeavyBagInteractionProxyActive = true;
				HeavyBagRestLocation = CarriedBaseLocation;
				CarriedBaseLocation += FVector(-8.0f, 2.0f, -48.0f);
				IGAudio::SpawnOneShotAt(
					this,
					UIGToneSequenceSoundWave::CreatePlasticBagSetDown(this),
					GetActorLocation() - FVector(0.0f, 0.0f, 88.0f),
					0.55f);
			}
		}

		InteractionComponent->PressInteraction();
		if (!InteractionComponent->GetFocusedActor())
		{
			TryRequestGetUpFallback();
		}
	}
}

void AIGPlayerCharacter::EndInteraction()
{
	if (bInteractionRedirectedToInterludeListen)
	{
		bInteractionRedirectedToInterludeListen = false;
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AIGMissingFloorFifthDawnDirector> It(World); It; ++It)
			{
				if (It->SetPlayerListening(false))
				{
					break;
				}
			}
		}
		return;
	}
	if (bInteractionRedirectedToListen)
	{
		bInteractionRedirectedToListen = false;
		EndListen();
		return;
	}
	if (InteractionComponent)
	{
		InteractionComponent->ReleaseInteraction();
	}
	if (bHeavyBagInteractionProxyActive)
	{
		CarriedBaseLocation = HeavyBagRestLocation;
		bHeavyBagInteractionProxyActive = false;
		HeavyBagRestLocation = FVector::ZeroVector;
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreatePlasticBagLift(this),
			GetActorLocation() - FVector(0.0f, 0.0f, 72.0f),
			0.42f);
	}
}

bool AIGPlayerCharacter::BeginScriptedHeavyBagRest(const float Seconds)
{
	const AIGPickupItem* CarriedPickup =
		Cast<AIGPickupItem>(CarriedActor.Get());
	const bool bProfileCCommitted =
		CarriedPickup
		&& CarriedPickup->RebirthPurchaseProfileOnPickup
			== EIGRebirthPurchaseProfile::ProfileC2LX2
		&& IGStory::HasState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH01.Morning.WaterPurchased")),
				false));
	if (!bProfileCCommitted || bHeavyBagInteractionProxyActive)
	{
		return false;
	}

	bHeavyBagInteractionProxyActive = true;
	HeavyBagRestLocation = CarriedBaseLocation;
	CarriedBaseLocation += FVector(-8.0f, 2.0f, -48.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreatePlasticBagSetDown(this),
		GetActorLocation() - FVector(0.0f, 0.0f, 88.0f),
		0.55f);
	GetWorldTimerManager().SetTimer(
		HeavyBagRestTimer,
		this,
		&ThisClass::EndScriptedHeavyBagRest,
		FMath::Max(Seconds, 0.5f),
		false);
	return true;
}

void AIGPlayerCharacter::EndScriptedHeavyBagRest()
{
	// An interaction release may already have re-gripped the bag; the flag
	// keeps the restore idempotent.
	if (!bHeavyBagInteractionProxyActive)
	{
		return;
	}
	CarriedBaseLocation = HeavyBagRestLocation;
	bHeavyBagInteractionProxyActive = false;
	HeavyBagRestLocation = FVector::ZeroVector;
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreatePlasticBagLift(this),
		GetActorLocation() - FVector(0.0f, 0.0f, 72.0f),
		0.42f);
}

void AIGPlayerCharacter::TryRequestGetUpFallback()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// The wake-up must never soft-lock on aim: with nothing focused, Interact
	// still silences the ringing alarm and then gets the player out of bed.
	for (TActorIterator<AIGWakeUpDirector> It(World); It; ++It)
	{
		AIGWakeUpDirector* Director = *It;
		if (!Director)
		{
			continue;
		}

		if (Director->GetWakeState() == EIGWakeState::AwaitAlarm)
		{
			Director->RequestStopAlarmFallback();
			return;
		}

		if (Director->GetWakeState() == EIGWakeState::BedLocked)
		{
			Director->RequestGetUp();
			return;
		}
	}
}
