#include "Entity/IGNightLoopDirector.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/IGListenerEntity.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGPlayerCharacter.h"
#include "TimerManager.h"

AIGNightLoopDirector::AIGNightLoopDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AIGNightLoopDirector::BeginPlay()
{
	Super::BeginPlay();

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

	bResetInFlight = true;
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
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
			Character->EnableInput(Controller);
			if (Controller->PlayerCameraManager)
			{
				Controller->PlayerCameraManager->StartCameraFade(
					1.0f, 0.0f, GetWakeFadeInSeconds(), FLinearColor::Black,
					/*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/false);
			}
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

	CapturedPlayer = nullptr;
	bResetInFlight = false;
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

UIGMissingFloorNarrativeSubsystem* AIGNightLoopDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}
