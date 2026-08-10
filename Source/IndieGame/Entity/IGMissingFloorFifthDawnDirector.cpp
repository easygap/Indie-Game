#include "Entity/IGMissingFloorFifthDawnDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "TimerManager.h"

namespace IGFifthDawn
{
	constexpr float DurationSeconds = 160.0f;
	// 7/27 start, water shift, 7/28, 7/29 call/reply, 7/30, 7/31,
	// final two knocks, all beds cut, wake.
	constexpr float CueTimes[] =
	{
		0.0f, 24.0f, 24.0f, 52.0f, 58.0f, 74.0f,
		80.0f, 115.0f, 118.0f, 148.0f, 159.2f, DurationSeconds
	};
}

AIGMissingFloorFifthDawnDirector::AIGMissingFloorFifthDawnDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AIGMissingFloorFifthDawnDirector::StartInterlude(
	AIGPlayerCharacter* InPlayer)
{
	if (bActive || !IsValid(InPlayer) || !GetWorld())
	{
		return false;
	}

	Player = InPlayer;
	ElapsedSeconds = 0.0f;
	FiredCueMask = 0;
	PlayerKnockCount = 0;
	NextCueIndex = 1;
	bPlayerListening = false;
	bActive = true;
	StartWorldSeconds = GetWorld()->GetTimeSeconds();

	WaterBed = NewObject<UAudioComponent>(this, TEXT("FifthDawnWaterBed"));
	PrayerBed = NewObject<UAudioComponent>(this, TEXT("FifthDawnPrayerBed"));
	BreathBed = NewObject<UAudioComponent>(this, TEXT("FifthDawnBreathBed"));
	if (!WaterBed || !PrayerBed || !BreathBed)
	{
		bActive = false;
		return false;
	}
	WaterBed->RegisterComponent();
	WaterBed->SetSound(
		UIGToneSequenceSoundWave::CreateFloodedCorridorWaterBed(this));
	WaterBed->bAllowSpatialization = false;
	WaterBed->bAutoDestroy = false;
	WaterBed->SetVolumeMultiplier(0.28f);
	WaterBed->Play();

	PrayerBed->RegisterComponent();
	PrayerBed->SetSound(
		UIGToneSequenceSoundWave::CreateMuffledPrayerRadio(this));
	PrayerBed->bAllowSpatialization = false;
	PrayerBed->bAutoDestroy = false;
	PrayerBed->SetVolumeMultiplier(0.12f);
	PrayerBed->Play();

	BreathBed->RegisterComponent();
	BreathBed->SetSound(
		UIGToneSequenceSoundWave::CreateTrappedBreathBed(this));
	BreathBed->bAllowSpatialization = false;
	BreathBed->bAutoDestroy = false;
	BreathBed->SetVolumeMultiplier(0.18f);
	BreathBed->Play();

	InPlayer->GetCharacterMovement()->DisableMovement();
	if (APlayerController* Controller =
		Cast<APlayerController>(InPlayer->GetController()))
	{
		if (Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->StartCameraFade(
				0.0f,
				1.0f,
				0.85f,
				FLinearColor::Black,
				false,
				true);
		}
	}
	SetSensoryHud(true);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGMissingFloorNarrativeSubsystem* Narrative =
			GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>())
		{
			// Also blocks H/F9 while the scene is dark; the physical day seal is
			// still owned by AIGNightPhaseDirector.
			Narrative->SetHourSealed(true);
		}
	}

	FireCue(0);
	ScheduleNextCue();
	return true;
}

void AIGMissingFloorFifthDawnDirector::ScheduleNextCue()
{
	UWorld* World = GetWorld();
	if (!bActive || !World
		|| NextCueIndex >= static_cast<int32>(UE_ARRAY_COUNT(IGFifthDawn::CueTimes)))
	{
		return;
	}

	// 각 큐를 이전 큐가 아니라 시작 시각에 맞춰 예약한다. 한 프레임이
	// 늦어져도 160초 타임라인 전체에 오차가 누적되지 않는다.
	const double WorldElapsed = World->GetTimeSeconds() - StartWorldSeconds;
	const float Delay = FMath::Max(
		static_cast<float>(IGFifthDawn::CueTimes[NextCueIndex] - WorldElapsed),
		KINDA_SMALL_NUMBER);
	World->GetTimerManager().SetTimer(
		CueTimerHandle,
		this,
		&AIGMissingFloorFifthDawnDirector::HandleNextCue,
		Delay,
		false);
}

