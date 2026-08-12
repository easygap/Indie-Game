#include "Audio/IGMissingFloorAudioSubsystem.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "HAL/PlatformTime.h"

namespace IGMissingFloorMix
{
	constexpr float SilentDecibels = -96.0f;
	constexpr float EntityNearDistance = 600.0f;
	constexpr float EntityFarHysteresis = 640.0f;
	constexpr float EntityDistanceLeaseSeconds = 0.55f;
	constexpr float TitleKnockDelaySeconds = 4.0f;
	constexpr float TitleReplyDelaySeconds = 1.15f;
	constexpr float TitleCycleSeconds = 12.0f;

	constexpr float BaseDecibels[] =
	{
		0.0f,   // ENTITY
		-3.0f,  // PLAYER
		-1.0f,  // PUZZLE
		-8.0f,  // WORLD
		-10.0f, // UI
		-6.0f   // SCORE
	};

	constexpr int32 VoiceCaps[] =
	{
		4,  // ENTITY
		6,  // PLAYER
		6,  // PUZZLE
		12, // WORLD
		8,  // UI: navigation remains responsive under caption churn
		2   // SCORE: current loop plus a release tail
	};

	const TCHAR* BusNames[] =
	{
		TEXT("BUS_ENTITY"),
		TEXT("BUS_PLAYER"),
		TEXT("BUS_PUZZLE"),
		TEXT("BUS_WORLD"),
		TEXT("BUS_UI"),
		TEXT("BUS_SCORE")
	};

	float DecibelsToLinear(const float Decibels)
	{
		return Decibels <= SilentDecibels
			? 0.0f
			: FMath::Pow(10.0f, Decibels / 20.0f);
	}

	int32 ToIndex(const EIGAudioBus Bus)
	{
		return FMath::Clamp(
			static_cast<int32>(Bus),
			0,
			static_cast<int32>(EIGAudioBus::Count) - 1);
	}
}

void UIGMissingFloorAudioSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildBusGraph();
}

void UIGMissingFloorAudioSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (bMixPushed && RuntimeMix)
		{
			UGameplayStatics::PopSoundMixModifier(World, RuntimeMix);
		}
	}
	StopScore(0.0f);
	bMixPushed = false;
	RuntimeMix = nullptr;
	BusSoundClasses.Reset();
	for (TArray<FTrackedVoice>& Voices : ActiveVoices)
	{
		Voices.Reset();
	}
	Super::Deinitialize();
}

void UIGMissingFloorAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (RuntimeMix && !bMixPushed)
	{
		UGameplayStatics::PushSoundMixModifier(&InWorld, RuntimeMix);
		bMixPushed = true;
	}
	RefreshMix(0.0f);
}

void UIGMissingFloorAudioSubsystem::Tick(const float DeltaTime)
{
	if (bTitleMode)
	{
		const double Now = FPlatformTime::Seconds();
		if (PendingTitleReplyRealTime > 0.0
			&& Now >= PendingTitleReplyRealTime)
		{
			PendingTitleReplyRealTime = -1.0;
			PlayTitleReply();
		}
		if (NextTitleKnockRealTime > 0.0 && Now >= NextTitleKnockRealTime)
		{
			PlayTitleKnockCycle();
		}
	}
	VoicePruneAccumulator += DeltaTime;
	if (VoicePruneAccumulator >= 0.25f)
	{
		VoicePruneAccumulator = 0.0f;
		PruneVoices();
	}

	const UWorld* World = GetWorld();
	if (bEntityNearPlayer && World
		&& World->GetTimeSeconds() - LastEntityDistanceUpdateSeconds
			> IGMissingFloorMix::EntityDistanceLeaseSeconds)
	{
		bEntityNearPlayer = false;
		RefreshMix();
	}
}

TStatId UIGMissingFloorAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
		UIGMissingFloorAudioSubsystem,
		STATGROUP_Tickables);
}

void UIGMissingFloorAudioSubsystem::PrepareSound(
	USoundBase* Sound,
	const EIGAudioBus Bus) const
{
	if (!Sound)
	{
		return;
	}
	if (USoundClass* SoundClass = GetBusSoundClass(Bus))
	{
		Sound->SoundClassObject = SoundClass;
	}
}

