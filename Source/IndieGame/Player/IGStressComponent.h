#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IGStressComponent.generated.h"

class UAudioComponent;
class UCameraComponent;
class UPostProcessComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FIGStressChangedSignature,
	float, Stress);

/**
 * The player's fear state, in one number from 0 (calm) to 1 (panicking).
 *
 * Stress rises from three sources — standing in the dark, being near
 * something that is hunting you, and scripted scares — and bleeds off slowly
 * when none of those apply. It is deliberately slow to fall: the point is
 * that the corridor stays frightening for a while after the light comes back.
 *
 * What it drives:
 *  - heartbeat rate and volume (procedural, no audio assets)
 *  - breathing rate, which the character's camera sway reads
 *  - a post-process ramp: vignette closes in, colour drains, the lens
 *    aberrates slightly at the edges
 *  - a fine camera tremor at high stress
 *
 * Nothing here kills the player. Fear is a lens, not a health bar.
 */
UCLASS(ClassGroup = (IndieGame), meta = (BlueprintSpawnableComponent))
class INDIEGAME_API UIGStressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UIGStressComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaSeconds,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Current fear, 0..1. */
	UFUNCTION(BlueprintPure, Category = "Stress")
	float GetStress() const { return Stress; }

	/** An instant jolt: a slam, a figure appearing, a light dying. */
	UFUNCTION(BlueprintCallable, Category = "Stress")
	void ApplyScare(float Amount);

	/**
	 * Sustained pressure this frame, 0..1, from whatever is hunting the
	 * player. The chase director sets this every tick; it decays on its own
	 * if nothing does, so a dead pursuer cannot leave the player pinned.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stress")
	void SetThreatPressure(float Pressure);

	/** Told by the player pawn how dark it is where they stand, 0..1. */
	UFUNCTION(BlueprintCallable, Category = "Stress")
	void SetDarkness(float InDarkness);

	/**
	 * Silences the bodily pulse for an authored reveal window. Any pulse already
	 * playing is stopped; optionally one pulse resolves the silence at its end.
	 */
	void SuppressHeartbeat(float DurationSeconds, bool bPlayOneBeatOnRelease);

	/** Current breaths per minute, for the pawn's breath sway. */
	UFUNCTION(BlueprintPure, Category = "Stress")
	float GetBreathsPerMinute() const;

	/** Camera tremor in degrees, applied by the pawn on top of its own sway. */
	UFUNCTION(BlueprintPure, Category = "Stress")
	FRotator GetTremor() const { return Tremor; }

	UPROPERTY(BlueprintAssignable, Category = "Stress|Events")
	FIGStressChangedSignature OnStressChanged;

protected:
	/**
	 * Stress gained per second while standing in full darkness.
	 *
	 * This is per SECOND, which is easy to misread. At the original 0.055 the
	 * meter reached half in nine seconds and pegged in eighteen — fine for a
	 * corridor you cross once, useless for a chapter that spends minutes in
	 * the dark, because a saturated meter stops meaning anything. At 0.012
	 * full darkness takes ~42 s to reach half and ~83 s to saturate, so being
	 * in the dark sets a floor and the scares still have somewhere to go.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stress", meta = (ClampMin = "0.0"))
	float DarknessRate = 0.012f;

	/** Stress gained per second at full threat pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stress", meta = (ClampMin = "0.0"))
	float ThreatRate = 0.42f;

	/** Stress lost per second when nothing is applying pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stress", meta = (ClampMin = "0.0"))
	float RecoveryRate = 0.075f;

	/** Resting and panicked heart rates, in beats per minute. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stress", meta = (ClampMin = "30.0"))
	float RestingBPM = 62.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stress", meta = (ClampMin = "60.0"))
	float PanicBPM = 148.0f;

private:
	/** Enables frame updates only while fear state is changing or audible. */
	void RefreshTickState();
	void UpdateStress(float DeltaSeconds);
	void UpdateHeartbeat(float DeltaSeconds);
	void PlayHeartbeat(float EffectiveStress);
	void UpdateTremor(float DeltaSeconds);
	/** §18.3. 접근성의 멀미 완화 비네트 세기. 없으면 0이다. */
	float GetComfortVignetteStrength() const;
	void UpdatePostProcess();

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> FearPostProcess;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> HeartbeatComponent;

	float Stress = 0.0f;
	float Darkness = 0.0f;
	float ThreatPressure = 0.0f;
	/** Scare jolts decay separately so a jolt reads as a spike, not a plateau. */
	float ScareCharge = 0.0f;
	float BeatPhase = 0.0f;
	float HeartbeatSuppressionRemaining = 0.0f;
	bool bPlayHeartbeatOnSuppressionRelease = false;
	float TremorTime = 0.0f;
	FRotator Tremor = FRotator::ZeroRotator;
};
