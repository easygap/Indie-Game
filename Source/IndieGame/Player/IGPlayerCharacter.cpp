#include "Player/IGPlayerCharacter.h"

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
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "Interaction/IGPickupItem.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGStoryHelpers.h"
#include "UObject/UObjectIterator.h"
#include "Interaction/IGReadableNote.h"
#include "Player/IGFlashlightComponent.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGInteractionComponent.h"
#include "Player/IGStressComponent.h"
#include "Sequence/IGWakeUpDirector.h"
#include "Save/IGSaveSubsystem.h"
#include "UObject/ConstructorHelpers.h"

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
	MovementComponent->MaxWalkSpeedCrouched = 160.0f;
	MovementComponent->MaxAcceleration = 1200.0f;
	MovementComponent->BrakingDecelerationWalking = 1200.0f;
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
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SleeveMaterialFinder(
		TEXT("/Game/Prototype/Materials/M_BeddingUV.M_BeddingUV"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StitchMaterialFinder(
		TEXT("/Game/Prototype/Materials/M_PlasticDark.M_PlasticDark"));
	if (CylinderMeshFinder.Succeeded())
	{
		OutfitSleeveProxy->SetStaticMesh(CylinderMeshFinder.Object);
	}
	if (SleeveMaterialFinder.Succeeded())
	{
		OutfitSleeveProxy->SetMaterial(0, SleeveMaterialFinder.Object);
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
		Stitch->SetRelativeLocation(
			FVector(-10.0f, -52.0f, -12.0f + StitchIndex * 12.0f));
		Stitch->SetRelativeRotation(FRotator(0.0f, 0.0f, 18.0f));
		Stitch->SetRelativeScale3D(FVector(0.16f, 0.045f, 0.012f));
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

	// A short tick whenever something new becomes usable is the cheapest way
	// to make aiming at objects feel responsive rather than guessy.
	if (InteractionComponent)
	{
		InteractionComponent->OnFocusChanged.AddUniqueDynamic(
			this, &AIGPlayerCharacter::HandleFocusChanged);
	}

	SetActorTickEnabled(true);
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
		FirstPersonCamera->SetRelativeLocation(CameraBaseLocation);
	}
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

	BreathTime += DeltaSeconds;

	FVector TargetOffset = FVector::ZeroVector;
	if (bWalking)
	{
		TraveledDistanceAccum += GroundSpeed * DeltaSeconds;

		const int32 StepIndex = FMath::FloorToInt32(TraveledDistanceAccum / StepDistance);
		if (StepIndex != LastStepIndex)
		{
			LastStepIndex = StepIndex;
			PlayFootstep(SpeedScale);
		}

		// One full sine cycle spans two footsteps (left/right).
		const float StepPhase =
			(TraveledDistanceAccum / StepDistance) * UE_PI;
		TargetOffset.Z += -FMath::Abs(FMath::Sin(StepPhase)) * BobAmplitude * SpeedScale;
		TargetOffset.Y += FMath::Sin(StepPhase) * 0.8f * SpeedScale;
	}

	// Slow breathing sway; more noticeable while standing still. The rate is
	// the fear model's, so the chest visibly speeds up before the player has
	// worked out why they are frightened.
	const float BreathScale = FMath::Lerp(1.0f, 0.35f, SpeedScale);
	const float BreathsPerMinute = StressComponent ? StressComponent->GetBreathsPerMinute() : 13.0f;
	const float BreathHz = BreathsPerMinute / 60.0f;
	const float BreathDepth = StressComponent
		? FMath::Lerp(0.55f, 1.35f, StressComponent->GetStress())
		: 0.55f;
	TargetOffset.Z += FMath::Sin(BreathTime * 2.0f * UE_PI * BreathHz) * BreathDepth * BreathScale;

	// Pressing Interact nudges the head forward and down, then springs back.
	if (InteractPunch > KINDA_SMALL_NUMBER)
	{
		TargetOffset.X += InteractPunch * 2.4f;
		TargetOffset.Z -= InteractPunch * 1.6f;
		InteractPunch = FMath::FInterpTo(InteractPunch, 0.0f, DeltaSeconds, 7.0f);
	}

	const FVector SmoothedLocation = FMath::VInterpTo(
		FirstPersonCamera->GetRelativeLocation(),
		CameraBaseLocation + TargetOffset,
		DeltaSeconds,
		10.0f);
	FirstPersonCamera->SetRelativeLocation(SmoothedLocation);

	// Fear tremor rides on the camera's own rotation rather than the control
	// rotation, so it shakes the view without fighting the player's aim.
	if (StressComponent)
	{
		FirstPersonCamera->SetRelativeRotation(StressComponent->GetTremor());
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

	// Every footfall knocks the torch: alternate the kick left/right so the
	// beam walks with the body instead of floating.
	if (Flashlight && Flashlight->IsOn())
	{
		const float Side = (LastStepIndex % 2 == 0) ? 1.0f : -1.0f;
		Flashlight->AddImpulse(
			FRotator(-0.5f * SpeedScale, Side * 0.9f * SpeedScale, 0.0f));
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
	// Save/load remains available even when no Blueprint input asset or front
	// end menu has been authored yet. Autosaves are the only shipped slots.
	PlayerInputComponent->BindKey(
		EKeys::F9,
		IE_Pressed,
		this,
		&ThisClass::LoadLatestAutosave);
}

void AIGPlayerCharacter::LoadLatestAutosave()
{
	UGameInstance* GameInstance = GetGameInstance();
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

	if (InteractionComponent)
	{
		// Reaching out reads as a small forward dip of the head.
		if (InteractionComponent->GetFocusedActor())
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