void UIGMissingFloorAudioSubsystem::RegisterComponent(
	UAudioComponent* Component,
	const EIGAudioBus Bus)
{
	if (!Component)
	{
		return;
	}

	const int32 BusIndex = IGMissingFloorMix::ToIndex(Bus);
	if (BusSoundClasses.IsValidIndex(BusIndex))
	{
		Component->SoundClassOverride = BusSoundClasses[BusIndex];
		PrepareSound(Component->GetSound(), Bus);
	}

	TArray<FTrackedVoice>& Voices = ActiveVoices[BusIndex];
	Voices.RemoveAll([](const FTrackedVoice& Voice)
	{
		return !Voice.Component.IsValid();
	});

	const int32 VoiceCap = GetVoiceCap(Bus);
	while (Voices.Num() >= VoiceCap && Voices.Num() > 0)
	{
		int32 OldestIndex = 0;
		for (int32 Index = 1; Index < Voices.Num(); ++Index)
		{
			if (Voices[Index].Serial < Voices[OldestIndex].Serial)
			{
				OldestIndex = Index;
			}
		}
		if (UAudioComponent* Oldest = Voices[OldestIndex].Component.Get())
		{
			Oldest->FadeOut(0.12f, 0.0f);
		}
		Voices.RemoveAt(OldestIndex);
	}

	Voices.Add({Component, NextVoiceSerial++});
}

void UIGMissingFloorAudioSubsystem::SetThreatState(
	const EIGAudioThreatState NewState)
{
	if (ThreatState == NewState)
	{
		return;
	}
	ThreatState = NewState;
	bEntityListening = NewState == EIGAudioThreatState::Listening;
	RefreshMix();
	if (!bTitleMode)
	{
		SwitchScore(NewState);
	}
}

void UIGMissingFloorAudioSubsystem::SetPlayerListening(const bool bListening)
{
	if (bPlayerListening == bListening)
	{
		return;
	}
	bPlayerListening = bListening;
	RefreshMix();
}

void UIGMissingFloorAudioSubsystem::SetAuthoredSilence(const bool bSilent)
{
	if (bAuthoredSilence == bSilent)
	{
		return;
	}
	bAuthoredSilence = bSilent;
	RefreshMix(bSilent ? 0.08f : 0.45f);
}

void UIGMissingFloorAudioSubsystem::SetEntityDistance(
	const float DistanceCentimeters)
{
	if (const UWorld* World = GetWorld())
	{
		LastEntityDistanceUpdateSeconds = World->GetTimeSeconds();
	}
	const bool bWasNear = bEntityNearPlayer;
	if (bEntityNearPlayer)
	{
		bEntityNearPlayer = DistanceCentimeters
			<= IGMissingFloorMix::EntityFarHysteresis;
	}
	else
	{
		bEntityNearPlayer = DistanceCentimeters
			<= IGMissingFloorMix::EntityNearDistance;
	}
	if (bWasNear != bEntityNearPlayer)
	{
		RefreshMix();
	}
}

void UIGMissingFloorAudioSubsystem::SetTitleMode(const bool bEnabled)
{
	if (bTitleMode == bEnabled)
	{
		return;
	}
	bTitleMode = bEnabled;
	if (bTitleMode)
	{
		StartTitleSoundscape();
	}
	else
	{
		StopTitleSoundscape();
	}
}

void UIGMissingFloorAudioSubsystem::SetUserMasterVolume(
	const float Volume01,
	const float FadeSeconds)
{
	const float Clamped = FMath::Clamp(Volume01, 0.25f, 1.0f);
	if (FMath::IsNearlyEqual(UserMasterVolume, Clamped, 0.001f))
	{
		return;
	}
	UserMasterVolume = Clamped;
	RefreshMix(FadeSeconds);
}

void UIGMissingFloorAudioSubsystem::PlayCalibrationKnock()
{
	if (!GetWorld())
	{
		return;
	}
	++CalibrationKnockPlayCount;
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const APlayerCameraManager* Camera = Controller
		? Controller->PlayerCameraManager
		: nullptr;
	const FVector Location = Camera
		? Camera->GetCameraLocation()
			+ Camera->GetActorForwardVector() * 145.0f
			+ Camera->GetActorRightVector() * 55.0f
			+ FVector::UpVector * 285.0f
		: FVector(145.0f, 55.0f, 285.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockTriple(this, 0.78f),
		Location,
		0.62f,
		1.0f,
		110.0f,
		1650.0f,
		EIGAudioBus::Entity,
		true);
}

