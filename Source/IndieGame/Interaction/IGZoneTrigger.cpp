#include "Interaction/IGZoneTrigger.h"

#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Narrative/IGStoryHelpers.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Narrative/IGStoryStateSubsystem.h"
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

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->OnStoryStateTagChanged.AddUniqueDynamic(
				this,
				&ThisClass::HandleStoryStateChanged);
		}
	}

	ReconcileWithStoryState();
}

void AIGZoneTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->OnStoryStateTagChanged.RemoveDynamic(
				this,
				&ThisClass::HandleStoryStateChanged);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AIGZoneTrigger::ReconcileWithStoryState()
{
	// A configured checkpoint tag is authoritative both during initial spawn
	// and when a save is applied to a world that is already running.
	bTriggered = StateTagOnEnter.IsValid()
		&& IGStory::HasState(this, StateTagOnEnter);
	if (bTriggered)
	{
		ZoneBox->OnComponentBeginOverlap.RemoveDynamic(
			this,
			&ThisClass::HandleBeginOverlap);
	}
	else
	{
		ZoneBox->OnComponentBeginOverlap.AddUniqueDynamic(
			this,
			&ThisClass::HandleBeginOverlap);
	}
}

void AIGZoneTrigger::HandleStoryStateChanged(
	const FGameplayTag StateTag,
	const bool bAdded)
{
	if (!StateTagOnEnter.IsValid()
		|| !StateTag.MatchesTagExact(StateTagOnEnter))
	{
		return;
	}

	bTriggered = bAdded;
	if (bTriggered)
	{
		ZoneBox->OnComponentBeginOverlap.RemoveDynamic(
			this,
			&ThisClass::HandleBeginOverlap);
	}
	else
	{
		ZoneBox->OnComponentBeginOverlap.AddUniqueDynamic(
			this,
			&ThisClass::HandleBeginOverlap);
	}
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

	if (RequiredNightIndex != INDEX_NONE || !RequiredNarrativeBeat.IsNone())
	{
		const UGameInstance* Instance = GetGameInstance();
		const UIGMissingFloorNarrativeSubsystem* Narrative = Instance
			? Instance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>() : nullptr;
		if (!Narrative
			|| (RequiredNightIndex != INDEX_NONE && Narrative->GetNightIndex() != RequiredNightIndex)
			|| (!RequiredNarrativeBeat.IsNone() && !Narrative->HasBeatPlayed(RequiredNarrativeBeat)))
		{
			return;
		}
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
