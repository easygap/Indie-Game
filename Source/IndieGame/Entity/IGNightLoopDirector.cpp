#include "Entity/IGNightLoopDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/IGListenerEntity.h"
#include "Entity/IGMissingFloorMercyDirector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "TimerManager.h"

namespace IGNightLoop
{
	constexpr int32 MercyNoteCaptureThreshold = 5;
	constexpr float MercyNoteRevealDelaySeconds = 0.18f;
	constexpr float MercyNoteSlideSeconds = 0.82f;
	const FVector MercyNoteStartLocation(-150.0f, -239.0f, 900.12f);
	const FVector MercyNoteRestLocation(-150.0f, -269.5f, 900.12f);
	constexpr float MercyNoteStartYaw = -0.5f;
	constexpr float MercyNoteRestYaw = -3.5f;
}

AIGNightLoopDirector::AIGNightLoopDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AIGNightLoopDirector::BeginPlay()
{
	Super::BeginPlay();
	InitializeMercyNote();
	if (const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		CaptureCount = FMath::Max(CaptureCount, Narrative->GetCaptureCount());
		if (CaptureCount >= IGNightLoop::MercyNoteCaptureThreshold)
		{
			SetMercyNoteAtRest();
		}
	}

	// A placed director adopts any entity already in the world; a spawner
	// that creates both can also pair them explicitly via RegisterEntity.
	if (!ListenerEntity.IsValid())
	{
		for (TActorIterator<AIGListenerEntity> It(GetWorld()); It; ++It)
		{
			RegisterEntity(*It);
			break;
		}
	}
}

void AIGNightLoopDirector::SetWakeTransform(const FTransform& Transform)
{
	WakeTransform = Transform;
	bWakeTransformSet = true;
}

bool AIGNightLoopDirector::RestorePlayerAtWakePoint(
	AIGPlayerCharacter* Character) const
{
	if (!bWakeTransformSet || !IsValid(Character))
	{
		return false;
	}
	Character->TeleportTo(
		WakeTransform.GetLocation(),
		WakeTransform.Rotator(),
		false,
		true);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	if (APlayerController* Controller =
		Cast<APlayerController>(Character->GetController()))
	{
		Controller->SetControlRotation(WakeTransform.Rotator());
	}
	return true;
}

void AIGNightLoopDirector::RegisterEntity(AIGListenerEntity* Entity)
{
	if (!Entity || ListenerEntity.Get() == Entity)
	{
		return;
	}
	ListenerEntity = Entity;
	Entity->OnPlayerCaptured.AddUObject(
		this, &AIGNightLoopDirector::HandlePlayerCaptured);
}