void UIGMissingFloorAudioSubsystem::PlayTruthConfirmation(
	const int32 ConfirmationIndex)
{
	UWorld* World = GetWorld();
	if (!World || ConfirmationIndex <= 0)
	{
		return;
	}
	UIGToneSequenceSoundWave* Strike =
		UIGToneSequenceSoundWave::CreateTuningStrike(
			this,
			ConfirmationIndex);
	PrepareSound(Strike, EIGAudioBus::Score);
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(
		World,
		Strike,
		0.82f,
		1.0f,
		0.0f,
		nullptr,
		false,
		true);
	if (Component)
	{
		Component->SetUISound(false);
		RegisterComponent(Component, EIGAudioBus::Score);
		Component->Play();
	}
}

void UIGMissingFloorAudioSubsystem::PlayEndingATuningResolution()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	bAuthoredSilence = false;
	RefreshMix(0.45f);
	StopScore(0.45f);
	ActiveScoreState = EIGAudioThreatState::Finale;
	UIGToneSequenceSoundWave* Resolution =
		UIGToneSequenceSoundWave::CreateTuningMotif(this, true);
	PrepareSound(Resolution, EIGAudioBus::Score);
	ScoreComponent = UGameplayStatics::CreateSound2D(
		World,
		Resolution,
		0.78f,
		1.0f,
		0.0f,
		nullptr,
		false,
		true);
	if (ScoreComponent)
	{
		ScoreComponent->SetUISound(false);
		RegisterComponent(ScoreComponent, EIGAudioBus::Score);
		ScoreComponent->Play();
	}
}

float UIGMissingFloorAudioSubsystem::GetEffectiveBusDecibels(
	const EIGAudioBus Bus) const
{
	const int32 BusIndex = IGMissingFloorMix::ToIndex(Bus);
	float Decibels = IGMissingFloorMix::BaseDecibels[BusIndex];
	if (Bus == EIGAudioBus::Score && bAuthoredSilence)
	{
		return IGMissingFloorMix::SilentDecibels;
	}
	if (Bus == EIGAudioBus::World && bAuthoredSilence)
	{
		return -24.0f;
	}
	if (Bus == EIGAudioBus::Player && bEntityNearPlayer)
	{
		Decibels -= 4.0f;
	}
	else if (Bus == EIGAudioBus::World
		&& (bPlayerListening || bEntityListening))
	{
		Decibels -= 6.0f;
	}
	return Decibels;
}

int32 UIGMissingFloorAudioSubsystem::GetVoiceCap(const EIGAudioBus Bus) const
{
	return IGMissingFloorMix::VoiceCaps[IGMissingFloorMix::ToIndex(Bus)];
}

USoundClass* UIGMissingFloorAudioSubsystem::GetBusSoundClass(
	const EIGAudioBus Bus) const
{
	const int32 BusIndex = IGMissingFloorMix::ToIndex(Bus);
	return BusSoundClasses.IsValidIndex(BusIndex)
		? BusSoundClasses[BusIndex]
		: nullptr;
}

bool UIGMissingFloorAudioSubsystem::ValidateContract(
	FString& OutFailure) const
{
	if (BusSoundClasses.Num() != BusCount || !RuntimeMix)
	{
		OutFailure = TEXT("six-bus graph was not constructed");
		return false;
	}
	for (int32 Index = 0; Index < BusCount; ++Index)
	{
		const USoundClass* SoundClass = BusSoundClasses[Index];
		if (!SoundClass)
		{
			OutFailure = FString::Printf(TEXT("bus %d has no SoundClass"), Index);
			return false;
		}
		const float Expected = IGMissingFloorMix::DecibelsToLinear(
			IGMissingFloorMix::BaseDecibels[Index]);
		if (!FMath::IsNearlyEqual(SoundClass->Properties.Volume, Expected, 0.001f))
		{
			OutFailure = FString::Printf(
				TEXT("bus %d base gain drifted"), Index);
			return false;
		}
		if (IGMissingFloorMix::VoiceCaps[Index] <= 0)
		{
			OutFailure = FString::Printf(TEXT("bus %d has no voice cap"), Index);
			return false;
		}
	}
	if (!FMath::IsNearlyEqual(
		GetEffectiveBusDecibels(EIGAudioBus::Entity), 0.0f, 0.01f))
	{
		OutFailure = TEXT("entity bus is not invariant at 0 dB");
		return false;
	}
	if (UserMasterVolume < 0.25f || UserMasterVolume > 1.0f)
	{
		OutFailure = TEXT("user master volume escaped the calibrated range");
		return false;
	}
	OutFailure.Reset();
	return true;
}

