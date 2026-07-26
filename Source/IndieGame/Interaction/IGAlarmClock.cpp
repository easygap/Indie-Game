#include "Interaction/IGAlarmClock.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AIGAlarmClock::AIGAlarmClock()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ClockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClockMesh"));
	ClockMesh->SetupAttachment(SceneRoot);

	AlarmAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AlarmAudio"));
	AlarmAudioComponent->SetupAttachment(SceneRoot);
	AlarmAudioComponent->bAutoActivate = false;
	AlarmAudioComponent->bAutoDestroy = false;

	InteractionPrompt = NSLOCTEXT("IGAlarmClock", "StopAlarmPrompt", "Turn off alarm");
	InteractionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Interaction.AlarmClock")), false);
	InteractionHoldDuration = 0.0f;
}

void AIGAlarmClock::BeginPlay()
{
	Super::BeginPlay();
	if (!InteractionTag.IsValid())
	{
		InteractionTag = FGameplayTag::RequestGameplayTag(
			FName(TEXT("Interaction.AlarmClock")),
			false);
	}

	if (bStartRingingOnBeginPlay)
	{
		StartAlarm();
	}
	else
	{
		SetInteractionEnabled(false);
	}
}

void AIGAlarmClock::Rearm()
{
	bHasCompleted = false;
}

void AIGAlarmClock::StartAlarm()
{
	if (bHasCompleted || bIsRinging)
	{
		return;
	}

	bIsRinging = true;
	SetInteractionEnabled(true);

	if (AlarmAudioComponent && !AlarmAudioComponent->IsPlaying())
	{
		AlarmAudioComponent->Play();
	}
}

bool AIGAlarmClock::StopAlarm()
{
	if (bHasCompleted)
	{
		return false;
	}

	// Commit state before external callbacks, making this re-entrancy safe.
	ApplyRestoredStoppedState();
	OnAlarmStopped.Broadcast(this);
	return true;
}

void AIGAlarmClock::ApplyRestoredStoppedState()
{
	bHasCompleted = true;
	bIsRinging = false;
	SetInteractionEnabled(false);

	if (AlarmAudioComponent)
	{
		AlarmAudioComponent->Stop();
	}
}

bool AIGAlarmClock::CanInteract_Implementation(AActor* Interactor) const
{
	return Super::CanInteract_Implementation(Interactor) && bIsRinging && !bHasCompleted;
}

void AIGAlarmClock::CompleteInteraction_Implementation(const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);
	StopAlarm();
}
