#include "Entity/IGMissingFloorFifthDawnDirector.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
#include "Audio/IGAudioHelpers.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Entity/IGReplaySkip.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "IndieGame.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "TimerManager.h"

namespace IGFifthDawn
{
	constexpr float DurationSeconds = 160.0f;
	constexpr float ReplaySkipDurationSeconds = IGReplaySkip::HoldSeconds;
	constexpr float ReplaySkipRewindMultiplier = IGReplaySkip::RewindMultiplier;
	constexpr const TCHAR* ProfileSection = TEXT("IndieGame.MissingFloorProfile");
	constexpr const TCHAR* ExperiencedKey = TEXT("FifthDawnExperienced");
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
	// 재관람 스킵의 연속 진행률을 그리는 동안에만 Tick을 깨운다.
	// 입력이 끝나고 되감기까지 완료되면 즉시 다시 비활성화한다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AIGMissingFloorFifthDawnDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive || !bReplaySkipAvailable)
	{
		SetActorTickEnabled(false);
		return;
	}

	const float RequiredSeconds = GetReplaySkipDurationSeconds();
	const float SafeDelta = FMath::Max(DeltaSeconds, 0.0f);
	if (bReplaySkipInputActive)
	{
		ReplaySkipProgress = FMath::Min(
			ReplaySkipProgress + SafeDelta / RequiredSeconds,
			1.0f);
	}
	else if (bReplaySkipRewinding)
	{
		ReplaySkipProgress = FMath::Max(
			ReplaySkipProgress
				- SafeDelta * IGFifthDawn::ReplaySkipRewindMultiplier / RequiredSeconds,
			0.0f);
		bReplaySkipRewinding = ReplaySkipProgress > 0.0f;
	}

	UpdateSensoryHudSkip();
	if (ReplaySkipProgress >= 1.0f)
	{
		FinishInterlude();
		return;
	}
	if (!bReplaySkipInputActive && !bReplaySkipRewinding)
	{
		SetActorTickEnabled(false);
	}
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
	ReplaySkipProgress = 0.0f;
	bReplaySkipInputActive = false;
	bReplaySkipRewinding = false;
	bReplayAvailabilityForcedForSession =
		FParse::Param(FCommandLine::Get(), TEXT("IGFifthDawnReplay"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGListenerGreyboxProbe"));
	bReplaySkipAvailable = HasExperiencedInterludeProfile()
		|| bReplayAvailabilityForcedForSession;
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
	if (UIGMissingFloorAudioSubsystem* AudioDirector =
		GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>())
	{
		AudioDirector->RegisterComponent(WaterBed, EIGAudioBus::World);
	}
	WaterBed->Play();

	PrayerBed->RegisterComponent();
	PrayerBed->SetSound(
		UIGToneSequenceSoundWave::CreateMuffledPrayerRadio(this));
	PrayerBed->bAllowSpatialization = false;
	PrayerBed->bAutoDestroy = false;
	PrayerBed->SetVolumeMultiplier(0.12f);
	if (UIGMissingFloorAudioSubsystem* AudioDirector =
		GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>())
	{
		AudioDirector->RegisterComponent(PrayerBed, EIGAudioBus::World);
	}
	PrayerBed->Play();

	BreathBed->RegisterComponent();
	BreathBed->SetSound(
		UIGToneSequenceSoundWave::CreateTrappedBreathBed(this));
	BreathBed->bAllowSpatialization = false;
	BreathBed->bAutoDestroy = false;
	BreathBed->SetVolumeMultiplier(0.18f);
	if (UIGMissingFloorAudioSubsystem* AudioDirector =
		GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>())
	{
		AudioDirector->RegisterComponent(BreathBed, EIGAudioBus::Player);
	}
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
	// 안내만 보일 때는 정적이다. 홀드하거나 진행률을 되감을 때만 Tick을 켠다.
	SetActorTickEnabled(false);
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
				"[가까이] 얕은 숨  ·  [오른쪽] 물이 흐르는 소리"),
			4.0f);
		break;
	case 1:
		// 24 seconds: a concrete audio change before the 30-second error line.
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateDoorThud(this),
			SoundOrigin,
			0.24f,
			1.0f,
			160.0f,
			1200.0f,
			EIGAudioBus::World);
		PushDirectionCaption(
			NSLOCTEXT(
				"IGMissingFloor",
				"FifthDawnCaptionWaterShift",
				"[머리 위] 배관이 크게 덜컹거린다"),
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
				"[7월 29일] 벽을 긁는 소리"),
			2.8f);
		break;
	case 4:
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateAnswerKnockPattern(this, 0.84f),
			SoundOrigin,
			0.52f,
			1.0f,
			140.0f,
			1000.0f,
			EIGAudioBus::Player);
		break;
	case 5:
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateAnswerKnockPattern(this, 0.93f),
			SoundOrigin,
			0.38f,
			1.0f,
			140.0f,
			1400.0f,
			EIGAudioBus::Entity);
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
				"[7월 30일] 느리고 약한 숨소리"),
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
			0.30f,
			1.0f,
			120.0f,
			900.0f,
			EIGAudioBus::Player);
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
				"[가까이] 희미한 숨소리"),
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
	if (ElapsedSeconds >= IGFifthDawn::CueTimes[8])
	{
		// 7월 31일 뒤. 손은 움직이는데 소리가 안 난다 — 그가 마지막에 겪은
		// 것을 손가락으로 겪는다. 입력은 먹고, 벽은 답하지 않는다.
		++PlayerKnockCount;
		return true;
	}
	const float Muffle = FMath::Clamp(0.18f + PlayerKnockCount * 0.075f, 0.18f, 0.90f);
	++PlayerKnockCount;
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockSingle(this, Muffle),
		Player.IsValid() ? Player->GetActorLocation() : GetActorLocation(),
		FMath::Max(0.24f, 0.72f - PlayerKnockCount * 0.035f),
		1.0f,
		120.0f,
		900.0f,
		EIGAudioBus::Player);
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

