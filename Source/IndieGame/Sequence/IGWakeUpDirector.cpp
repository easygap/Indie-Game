#include "Sequence/IGWakeUpDirector.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "IndieGame.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/IGAlarmClock.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGInteractionComponent.h"
#include "Save/IGSaveSubsystem.h"
#include "TimerManager.h"

AIGWakeUpDirector::AIGWakeUpDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AIGWakeUpDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bNativeGetUpActive || WakeState != EIGWakeState::GettingUp)
	{
		SetActorTickEnabled(false);
		return;
	}

	// Three-beat get-up: roll upright while sitting, a breath of stillness,
	// then rise with a slight forward lean and an overshoot that settles.
	UCameraComponent* Camera = ResolvePlayerCamera();
	APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const float Duration = FMath::Max(GettingUpFallbackDuration, KINDA_SMALL_NUMBER);
	NativeGetUpElapsed += FMath::Max(0.0f, DeltaSeconds);
	const float Alpha = FMath::Clamp(NativeGetUpElapsed / Duration, 0.0f, 1.0f);

	const float SitAlpha = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(Alpha / 0.42f, 0.0f, 1.0f));
	const float StandAlpha = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp((Alpha - 0.54f) / 0.46f, 0.0f, 1.0f));
	const float BreathDip =
		(Alpha > 0.40f && Alpha < 0.58f)
			? FMath::Sin((Alpha - 0.40f) / 0.18f * UE_PI) * -1.6f
			: 0.0f;

	const float StartZ = GetUpStartCameraTransform.GetLocation().Z;
	const float EndZ = StandingCameraTransform.GetLocation().Z;
	const float SitZ = FMath::Lerp(StartZ, StartZ + (EndZ - StartZ) * 0.42f, SitAlpha);
	const float Overshoot = FMath::Sin(StandAlpha * UE_PI) * (1.0f - StandAlpha) * 2.2f;
	const float CameraZ = FMath::Lerp(SitZ, EndZ, StandAlpha) + BreathDip + Overshoot;
	const float ForwardLean = FMath::Sin(SitAlpha * UE_PI) * 4.0f
		+ FMath::Sin(StandAlpha * UE_PI) * 5.0f;

	if (Camera)
	{
		Camera->SetRelativeLocation(FVector(ForwardLean, 0.0f, CameraZ));
	}

	if (PlayerController)
	{
		// Roll from lying-on-the-pillow back to level as the body sits up.
		FRotator ControlRotation = PlayerController->GetControlRotation();
		ControlRotation.Roll = FMath::Lerp(GetUpStartControlRoll, 0.0f, SitAlpha);
		ControlRotation.Pitch = FMath::ClampAngle(FMath::Lerp(
			FRotator::NormalizeAxis(GetUpStartControlPitch),
			-4.0f + 4.0f * StandAlpha,
			SitAlpha), -89.0f, 89.0f);
		PlayerController->SetControlRotation(ControlRotation);
	}

	if (Alpha >= 1.0f)
	{
		CompleteGettingUp();
	}
}

void AIGWakeUpDirector::SetAlarmClock(AIGAlarmClock* InAlarmClock)
{
	if (AlarmClock == InAlarmClock)
	{
		return;
	}

	if (AlarmClock && HasActorBegunPlay())
	{
		AlarmClock->OnAlarmStopped.RemoveDynamic(this, &ThisClass::HandleAlarmClockStopped);
	}

	AlarmClock = InAlarmClock;
	if (AlarmClock && HasActorBegunPlay())
	{
		AlarmClock->OnAlarmStopped.AddUniqueDynamic(this, &ThisClass::HandleAlarmClockStopped);
		if (WakeState != EIGWakeState::NotStarted && WakeState != EIGWakeState::FreeRoam)
		{
			AlarmClock->StartAlarm();
			AlarmClock->SetInteractionEnabled(WakeState == EIGWakeState::AwaitAlarm);
		}
	}
}

void AIGWakeUpDirector::BeginPlay()
{
	Super::BeginPlay();
	ResolveDefaultTags();
	ResolvePlayerCamera();

	if (AlarmClock)
	{
		AlarmClock->OnAlarmStopped.AddUniqueDynamic(this, &ThisClass::HandleAlarmClockStopped);
	}

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

	if (HasStoryState(StandingStateTag))
	{
		RestoreStandingCheckpoint();
	}
	else if (bAutoStart)
	{
		StartWakeUp();
	}
}

void AIGWakeUpDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TransitionTimerHandle);

	if (AlarmClock)
	{
		AlarmClock->OnAlarmStopped.RemoveDynamic(this, &ThisClass::HandleAlarmClockStopped);
	}

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

	SetMovementLocked(false);
	SetLookLocked(false);
	SetInteractionLocked(false);
	Super::EndPlay(EndPlayReason);
}