bool UIGMissingFloorAudioSubsystem::IsTitleReplyTime(
	const FDateTime& LocalTime)
{
	const int32 MinuteOfDay = LocalTime.GetHour() * 60 + LocalTime.GetMinute();
	return MinuteOfDay >= 4 * 60 + 30 && MinuteOfDay <= 5 * 60 + 30;
}

void UIGMissingFloorAudioSubsystem::BuildBusGraph()
{
	BusSoundClasses.SetNum(BusCount);
	for (int32 Index = 0; Index < BusCount; ++Index)
	{
		USoundClass* SoundClass = NewObject<USoundClass>(
			this,
			IGMissingFloorMix::BusNames[Index]);
		SoundClass->Properties.Volume = IGMissingFloorMix::DecibelsToLinear(
			IGMissingFloorMix::BaseDecibels[Index]);
		SoundClass->Properties.Pitch = 1.0f;
		SoundClass->Properties.bIsUISound = Index == static_cast<int32>(EIGAudioBus::UI);
		BusSoundClasses[Index] = SoundClass;
	}
	RuntimeMix = NewObject<USoundMix>(this, TEXT("MIX_MISSING_FLOOR_RUNTIME"));
}

void UIGMissingFloorAudioSubsystem::RefreshMix(const float FadeSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !RuntimeMix)
	{
		return;
	}

	for (int32 Index = 0; Index < BusCount; ++Index)
	{
		const EIGAudioBus Bus = static_cast<EIGAudioBus>(Index);
		float AdditionalDecibels = 0.0f;
		if (Bus == EIGAudioBus::Score && bAuthoredSilence)
		{
			AdditionalDecibels = IGMissingFloorMix::SilentDecibels;
		}
		else if (Bus == EIGAudioBus::World && bAuthoredSilence)
		{
			// WORLD starts at -8 dB; another -16 lands on the authored
			// -24 dB silence floor while player breath remains untouched.
			AdditionalDecibels = -16.0f;
		}
		else if (Bus == EIGAudioBus::Player && bEntityNearPlayer)
		{
			AdditionalDecibels = -4.0f;
		}
		else if (Bus == EIGAudioBus::World
			&& (bPlayerListening || bEntityListening))
		{
			AdditionalDecibels = -6.0f;
		}
		UGameplayStatics::SetSoundMixClassOverride(
			World,
			RuntimeMix,
			BusSoundClasses[Index],
			IGMissingFloorMix::DecibelsToLinear(AdditionalDecibels)
				* UserMasterVolume,
			1.0f,
			FMath::Max(0.0f, FadeSeconds),
			false);
	}
}

void UIGMissingFloorAudioSubsystem::PruneVoices()
{
	for (TArray<FTrackedVoice>& Voices : ActiveVoices)
	{
		Voices.RemoveAll([](const FTrackedVoice& Voice)
		{
			return !Voice.Component.IsValid()
				|| (!Voice.Component->IsPlaying()
					&& Voice.Component->bAutoDestroy);
		});
	}
}