void AIGNightLoopDirector::HandlePlayerCaptured(APawn* Player)
{
	if (bResetInFlight)
	{
		return;
	}
	AIGPlayerCharacter* Character = Cast<AIGPlayerCharacter>(Player);
	if (!Character)
	{
		return;
	}
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (Narrative
		&& Narrative->GetNightIndex() == 4
		&& Narrative->GetAggressionTier() >= 3
		&& Narrative->IsNightFourMaskRunning())
	{
		// 이 포획은 엔딩 C가 단독으로 처리한다. 일반 침대 리셋까지 시작하면
		// 하나의 멀티캐스트에서 서로 충돌하는 두 타임라인이 진행된다.
		return;
	}

	bResetInFlight = true;
	const int32 PersistedCaptureCount = Narrative
		? Narrative->GetCaptureCount()
		: 0;
	CaptureCount = FMath::Max(CaptureCount + 1, PersistedCaptureCount + 1);
	CapturedPlayer = Character;
	SpawnCaptureHandprint(Character);

	// Without an authored wake point the first capture teaches us one: the
	// spot the player stood when the night began is better than nothing,
	// but directors in real maps must call SetWakeTransform.
	if (!bWakeTransformSet)
	{
		WakeTransform = Character->GetActorTransform();
		bWakeTransformSet = true;
	}

	if (APlayerController* Controller =
		Cast<APlayerController>(Character->GetController()))
	{
		Character->PlayCaptureFeedback(FadeOutSeconds);
		Character->DisableInput(Controller);
		if (Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->StartCameraFade(
				0.0f, 1.0f, FadeOutSeconds, FLinearColor::Black,
				/*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/true);
		}
	}

	GetWorldTimerManager().SetTimer(
		ResetTimer,
		this,
		&AIGNightLoopDirector::FinishReset,
		FMath::Max(FadeOutSeconds, 0.05f),
		false);
}

void AIGNightLoopDirector::FinishReset()
{
	bool bWakeRecoveryScheduled = false;
	AIGPlayerCharacter* Character = CapturedPlayer.Get();
	if (Character)
	{
		Character->TeleportTo(
			WakeTransform.GetLocation(),
			WakeTransform.Rotator(),
			false,
			true);
		if (UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}

		if (APlayerController* Controller =
			Cast<APlayerController>(Character->GetController()))
		{
			Controller->SetControlRotation(WakeTransform.Rotator());
			const float WakeEchoSeconds = GetWakeEchoSeconds();
			const float WakeRecoverySeconds = GetWakeRecoverySeconds();
			if (AIGHorrorHUD* HorrorHUD = Cast<AIGHorrorHUD>(Controller->GetHUD()))
			{
				HorrorHUD->PlayCaptureWakeEcho(
					CaptureCount,
					WakeEchoSeconds,
					WakeRecoverySeconds);
			}
			if (Controller->PlayerCameraManager)
			{
				Controller->PlayerCameraManager->StartCameraFade(
					1.0f, 0.0f, GetWakeFadeInSeconds(), FLinearColor::Black,
					/*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/false);
			}

			// A single duvet settle anchors the teleport at the bed. It must not
			// repeat the two capture knocks or add a failure sting.
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateClothSettle(this),
				WakeTransform.GetLocation(),
				0.72f,
				0.94f,
				75.0f,
				480.0f);
			GetWorldTimerManager().SetTimer(
				WakeRecoveryTimer,
				this,
				&AIGNightLoopDirector::FinishWakeRecovery,
				WakeRecoverySeconds,
				false);
			bWakeRecoveryScheduled = true;
		}
	}

	if (AIGListenerEntity* Entity = ListenerEntity.Get())
	{
		// The tier lives in the narrative snapshot, not on the pawn, so a
		// quit-and-resume cannot hand the player back a patient pursuer.
		if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
		{
			Entity->SetAggressionTier(Narrative->RecordCapture());
			Entity->ResetToPatrolStart(/*bRaiseAggression=*/false);
		}
		else
		{
			Entity->ResetToPatrolStart(/*bRaiseAggression=*/true);
		}
	}

	QueueMercyNoteReveal();

	// §20.3-1: two resets with nothing learned in between and the world adds one
	// more thing to look at. The fifth-capture note above is the third net and a
	// separate beat; these two never stand in for each other.
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AIGMissingFloorMercyDirector> It(World); It; ++It)
		{
			It->NotifyCaptureReset();
			break;
		}
	}

	if (!bWakeRecoveryScheduled)
	{
		FinishWakeRecovery();
	}
}

void AIGNightLoopDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bMercyNoteSliding || !MercyNote)
	{
		SetActorTickEnabled(false);
		return;
	}

	MercyNoteSlideElapsedSeconds += FMath::Max(0.0f, DeltaSeconds);
	const float Alpha = FMath::Clamp(
		MercyNoteSlideElapsedSeconds / IGNightLoop::MercyNoteSlideSeconds,
		0.0f,
		1.0f);
	const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	MercyNote->SetWorldLocation(FMath::Lerp(
		IGNightLoop::MercyNoteStartLocation,
		IGNightLoop::MercyNoteRestLocation,
		SmoothAlpha));
	MercyNote->SetWorldRotation(FRotator(
		0.0f,
		FMath::Lerp(
			IGNightLoop::MercyNoteStartYaw,
			IGNightLoop::MercyNoteRestYaw,
			SmoothAlpha),
		0.0f));

	if (Alpha >= 1.0f)
	{
		SetMercyNoteAtRest();
	}
}

void AIGNightLoopDirector::FinishWakeRecovery()
{
	if (AIGPlayerCharacter* Character = CapturedPlayer.Get())
	{
		if (APlayerController* Controller =
			Cast<APlayerController>(Character->GetController()))
		{
			Character->EnableInput(Controller);
		}
	}

	CapturedPlayer = nullptr;
	bResetInFlight = false;
}

bool AIGNightLoopDirector::IsMercyNoteVisible() const
{
	return MercyNote
		&& MercyNote->IsVisible()
		&& !MercyNote->bHiddenInGame;
}

FVector AIGNightLoopDirector::GetMercyNoteLocation() const
{
	return MercyNote
		? MercyNote->GetComponentLocation()
		: FVector::ZeroVector;
}

void AIGNightLoopDirector::PrimeMercyNoteCaptureProbe()
{
	CaptureCount = IGNightLoop::MercyNoteCaptureThreshold - 1;
	bMercyNoteRevealed = false;
	bMercyNoteSliding = false;
	GetWorldTimerManager().ClearTimer(MercyNoteRevealTimer);
	SetActorTickEnabled(false);
	if (MercyNote)
	{
		MercyNote->SetWorldLocation(IGNightLoop::MercyNoteStartLocation);
		MercyNote->SetWorldRotation(FRotator(
			0.0f, IGNightLoop::MercyNoteStartYaw, 0.0f));
		MercyNote->SetVisibility(false, true);
		MercyNote->SetHiddenInGame(true, true);
	}
}

