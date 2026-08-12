#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "IGMissingFloorAudioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class USoundClass;
class USoundMix;

/** Six logical mix lanes locked by STORY_BIBLE_MISSING_FLOOR.md §21.1. */
UENUM(BlueprintType)
enum class EIGAudioBus : uint8
{
	Entity,
	Player,
	Puzzle,
	World,
	UI,
	Score,
	Count UMETA(Hidden)
};

/** Listener-driven score states. These are presentation states, not AI logic. */
UENUM(BlueprintType)
enum class EIGAudioThreatState : uint8
{
	Calm,
	Banging,
	Listening,
	Investigating,
	Chasing,
	Finale
};

/**
 * Runtime mix and score director for The Missing Floor.
 *
 * The subsystem owns six transient SoundClasses, their bus-level ducking,
 * oldest-first voice limiting and the three procedural score motifs. Spatial
 * one-shots opt in through IGAudio::SpawnOneShotAt; persistent components call
 * RegisterComponent before Play. Nothing here changes puzzle or AI truth.
 */
UCLASS()
class INDIEGAME_API UIGMissingFloorAudioSubsystem final
	: public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Assigns the bus SoundClass before a transient sound begins playback. */
	void PrepareSound(USoundBase* Sound, EIGAudioBus Bus) const;

	/**
	 * Assigns and tracks a component, enforcing the authored voice cap by
	 * fading the oldest live voice instead of cutting it.
	 */
	void RegisterComponent(UAudioComponent* Component, EIGAudioBus Bus);

	void SetThreatState(EIGAudioThreatState NewState);
	void SetPlayerListening(bool bListening);
	void SetAuthoredSilence(bool bSilent);
	void SetEntityDistance(float DistanceCentimeters);
	void SetTitleMode(bool bEnabled);
	/** 보정한 사용자 이득을 여섯 버스에 같은 비율로 적용한다. */
	void SetUserMasterVolume(float Volume01, float FadeSeconds = 0.08f);
	float GetUserMasterVolume() const { return UserMasterVolume; }
	/** 보정용 위층 노크를 ENTITY 버스와 HRTF 경로로 재생한다. */
	void PlayCalibrationKnock();
	int32 GetCalibrationKnockPlayCount() const
	{
		return CalibrationKnockPlayCount;
	}
	/** Plays one non-diegetic tuning strike when a truth crosses. */
	void PlayTruthConfirmation(int32 ConfirmationIndex);
	/** Stops the unresolved finale bed and plays the sole consonant resolution. */
	void PlayEndingATuningResolution();

	UFUNCTION(BlueprintPure, Category = "Audio|Missing Floor")
	EIGAudioThreatState GetThreatState() const { return ThreatState; }

	UFUNCTION(BlueprintPure, Category = "Audio|Missing Floor")
	bool IsPlayerListening() const { return bPlayerListening; }

	UFUNCTION(BlueprintPure, Category = "Audio|Missing Floor")
	bool IsAuthoredSilence() const { return bAuthoredSilence; }

	UFUNCTION(BlueprintPure, Category = "Audio|Missing Floor")
	bool IsEntityNearPlayer() const { return bEntityNearPlayer; }

	UFUNCTION(BlueprintPure, Category = "Audio|Missing Floor")
	bool IsTitleModeActive() const { return bTitleMode; }

	/** Effective dB including state ducking; silence reports -96 dB. */
	float GetEffectiveBusDecibels(EIGAudioBus Bus) const;
	int32 GetVoiceCap(EIGAudioBus Bus) const;
	USoundClass* GetBusSoundClass(EIGAudioBus Bus) const;

	/** Headless/runtime receipt for the complete six-bus contract. */
	bool ValidateContract(FString& OutFailure) const;

	/** Inclusive local-time easter-egg window, 04:30 through 05:30. */
	static bool IsTitleReplyTime(const FDateTime& LocalTime);

private:
	struct FTrackedVoice
	{
		TWeakObjectPtr<UAudioComponent> Component;
		uint64 Serial = 0;
	};

	static constexpr int32 BusCount = static_cast<int32>(EIGAudioBus::Count);

	void BuildBusGraph();
	void RefreshMix(float FadeSeconds = 0.12f);
	void PruneVoices();
	void SwitchScore(EIGAudioThreatState NewState, bool bForce = false);
	void StopScore(float FadeSeconds);
	void StartTitleSoundscape();
	void StopTitleSoundscape();
	void PlayTitleKnockCycle();
	void PlayTitleReply();
	FVector ResolveTitleCueLocation(bool bReply) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundClass>> BusSoundClasses;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> RuntimeMix;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ScoreComponent;

	TArray<FTrackedVoice> ActiveVoices[BusCount];
	uint64 NextVoiceSerial = 1;
	float VoicePruneAccumulator = 0.0f;
	double LastEntityDistanceUpdateSeconds = -1000.0;
	EIGAudioThreatState ThreatState = EIGAudioThreatState::Calm;
	EIGAudioThreatState ActiveScoreState = EIGAudioThreatState::Calm;
	bool bPlayerListening = false;
	bool bEntityListening = false;
	bool bAuthoredSilence = false;
	bool bEntityNearPlayer = false;
	bool bTitleMode = false;
	bool bMixPushed = false;
	float UserMasterVolume = 1.0f;
	int32 CalibrationKnockPlayCount = 0;
	double NextTitleKnockRealTime = -1.0;
	double PendingTitleReplyRealTime = -1.0;
};
