#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "IGMissingFloorAudioSubsystem.generated.h"

class UAudioComponent;
class UReverbEffect;
class USoundBase;
class USoundClass;
class USoundMix;
struct FSoundAttenuationSettings;

enum class EIGFootstepSurface : uint8;

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
 * 듣고 있는 건물 공간. STORY_BIBLE_MISSING_FLOOR.md §10.4의 거리 리버브
 * 문법은 프리셋을 두 개로 고정한다 — 흡음이 있는 복도와, 흡음이 없는
 * 노출 콘크리트 계단실. 세 번째 값은 프리셋이 아니라 프리셋의 부재다:
 * 옥상은 건물 밖이므로 어떤 반향도 걸지 않는다.
 */
UENUM(BlueprintType)
enum class EIGAcousticSpace : uint8
{
	/** 4·5층 복도와 세대 안. 석고 벽과 문이 고역을 먼저 먹는다. */
	Corridor,
	/** 계단실. 평행한 콘크리트 참이 두 배 넘게 길게 울린다. */
	Stairwell,
	/** 옥상 등 건물 밖. 반향 없음. */
	Open,
	Count UMETA(Hidden)
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

	/**
	 * §10.4 거리 리버브 문법: 지금 듣고 있는 공간의 반향을 교체한다.
	 * 매 0.2초 폴링이 플레이어가 밟은 표면으로 같은 값을 계산하므로 보통은
	 * 부를 필요가 없다. 연출이 공간을 강제해야 할 때만 직접 호출한다.
	 */
	void SetAcousticSpace(EIGAcousticSpace NewSpace);

	UFUNCTION(BlueprintPure, Category = "Audio|Missing Floor")
	EIGAcousticSpace GetAcousticSpace() const { return AcousticSpace; }

	UReverbEffect* GetAcousticPreset(EIGAcousticSpace Space) const;

	/**
	 * 밟고 있는 표면 하나로 공간을 판정한다. 계단 디딤판만 계단실이고
	 * 옥상 슬래브만 건물 밖이며, 나머지 전부는 복도로 취급한다. 세계
	 * 지오메트리를 오디오가 따로 알 필요가 없고, 표면 태그는 이미
	 * §21.2 발소리 매트릭스가 저자 데이터로 들고 있다.
	 */
	static EIGAcousticSpace ClassifyAcousticSpace(EIGFootstepSurface Surface);

	/**
	 * 버스별 거리→젖음 곡선을 감쇠 설정에 붙인다. 서브시스템 인스턴스가
	 * 아직 없는 시점에도 쓸 수 있도록 정적이다.
	 */
	static void ConfigureReverbSend(
		FSoundAttenuationSettings& Settings,
		EIGAudioBus Bus,
		float InnerRadius,
		float FalloffDistance);

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
	void BuildAcousticPresets();
	void ApplyAcousticSpace(float FadeSeconds);
	void PollAcousticSpace();
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

	/** 복도·계단실 두 프리셋. Open은 프리셋 없이 태그를 내린다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UReverbEffect>> AcousticPresets;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ScoreComponent;

	static constexpr int32 AcousticSpaceCount =
		static_cast<int32>(EIGAcousticSpace::Count);

	TArray<FTrackedVoice> ActiveVoices[BusCount];
	uint64 NextVoiceSerial = 1;
	float VoicePruneAccumulator = 0.0f;
	float AcousticPollAccumulator = 0.0f;
	double LastEntityDistanceUpdateSeconds = -1000.0;
	EIGAcousticSpace AcousticSpace = EIGAcousticSpace::Corridor;
	bool bAcousticSpaceResolved = false;
	bool bAcousticSpaceApplied = false;
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