void AIGMissingFloorFifthDawnDirector::HandleNextCue()
{
	if (!bActive
		|| NextCueIndex >= static_cast<int32>(UE_ARRAY_COUNT(IGFifthDawn::CueTimes)))
	{
		return;
	}

	// 24초 경계처럼 같은 시각에 놓인 큐는 인덱스 순서로 한 번에 처리한다.
	const float CueTime = IGFifthDawn::CueTimes[NextCueIndex];
	ElapsedSeconds = CueTime;
	do
	{
		FireCue(NextCueIndex);
		++NextCueIndex;
	}
	while (bActive
		&& NextCueIndex < static_cast<int32>(UE_ARRAY_COUNT(IGFifthDawn::CueTimes))
		&& FMath::IsNearlyEqual(IGFifthDawn::CueTimes[NextCueIndex], CueTime));

	ScheduleNextCue();
}

void AIGMissingFloorFifthDawnDirector::FireCue(const int32 CueIndex)
{
	if (CueIndex < 0 || CueIndex >= 32
		|| (FiredCueMask & (1u << CueIndex)) != 0)
	{
		return;
	}
	FiredCueMask |= 1u << CueIndex;

	const FVector SoundOrigin = Player.IsValid()
		? Player->GetActorLocation()
		: GetActorLocation();
	switch (CueIndex)
	{
	case 0:
		PushDirectionCaption(
			NSLOCTEXT(
				"IGMissingFloor",
				"FifthDawnCaptionStart",
				"[가까이] 얕은 숨  ·  [오른쪽] 물이 천천히 밀린다"),
			4.0f);
		break;
	case 1:
		// 24 seconds: a concrete audio change before the 30-second error line.
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateDoorThud(this),
			SoundOrigin,
			0.24f);
		PushDirectionCaption(
			NSLOCTEXT(
				"IGMissingFloor",
				"FifthDawnCaptionWaterShift",
				"[머리 위] 배관이 한 번 크게 밀린다"),
			2.6f);
		break;
	case 2:
		if (PrayerBed)
		{
			PrayerBed->SetPitchMultiplier(0.97f);
		}
		if (BreathBed)
		{
			BreathBed->SetVolumeMultiplier(bPlayerListening ? 0.025f : 0.16f);
		}
		PushDirectionCaption(
			NSLOCTEXT(
				"IGMissingFloor",
				"FifthDawnCaptionSecondDawn",
				"[7월 28일] 발소리와 예불이 더 멀어진다"),
			2.8f);
		break;
	case 3:
		if (BreathBed)
		{
			BreathBed->SetVolumeMultiplier(bPlayerListening ? 0.025f : 0.14f);
		}
		PushDirectionCaption(
			NSLOCTEXT(
				"IGMissingFloor",
				"FifthDawnCaptionThirdDawn",
				"[7월 29일] 벽 가까이에서 손이 움직인다"),
			2.8f);
		break;
	case 4:
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateAnswerKnockPattern(this, 0.84f),
			SoundOrigin,
			0.52f);
		break;
	case 5:
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateAnswerKnockPattern(this, 0.93f),
			SoundOrigin,
			0.38f);
		PushDirectionCaption(
			NSLOCTEXT(
				"IGMissingFloor",
				"FifthDawnCaptionHwangReply",
				"[아주 멀리 · 7월 29일] 둘, 쉬고, 하나"),
			3.2f);
		break;
	case 6:
		if (WaterBed)
		{
			WaterBed->SetVolumeMultiplier(bPlayerListening ? 0.42f : 0.22f);
		}
		if (BreathBed)
		{
			BreathBed->SetVolumeMultiplier(bPlayerListening ? 0.025f : 0.10f);
		}
		PushDirectionCaption(
			NSLOCTEXT(
				"IGMissingFloor",
				"FifthDawnCaptionFourthDawn",
				"[7월 30일] 숨 사이가 길어진다"),
			2.8f);
		break;
	case 7:
		if (BreathBed)
		{
			BreathBed->SetVolumeMultiplier(bPlayerListening ? 0.025f : 0.07f);
		}
		PushDirectionCaption(
			NSLOCTEXT(
				"IGMissingFloor",
				"FifthDawnCaptionFinalDawn",
				"[7월 31일] 다섯 번째 새벽"),
			2.4f);
		break;
	case 8:
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateWallKnockReply(this),
			SoundOrigin,
			0.30f);
		PushDirectionCaption(
			NSLOCTEXT(
				"IGMissingFloor",
				"FifthDawnCaptionNoReply",
				"[벽 안] 두 번  ·  대답 없음"),
			3.0f);
		break;
	case 9:
		if (WaterBed)
		{
			WaterBed->Stop();
		}
		if (PrayerBed)
		{
			PrayerBed->Stop();
		}
		PushDirectionCaption(
			NSLOCTEXT(
				"IGMissingFloor",
				"FifthDawnCaptionBreathOnly",
				"[가까이] 숨 하나만 남는다"),
			3.2f);
		break;
	case 10:
		if (BreathBed)
		{
			BreathBed->Stop();
		}
		break;
	case 11:
		FinishInterlude();
		break;
	default:
		break;
	}
}

