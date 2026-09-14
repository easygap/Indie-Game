#include "Environment/IGStoreClerk.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AIGStoreClerk::AIGStoreClerk()
{
	PrimaryActorTick.bCanEverTick = false;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Narin"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Body->bEnableUpdateRateOptimizations = true;
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
}

void AIGStoreClerk::BeginPlay()
{
	Super::BeginPlay();
	Body->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Meshes/SK_NarinClerk.SK_NarinClerk")));
	if (UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Meshes/A_NarinClerk_Idle.A_NarinClerk_Idle")))
	{
		Body->PlayAnimation(Idle, true);
	}
	GetWorldTimerManager().SetTimer(FacingTimer, this, &AIGStoreClerk::UpdateCustomerFacing, 0.1f, true);
}

void AIGStoreClerk::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FacingTimer);
	Super::EndPlay(EndPlayReason);
}

void AIGStoreClerk::UpdateCustomerFacing()
{
	if (IsHidden()) { return; }
	const APawn* Customer = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Customer) { return; }
	const FVector Offset = Customer->GetActorLocation() - GetActorLocation();
	float DesiredYaw = 0.f;
	if (Offset.SizeSquared() < FMath::Square(380.f) && Offset.Y < -55.f)
	{
		DesiredYaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(0.f, Offset.Rotation().Yaw + 90.f), -22.f, 22.f);
	}
	SetActorRotation(FRotator(0.f, FMath::FInterpTo(GetActorRotation().Yaw, DesiredYaw, 0.1f, 1.4f), 0.f));
}
