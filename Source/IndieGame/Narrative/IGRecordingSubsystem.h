#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "IGRecordingSubsystem.generated.h"

class UAudioComponent;
class UIGNoiseSubsystem;
struct FIGNoiseEvent;

/** One thing the phone was pointed at, and whether the machine kept it. */
struct FIGRecordedSound
{
	/** Seconds from the moment recording started. */
	float OffsetSeconds = 0.0f;

	/** Post-masking loudness as the building heard it. */
	float Loudness = 0.0f;

	/**
	 * How long the gap is when this one is suppressed. §5.5 wants the silence to
	 * be exactly as long as the sound it replaces, so the length is recorded
	 * even for sounds that never make it onto the tape.
	 */
	float DurationSeconds = 0.0f;

	/** True when the machine refused it: 그 시간의 소리. */
	bool bSuppressed = false;
};

/**
 * §5.5 기록되지 않는 시간 — the recording rule.
 *
 * '그 시간'의 소리는 어떤 기계에도 담기지 않는다. Playing back a night leaves
 * her own footsteps and her own breathing on the tape, and **exactly as much
 * silence as there was knocking**. Nothing else is edited; the gap is the shape
 * of what was removed.
 *
 * Per §14 this is not audio capture. It is a log of noise events replayed as a
 * single procedural wave, which is why the silences can be exact: the suppressed
 * events simply contribute no notes and their duration stays in the timeline.
 *
 * The rule does three jobs at once (§5.5): it explains why she cannot hand the
 * police a file, why the investigable evidence is therefore paper and physical
 * traces, and why Mok Hansu has lasted a year believing a crime with no record
 * is a crime that did not happen.
 *
 * It is planted in night two and lifted exactly once, right after the wall opens
 * in night four — because the hour is ending. The first sound her phone ever
 * manages to keep is what ending A reports.
 */
UCLASS()
class INDIEGAME_API UIGRecordingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Arms the phone. Any previous take is discarded — she reuses one memo. */
	UFUNCTION(BlueprintCallable, Category = "Recording")
	void StartRecording();

	/** Leaves the take on the phone for playback. */
	UFUNCTION(BlueprintCallable, Category = "Recording")
	void StopRecording();

	UFUNCTION(BlueprintPure, Category = "Recording")
	bool IsRecording() const { return bRecording; }

	/** True once a take exists, whether or not anything survived in it. */
	UFUNCTION(BlueprintPure, Category = "Recording")
	bool HasTake() const { return Recorded.Num() > 0 || TakeSeconds > 0.0f; }

	/**
	 * Plays the take back through the phone's own speaker, at Location.
	 * Returns false when there is nothing to play. The excerpt is built around
	 * the first refusal so the gap is the thing the player hears.
	 */
	bool PlayBack(const FVector& Location);

	/** Throws the take away. */
	UFUNCTION(BlueprintCallable, Category = "Recording")
	void ClearTake();

	/**
	 * Whether the machines are keeping the hour's sounds yet. False all game
	 * until the night-four wall opens, and derived rather than stored so it can
	 * never disagree with the save.
	 */
	UFUNCTION(BlueprintPure, Category = "Recording")
	bool IsRuleLifted() const;

	/**
	 * 그가 낸 소리를 테이프에 직접 올린다. 소음 버스를 거치지 않는다 — 그가
	 * 자기 노크를 듣고 자기를 쫓거나 열지도를 데우면 안 된다. 순찰 노크가
	 * 테이프에 구멍을 남기는 것은 이 길 하나뿐이다.
	 */
	void RecordEntitySound(const FVector& Location, float Loudness, AActor* Instigator);

	// -- receipts for the probe and the contracts ---------------------------
	int32 GetRecordedCount() const { return Recorded.Num(); }
	int32 GetSuppressedCount() const;
	int32 GetSurvivingCount() const;
	/** Total seconds of silence standing in for refused sound. */
	float GetSuppressedSeconds() const;
	float GetTakeSeconds() const { return TakeSeconds; }
	/** Takes dispatched to the speaker, whether or not a device existed. */
	int32 GetPlaybackCount() const { return PlaybackCount; }

	/** Harness hook: files one event without needing an entity in the world. */
	void RecordForTesting(float OffsetSeconds, float Loudness, bool bFromEntity);

	/** A take longer than this is excerpted around the first refusal. */
	static constexpr float MaximumPlaybackSeconds = 16.0f;

	/** Below this an event is her own body and always survives. */
	static constexpr float BreathLoudnessFloor = 0.02f;

private:
	void HandleNoiseReported(const FIGNoiseEvent& Event);
	bool ShouldSuppress(const FIGNoiseEvent& Event) const;

	UPROPERTY(Transient)
	TObjectPtr<UIGNoiseSubsystem> NoiseSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> PlaybackComponent;

	TArray<FIGRecordedSound> Recorded;
	FDelegateHandle NoiseHandle;
	double RecordingStartSeconds = 0.0;
	float TakeSeconds = 0.0f;
	int32 PlaybackCount = 0;
	bool bRecording = false;
};
