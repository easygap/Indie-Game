#include "Audio/IGAudioPresentationProbe.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Entity/IGListenerEntity.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "Player/IGPlayerCharacter.h"
#include "Player/IGStressComponent.h"
#include "Sound/SoundClass.h"

AIGAudioPresentationProbe::AIGAudioPresentationProbe()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AIGAudioPresentationProbe::Check(const bool bCondition, const TCHAR* Name)
{
	Failures += bCondition ? 0 : 1;
	UE_LOG(LogTemp, Display, TEXT("AUDIO_PRESENTATION_CHECK %s %s"), Name,
		bCondition ? TEXT("PASS") : TEXT("FAIL"));
}

void AIGAudioPresentationProbe::CheckVisibility()
{
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const FVector Eye = Player->GetPawnViewLocation();
	const FVector Face = Entity->GetCaptureFaceLocation();
	Controller->SetControlRotation((Face - Eye).Rotation());
	UBoxComponent* Wall = NewObject<UBoxComponent>(this);
	Wall->SetBoxExtent(FVector(8, 200, 300));
	Wall->SetCollisionProfileName(TEXT("BlockAll"));
	Wall->RegisterComponent();
	Wall->SetWorldLocation((Eye + Face) * 0.5f);
	Entity->TryCloseCallStinger(Player.Get(), 280);
	Check(Entity->LastCloseCallSeconds < 0, TEXT("wall_blocks_scare_without_spending_cooldown"));
	Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Controller->SetControlRotation(FRotator(85, 180, 0));
	Entity->TryCloseCallStinger(Player.Get(), 280);
	Check(Entity->LastCloseCallSeconds < 0, TEXT("looking_away_does_not_scare"));
	Controller->SetControlRotation((Face - Eye).Rotation());
	Audio->SetAuthoredSilence(true);
	Entity->TryCloseCallStinger(Player.Get(), 280);
	Check(Entity->LastCloseCallSeconds < 0, TEXT("silence_blocks_scare_without_spending_cooldown"));
	Audio->SetAuthoredSilence(false);
	Entity->TryCloseCallStinger(Player.Get(), 280);
	Check(Entity->LastCloseCallSeconds >= 0, TEXT("visible_face_triggers_scare"));
	const double FirstScare = Entity->LastCloseCallSeconds;
	Entity->TryCloseCallStinger(Player.Get(), 280);
	Check(Entity->LastCloseCallSeconds == FirstScare, TEXT("close_call_cooldown"));
	Wall->DestroyComponent();
}

void AIGAudioPresentationProbe::CheckMixerGain(
	const EIGAudioBus Bus, const float ExpectedDecibels, const TCHAR* Name)
{
	FAudioDevice* Device = GetWorld()->GetAudioDeviceRaw();
	if (!Device) { Check(false, Name); return; }
	USoundClass* SoundClass = Audio->GetBusSoundClass(Bus);
	float Actual = -1;
	// 보고용 계산식 대신 오디오 스레드가 실제 적용한 값을 읽는다.
	FAudioThread::RunCommandOnAudioThread([Device, SoundClass, &Actual]()
	{
		if (const FSoundClassProperties* Properties = Device->GetSoundClassCurrentProperties(SoundClass))
		{
			Actual = Properties->Volume;
		}
	});
	FAudioCommandFence Fence;
	Fence.BeginFence();
	Fence.Wait();
	const float Expected = ExpectedDecibels <= -96 ? 0 : FMath::Pow(10.f, ExpectedDecibels / 20.f);
	Check(FMath::IsNearlyEqual(Actual, Expected, 0.003f), Name);
	UE_LOG(LogTemp, Display, TEXT("AUDIO_PRESENTATION_MIX %s actual=%.5f expected=%.5f"), Name, Actual, Expected);
}