void AIGNightLoopDirector::PlayMercyNoteCapturePreview()
{
	if (!MercyNote)
	{
		InitializeMercyNote();
	}
	if (!MercyNote)
	{
		return;
	}
	bMercyNoteRevealed = true;
	GetWorldTimerManager().ClearTimer(MercyNoteRevealTimer);
	BeginMercyNoteSlide();
}

bool AIGNightLoopDirector::InitializeMercyNote()
{
	if (MercyNote)
	{
		return true;
	}

	UStaticMesh* NoteMesh = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Game/Meshes/SM_CaptureMercyNote.SM_CaptureMercyNote"));
	UMaterialInterface* NoteMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Prototype/Materials/M_CaptureMercyNote."
			"M_CaptureMercyNote"));
	if (!NoteMesh || !NoteMaterial)
	{
		return false;
	}

	MercyNote = NewObject<UStaticMeshComponent>(
		this,
		TEXT("CaptureMercyNote"));
	if (!MercyNote)
	{
		return false;
	}
	MercyNote->SetMobility(EComponentMobility::Movable);
	MercyNote->SetStaticMesh(NoteMesh);
	MercyNote->SetMaterial(0, NoteMaterial);
	MercyNote->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	MercyNote->SetGenerateOverlapEvents(false);
	MercyNote->SetCanEverAffectNavigation(false);
	// 종이처럼 얇은 이동 그림자는 테라조 위에 긴 시간축 잔상을 남긴다.
	// 알베도와 거친 정도의 대비만으로도 종이가 바닥에 붙어 보인다.
	MercyNote->SetCastShadow(false);
	MercyNote->SetReceivesDecals(false);
	MercyNote->SetCullDistance(850.0f);
	MercyNote->SetAffectDistanceFieldLighting(false);
	MercyNote->ComponentTags.AddUnique(FName(TEXT("MissingFloor.CaptureMercyNote")));
	AddInstanceComponent(MercyNote);
	MercyNote->RegisterComponent();
	MercyNote->SetWorldLocation(IGNightLoop::MercyNoteStartLocation);
	MercyNote->SetWorldRotation(FRotator(
		0.0f, IGNightLoop::MercyNoteStartYaw, 0.0f));
	MercyNote->SetVisibility(false, true);
	MercyNote->SetHiddenInGame(true, true);
	return true;
}

void AIGNightLoopDirector::QueueMercyNoteReveal()
{
	if (CaptureCount < IGNightLoop::MercyNoteCaptureThreshold
		|| bMercyNoteRevealed
		|| !MercyNote)
	{
		return;
	}

	bMercyNoteRevealed = true;
	GetWorldTimerManager().SetTimer(
		MercyNoteRevealTimer,
		this,
		&AIGNightLoopDirector::BeginMercyNoteSlide,
		IGNightLoop::MercyNoteRevealDelaySeconds,
		false);
}

void AIGNightLoopDirector::BeginMercyNoteSlide()
{
	if (!MercyNote)
	{
		return;
	}

	MercyNoteSlideElapsedSeconds = 0.0f;
	bMercyNoteSliding = true;
	MercyNote->SetWorldLocation(IGNightLoop::MercyNoteStartLocation);
	MercyNote->SetWorldRotation(FRotator(
		0.0f, IGNightLoop::MercyNoteStartYaw, 0.0f));
	MercyNote->SetHiddenInGame(false, true);
	MercyNote->SetVisibility(true, true);
	SetActorTickEnabled(true);

	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreatePaperDoorSlide(this),
		IGNightLoop::MercyNoteRestLocation + FVector(0.0f, 0.0f, 8.0f),
		0.78f,
		0.96f,
		90.0f,
		720.0f,
		EIGAudioBus::World);
}

void AIGNightLoopDirector::SetMercyNoteAtRest()
{
	if (!MercyNote)
	{
		return;
	}
	bMercyNoteRevealed = true;
	bMercyNoteSliding = false;
	MercyNoteSlideElapsedSeconds = IGNightLoop::MercyNoteSlideSeconds;
	MercyNote->SetWorldLocation(IGNightLoop::MercyNoteRestLocation);
	MercyNote->SetWorldRotation(FRotator(
		0.0f, IGNightLoop::MercyNoteRestYaw, 0.0f));
	MercyNote->ResetSceneVelocity();
	MercyNote->SetHiddenInGame(false, true);
	MercyNote->SetVisibility(true, true);
	SetActorTickEnabled(false);
}

