#include "Sequence/IGGameplayRealismProbe.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/IGListenerEntity.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "IndieGame.h"
#include "InputKeyEventArgs.h"
#include "Interaction/IGReadableNote.h"
#include "Materials/MaterialInterface.h"
#include "Player/IGInteractionComponent.h"
#include "Player/IGPlayerCharacter.h"

AIGGameplayRealismProbe::AIGGameplayRealismProbe()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

UBoxComponent* AIGGameplayRealismProbe::AddBlock(const FVector& Location, const FVector& Extent)
{
	UBoxComponent* Block = NewObject<UBoxComponent>(this);
	Block->SetBoxExtent(Extent);
	Block->SetCollisionProfileName(TEXT("BlockAll"));
	Block->SetCanEverAffectNavigation(false);
	Block->RegisterComponent();
	Block->SetWorldLocation(Location);
	return Block;
}

void AIGGameplayRealismProbe::SendKey(const FKey& Key, const bool bPressed)
{
	const FInputKeyEventArgs Event(nullptr, FInputDeviceId::CreateFromInternalId(0), Key,
		bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f, false, FPlatformTime::Cycles64());
	Controller->InputKey(Event);
}

void AIGGameplayRealismProbe::Check(const bool bCondition, const TCHAR* Name)
{
	Failures += bCondition ? 0 : 1;
	UE_LOG(LogIndieGame, Display, TEXT("REALISM_CHECK %s %s"), Name, bCondition ? TEXT("PASS") : TEXT("FAIL"));
}

void AIGGameplayRealismProbe::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Seconds += DeltaSeconds;
	if (Phase == 0)
	{
		Controller = GetWorld()->GetFirstPlayerController();
		Player = Controller.IsValid() ? Cast<AIGPlayerCharacter>(Controller->GetPawn()) : nullptr;
		if (!Player.IsValid())
		{
			if (Seconds > 15.0f) { Check(false, TEXT("player_spawn")); FPlatformMisc::RequestExitWithStatus(false, 1); }
			return;
		}
		AddBlock(FVector(0, 0, -12), FVector(3000, 3000, 12));
		Player->SetActorLocation(FVector(0, 0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2));
		Controller->SetControlRotation(FRotator::ZeroRotator);
		Controller->ResetIgnoreMoveInput();
		Controller->ResetIgnoreLookInput();
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Player->SetCameraMotionEnabled(false);
		Phase = 1; Seconds = 0;
	}
	else if (Phase == 1 && Seconds > 0.5f)
	{
		SendKey(EKeys::LeftShift, true); SendKey(EKeys::W, true);
		Phase = 2; Seconds = 0;
	}
	else if (Phase == 2 && Seconds > 1.3f)
	{
		Check(Player->GetVelocity().Size2D() > 440.0f, TEXT("sprint_key_reaches_speed"));
		BrakeStart = Player->GetActorLocation();
		SendKey(EKeys::W, false);
		Phase = 3; Seconds = 0;
	}
	else if (Phase == 3 && (Player->GetVelocity().Size2D() < 1.0f || Seconds > 0.8f))
	{
		const float Distance = FVector::Dist2D(BrakeStart, Player->GetActorLocation());
		UE_LOG(LogIndieGame, Display, TEXT("REALISM_BRAKE distance_cm=%.2f seconds=%.3f"), Distance, Seconds);
		Check(Distance > 5.0f && Distance < 75.0f && Seconds < 0.45f, TEXT("sprint_release_stops_before_door_width"));
		SendKey(EKeys::LeftShift, false);
		Phase = 4; Seconds = 0;
	}
	else if (Phase == 4 && Seconds > 0.2f)
	{
		CheckInteractionsAndCapture();
		UE_LOG(LogIndieGame, Display, TEXT("REALISM_PROBE %s failures=%d"), Failures ? TEXT("FAIL") : TEXT("PASS"), Failures);
		Phase = 5;
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}
}

void AIGGameplayRealismProbe::CheckInteractionsAndCapture()
{
	Player->GetCharacterMovement()->StopMovementImmediately();
	Player->GetCharacterMovement()->DisableMovement();
	Player->SetActorLocation(FVector(0, 0, 90));
	Controller->SetControlRotation(FRotator::ZeroRotator);
	Controller->PlayerCameraManager->UpdateCamera(0.0f);
	FVector Eye; FRotator View;
	Controller->GetPlayerViewPoint(Eye, View);
	UIGInteractionComponent* Interaction = Player->FindComponentByClass<UIGInteractionComponent>();
	Check(Interaction != nullptr, TEXT("interaction_component"));
	if (!Interaction) { return; }
	Interaction->SetInteractionInputEnabled(true);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIGReadableNote* Note = GetWorld()->SpawnActor<AIGReadableNote>(Eye + FVector(130, 8, 0), FRotator::ZeroRotator, Spawn);
	Note->ConfigurePrototypeVisuals(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")),
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Prototype/Materials/M_Stucco_X.M_Stucco_X")),
		FVector(4, 4, 10));
	Interaction->RefreshFocus();
	Check(Interaction->GetFocusedActor() == Note, TEXT("small_visible_prop_focus"));
	// 구체 스캔은 모서리를 스치지만 눈과 물체 사이를 막는 얇은 문틀.
	UBoxComponent* Occluder = AddBlock(Eye + FVector(110, 7, 0), FVector(2, 3, 15));
	Interaction->RefreshFocus();
	Check(Interaction->GetFocusedActor() != Note, TEXT("occluded_prop_rejected"));
	Occluder->DestroyComponent(); Note->Destroy();
	Interaction->RefreshFocus();
	AIGListenerEntity* Entity = GetWorld()->SpawnActor<AIGListenerEntity>(FVector(90, 0, 58), FRotator(0, 180, 0), Spawn);
	Entity->SetActorTickEnabled(false);
	Entity->SetDifficultyForTesting(EIGNightDifficulty::Standard);
	Entity->ParkForBeat(FVector(90, 0, 58), 180.0f);
	UBoxComponent* Door = AddBlock(FVector(45, 0, 110), FVector(4, 60, 110));
	Entity->Tick(0.016f);
	Check(Entity->GetListenerState() != EIGListenerState::CaptureHold, TEXT("closed_door_blocks_capture"));
	Door->DestroyComponent();
	Entity->Tick(0.016f);
	Check(Entity->GetListenerState() == EIGListenerState::CaptureHold, TEXT("open_door_allows_capture"));
	USkeletalMeshComponent* Body = Entity->FindComponentByClass<USkeletalMeshComponent>();
	Check(Body && Body->GetSkeletalMeshAsset() && Body->GetNumLODs() == 4, TEXT("runtime_character_has_four_lods"));
	Entity->Destroy();
}
