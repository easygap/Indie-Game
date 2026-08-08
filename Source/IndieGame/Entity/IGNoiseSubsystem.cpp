#include "Entity/IGNoiseSubsystem.h"

#include "Engine/World.h"

FIGNoiseEvent UIGNoiseSubsystem::ReportNoise(
	const FVector& Location,
	const float Loudness,
	AActor* Instigator)
{
	FIGNoiseEvent Event;
	Event.Location = Location;
	Event.Instigator = Instigator;

	const UWorld* World = GetWorld();
	Event.TimeSeconds = World ? World->GetRealTimeSeconds() : 0.0;

	const float Effective =
		FMath::Clamp(Loudness, 0.0f, 1.0f) - GetMaskingAt(Location);
	if (Effective <= 0.0f)
	{
		// Swallowed by a hum or the entity's own knocking. Nothing sounded,
		// as far as the building is concerned.
		return Event;
	}

	Event.Loudness = Effective;
	Event.Radius = Effective * CarryPerLoudness;
	OnNoiseReported.Broadcast(Event);
	return Event;
}

int32 UIGNoiseSubsystem::RegisterHumSource(
	const FVector& Location,
	const float Radius,
	const float Masking)
{
	FIGHumSource Source;
	Source.Handle = NextHumHandle++;
	Source.Location = Location;
	Source.Radius = FMath::Max(Radius, 0.0f);
	Source.Masking = FMath::Clamp(Masking, 0.0f, 1.0f);
	HumSources.Add(Source);
	return Source.Handle;
}

void UIGNoiseSubsystem::UnregisterHumSource(const int32 Handle)
{
	HumSources.RemoveAll([Handle](const FIGHumSource& Source)
	{
		return Source.Handle == Handle;
	});
}

void UIGNoiseSubsystem::SetGlobalMasking(const float Masking)
{
	GlobalMasking = FMath::Clamp(Masking, 0.0f, 1.0f);
}

float UIGNoiseSubsystem::GetMaskingAt(const FVector& Location) const
{
	// Hums do not stack: standing between two machines is as covered as the
	// stronger one alone. The global window stacks on top — moving on the
	// entity's beat while beside the fridge really is the safest thing.
	float BestHum = 0.0f;
	for (const FIGHumSource& Source : HumSources)
	{
		if (Source.Radius <= 0.0f)
		{
			continue;
		}
		const float Distance = FVector::Dist(Location, Source.Location);
		if (Distance >= Source.Radius)
		{
			continue;
		}
		// Full masking at the machine, fading linearly to zero at the edge.
		const float Falloff = 1.0f - Distance / Source.Radius;
		BestHum = FMath::Max(BestHum, Source.Masking * Falloff);
	}
	return FMath::Clamp(BestHum + GlobalMasking, 0.0f, 1.0f);
}
