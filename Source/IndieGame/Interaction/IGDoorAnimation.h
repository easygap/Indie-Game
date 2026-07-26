#pragma once

#include "CoreMinimal.h"

/**
 * Minimal smoothstep tween shared by hinged and sliding door actors.
 * Owners enable Tick only while bActive is true, keeping doors tick-free at rest.
 */
struct FIGDoorAnimation
{
	float Elapsed = 0.0f;
	float Duration = 0.9f;
	float StartValue = 0.0f;
	float TargetValue = 0.0f;
	float CurrentValue = 0.0f;
	bool bActive = false;

	void Begin(const float FromValue, const float ToValue, const float InDuration)
	{
		StartValue = FromValue;
		TargetValue = ToValue;
		CurrentValue = FromValue;
		Duration = FMath::Max(InDuration, 0.05f);
		Elapsed = 0.0f;
		bActive = true;
	}

	/** Advances the tween; returns true when it finished on this update. */
	bool Advance(const float DeltaSeconds)
	{
		if (!bActive)
		{
			return false;
		}

		Elapsed += FMath::Max(0.0f, DeltaSeconds);
		const float LinearAlpha = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
		CurrentValue = FMath::Lerp(
			StartValue,
			TargetValue,
			FMath::SmoothStep(0.0f, 1.0f, LinearAlpha));

		if (LinearAlpha >= 1.0f)
		{
			bActive = false;
			CurrentValue = TargetValue;
			return true;
		}

		return false;
	}
};