bool AIGMissingFloorFifthDawnDirector::RegisterPlayerKnock()
{
	if (!bActive)
	{
		return false;
	}
	const float Muffle = FMath::Clamp(0.18f + PlayerKnockCount * 0.075f, 0.18f, 0.90f);
	++PlayerKnockCount;
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockSingle(this, Muffle),
		Player.IsValid() ? Player->GetActorLocation() : GetActorLocation(),
		FMath::Max(0.24f, 0.72f - PlayerKnockCount * 0.035f));
	return true;
}

bool AIGMissingFloorFifthDawnDirector::SetPlayerListening(
	const bool bListening)
{
	if (!bActive)
	{
		return false;
	}
	bPlayerListening = bListening;
	if (WaterBed && WaterBed->IsPlaying())
	{
		WaterBed->SetVolumeMultiplier(bListening ? 0.42f : 0.28f);
	}
	if (PrayerBed && PrayerBed->IsPlaying())
	{
		PrayerBed->SetVolumeMultiplier(bListening ? 0.22f : 0.12f);
	}
	if (BreathBed && BreathBed->IsPlaying())
	{
		const float BreathBase = ElapsedSeconds < 52.0f
			? 0.18f
			: ElapsedSeconds < 80.0f
				? 0.14f
				: ElapsedSeconds < 115.0f
					? 0.10f
					: 0.07f;
		BreathBed->SetVolumeMultiplier(bListening ? 0.025f : BreathBase);
	}
	return true;
}

bool AIGMissingFloorFifthDawnDirector::ValidateTimeline() const
{
	return UE_ARRAY_COUNT(IGFifthDawn::CueTimes) == 12
		&& FMath::IsNearlyEqual(IGFifthDawn::CueTimes[0], 0.0f)
		&& FMath::IsNearlyEqual(IGFifthDawn::CueTimes[1], 24.0f)
		&& FMath::IsNearlyEqual(IGFifthDawn::CueTimes[3], 52.0f)
		&& FMath::IsNearlyEqual(IGFifthDawn::CueTimes[4], 58.0f)
		&& FMath::IsNearlyEqual(IGFifthDawn::CueTimes[5], 74.0f)
		&& FMath::IsNearlyEqual(IGFifthDawn::CueTimes[7], 115.0f)
		&& FMath::IsNearlyEqual(IGFifthDawn::CueTimes[9], 148.0f)
		&& FMath::IsNearlyEqual(IGFifthDawn::CueTimes[10], 159.2f)
		&& FMath::IsNearlyEqual(IGFifthDawn::CueTimes[11], 160.0f);
}

bool AIGMissingFloorFifthDawnDirector::CompleteImmediatelyForProbe()
{
	if (!bActive)
	{
		return false;
	}
	FinishInterlude();
	return true;
}

void AIGMissingFloorFifthDawnDirector::FinishInterlude()
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	GetWorldTimerManager().ClearTimer(CueTimerHandle);
	if (WaterBed)
	{
		WaterBed->Stop();
	}
	if (PrayerBed)
	{
		PrayerBed->Stop();
	}
	if (BreathBed)
	{
		BreathBed->Stop();
	}
	SetSensoryHud(false);
	if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
	{
		PlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		if (APlayerController* Controller =
			Cast<APlayerController>(PlayerCharacter->GetController()))
		{
			if (Controller->PlayerCameraManager)
			{
				Controller->PlayerCameraManager->StartCameraFade(
					1.0f,
					0.0f,
					0.8f,
					FLinearColor::Black,
					false,
					false);
			}
		}
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGMissingFloorNarrativeSubsystem* Narrative =
			GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>())
		{
			Narrative->SetFifthDawnInterludeCompleted(true);
		}
	}
	OnCompleted.Broadcast();
}

void AIGMissingFloorFifthDawnDirector::SetSensoryHud(
	const bool bEnabled) const
{
	const AIGPlayerCharacter* PlayerCharacter = Player.Get();
	const APlayerController* Controller = PlayerCharacter
		? Cast<APlayerController>(PlayerCharacter->GetController())
		: nullptr;
	if (AIGHorrorHUD* Hud = Controller
		? Cast<AIGHorrorHUD>(Controller->GetHUD())
		: nullptr)
	{
		Hud->SetSensoryInterludePresentation(bEnabled);
	}
}

void AIGMissingFloorFifthDawnDirector::PushDirectionCaption(
	const FText& Caption,
	const float Seconds) const
{
	AIGHorrorHUD::PushAudioCaption(this, Caption, Seconds);
}

void AIGMissingFloorFifthDawnDirector::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CueTimerHandle);
	if (WaterBed)
	{
		WaterBed->Stop();
	}
	if (PrayerBed)
	{
		PrayerBed->Stop();
	}
	if (BreathBed)
	{
		BreathBed->Stop();
	}
	SetSensoryHud(false);
	Super::EndPlay(EndPlayReason);
}