void AIGWakeUpDirector::ResetForNewChapter()
{
	// Drop every transition/presentation bit from the previous run. Restoring
	// the standing pose first prevents a mid-animation reset from becoming the
	// next morning's baseline; StartWakeUp applies the lying pose afterward.
	GetWorldTimerManager().ClearTimer(TransitionTimerHandle);
	bNativeGetUpActive = false;
	NativeGetUpElapsed = 0.0f;
	GetUpStartCameraTransform = FTransform::Identity;
	GetUpStartControlRoll = 0.0f;
	GetUpStartControlPitch = 0.0f;
	SetActorTickEnabled(false);
	RestoreStandingCameraPose();

	// Stop any sound without emitting the alarm-completed delegate, then clear
	// the alarm's one-shot latch so StartWakeUp can ring it again.
	if (AlarmClock)
	{
		AlarmClock->ApplyRestoredStoppedState();
		AlarmClock->Rearm();
		AlarmClock->SetInteractionEnabled(false);
	}

	bAlarmStopped = false;
	bAlarmStopPending = false;
	SetMovementLocked(false);
	SetLookLocked(false);
	SetInteractionLocked(false);
	TransitionTo(EIGWakeState::NotStarted);
}

void AIGWakeUpDirector::ConfigureChapterSaveTags(
	const FGameplayTag InChapterId,
	const FGameplayTag InAlarmStoppedStateTag,
	const FGameplayTag InStandingStateTag,
	const FGameplayTag InStandingCheckpointTag,
	const bool bInAutosaveWhenStanding)
{
	if (InChapterId.IsValid())
	{
		ChapterId = InChapterId;
	}
	if (InAlarmStoppedStateTag.IsValid())
	{
		AlarmStoppedStateTag = InAlarmStoppedStateTag;
	}
	if (InStandingStateTag.IsValid())
	{
		StandingStateTag = InStandingStateTag;
	}
	if (InStandingCheckpointTag.IsValid())
	{
		StandingCheckpointTag = InStandingCheckpointTag;
	}
	bAutosaveWhenStanding = bInAutosaveWhenStanding;
}

bool AIGWakeUpDirector::StartWakeUp()
{
	if (WakeState != EIGWakeState::NotStarted)
	{
		return WakeState != EIGWakeState::FreeRoam;
	}

	SetMovementLocked(true);
	SetLookLocked(true);
	SetInteractionLocked(true);
	TransitionTo(EIGWakeState::FadeIn);
	ApplyLyingCameraPose();

	if (APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr)
	{
		if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
		{
			CameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false);
			if (FadeInDuration > KINDA_SMALL_NUMBER)
			{
				CameraManager->StartCameraFade(
					1.0f,
					0.0f,
					FadeInDuration,
					FLinearColor::Black,
					false,
					false);
			}
			else
			{
				CameraManager->StopCameraFade();
			}
		}
	}

	if (AlarmClock)
	{
		AlarmClock->StartAlarm();
		// The sound begins under the opening fade, but interaction only opens in AwaitAlarm.
		AlarmClock->SetInteractionEnabled(false);
	}

	PresentWakeStarted();
	if (WakeState != EIGWakeState::FadeIn)
	{
		return true;
	}

	if (FadeInDuration <= KINDA_SMALL_NUMBER)
	{
		NotifyEyesOpened();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			TransitionTimerHandle,
			this,
			&ThisClass::HandleFadeTimerElapsed,
			FadeInDuration,
			false);
	}

	return true;
}

bool AIGWakeUpDirector::NotifyEyesOpened()
{
	if (WakeState == EIGWakeState::AwaitAlarm || WakeState == EIGWakeState::BedLocked)
	{
		return true;
	}

	if (WakeState != EIGWakeState::FadeIn)
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(TransitionTimerHandle);
	TransitionTo(EIGWakeState::BedLocked);
	PresentEyesOpened();
	if (WakeState != EIGWakeState::BedLocked)
	{
		return WakeState == EIGWakeState::AwaitAlarm
			|| WakeState == EIGWakeState::GettingUp
			|| WakeState == EIGWakeState::FreeRoam;
	}

	TransitionTo(EIGWakeState::AwaitAlarm);
	SetLookLocked(false);
	SetInteractionLocked(false);

	if (bAlarmStopPending || (AlarmClock && AlarmClock->HasCompleted()))
	{
		bAlarmStopPending = false;
		NotifyAlarmStopped();
	}
	else if (AlarmClock)
	{
		AlarmClock->SetInteractionEnabled(true);
	}

	return true;
}

