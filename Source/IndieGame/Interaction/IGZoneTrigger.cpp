#include "Interaction/IGZoneTrigger.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Narrative/IGStoryHelpers.h"
#include "Player/IGHorrorHUD.h"

AIGZoneTrigger::AIGZoneTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	ZoneBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBox"));
	SetRootComponent(ZoneBox);
	ZoneBox->SetBoxExtent(FVector(80.0f, 80.0f, 110.0f));
	ZoneBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ZoneBox->SetGenerateOverlapEvents(true);
}

void AIGZoneTrigger::SetZoneExtent(const FVector& HalfExtent)
{
	ZoneBox->SetBoxExtent(HalfExtent);
}

void AIGZoneTrigger::BeginPlay()
{
	Super::BeginPlay();

	// Restored checkpoints treat an already-recorded state as consumed.
	if (StateTagOnEnter.IsValid() && IGStory::HasState(this, StateTagOnEnter))
	{
		bTriggered = true;
		return;
	}

	ZoneBox->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
}

void AIGZoneTrigger::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bTriggered)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	bTriggered = true;

	if (StateTagOnEnter.IsValid())
	{
		IGStory::AddState(this, StateTagOnEnter);
	}

	if (!ThoughtOnEnter.IsEmpty())
	{
		AIGHorrorHUD::PushThought(this, ThoughtOnEnter, 3.6f);
	}

	OnZoneTriggered.Broadcast(this);
}
