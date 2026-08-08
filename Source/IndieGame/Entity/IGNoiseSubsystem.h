#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "IGNoiseSubsystem.generated.h"

/**
 * One reported sound, after masking. Radius is how far the sound carries;
 * whether something reacts is the listener's business, not the reporter's.
 */
USTRUCT(BlueprintType)
struct FIGNoiseEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Noise")
	FVector Location = FVector::ZeroVector;

	/** Post-masking loudness, 0..1. Zero-loudness events are never broadcast. */
	UPROPERTY(BlueprintReadOnly, Category = "Noise")
	float Loudness = 0.0f;

	/** Carry distance in centimeters, derived from post-masking loudness. */
	UPROPERTY(BlueprintReadOnly, Category = "Noise")
	float Radius = 0.0f;

	/** Who made the sound. Weak: reporters outlive nothing on its account. */
	UPROPERTY()
	TWeakObjectPtr<AActor> Instigator;

	/** World real time when the sound happened, for recency checks. */
	UPROPERTY(BlueprintReadOnly, Category = "Noise")
	double TimeSeconds = 0.0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FIGNoiseReportedSignature, const FIGNoiseEvent&);

/**
 * The building's ear. Every deliberate or accidental player sound — footsteps,
 * doors, drawers, dropped props, valves, hammer blows, an audible heartbeat —
 * is reported here as a loudness in [0..1]; the subsystem applies masking and
 * broadcasts the surviving event to whatever is listening (the one upstairs).
 *
 * Masking has two layers:
 *  - hum sources: registered machine beds (fridge, boiler, breaker panel)
 *    that swallow quiet sounds made close to them;
 *  - a global window the entity raises while its own knocking fills the
 *    building — the player's safe beat to move on.
 *
 * The subsystem never decides who heard what. It only says what sounded.
 */
UCLASS()
class INDIEGAME_API UIGNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Reports one sound. Loudness is the pre-masking value from the design
	 * table (walk 0.15, door 0.35, hammer 1.0 ...). Returns the event that
	 * was broadcast, or a zero-loudness event when masking swallowed it.
	 */
	FIGNoiseEvent ReportNoise(
		const FVector& Location,
		float Loudness,
		AActor* Instigator = nullptr);

	/**
	 * Registers a machine hum that masks nearby sounds. Returns a handle for
	 * unregistration; the source is a fixed point (machines do not walk).
	 */
	int32 RegisterHumSource(const FVector& Location, float Radius, float Masking);
	void UnregisterHumSource(int32 Handle);

	/**
	 * Building-wide masking while the entity's own knocks (or the finale's
	 * open taps) cover everything. Owned by whoever is making the loud bed.
	 */
	void SetGlobalMasking(float Masking);
	float GetGlobalMasking() const { return GlobalMasking; }

	/** Total masking applied to a sound made at Location, 0..1. */
	UFUNCTION(BlueprintPure, Category = "Noise")
	float GetMaskingAt(const FVector& Location) const;

	/** Code listeners (the entity, telemetry, the ripple HUD). */
	FIGNoiseReportedSignature OnNoiseReported;

	/** How far a full-loudness (1.0) sound carries, in centimeters. */
	static constexpr float CarryPerLoudness = 2600.0f;

private:
	struct FIGHumSource
	{
		int32 Handle = 0;
		FVector Location = FVector::ZeroVector;
		float Radius = 0.0f;
		float Masking = 0.0f;
	};

	TArray<FIGHumSource> HumSources;
	int32 NextHumHandle = 1;
	float GlobalMasking = 0.0f;
};
