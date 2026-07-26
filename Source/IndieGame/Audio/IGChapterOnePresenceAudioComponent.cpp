#include "Audio/IGChapterOnePresenceAudioComponent.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "TimerManager.h"

namespace IGChapterOnePresence
{
	// Fixed cadence keeps automated walkthroughs reproducible. The unequal
	// gaps stop the scrape from becoming an obvious game-audio loop.
	constexpr float InitialCardboardDelay = 1.8f;
	constexpr float CardboardIntervals[] = {17.5f, 23.0f, 15.5f, 20.5f};
}

UIGChapterOnePresenceAudioComponent::UIGChapterOnePresenceAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UIGChapterOnePresenceAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	EnteredStoreTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("State.CH01.Morning.EnteredStore")),
		false);
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->OnStoryStateTagChanged.AddUniqueDynamic(
				this,
				&ThisClass::HandleStoryStateChanged);
		}
	}
	SetPresenceEnabled(true);
}

void UIGChapterOnePresenceAudioComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->OnStoryStateTagChanged.RemoveDynamic(
				this,
				&ThisClass::HandleStoryStateChanged);
		}
	}
	SetPresenceEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void UIGChapterOnePresenceAudioComponent::SetPresenceEnabled(const bool bEnabled)
{
	if (bPresenceEnabled == bEnabled)
	{
		return;
	}
	bPresenceEnabled = bEnabled;

	if (!bPresenceEnabled)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CardboardTimerHandle);
		}
		bCardboardScheduleStarted = false;
		if (PrayerRadioComponent)
		{
			PrayerRadioComponent->Stop();
			PrayerRadioComponent->DestroyComponent();
			PrayerRadioComponent = nullptr;
		}
		return;
	}

	StartPrayerRadio();
	if (const UGameInstance* GameInstance =
		GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>();
			StoryState && StoryState->HasState(EnteredStoreTag))
		{
			StartStorePresence();
		}
	}
}

void UIGChapterOnePresenceAudioComponent::HandleStoryStateChanged(
	const FGameplayTag StateTag,
	const bool bAdded)
{
	if (bAdded && StateTag.MatchesTagExact(EnteredStoreTag))
	{
		StartStorePresence();
	}
}

void UIGChapterOnePresenceAudioComponent::StartStorePresence()
{
	if (!bPresenceEnabled || bCardboardScheduleStarted)
	{
		return;
	}
	bCardboardScheduleStarted = true;
	CardboardIntervalIndex = 0;
	ScheduleNextCardboardDrag(IGChapterOnePresence::InitialCardboardDelay);
}

void UIGChapterOnePresenceAudioComponent::StartPrayerRadio()
{
	AActor* Owner = GetOwner();
	if (!Owner || PrayerRadioComponent)
	{
		return;
	}

	PrayerRadioComponent = NewObject<UAudioComponent>(
		Owner,
		TEXT("ChapterOnePrayerRadio"));
	PrayerRadioComponent->bAutoActivate = false;
	PrayerRadioComponent->bAutoDestroy = false;
	PrayerRadioComponent->bOverrideAttenuation = true;
	PrayerRadioComponent->AttenuationOverrides.bAttenuate = true;
	PrayerRadioComponent->AttenuationOverrides.bSpatialize = true;
	PrayerRadioComponent->AttenuationOverrides.DistanceAlgorithm =
		EAttenuationDistanceModel::NaturalSound;
	PrayerRadioComponent->AttenuationOverrides.AttenuationShapeExtents =
		FVector(38.0f, 0.0f, 0.0f);
	PrayerRadioComponent->AttenuationOverrides.FalloffDistance = 430.0f;
	PrayerRadioComponent->AttenuationOverrides.dBAttenuationAtMax = -58.0f;
	PrayerRadioComponent->SetSound(
		UIGToneSequenceSoundWave::CreateMuffledPrayerRadio(Owner));
	PrayerRadioComponent->SetVolumeMultiplier(0.42f);
	PrayerRadioComponent->RegisterComponent();
	PrayerRadioComponent->SetWorldLocation(PrayerRadioWorldLocation);
	PrayerRadioComponent->Play();
}

void UIGChapterOnePresenceAudioComponent::ScheduleNextCardboardDrag(
	const float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (!bPresenceEnabled || !World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		CardboardTimerHandle,
		this,
		&ThisClass::PlayCardboardDrag,
		FMath::Max(0.1f, DelaySeconds),
		false);
}

void UIGChapterOnePresenceAudioComponent::PlayCardboardDrag()
{
	if (!bPresenceEnabled)
	{
		return;
	}

	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateCardboardDrag(this),
		StoreBackroomWorldLocation,
		0.56f,
		1.0f,
		55.0f,
		760.0f);

	const int32 IntervalCount =
		static_cast<int32>(UE_ARRAY_COUNT(IGChapterOnePresence::CardboardIntervals));
	const float NextDelay =
		IGChapterOnePresence::CardboardIntervals[CardboardIntervalIndex % IntervalCount];
	++CardboardIntervalIndex;
	ScheduleNextCardboardDrag(NextDelay);
}
