#include "Audio/IGAudioRenderProbe.h"

#include "Audio/IGAmbienceSoundWave.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"

namespace IGAudioRenderProbe
{
	namespace
	{
		constexpr int32 SampleRate = 48000;

		struct FCue
		{
			const TCHAR* Name;
			TFunction<USoundWaveProcedural*(UObject*)> Make;
			/** 루프는 이만큼만, 유한 파형은 제 길이만큼 굽는다. */
			float LoopSeconds;
		};

		void WriteWav(const FString& Path, const TArray<int16>& Samples)
		{
			TArray<uint8> Bytes;
			const uint32 DataBytes = Samples.Num() * sizeof(int16);
			auto Put32 = [&Bytes](const uint32 Value)
			{
				Bytes.Append(reinterpret_cast<const uint8*>(&Value), 4);
			};
			auto Put16 = [&Bytes](const uint16 Value)
			{
				Bytes.Append(reinterpret_cast<const uint8*>(&Value), 2);
			};
			Bytes.Append(reinterpret_cast<const uint8*>("RIFF"), 4);
			Put32(36 + DataBytes);
			Bytes.Append(reinterpret_cast<const uint8*>("WAVE"), 4);
			Bytes.Append(reinterpret_cast<const uint8*>("fmt "), 4);
			Put32(16);
			Put16(1);
			Put16(1);
			Put32(SampleRate);
			Put32(SampleRate * sizeof(int16));
			Put16(sizeof(int16));
			Put16(16);
			Bytes.Append(reinterpret_cast<const uint8*>("data"), 4);
			Put32(DataBytes);
			Bytes.Append(reinterpret_cast<const uint8*>(Samples.GetData()), DataBytes);
			FFileHelper::SaveArrayToFile(Bytes, *Path);
		}

		int32 Render(USoundWaveProcedural* Wave, const float LoopSeconds, TArray<int16>& OutSamples)
		{
			float Seconds = LoopSeconds;
			if (const UIGToneSequenceSoundWave* Tone = Cast<UIGToneSequenceSoundWave>(Wave))
			{
				if (!Tone->IsConfiguredLooping())
				{
					Seconds = FMath::Clamp(Tone->GetConfiguredDurationSeconds(), 0.05f, 12.0f);
				}
			}
			const int32 Total = FMath::RoundToInt(Seconds * SampleRate);
			OutSamples.Reset(Total);
			TArray<uint8> Block;
			int32 Remaining = Total;
			while (Remaining > 0)
			{
				const int32 Chunk = FMath::Min(Remaining, 4096);
				const int32 Got = Wave->OnGeneratePCMAudio(Block, Chunk);
				if (Got <= 0)
				{
					break;
				}
				OutSamples.Append(reinterpret_cast<const int16*>(Block.GetData()), Got);
				Remaining -= Got;
			}
			return OutSamples.Num();
		}

		UIGAmbienceSoundWave* Ambience(UObject* Outer, const EIGAmbienceMode Mode)
		{
			UIGAmbienceSoundWave* Wave = NewObject<UIGAmbienceSoundWave>(Outer);
			Wave->Configure(Mode, 0x5EEDu);
			return Wave;
		}
	}