bool AIGWakeUpDirector::NotifyAlarmStopped()
{
	if (bAlarmStopped)
	{
		return true;
	}

	if (AlarmClock && !AlarmClock->HasCompleted())
	{
		AlarmClock->ApplyRestoredStoppedState();
	}

	if (WakeState != EIGWakeState::AwaitAlarm)
	{
		// Preserve an early Blueprint/direct call and consume it once AwaitAlarm is entered.
		bAlarmStopPending = true;
		return true;
	}

	bAlarmStopPending = false;
	bAlarmStopped = true;
	AddStoryState(AlarmStoppedStateTag);
	TransitionTo(EIGWakeState::BedLocked);
	PresentAlarmStopped();
	return true;
}

bool AIGWakeUpDirector::RequestStopAlarmFallback()
{
	if (WakeState != EIGWakeState::AwaitAlarm || bAlarmStopped)
	{
		return false;
	}

	if (AlarmClock && !AlarmClock->HasCompleted())
	{
		return AlarmClock->StopAlarm();
	}

	NotifyAlarmStopped();
	return true;
}

bool AIGWakeUpDirector::RequestGetUp()
{
	if (WakeState == EIGWakeState::GettingUp || WakeState == EIGWakeState::FreeRoam)
	{
		return true;
	}

	if (WakeState != EIGWakeState::BedLocked || !bAlarmStopped)
	{
		return false;
	}

	SetLookLocked(true);
	SetInteractionLocked(true);
	TransitionTo(EIGWakeState::GettingUp);
	BeginNativeGetUpPresentation();
	PresentGettingUp();
	if (WakeState != EIGWakeState::GettingUp)
	{
		return WakeState == EIGWakeState::FreeRoam;
	}

	if (GettingUpFallbackDuration <= KINDA_SMALL_NUMBER)
	{
		CompleteGettingUp();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			TransitionTimerHandle,
			this,
			&ThisClass::HandleGettingUpFallback,
			GettingUpFallbackDuration,
			false);
	}

	return true;
}

bool AIGWakeUpDirector::CompleteGettingUp()
{
	if (WakeState == EIGWakeState::FreeRoam)
	{
		return true;
	}

	if (WakeState != EIGWakeState::GettingUp)
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(TransitionTimerHandle);
	bNativeGetUpActive = false;
	SetActorTickEnabled(false);
	RestoreStandingCameraPose();
	TransitionTo(EIGWakeState::FreeRoam);
	AddStoryState(StandingStateTag);
	SetMovementLocked(false);
	SetLookLocked(false);
	SetInteractionLocked(false);
	PresentFreeRoamRestored();
	SaveStandingCheckpoint();
	return true;
}

void AIGWakeUpDirector::RestoreStandingCheckpoint()
{
	GetWorldTimerManager().ClearTimer(TransitionTimerHandle);
	bNativeGetUpActive = false;
	SetActorTickEnabled(false);
	RestoreStandingCameraPose();
	bAlarmStopped = true;
	TransitionTo(EIGWakeState::FreeRoam);
	AddStoryState(AlarmStoppedStateTag);
	AddStoryState(StandingStateTag);

	if (AlarmClock)
	{
		AlarmClock->ApplyRestoredStoppedState();
	}

	SetMovementLocked(false);
	SetLookLocked(false);
	SetInteractionLocked(false);
	PresentFreeRoamRestored();
}

void AIGWakeUpDirector::HandleAlarmClockStopped(AIGAlarmClock* StoppedAlarm)
{
	if (StoppedAlarm == AlarmClock)
	{
		NotifyAlarmStopped();
	}
}

void AIGWakeUpDirector::HandleStoryStateChanged(const FGameplayTag StateTag, const bool bAdded)
{
	if (bAdded
		&& StateTag.MatchesTagExact(StandingStateTag)
		&& WakeState != EIGWakeState::FreeRoam)
	{
		RestoreStandingCheckpoint();
	}
}

void AIGWakeUpDirector::HandleFadeTimerElapsed()
{
	NotifyEyesOpened();
}

void AIGWakeUpDirector::HandleGettingUpFallback()
{
	CompleteGettingUp();
}

bool AIGWakeUpDirector::TransitionTo(const EIGWakeState NewState)
{
	if (WakeState == NewState)
	{
		return false;
	}

	const EIGWakeState PreviousState = WakeState;
	WakeState = NewState;
	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("Wake state changed: %s -> %s"),
		*UEnum::GetValueAsString(PreviousState),
		*UEnum::GetValueAsString(NewState));
	OnWakeStateChanged.Broadcast(PreviousState, NewState);
	return true;
}

