#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "IGFlashlightComponent.generated.h"

class USpotLightComponent;
class UPointLightComponent;
class UIGAccessibilitySubsystem;
class UIGBeamDustComponent;

/**
 * The handheld light the player carries from chapter two on.
 *
 * It is deliberately a poor light: a warm, narrow, slightly uneven beam from
 * a cheap convenience-store torch. Three things make it feel handheld rather
 * than head-mounted — the beam lags the view by a few degrees, it swings with
 * the walk cycle, and the contact of a footfall nudges it. Brief brown-outs
 * are pressure cues only: the light never depletes into a progression lock.
 */
UCLASS(ClassGroup = (IndieGame), meta = (BlueprintSpawnableComponent))
class INDIEGAME_API UIGFlashlightComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UIGFlashlightComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaSeconds,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Turns the torch on/off. Returns the new state. */
	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	bool Toggle();

	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void SetOn(bool bNewOn);

	UFUNCTION(BlueprintPure, Category = "Flashlight")
	bool IsOn() const { return bOn; }

	/**
	 * Whether the beam is actually putting out useful light right now.
	 *
	 * Not the same as IsOn(): a pressure cue can brown the beam out to about a
	 * tenth for a fraction of a second at a time while the switch stays on.
	 * The fear model has to see those frames as darkness, otherwise the
	 * visually blackest moments in the game read as "fully lit" and the
	 * player's pulse drops exactly when it should spike.
	 */
	UFUNCTION(BlueprintPure, Category = "Flashlight")
	bool IsProvidingLight() const { return bOn && BrownOutTimer <= 0.0f; }

	/** Whether the player has picked the torch up at all. */
	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void SetAvailable(bool bNewAvailable);

	UFUNCTION(BlueprintPure, Category = "Flashlight")
	bool IsAvailable() const { return bAvailable; }

	/** Compatibility value; REBIRTH keeps it at full charge. */
	UFUNCTION(BlueprintPure, Category = "Flashlight")
	float GetBatteryFraction() const { return BatteryFraction; }

	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void RefillBattery(float Fraction);

	/** Nudges the beam, e.g. on a footfall or a scare. */
	void AddImpulse(const FRotator& Impulse);

	/** Presentation-only darkness cue; switch and availability stay intact. */
	void TriggerBrownOut(float DurationSeconds);

	/** The suspended plaster dust this beam reveals. See §11 V1. */
	UFUNCTION(BlueprintPure, Category = "Flashlight")
	UIGBeamDustComponent* GetBeamDust() const { return BeamDust; }

protected:
	/** Beam intensity in candelas at full charge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flashlight", meta = (ClampMin = "0.0"))
	float BeamIntensity = 7600.0f;

	/** How fast the beam catches up to the view; lower is heavier in the hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flashlight", meta = (ClampMin = "0.5"))
	float SwayFollowSpeed = 7.5f;

private:
	void UpdateSway(float DeltaSeconds);
	float SampleFlicker(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Flashlight", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpotLightComponent> Beam;

	/** Short-range fill so the torch lights the ground at the player's feet. */
	UPROPERTY(VisibleAnywhere, Category = "Flashlight", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Spill;

	/**
	 * Airborne plaster dust inside the cone. It belongs to the torch because
	 * only the torch can reveal it, and it is driven from the beam's own swayed
	 * transform so the motes hang in the building rather than on the view.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Flashlight", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UIGBeamDustComponent> BeamDust;

	UPROPERTY(Transient)
	TObjectPtr<UIGAccessibilitySubsystem> AccessibilitySubsystem;

	FRotator SwayOffset = FRotator::ZeroRotator;
	FRotator ImpulseOffset = FRotator::ZeroRotator;
	FRotator PreviousWorldRotation = FRotator::ZeroRotator;
	float BatteryFraction = 1.0f;
	float FlickerTime = 0.0f;
	float FlickerValue = 1.0f;
	float BrownOutTimer = 0.0f;
	uint32 NoiseCounter = 0;
	bool bOn = false;
	bool bAvailable = false;
};