void UIGMissingFloorAudioSubsystem::SwitchScore(
	const EIGAudioThreatState NewState,
	const bool bForce)
{
	if (!bForce && ActiveScoreState == NewState)
	{
		return;
	}

	const float ReleaseSeconds = ActiveScoreState == EIGAudioThreatState::Chasing
		? 4.0f
		: 0.30f;
	StopScore(ReleaseSeconds);
	ActiveScoreState = NewState;

	UIGToneSequenceSoundWave* Score = nullptr;
	float Volume = 1.0f;
	if (bTitleMode)
	{
		Score = UIGToneSequenceSoundWave::CreateTuningMotif(this, false);
		Volume = 0.78f;
	}
	else
	{
		switch (NewState)
		{
		case EIGAudioThreatState::Investigating:
			Score = UIGToneSequenceSoundWave::CreateCavityDrone(this);
			Volume = 0.82f;
			break;
		case EIGAudioThreatState::Chasing:
			Score = UIGToneSequenceSoundWave::CreateChaseScore(this);
			Volume = 1.0f;
			break;
		case EIGAudioThreatState::Finale:
			Score = UIGToneSequenceSoundWave::CreateCavityDrone(this);
			Volume = 0.62f;
			break;
		case EIGAudioThreatState::Calm:
		case EIGAudioThreatState::Banging:
		case EIGAudioThreatState::Listening:
		default:
			break;
		}
	}

	if (!Score || !GetWorld())
	{
		return;
	}
	PrepareSound(Score, EIGAudioBus::Score);
	ScoreComponent = UGameplayStatics::CreateSound2D(
		GetWorld(),
		Score,
		Volume,
		1.0f,
		0.0f,
		nullptr,
		false,
		true);
	if (ScoreComponent)
	{
		ScoreComponent->SetUISound(bTitleMode);
	}
	RegisterComponent(ScoreComponent, EIGAudioBus::Score);
	if (ScoreComponent)
	{
		ScoreComponent->Play();
	}
}

void UIGMissingFloorAudioSubsystem::StopScore(const float FadeSeconds)
{
	if (!ScoreComponent)
	{
		return;
	}
	if (FadeSeconds <= KINDA_SMALL_NUMBER)
	{
		ScoreComponent->Stop();
	}
	else
	{
		ScoreComponent->FadeOut(FadeSeconds, 0.0f);
	}
	ScoreComponent = nullptr;
}

void UIGMissingFloorAudioSubsystem::StartTitleSoundscape()
{
	bAuthoredSilence = false;
	RefreshMix(0.25f);
	SwitchScore(EIGAudioThreatState::Calm, true);
	NextTitleKnockRealTime =
		FPlatformTime::Seconds() + IGMissingFloorMix::TitleKnockDelaySeconds;
	PendingTitleReplyRealTime = -1.0;
}

void UIGMissingFloorAudioSubsystem::StopTitleSoundscape()
{
	NextTitleKnockRealTime = -1.0;
	PendingTitleReplyRealTime = -1.0;
	SwitchScore(ThreatState, true);
}

void UIGMissingFloorAudioSubsystem::PlayTitleKnockCycle()
{
	if (!bTitleMode || !GetWorld())
	{
		return;
	}
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockTriple(this, 0.72f),
		ResolveTitleCueLocation(false),
		0.72f,
		1.0f,
		120.0f,
		1800.0f,
		EIGAudioBus::Entity,
		true);

	const double Now = FPlatformTime::Seconds();
	if (IsTitleReplyTime(FDateTime::Now()))
	{
		PendingTitleReplyRealTime =
			Now + IGMissingFloorMix::TitleReplyDelaySeconds;
	}
	NextTitleKnockRealTime = Now + IGMissingFloorMix::TitleCycleSeconds;
}

void UIGMissingFloorAudioSubsystem::PlayTitleReply()
{
	if (!bTitleMode)
	{
		return;
	}
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockReply(this),
		ResolveTitleCueLocation(true),
		0.62f,
		0.94f,
		100.0f,
		1700.0f,
		EIGAudioBus::Entity,
		true);
}

FVector UIGMissingFloorAudioSubsystem::ResolveTitleCueLocation(
	const bool bReply) const
{
	const APlayerController* Controller = GetWorld()
		? GetWorld()->GetFirstPlayerController()
		: nullptr;
	const APlayerCameraManager* Camera = Controller
		? Controller->PlayerCameraManager
		: nullptr;
	if (!Camera)
	{
		return FVector(0.0f, 0.0f, bReply ? 420.0f : 280.0f);
	}
	const FVector Forward = Camera->GetActorForwardVector();
	const FVector Right = Camera->GetActorRightVector();
	return Camera->GetCameraLocation()
		+ Forward * (bReply ? -110.0f : 180.0f)
		+ Right * (bReply ? -90.0f : 70.0f)
		+ FVector::UpVector * (bReply ? 430.0f : 260.0f);
}