bool AIGMissingFloorFifthDawnDirector::BeginReplaySkipInput()
{
	if (!bActive || !bReplaySkipAvailable)
	{
		return false;
	}

	if (UsesToggleSkipInput() && bReplaySkipInputActive)
	{
		bReplaySkipInputActive = false;
		bReplaySkipRewinding = ReplaySkipProgress > 0.0f;
	}
	else
	{
		bReplaySkipInputActive = true;
		bReplaySkipRewinding = false;
	}
	SetActorTickEnabled(true);
	UpdateSensoryHudSkip();
	return true;
}

bool AIGMissingFloorFifthDawnDirector::EndReplaySkipInput()
{
	if (!bActive || !bReplaySkipAvailable)
	{
		return false;
	}
	if (!UsesToggleSkipInput())
	{
		bReplaySkipInputActive = false;
		bReplaySkipRewinding = ReplaySkipProgress > 0.0f;
		SetActorTickEnabled(true);
		UpdateSensoryHudSkip();
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
	// 자동 검증이 개발 PC의 관람 이력을 재관람 상태로 바꾸면 안 된다.
	FinishInterlude(/*bPersistExperience=*/false);
	return true;
}

void AIGMissingFloorFifthDawnDirector::FinishInterlude(
	const bool bPersistExperience)
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	bReplaySkipInputActive = false;
	bReplaySkipRewinding = false;
	SetActorTickEnabled(false);
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
	if (!Player.Get())
	{
		// 폰이 사라진 채로 끝났다. 복구가 폰에 묶여 있으면 암전과 이동
		// 잠금이 그대로 남는다.
		AbortSlotBlackout(TEXT("slot finished without a pawn"));
	}
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
	if (bPersistExperience && !bReplayAvailabilityForcedForSession)
	{
		PersistInterludeExperience();
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
		Hud->SetSensoryInterludeSkipState(
			bEnabled && bReplaySkipAvailable,
			bEnabled ? ReplaySkipProgress : 0.0f,
			bEnabled && bReplaySkipInputActive,
			GetReplaySkipDurationSeconds(),
			UsesToggleSkipInput());
	}
}

