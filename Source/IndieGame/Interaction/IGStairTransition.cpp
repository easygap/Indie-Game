#include "Interaction/IGStairTransition.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

AIGStairTransition::AIGStairTransition()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	auto CreatePortal = [this](const TCHAR* Name)
	{
		UBoxComponent* Portal = CreateDefaultSubobject<UBoxComponent>(Name);
		Portal->SetupAttachment(SceneRoot);
		Portal->SetBoxExtent(FVector(24.0f, 54.0f, 74.0f));
		Portal->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Portal->SetCollisionObjectType(ECC_WorldDynamic);
		Portal->SetCollisionResponseToAllChannels(ECR_Ignore);
		Portal->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Portal->SetGenerateOverlapEvents(true);
		Portal->SetCanEverAffectNavigation(false);
		return Portal;
	};
	UpperPortal = CreatePortal(TEXT("UpperPortal"));
	LowerPortal = CreatePortal(TEXT("LowerPortal"));
}

void AIGStairTransition::Configure(
	const FVector& InUpperTriggerLocation,
	const FVector& InUpperExitLocation,
	const FRotator& InUpperExitRotation,
	const FVector& InLowerTriggerLocation,
	const FVector& InLowerExitLocation,
	const FRotator& InLowerExitRotation)
{
	UpperPortal->SetWorldLocation(InUpperTriggerLocation);
	LowerPortal->SetWorldLocation(InLowerTriggerLocation);
	UpperExitLocation = InUpperExitLocation;
	LowerExitLocation = InLowerExitLocation;
	UpperExitRotation = InUpperExitRotation;
	LowerExitRotation = InLowerExitRotation;
}

void AIGStairTransition::BeginPlay()
{
	Super::BeginPlay();
	UpperPortal->OnComponentBeginOverlap.AddUniqueDynamic(
		this,
		&ThisClass::HandleUpperOverlap);
	LowerPortal->OnComponentBeginOverlap.AddUniqueDynamic(
		this,
		&ThisClass::HandleLowerOverlap);
}

void AIGStairTransition::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TransferTimer);
	GetWorldTimerManager().ClearTimer(CooldownTimer);
	if (APlayerController* Controller = PendingController.Get())
	{
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);
	}
	Super::EndPlay(EndPlayReason);
}

void AIGStairTransition::HandleUpperOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	BeginTransfer(OtherActor, true);
}

void AIGStairTransition::HandleLowerOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	BeginTransfer(OtherActor, false);
}

void AIGStairTransition::BeginTransfer(AActor* OtherActor, const bool bGoingDown)
{
	if (bTransferPending || bCooldown)
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(OtherActor);
	APlayerController* Controller = Pawn
		? Cast<APlayerController>(Pawn->GetController())
		: nullptr;
	if (!Pawn || !Controller || Pawn != Controller->GetPawn())
	{
		return;
	}

	bTransferPending = true;
	bPendingGoingDown = bGoingDown;
	PendingPawn = Pawn;
	PendingController = Controller;
	Controller->SetIgnoreMoveInput(true);
	Controller->SetIgnoreLookInput(true);
	if (APlayerCameraManager* Camera = Controller->PlayerCameraManager)
	{
		Camera->StartCameraFade(
			0.0f,
			1.0f,
			0.12f,
			FLinearColor::Black,
			false,
			true);
	}
	GetWorldTimerManager().SetTimer(
		TransferTimer,
		this,
		&ThisClass::CompleteTransfer,
		0.12f,
		false);
}

void AIGStairTransition::CompleteTransfer()
{
	APawn* Pawn = PendingPawn.Get();
	APlayerController* Controller = PendingController.Get();
	if (Pawn && Controller)
	{
		const FVector& TargetLocation =
			bPendingGoingDown ? LowerExitLocation : UpperExitLocation;
		const FRotator& TargetRotation =
			bPendingGoingDown ? LowerExitRotation : UpperExitRotation;
		Pawn->SetActorLocationAndRotation(
			TargetLocation,
			TargetRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		Controller->SetControlRotation(TargetRotation);
		if (APlayerCameraManager* Camera = Controller->PlayerCameraManager)
		{
			Camera->StartCameraFade(
				1.0f,
				0.0f,
				0.18f,
				FLinearColor::Black,
				false,
				false);
		}
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);
	}

	PendingPawn.Reset();
	PendingController.Reset();
	bTransferPending = false;
	bCooldown = true;
	OnTransitionCompleted.Broadcast(bPendingGoingDown);
	GetWorldTimerManager().SetTimer(
		CooldownTimer,
		this,
		&ThisClass::ClearCooldown,
		0.7f,
		false);
}

void AIGStairTransition::ClearCooldown()
{
	bCooldown = false;
}