void AIGAudioPresentationProbe::CheckChaseWave()
{
	UIGToneSequenceSoundWave* Wave = UIGToneSequenceSoundWave::CreateChaseScore(this);
	const double Expected = 8.0 * 60.0 / 118.0;
	Check(FMath::Abs(Wave->LoopSampleCount / 48000.0 - Expected) < 2.0 / 48000.0,
		TEXT("chase_repeats_at_118_bpm_without_extra_gap"));
	TArray<uint8> Samples;
	int32 Peak = 0;
	int64 Energy = 0;
	// 세 마디를 실제 합성기로 굽는다. 설정값이 맞아도 무음이거나 잘린 파형이면 실패다.
	const int32 Count = static_cast<int32>(Wave->LoopSampleCount * 3);
	Wave->OnGeneratePCMAudio(Samples, Count);
	const int16* Pcm = reinterpret_cast<const int16*>(Samples.GetData());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Peak = FMath::Max(Peak, FMath::Abs(static_cast<int32>(Pcm[Index])));
		Energy += static_cast<int64>(Pcm[Index]) * Pcm[Index];
	}
	Check(Peak > 500 && Peak < 29000 && Energy > Count * 10000LL,
		TEXT("chase_wave_has_signal_and_headroom"));
	UE_LOG(LogTemp, Display, TEXT("AUDIO_PRESENTATION_WAVE seconds=%.6f peak=%.5f rms=%.5f"),
		Wave->LoopSampleCount / 48000.0, Peak / 32767.0,
		FMath::Sqrt(static_cast<double>(Energy) / Count) / 32767.0);
}

