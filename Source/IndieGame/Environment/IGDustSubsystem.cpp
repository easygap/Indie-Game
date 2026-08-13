#include "Environment/IGDustSubsystem.h"

#include "Engine/World.h"

void UIGDustSubsystem::ReportDisturbance(
	const FVector& Location,
	const float Strength)
{
	const float Clamped = FMath::Clamp(Strength, 0.0f, 1.0f);
	if (Clamped <= 0.0f)
	{
		return;
	}

	PruneExpired();
	const double Now = GetNow();

	// A crawl reports every few centimeters. Refreshing the newest sample keeps
	// the lane a line of distinct stirs instead of one saturated blob, which is
	// what makes the direction of travel readable in the beam.
	if (Disturbances.Num() > 0)
	{
		FIGDustDisturbance& Newest = Disturbances.Last();
		if (FVector::DistSquared(Newest.Location, Location)
			<= MergeDistance * MergeDistance)
		{
			Newest.Location = Location;
			Newest.Strength = FMath::Max(Newest.Strength, Clamped);
			Newest.TimeSeconds = Now;
			return;
		}
	}

	if (Disturbances.Num() >= MaxDisturbances)
	{
		// Oldest first: the far end of the lane fades before the near end.
		Disturbances.RemoveAt(0, 1, EAllowShrinking::No);
	}
	Disturbances.Add({Location, Clamped, Now});
}

float UIGDustSubsystem::GetDensityMultiplierAt(const FVector& Location) const
{
	PruneExpired();
	if (Disturbances.Num() == 0)
	{
		return 1.0f;
	}

	const double Now = GetNow();
	float Strongest = 0.0f;
	for (const FIGDustDisturbance& Sample : Disturbances)
	{
		const float DistanceSquared =
			static_cast<float>(FVector::DistSquared(Sample.Location, Location));
		if (DistanceSquared >= DisturbanceRadius * DisturbanceRadius)
		{
			continue;
		}
		const float AgeSeconds = static_cast<float>(Now - Sample.TimeSeconds);
		const float AgeFade = 1.0f
			- FMath::Clamp(AgeSeconds / DisturbanceLifetimeSeconds, 0.0f, 1.0f);
		// Smooth radial falloff: a hard edge would draw a visible sphere in the
		// beam, and the player must read a lane, not a bubble.
		const float Radial = 1.0f
			- FMath::Sqrt(DistanceSquared) / DisturbanceRadius;
		Strongest = FMath::Max(Strongest, Sample.Strength * AgeFade * Radial);
		if (Strongest >= 1.0f)
		{
			break;
		}
	}

	return 1.0f
		+ (MaxDensityMultiplier - 1.0f) * FMath::Clamp(Strongest, 0.0f, 1.0f);
}

void UIGDustSubsystem::CollectDisturbances(
	const FVector& Center,
	const float Radius,
	TArray<FIGDustDisturbance>& OutSamples) const
{
	OutSamples.Reset();
	PruneExpired();
	if (Disturbances.Num() == 0)
	{
		return;
	}

	const double Now = GetNow();
	const float RadiusSquared = FMath::Square(FMath::Max(0.0f, Radius));
	for (int32 Index = Disturbances.Num() - 1; Index >= 0; --Index)
	{
		const FIGDustDisturbance& Sample = Disturbances[Index];
		if (FVector::DistSquared(Sample.Location, Center) > RadiusSquared)
		{
			continue;
		}
		const float AgeSeconds = static_cast<float>(Now - Sample.TimeSeconds);
		const float AgeFade = 1.0f
			- FMath::Clamp(AgeSeconds / DisturbanceLifetimeSeconds, 0.0f, 1.0f);
		const float Decayed = Sample.Strength * AgeFade;
		if (Decayed <= 0.0f)
		{
			continue;
		}
		OutSamples.Add({Sample.Location, Decayed, Sample.TimeSeconds});
	}
}

void UIGDustSubsystem::ClearDisturbances()
{
	Disturbances.Reset();
}

void UIGDustSubsystem::ReportSettledPrint(
	const FVector& Location,
	const float YawDegrees,
	const EIGDustPrintKind Kind)
{
	// Standing still would otherwise stack a hundred prints in one spot and
	// evict the trail the player is trying to read.
	for (FIGDustPrint& Existing : SettledPrints)
	{
		if (Existing.Kind == Kind
			&& FVector::DistSquared(Existing.Location, Location)
				<= PrintMergeDistance * PrintMergeDistance)
		{
			Existing.Location = Location;
			Existing.YawDegrees = YawDegrees;
			return;
		}
	}

	if (SettledPrints.Num() >= MaxSettledPrints)
	{
		// Oldest first: the far end of the search fades before where you are.
		SettledPrints.RemoveAt(0, 1, EAllowShrinking::No);
	}
	SettledPrints.Add({Location, YawDegrees, Kind});
}

void UIGDustSubsystem::CollectSettledPrints(
	TArray<FIGDustPrint>& OutPrints) const
{
	OutPrints = SettledPrints;
}

void UIGDustSubsystem::ClearSettledPrints()
{
	SettledPrints.Reset();
}

int32 UIGDustSubsystem::GetLiveDisturbanceCount() const
{
	PruneExpired();
	return Disturbances.Num();
}

void UIGDustSubsystem::PruneExpired() const
{
	if (Disturbances.Num() == 0)
	{
		return;
	}
	const double Now = GetNow();
	Disturbances.RemoveAll([Now](const FIGDustDisturbance& Sample)
	{
		return Now - Sample.TimeSeconds >= DisturbanceLifetimeSeconds
			|| Sample.Strength <= 0.0f;
	});
}

double UIGDustSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}