bool AIGNightLoopDirector::SpawnCaptureHandprint(AIGPlayerCharacter* Character)
{
	UWorld* World = GetWorld();
	if (!World || !Character)
	{
		return false;
	}

	const FVector TraceStart = Character->GetActorLocation()
		+ FVector(0.0f, 0.0f, 20.0f);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(IGMissingFloorCaptureHandprint),
		false,
		Character);
	QueryParams.AddIgnoredActor(this);
	if (const AIGListenerEntity* Entity = ListenerEntity.Get())
	{
		QueryParams.AddIgnoredActor(Entity);
	}

	FHitResult BestHit;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	constexpr int32 WallProbeCount = 8;
	constexpr float WallProbeDistance = 260.0f;
	for (int32 ProbeIndex = 0; ProbeIndex < WallProbeCount; ++ProbeIndex)
	{
		const float AngleRadians = (2.0f * UE_PI * ProbeIndex) / WallProbeCount;
		const FVector Direction(
			FMath::Cos(AngleRadians),
			FMath::Sin(AngleRadians),
			0.0f);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(
				Hit,
				TraceStart,
				TraceStart + Direction * WallProbeDistance,
				ECC_Visibility,
				QueryParams)
			|| FMath::Abs(Hit.ImpactNormal.Z) > 0.35f)
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(
			TraceStart,
			Hit.ImpactPoint);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestHit = Hit;
		}
	}
	if (!BestHit.bBlockingHit)
	{
		return false;
	}

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* HandprintMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Prototype/Materials/M_MissingFloorHandprints."
			"M_MissingFloorHandprints"));
	if (!CubeMesh || !HandprintMaterial)
	{
		return false;
	}

	CaptureHandprints.RemoveAllSwap(
		[](const TObjectPtr<UStaticMeshComponent>& Handprint)
		{
			return !IsValid(Handprint);
		});
	constexpr int32 MaximumCaptureHandprints = 12;
	while (CaptureHandprints.Num() >= MaximumCaptureHandprints)
	{
		if (UStaticMeshComponent* Oldest = CaptureHandprints[0])
		{
			Oldest->DestroyComponent();
		}
		CaptureHandprints.RemoveAt(0);
	}

	UStaticMeshComponent* Handprint = NewObject<UStaticMeshComponent>(
		this,
		MakeUniqueObjectName(
			this,
			UStaticMeshComponent::StaticClass(),
			TEXT("CaptureHandprint")));
	if (!Handprint)
	{
		return false;
	}
	Handprint->SetStaticMesh(CubeMesh);
	Handprint->SetMaterial(0, HandprintMaterial);
	Handprint->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Handprint->SetGenerateOverlapEvents(false);
	Handprint->SetCanEverAffectNavigation(false);
	Handprint->SetCastShadow(false);
	Handprint->SetReceivesDecals(false);
	Handprint->ComponentTags.AddUnique(
		FName(TEXT("MissingFloor.CaptureHandprint")));
	AddInstanceComponent(Handprint);
	Handprint->RegisterComponent();

	const float Variant = static_cast<float>((CaptureCount * 37) % 11) / 10.0f;
	const float TwistDegrees = FMath::Lerp(-7.0f, 8.0f, Variant);
	const FQuat AlignToWall = FQuat::FindBetweenNormals(
		FVector::ForwardVector,
		BestHit.ImpactNormal);
	const FQuat SurfaceTwist(
		BestHit.ImpactNormal,
		FMath::DegreesToRadians(TwistDegrees));
	Handprint->SetWorldLocationAndRotation(
		BestHit.ImpactPoint + BestHit.ImpactNormal * 0.25f,
		SurfaceTwist * AlignToWall);
	Handprint->SetWorldScale3D(FVector(
		0.003f,
		FMath::Lerp(0.52f, 0.62f, Variant),
		FMath::Lerp(0.62f, 0.74f, 1.0f - Variant)));
	CaptureHandprints.Add(Handprint);
	return true;
}

float AIGNightLoopDirector::GetWakeFadeInSeconds() const
{
	if (CaptureCount <= 1)
	{
		return 3.0f;
	}
	if (CaptureCount == 2)
	{
		return 2.2f;
	}
	if (CaptureCount <= 4)
	{
		return 1.4f;
	}
	return 0.4f;
}

float AIGNightLoopDirector::GetWakeEchoSeconds() const
{
	if (CaptureCount <= 1)
	{
		return 0.68f;
	}
	if (CaptureCount == 2)
	{
		return 0.48f;
	}
	if (CaptureCount <= 4)
	{
		return 0.30f;
	}
	return 0.16f;
}

float AIGNightLoopDirector::GetWakeRecoverySeconds() const
{
	// Keep input and normal HUD locked until the per-capture camera fade ends;
	// the short echo animation is allowed to disappear first.
	return FMath::Max(GetWakeFadeInSeconds(), GetWakeEchoSeconds());
}

UIGMissingFloorNarrativeSubsystem* AIGNightLoopDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}