void AIGAudioPresentationProbe::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Seconds += DeltaSeconds;
	if (Phase == 0)
	{
		APlayerController* Controller = GetWorld()->GetFirstPlayerController();
		Player = Controller ? Cast<AIGPlayerCharacter>(Controller->GetPawn()) : nullptr;
		if (!Player.IsValid())
		{
			if (Seconds > 15)
			{
				Check(false, TEXT("player_spawn"));
				FPlatformMisc::RequestExitWithStatus(false, 1);
			}
			return;
		}
		Audio = GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>();
		Stress = Player->GetStress();
		if (!Audio.IsValid() || !Stress.IsValid())
		{
			Check(false, TEXT("audio_and_stress_subsystems"));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		Audio->SetTitleMode(false);
		Audio->SetAuthoredSilence(false);
		Audio->SetUserMasterVolume(1);
		Audio->SetScoreUserVolume(1);
		Audio->SetAmbienceUserVolume(1);
		Audio->SetHeadphoneOutput(true);
		Player->GetCharacterMovement()->DisableMovement();
		Player->SetCameraMotionEnabled(false);
		Player->SetActorLocation(FVector(0, 0, 96));
		Entity = GetWorld()->SpawnActor<AIGListenerEntity>(FVector(280, 0, 58), FRotator(0, 180, 0));
		if (!Entity.IsValid())
		{
			Check(false, TEXT("entity_spawn"));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		Entity->SetActorTickEnabled(false);
		Entity->CachedPlayer = Player;
		Entity->UpdateDragLoop(160);
		Entity->UpdateBreathLoop(280);
		Check(Entity->DragLoopComponent && Entity->DragLoopComponent->IsPlaying(),
			TEXT("real_audio_device_and_drag_playback"));
		UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 25);
		CheckChaseWave();
		Phase = 1; Seconds = 0;
	}
	else if (Phase == 1 && Seconds > 0.6f)
	{
		CheckMixerGain(EIGAudioBus::Entity, 0, TEXT("entity_bus_registered_unattenuated"));
		CheckMixerGain(EIGAudioBus::Player, -3, TEXT("player_bus_registered"));
		CheckMixerGain(EIGAudioBus::Puzzle, -1, TEXT("puzzle_bus_registered"));
		CheckMixerGain(EIGAudioBus::World, -8, TEXT("world_bus_registered"));
		CheckMixerGain(EIGAudioBus::UI, -10, TEXT("ui_bus_registered"));
		CheckMixerGain(EIGAudioBus::Score, -6, TEXT("score_bus_registered"));
		CheckVisibility();
		// 두 상시 소리가 도는 중에 발음 상한을 넘긴다. 오래된 이동음이 멎으면 실패다.
		for (int32 Index = 0; Index < 8; ++Index)
		{
			IGAudio::SpawnOneShotAt(this, UIGToneSequenceSoundWave::CreateWallKnockSingle(this),
				Entity->GetActorLocation(), 0.4f, 1, 300, 1000, EIGAudioBus::Entity);
		}
		Audio->SetThreatState(EIGAudioThreatState::Chasing);
		Audio->SetEntityDistance(450);
		Chase = Audio->ScoreComponent;
		for (int32 Index = 1; Index <= 8; ++Index) { Audio->PlayTruthConfirmation(Index); }
		Phase = 2; Seconds = 0;
	}
	else if (Phase == 2 && Seconds > 0.7f)
	{
		Check(Entity->DragLoopComponent->IsPlaying(), TEXT("drag_survives_voice_pressure"));
		Check(Entity->BreathLoopComponent->IsPlaying(), TEXT("entity_breath_survives_voice_pressure"));
		Check(Chase.IsValid() && Chase->IsPlaying(), TEXT("truth_cues_do_not_stop_chase_music"));
		Check(Audio->PlayStinger(EIGStinger::ChaseStart, Entity->GetActorLocation()), TEXT("first_chase_scream"));
		Check(!Audio->PlayStinger(EIGStinger::ChaseStart, Entity->GetActorLocation()), TEXT("repeat_chase_scream_suppressed"));
		Check(!Audio->PlayStinger(EIGStinger::CloseCall, Entity->GetActorLocation()), TEXT("close_call_does_not_stack_on_chase"));
		Audio->SetThreatState(EIGAudioThreatState::Investigating);
		Phase = 3; Seconds = 0;
	}
	else if (Phase == 3 && Seconds > 0.7f)
	{
		Check(Chase.IsValid() && Chase->IsPlaying(), TEXT("chase_release_tail_survives"));
		Audio->SetThreatState(EIGAudioThreatState::Chasing);
		Check(Audio->ScoreComponent == Chase.Get(), TEXT("chase_resumes_same_component_and_beat"));
		Audio->SetEntityDistance(450);
		Phase = 4; Seconds = 0;
	}
	else if (Phase == 4 && Seconds > 0.6f)
	{
		Entity->BeginCapture(Player.Get());
		Stress->UpdateBreathLayer(0);
		Check(Audio->GetThreatState() == EIGAudioThreatState::Captured, TEXT("capture_has_own_presentation_state"));
		Check(!Audio->ScoreComponent && Audio->GetPresenceAlpha() == 0, TEXT("capture_clears_music_and_pressure"));
		Check(Stress->BreathLevelTarget == 0, TEXT("capture_leaves_room_for_contact_breath"));
		Phase = 5; Seconds = 0;
	}
	else if (Phase == 5 && Seconds > 0.5f)
	{
		Check(!Chase.IsValid() || !Chase->IsPlaying(), TEXT("chase_is_gone_during_capture"));
		Entity->SetDormant(true);
		Audio->SetThreatState(EIGAudioThreatState::Calm);
		Stress->SetExertion(1);
		Stress->UpdateBreathLayer(0);
		Stress->PlayGasp(true);
		InterruptedBreath = Stress->BreathOneShotComponent;
		Check(InterruptedBreath.IsValid(), TEXT("gasp_starts"));
		Stress->BeginBreathHold(4);
		Phase = 6; Seconds = 0;
	}
	else if (Phase == 6 && Seconds > 0.4f)
	{
		Check(!InterruptedBreath.IsValid() || !InterruptedBreath->IsPlaying(), TEXT("breath_hold_stops_existing_gasp"));
		Check(!Stress->BreathComponent || !Stress->BreathComponent->IsPlaying(), TEXT("breath_hold_stops_loop"));
		Stress->EndBreathHold(true);
		Stress->UpdateBreathLayer(0);
		Check(Stress->BreathOneShotComponent && Stress->BreathOneShotComponent->IsPlaying(), TEXT("release_has_one_gasp"));
		Audio->SetAuthoredSilence(true);
		Stress->UpdateBreathLayer(0);
		Phase = 7; Seconds = 0;
	}
	else if (Phase == 7 && Seconds > 0.4f)
	{
		CheckMixerGain(EIGAudioBus::Score, -96, TEXT("authored_silence_reaches_audio_device"));
		CheckMixerGain(EIGAudioBus::World, -24, TEXT("authored_world_duck_reaches_audio_device"));
		Check(!Stress->BreathComponent || !Stress->BreathComponent->IsPlaying(), TEXT("authored_silence_stops_breath_loop"));
		Stress->PlayGasp(true);
		Stress->PlayReliefExhale();
		Check(!Stress->BreathOneShotComponent, TEXT("authored_silence_blocks_breath_accents"));
		Audio->SetAuthoredSilence(false);
		Stress->SetExertion(1);
		Stress->UpdateBreathLayer(0);
		Check(Stress->BreathComponent && Stress->BreathComponent->IsPlaying(), TEXT("breath_returns_after_silence"));
		Audio->SetThreatState(EIGAudioThreatState::Chasing);
		Audio->SetEntityDistance(350);
		Phase = 8; Seconds = 0;
	}
	else if (Phase == 8 && Seconds > 4.5f)
	{
		// 거리 보고가 끊긴 뒤 잔류 압박이 사라지는지도 실제 틱으로 확인한다.
		Check(Audio->GetPresenceAlpha() == 0, TEXT("stale_entity_pressure_expires"));
		Audio->SetTitleMode(true);
		Check(Audio->GetPresenceAlpha() == 0, TEXT("title_has_no_gameplay_pressure"));
		Stress->UpdateBreathLayer(0);
		Check(Stress->BreathLevelTarget == 0, TEXT("title_has_no_player_breath"));
		const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("AudioPresentation"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		UAudioMixerBlueprintLibrary::StopRecordingOutput(
			this, EAudioRecordingExportType::WavFile, TEXT("presentation-mix"), Directory);
		Phase = 9; Seconds = 0;
	}
	else if (Phase == 9 && Seconds > 2)
	{
		const FString Recording = FPaths::ProjectSavedDir() / TEXT("AudioPresentation/presentation-mix.wav");
		Check(IFileManager::Get().FileSize(*Recording) > 48000, TEXT("master_mix_recording_written"));
		Audio->SetScoreUserVolume(0);
		Phase = 10;
		Seconds = 0;
	}
	else if (Phase == 10 && Seconds > 0.4f)
	{
		CheckMixerGain(EIGAudioBus::Score, -96, TEXT("music_slider_mutes_real_bus"));
		CheckMixerGain(EIGAudioBus::Entity, 0, TEXT("music_slider_preserves_entity_cues"));
		Audio->SetScoreUserVolume(1);
		Audio->SetUserMasterVolume(0.5f);
		Phase = 11; Seconds = 0;
	}
	else if (Phase == 11 && Seconds > 0.4f)
	{
		CheckMixerGain(EIGAudioBus::Entity, -6.0206f, TEXT("master_slider_reaches_entity_bus"));
		CheckMixerGain(EIGAudioBus::Score, -12.0206f, TEXT("master_slider_reaches_score_bus"));
		Audio->SetTitleMode(false);
		Audio->SetThreatState(EIGAudioThreatState::Calm);
		Audio->SetEntityDistance(450);
		Audio->SetEntityDistance(1350);
		Audio->SetEntityDistance(3000);
		Check(Audio->GetPresenceAlpha() == 0, TEXT("nearly_silent_pressure_reaches_exact_zero"));
		Phase = 12; Seconds = 0;
	}
	else if (Phase == 12 && Seconds > 0.5f)
	{
		Check(!Audio->PresenceComponent || !Audio->PresenceComponent->IsPlaying(),
			TEXT("nearly_silent_pressure_loop_stops"));
		UE_LOG(LogTemp, Display, TEXT("AUDIO_PRESENTATION_PROBE %s failures=%d"),
			Failures ? TEXT("FAIL") : TEXT("PASS"), Failures);
		Phase = 13;
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}
}
