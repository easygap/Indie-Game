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
	Finale,
	/** 붙잡힌 접촉음이 들리도록 음악을 걷는다. */
	Captured
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

/** 존재가 낼 수 있는 놀람 셋. 전부 그의 버스를 타고 그의 자리에서 난다. */
enum class EIGStinger : uint8
{
	/** 코앞에서 마주쳤다. 3.2m 안, 시야 안, 25초 쿨다운. */
	CloseCall,
	/** 추격이 시작됐다. 비명 뒤에 온다. */
	ChaseStart,
	/** 덮쳤다. 드라이 노크 둘 바로 앞. */
	Capture
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

	/**
	 * 밤 내내 우는 소리를 건다. 자리는 세지만 퇴출 대상은 아니다.
	 *
	 * 상한은 한 번 울고 마는 소리가 쌓이는 걸 막으려고 있다. 험이나 물소리는
	 * 그 대상이 아니다 — 오래됐다는 이유로 밀리면, 복도가 시끄러워진 순간에
	 * 엄폐가 조용해지고 퇴출은 페이드아웃이라 그 밤 내내 안 돌아온다.
	 */
	void RegisterPersistentBed(UAudioComponent* Component, EIGAudioBus Bus);

	/**
	 * §10.5. 헤드폰과 스피커를 오간다. 새로 나는 소리만 바꾸면 이미 돌고
	 * 있는 환경음 루프가 옛 방식으로 남아 설정이 반만 듣는 것처럼 된다.
	 * 그래서 살아 있는 목소리를 훑어 다시 건다.
	 */
	void SetHeadphoneOutput(bool bHeadphones);
	bool IsHeadphoneOutput() const { return bHeadphoneOutput; }

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
	/** 놀람 하나. 존재 버스, 그의 자리에서. */
	bool PlayStinger(EIGStinger Kind, const FVector& Location);
	/** 압박 층의 지금 볼륨 0~1. 계약과 프로브가 읽는다. */
	float GetPresenceAlpha() const { return PresenceAlpha; }
	void SetTitleMode(bool bEnabled);
	/** 보정한 사용자 이득을 여섯 버스에 같은 비율로 적용한다. */
	void SetUserMasterVolume(float Volume01, float FadeSeconds = 0.08f);
	float GetUserMasterVolume() const { return UserMasterVolume; }

	/**
	 * §21.1 대원칙: 존재의 소리는 절대 눌리지 않는다. 그래서 사용자 손이
	 * 닿는 버스는 SCORE와 WORLD 둘뿐이다. ENTITY·PLAYER·PUZZLE을 줄일 수
	 * 있게 두면 놓친 노크가 믹스의 여유처럼 보이지만, 그건 게임의 실패다.
	 *
	 * 음악은 0까지 내려간다 — 작가가 얹은 것이라 없어도 사건은 남는다.
	 * 환경음은 바닥이 있다 — 건물이 내는 소리 자체가 단서다.
	 */
	static constexpr float MinimumAmbienceVolume = 0.40f;

	void SetScoreUserVolume(float Volume01, float FadeSeconds = 0.08f);
	void SetAmbienceUserVolume(float Volume01, float FadeSeconds = 0.08f);
	float GetScoreUserVolume() const { return ScoreUserVolume; }
	float GetAmbienceUserVolume() const { return AmbienceUserVolume; }

	/** 버스별 사용자 배율. 손댈 수 없는 버스는 언제나 1이다. */
	float GetBusUserScale(EIGAudioBus Bus) const;
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
	/**
	 * §21.1 더킹. 이 함수가 그 표의 유일한 구현이다.
	 *
	 * 예전에는 같은 규칙이 RefreshMix와 GetEffectiveBusDecibels 두 곳에
	 * 적혀 있었다. 한쪽만 조율하면 들리는 믹스와 계약이 보고하는 믹스가
	 * 갈라지고, 그 차이는 귀로만 발견된다.
	 */
	float GetBusDuckingDecibels(EIGAudioBus Bus) const;

	float GetEffectiveBusDecibels(EIGAudioBus Bus) const;
	int32 GetVoiceCap(EIGAudioBus Bus) const;
	USoundClass* GetBusSoundClass(EIGAudioBus Bus) const;

	/** Headless/runtime receipt for the complete six-bus contract. */
	bool ValidateContract(FString& OutFailure) const;

	/** Inclusive local-time easter-egg window, 04:30 through 05:30. */
	static bool IsTitleReplyTime(const FDateTime& LocalTime);

private:
	friend class AIGAudioPresentationProbe;
	struct FTrackedVoice
	{
		TWeakObjectPtr<UAudioComponent> Component;
		uint64 Serial = 0;
		/** 늘 우는 소리. 자리는 차지하되 오래됐다고 밀리지는 않는다. */
		bool bPersistent = false;
	};

	static constexpr int32 BusCount = static_cast<int32>(EIGAudioBus::Count);

	void RegisterVoice(
		UAudioComponent* Component, EIGAudioBus Bus, bool bPersistent);
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
	/** 추격이 곧 재개되면 빠지던 음악을 같은 박자에서 되살린다. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ReleasingScoreComponent;
	EIGAudioThreatState ReleasingScoreState = EIGAudioThreatState::Calm;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> StingerComponent;
	double LastChaseStingerSeconds = -1000.0;
	/**
	 * 존재 거리로 켜지는 압박 층. 14m 밖에서 0, 3m 안에서 1. 순찰 중에는 음악이
	 * 없다는 §10.2의 원칙은 그대로다 — 이건 음악이 아니라 그가 가까이 있다는
	 * 몸의 감각이다. 2D, 스코어 버스, 상시 베드.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> PresenceComponent;
	float PresenceAlpha = 0.0f;
	void UpdatePresenceLayer(float DistanceCentimeters);
	void FadePresenceLayer(float Target, float Seconds);

	static constexpr int32 AcousticSpaceCount =
		static_cast<int32>(EIGAcousticSpace::Count);

	TArray<FTrackedVoice> ActiveVoices[BusCount];

	bool bHeadphoneOutput = true;
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
	float ScoreUserVolume = 1.0f;
	float AmbienceUserVolume = 1.0f;
	int32 CalibrationKnockPlayCount = 0;
	double NextTitleKnockRealTime = -1.0;
	double PendingTitleReplyRealTime = -1.0;
};