void AIGWakeUpDirector::ResolveDefaultTags()
{
	if (!ChapterId.IsValid())
	{
		ChapterId = FGameplayTag::RequestGameplayTag(FName(TEXT("Chapter.CH01")), false);
	}

	if (!AlarmStoppedStateTag.IsValid())
	{
		AlarmStoppedStateTag = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH01.Wake.AlarmStopped")),
			false);
	}

	if (!StandingStateTag.IsValid())
	{
		StandingStateTag = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH01.Wake.Standing")),
			false);
	}

	if (!StandingCheckpointTag.IsValid())
	{
		StandingCheckpointTag = FGameplayTag::RequestGameplayTag(
			FName(TEXT("Checkpoint.CH01.BedroomStanding")),
			false);
	}
}

void AIGWakeUpDirector::SetMovementLocked(const bool bLocked)
{
	if (bMovementLocked == bLocked)
	{
		return;
	}

	if (APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr)
	{
		PlayerController->SetIgnoreMoveInput(bLocked);
		bMovementLocked = bLocked;
	}
}

void AIGWakeUpDirector::SetLookLocked(const bool bLocked)
{
	if (bLookInputLocked == bLocked)
	{
		return;
	}

	if (APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr)
	{
		PlayerController->SetIgnoreLookInput(bLocked);
		bLookInputLocked = bLocked;
	}
}

void AIGWakeUpDirector::SetInteractionLocked(const bool bLocked)
{
	bInteractionLocked = bLocked;

	APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (UIGInteractionComponent* InteractionComponent = PlayerPawn
		? PlayerPawn->FindComponentByClass<UIGInteractionComponent>()
		: nullptr)
	{
		InteractionComponent->SetInteractionInputEnabled(!bLocked);
	}
}

void AIGWakeUpDirector::AddStoryState(const FGameplayTag StateTag) const
{
	if (StateTag.IsValid())
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UIGStoryStateSubsystem* StoryState =
				GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
			{
				StoryState->AddState(StateTag);
			}
		}
	}
}

bool AIGWakeUpDirector::HasStoryState(const FGameplayTag StateTag) const
{
	if (!StateTag.IsValid())
	{
		return false;
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			return StoryState->HasState(StateTag);
		}
	}

	return false;
}

void AIGWakeUpDirector::SaveStandingCheckpoint()
{
	if (!bAutosaveWhenStanding)
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UIGSaveSubsystem>())
		{
			const FName MapPackageName = GetWorld() ? GetWorld()->GetOutermost()->GetFName() : NAME_None;
			SaveSubsystem->RequestAutosave(ChapterId, MapPackageName, StandingCheckpointTag);
		}
	}
}

UCameraComponent* AIGWakeUpDirector::ResolvePlayerCamera()
{
	if (CachedPlayerCamera.IsValid())
	{
		return CachedPlayerCamera.Get();
	}

	APlayerController* PlayerController = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	UCameraComponent* Camera = PlayerPawn
		? PlayerPawn->FindComponentByClass<UCameraComponent>()
		: nullptr;
	if (Camera)
	{
		CachedPlayerCamera = Camera;
		StandingCameraTransform = Camera->GetRelativeTransform();
	}
	return Camera;
}

void AIGWakeUpDirector::ApplyLyingCameraPose()
{
	if (!bUseNativeCameraPresentation)
	{
		return;
	}

	if (UCameraComponent* Camera = ResolvePlayerCamera())
	{
		Camera->SetRelativeLocationAndRotation(LyingCameraLocation, LyingCameraRotation);
	}

	// Head on the pillow: the view genuinely lies on its side until sitting up.
	if (APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		FRotator ControlRotation = PlayerController->GetControlRotation();
		ControlRotation.Roll = -52.0f;
		PlayerController->SetControlRotation(ControlRotation);
	}
}

void AIGWakeUpDirector::BeginNativeGetUpPresentation()
{
	bNativeGetUpActive = false;
	if (!bUseNativeCameraPresentation)
	{
		return;
	}

	if (UCameraComponent* Camera = ResolvePlayerCamera())
	{
		GetUpStartCameraTransform = Camera->GetRelativeTransform();
		if (const APlayerController* PlayerController =
			GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			const FRotator ControlRotation = PlayerController->GetControlRotation();
			GetUpStartControlRoll = FRotator::NormalizeAxis(ControlRotation.Roll);
			GetUpStartControlPitch = ControlRotation.Pitch;
		}
		NativeGetUpElapsed = 0.0f;
		bNativeGetUpActive = true;
		SetActorTickEnabled(true);
	}
}

void AIGWakeUpDirector::RestoreStandingCameraPose()
{
	if (UCameraComponent* Camera = ResolvePlayerCamera())
	{
		Camera->SetRelativeTransform(StandingCameraTransform);
	}

	// Never leave residual roll on the standing view.
	if (APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		FRotator ControlRotation = PlayerController->GetControlRotation();
		ControlRotation.Roll = 0.0f;
		PlayerController->SetControlRotation(ControlRotation);
	}
}
