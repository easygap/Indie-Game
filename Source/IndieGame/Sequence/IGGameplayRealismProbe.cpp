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
#include "Player/IGHudGuidance.h"

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
		// 86cm 문을 중심에서 10cm 비껴 진입한다. 실제 이동으로 문틀 끼임을 확인한다.
		Player->SetActorLocation(FVector(0, 10, 98));
		Player->GetCharacterMovement()->StopMovementImmediately();
		Controller->SetControlRotation(FRotator::ZeroRotator);
		AddBlock(FVector(160, 143, 110), FVector(8, 100, 110));
		AddBlock(FVector(160, -143, 110), FVector(8, 100, 110));
		SendKey(EKeys::W, true);
		Phase = 5; Seconds = 0;
	}
	else if (Phase == 5 && Seconds > 1.6f)
	{
		SendKey(EKeys::W, false);
		Check(Player->GetActorLocation().X > 230.f, TEXT("offset_doorway_passage"));
		Player->GetCharacterMovement()->StopMovementImmediately();
		Phase = 6; Seconds = 0;
	}
	else if (Phase == 6 && Seconds > 0.5f)
	{
		StandingEyeHeight = Player->FindComponentByClass<UCameraComponent>()->GetComponentLocation().Z;
		UE_LOG(LogIndieGame, Display, TEXT("REALISM_VIEW standing eye=%.2f actor=%.2f relative=%.2f"), StandingEyeHeight, Player->GetActorLocation().Z, Player->GetFirstPersonCamera()->GetRelativeLocation().Z);
		Player->Crouch();
		Phase = 7; Seconds = 0;
	}
	else if (Phase == 7 && Seconds > 0.65f)
	{
		const float EyeDrop = StandingEyeHeight - Player->FindComponentByClass<UCameraComponent>()->GetComponentLocation().Z;
		UE_LOG(LogIndieGame, Display, TEXT("REALISM_VIEW crouched=%d eye_drop_cm=%.2f"), Player->bIsCrouched, EyeDrop);
		UE_LOG(LogIndieGame, Display, TEXT("REALISM_VIEW crouching actor=%.2f relative=%.2f"), Player->GetActorLocation().Z, Player->GetFirstPersonCamera()->GetRelativeLocation().Z);
		Check(Player->bIsCrouched && FMath::IsNearlyEqual(EyeDrop, 48.f, 1.f),
			TEXT("crouch_lowers_view_with_camera_motion_disabled"));
		Player->UnCrouch();
		Phase = 8; Seconds = 0;
	}
	else if (Phase == 8 && Seconds > 0.65f)
	{
		UE_LOG(LogIndieGame, Display, TEXT("REALISM_VIEW restored actor=%.2f relative=%.2f crouched=%d"), Player->GetActorLocation().Z, Player->GetFirstPersonCamera()->GetRelativeLocation().Z, Player->bIsCrouched);
		Check(!Player->bIsCrouched && FMath::IsNearlyEqual(Player->FindComponentByClass<UCameraComponent>()->GetComponentLocation().Z, StandingEyeHeight, 1.f),
			TEXT("uncrouch_restores_view_with_camera_motion_disabled"));
		CheckInteractionsAndCapture();
		UE_LOG(LogIndieGame, Display, TEXT("REALISM_PROBE %s failures=%d"), Failures ? TEXT("FAIL") : TEXT("PASS"), Failures);
		Phase = 9;
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}
}

void AIGGameplayRealismProbe::CheckInteractionsAndCapture()
{
	FIGHudGuidance Guide;
	Guide.Update(0, TEXT("짐을 푼다"), true);
	Check(Guide.ObjectiveAlpha() == 1 && Guide.ControlsAlpha() == 1, TEXT("guide_first_arrival_visible"));
	Guide.Update(8, TEXT("짐을 푼다"), true);
	Check(Guide.ObjectiveAlpha() == 0 && Guide.ControlsAlpha() == 1, TEXT("guide_objective_expires_first"));
	Guide.Update(13, TEXT("짐을 푼다"), true);
	Check(Guide.ControlsAlpha() == 0, TEXT("guide_tutorial_expires"));
	Guide.Update(0, TEXT("관리실에 간다"), false);
	Check(Guide.ObjectiveAlpha() == 1 && Guide.ControlsAlpha() == 0, TEXT("guide_new_goal_without_tutorial_repeat"));
	Guide.ToggleRecall();
	Guide.Update(9.8f, TEXT("관리실에 간다"), false);
	Check(Guide.ControlsAlpha() > 0 && Guide.ControlsAlpha() < 1, TEXT("guide_recall_fades"));
	Guide.Update(.3f, TEXT("관리실에 간다"), false);
	Check(Guide.ControlsAlpha() == 0 && Guide.ObjectiveAlpha() == 0, TEXT("guide_recall_expires"));
	Guide.ToggleRecall(); Guide.ToggleRecall();
	Check(Guide.ControlsAlpha() == 0, TEXT("guide_second_press_closes"));
	Guide.ToggleRecall(); Guide.Interrupt();
	Check(Guide.ControlsAlpha() == 0 && Guide.ObjectiveAlpha() == 0, TEXT("guide_capture_interrupts"));
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
	Check(!Entity->IsHidden(), TEXT("capture_keeps_physical_body_visible"));
	USkeletalMeshComponent* Body = Entity->FindComponentByClass<USkeletalMeshComponent>();
	Check(Body && Body->GetSkeletalMeshAsset() && Body->GetNumLODs() == 4, TEXT("runtime_character_has_four_lods"));
	if (Body)
	{
		for (int32 Frame = 0; Frame < 48; ++Frame)
		{
			Body->TickAnimation(1.0f / 60.0f, false);
			Body->RefreshBoneTransforms();
			Entity->Tick(1.0f / 60.0f);
		}
		const FVector FaceFromEye = Entity->GetCaptureFaceLocation() - Player->GetPawnViewLocation();
		Check(FaceFromEye.Size() > 35.0f && FaceFromEye.Size() < 110.0f
			&& FMath::Abs(FaceFromEye.Z) < 45.0f, TEXT("capture_face_stays_in_front_of_camera"));
		Entity->SetDormant(true);
		Entity->KeepCaptureVisible();
		Check(!Entity->IsHidden() && Entity->IsActorTickEnabled()
			&& Entity->GetListenerState() == EIGListenerState::CaptureHold,
			TEXT("failure_ending_keeps_capture_pose"));
	}
	Entity->Destroy();
}