	bool RunIfRequested(UObject* Outer)
	{
		if (!Outer || !FParse::Param(FCommandLine::Get(), TEXT("IGAudioRenderProbe")))
		{
			return false;
		}
		using W = UIGToneSequenceSoundWave;
		const FCue Cues[] = {
			{TEXT("footstep_vinyl"), [](UObject* O) { return W::CreateSurfaceFootstep(O, EIGFootstepSurface::Vinyl, 1.0f, 0.8f); }, 0.0f},
			{TEXT("footstep_concrete"), [](UObject* O) { return W::CreateSurfaceFootstep(O, EIGFootstepSurface::Concrete, 1.0f, 0.8f); }, 0.0f},
			{TEXT("footstep_metal"), [](UObject* O) { return W::CreateSurfaceFootstep(O, EIGFootstepSurface::MetalStair, 1.0f, 0.8f); }, 0.0f},
			{TEXT("footstep_gypsum"), [](UObject* O) { return W::CreateSurfaceFootstep(O, EIGFootstepSurface::GypsumDebris, 1.0f, 0.8f); }, 0.0f},
			{TEXT("footstep_water"), [](UObject* O) { return W::CreateSurfaceFootstep(O, EIGFootstepSurface::Water, 1.0f, 0.8f); }, 0.0f},
			{TEXT("knock_triple"), [](UObject* O) { return W::CreateWallKnockTriple(O, 0.0f); }, 0.0f},
			{TEXT("knock_triple_muffled"), [](UObject* O) { return W::CreateWallKnockTriple(O, 0.7f); }, 0.0f},
			{TEXT("knock_single"), [](UObject* O) { return W::CreateWallKnockSingle(O, 0.0f); }, 0.0f},
			{TEXT("knock_reply"), [](UObject* O) { return W::CreateWallKnockReply(O); }, 0.0f},
			{TEXT("answer_pattern"), [](UObject* O) { return W::CreateAnswerKnockPattern(O, 0.0f); }, 0.0f},
			{TEXT("door_thud"), [](UObject* O) { return W::CreateDoorThud(O); }, 0.0f},
			{TEXT("door_creak_open"), [](UObject* O) { return W::CreateDoorCreak(O, false); }, 0.0f},
			{TEXT("door_creak_close"), [](UObject* O) { return W::CreateDoorCreak(O, true); }, 0.0f},
			{TEXT("locked_rattle"), [](UObject* O) { return W::CreateLockedRattle(O); }, 0.0f},
			{TEXT("pickup_rustle"), [](UObject* O) { return W::CreatePickupRustle(O); }, 0.0f},
			{TEXT("switch_on"), [](UObject* O) { return W::CreateSwitchClick(O, true); }, 0.0f},
			{TEXT("switch_off"), [](UObject* O) { return W::CreateSwitchClick(O, false); }, 0.0f},
			{TEXT("entity_drag_tile"), [](UObject* O) { return W::CreateEntityDragLoop(O, false); }, 3.15f},
			{TEXT("entity_drag_vinyl"), [](UObject* O) { return W::CreateEntityDragLoop(O, true); }, 3.15f},
			{TEXT("entity_step_tile"), [](UObject* O) { return W::CreateEntityCrawlStep(O, false); }, 0.0f},
			{TEXT("entity_step_vinyl"), [](UObject* O) { return W::CreateEntityCrawlStep(O, true); }, 0.0f},
			{TEXT("entity_breath"), [](UObject* O) { return W::CreateEntityBreathLoop(O); }, 6.4f},
			{TEXT("entity_alert"), [](UObject* O) { return W::CreateEntityAlertVocal(O); }, 0.0f},
			{TEXT("entity_scream"), [](UObject* O) { return W::CreateEntityChaseScream(O); }, 0.0f},
			{TEXT("sting_close_call"), [](UObject* O) { return W::CreateCloseCallStinger(O); }, 0.0f},
			{TEXT("sting_capture_lunge"), [](UObject* O) { return W::CreateCaptureLunge(O); }, 0.0f},
			{TEXT("presence_layer"), [](UObject* O) { return W::CreatePresenceLayer(O); }, 6.0f},
			{TEXT("score_chase"), [](UObject* O) { return W::CreateChaseScore(O); }, 8.2f},
			{TEXT("score_cavity"), [](UObject* O) { return W::CreateCavityDrone(O); }, 8.0f},
			{TEXT("settle_pipe"), [](UObject* O) { return W::CreateSettlePipeKnock(O); }, 0.0f},
			{TEXT("settle_creak"), [](UObject* O) { return W::CreateSettleTimberCreak(O); }, 0.0f},
			{TEXT("settle_slam"), [](UObject* O) { return W::CreateSettleFarDoorSlam(O); }, 0.0f},
			{TEXT("settle_tick"), [](UObject* O) { return W::CreateSettlePlasterTick(O); }, 0.0f},
			{TEXT("mok_driver"), [](UObject* O) { return W::CreateCordlessDriverRun(O); }, 0.0f},
			{TEXT("hum_machine"), [](UObject* O) { return W::CreateMachineHumLoop(O); }, 3.6f},
			{TEXT("bed_room"), [](UObject* O) { return Ambience(O, EIGAmbienceMode::RoomTone); }, 6.0f},
			{TEXT("bed_corridor"), [](UObject* O) { return Ambience(O, EIGAmbienceMode::CorridorNight); }, 8.0f},
			{TEXT("bed_stairwell"), [](UObject* O) { return Ambience(O, EIGAmbienceMode::Stairwell); }, 8.0f},
			{TEXT("bed_upper"), [](UObject* O) { return Ambience(O, EIGAmbienceMode::UpperFloor); }, 8.0f},
		};

		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AudioProbe"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		int32 Written = 0;
		for (const FCue& Cue : Cues)
		{
			USoundWaveProcedural* Wave = Cue.Make(Outer);
			if (!Wave)
			{
				UE_LOG(LogTemp, Error, TEXT("MISSINGFLOOR_AUDIO_RENDER FAIL: %s made nothing"), Cue.Name);
				FPlatformMisc::RequestExitWithStatus(false, 1);
				return true;
			}
			TArray<int16> Samples;
			const int32 Count = Render(Wave, Cue.LoopSeconds, Samples);
			int32 Peak = 0;
			for (const int16 Sample : Samples)
			{
				Peak = FMath::Max(Peak, FMath::Abs(static_cast<int32>(Sample)));
			}
			WriteWav(FPaths::Combine(Directory, FString::Printf(TEXT("%s.wav"), Cue.Name)), Samples);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("MISSINGFLOOR_AUDIO_RENDER cue=%s samples=%d peak=%.3f"),
				Cue.Name,
				Count,
				Peak / 32767.0f);
			++Written;
		}
		UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_AUDIO_RENDER PASS cues=%d dir=%s"), Written, *Directory);
		FPlatformMisc::RequestExitWithStatus(false, 0);
		return true;
	}
}