void AIGMissingFloorFifthDawnDirector::UpdateSensoryHudSkip() const
{
	const AIGPlayerCharacter* PlayerCharacter = Player.Get();
	const APlayerController* Controller = PlayerCharacter
		? Cast<APlayerController>(PlayerCharacter->GetController())
		: nullptr;
	if (AIGHorrorHUD* Hud = Controller
		? Cast<AIGHorrorHUD>(Controller->GetHUD())
		: nullptr)
	{
		Hud->SetSensoryInterludeSkipState(
			bReplaySkipAvailable,
			ReplaySkipProgress,
			bReplaySkipInputActive,
			GetReplaySkipDurationSeconds(),
			UsesToggleSkipInput());
	}
}

float AIGMissingFloorFifthDawnDirector::GetReplaySkipDurationSeconds() const
{
	const UIGAccessibilitySubsystem* Accessibility = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const float DurationScale = Accessibility
		? Accessibility->GetHoldDurationScale()
		: 1.0f;
	return FMath::Max(
		IGFifthDawn::ReplaySkipDurationSeconds * DurationScale,
		0.25f);
}

bool AIGMissingFloorFifthDawnDirector::UsesToggleSkipInput() const
{
	const UIGAccessibilitySubsystem* Accessibility = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	return Accessibility && Accessibility->UsesToggleHoldInteractions();
}

bool AIGMissingFloorFifthDawnDirector::HasExperiencedInterludeProfile() const
{
	bool bExperienced = false;
	if (GConfig)
	{
		GConfig->GetBool(
			IGFifthDawn::ProfileSection,
			IGFifthDawn::ExperiencedKey,
			bExperienced,
			GGameUserSettingsIni);
	}
	return bExperienced;
}

void AIGMissingFloorFifthDawnDirector::PersistInterludeExperience() const
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetBool(
		IGFifthDawn::ProfileSection,
		IGFifthDawn::ExperiencedKey,
		true,
		GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void AIGMissingFloorFifthDawnDirector::PushDirectionCaption(
	const FText& Caption,
	const float Seconds) const
{
	AIGHorrorHUD::PushAudioCaption(this, Caption, Seconds);
}

void AIGMissingFloorFifthDawnDirector::AbortSlotBlackout(const TCHAR* Reason)
{
	UWorld* World = GetWorld();
	AIGPlayerCharacter* Character = Player.Get();
	APlayerController* Controller = Character
		? Cast<APlayerController>(Character->GetController())
		: nullptr;
	// 폰이 사라져도 화면을 가진 컨트롤러는 남는다. 암전은 그쪽 카메라
	// 매니저가 들고 있으므로 거기서 걷는다.
	if (!Controller && World)
	{
		Controller = World->GetFirstPlayerController();
	}
	if (Controller && Controller->PlayerCameraManager)
	{
		Controller->PlayerCameraManager->StopCameraFade();
	}
	if (Character)
	{
		if (UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
	UE_LOG(
		LogIndieGame,
		Warning,
		TEXT("IG_NIGHT5_SLOT aborted blackout: %s (controller=%s)"),
		Reason,
		Controller ? TEXT("yes") : TEXT("none"));
}

void AIGMissingFloorFifthDawnDirector::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	// 슬롯은 이동을 잠그고 bHoldWhenFinished로 암전을 걸어 둔 채 30초를 돈다.
	// 그 사이에 디렉터가 사라지면 푸는 쪽이 아무도 없다 — 이벤트도 프롬프트도
	// 없이 검은 화면에 조작만 죽은 상태가 남는다.
	if (bActive && EndPlayReason != EEndPlayReason::LevelTransition
		&& EndPlayReason != EEndPlayReason::EndPlayInEditor
		&& EndPlayReason != EEndPlayReason::Quit)
	{
		AbortSlotBlackout(TEXT("director destroyed while the slot was running"));
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
	SetActorTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}
