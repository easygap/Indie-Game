#include "Player/IGFlashlightComponent.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/CollisionProfile.h"

UIGFlashlightComponent::UIGFlashlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	Beam = CreateDefaultSubobject<USpotLightComponent>(TEXT("FlashlightBeam"));
	Beam->SetupAttachment(this);
	Beam->SetMobility(EComponentMobility::Movable);
	// A cheap torch: warm, tight hot spot with a soft outer falloff.
	Beam->SetInnerConeAngle(15.0f);
	Beam->SetOuterConeAngle(34.0f);
	Beam->SetAttenuationRadius(2600.0f);
	Beam->SetLightColor(FLinearColor(1.0f, 0.90f, 0.74f));
	Beam->SetSourceRadius(1.4f);
	Beam->SetSoftSourceRadius(3.0f);
	Beam->SetCastShadows(true);
	Beam->SetVolumetricScatteringIntensity(1.5f);
	Beam->SetVisibility(false);

	Spill = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashlightSpill"));
	Spill->SetupAttachment(this);
	Spill->SetMobility(EComponentMobility::Movable);
	Spill->SetRelativeLocation(FVector(24.0f, 0.0f, -14.0f));
	Spill->SetAttenuationRadius(240.0f);
	Spill->SetLightColor(FLinearColor(1.0f, 0.88f, 0.70f));
	Spill->SetSourceRadius(6.0f);
	Spill->SetCastShadows(false);
	Spill->SetVisibility(false);
}

void UIGFlashlightComponent::BeginPlay()
{
	Super::BeginPlay();
	PreviousWorldRotation = GetComponentRotation();
}

bool UIGFlashlightComponent::Toggle()
{
	SetOn(!bOn);
	return bOn;
}

void UIGFlashlightComponent::SetOn(const bool bNewOn)
{
	// A flat cell will not strike at all; the click is all you get.
	const bool bCanLight = bAvailable && BatteryFraction > 0.0f;
	bOn = bNewOn && bCanLight;
	Beam->SetVisibility(bOn);
	Spill->SetVisibility(bOn);
	if (bOn)
	{
		// Kick the beam so switching on reads as a hand movement.
		AddImpulse(FRotator(-1.6f, 2.2f, 0.0f));
	}
}

void UIGFlashlightComponent::SetAvailable(const bool bNewAvailable)
{
	bAvailable = bNewAvailable;
	if (!bAvailable)
	{
		SetOn(false);
	}
}

void UIGFlashlightComponent::RefillBattery(const float Fraction)
{
	BatteryFraction = FMath::Clamp(BatteryFraction + Fraction, 0.0f, 1.0f);
}

void UIGFlashlightComponent::AddImpulse(const FRotator& Impulse)
{
	ImpulseOffset += Impulse;
	ImpulseOffset.Pitch = FMath::Clamp(ImpulseOffset.Pitch, -9.0f, 9.0f);
	ImpulseOffset.Yaw = FMath::Clamp(ImpulseOffset.Yaw, -9.0f, 9.0f);
}

void UIGFlashlightComponent::TickComponent(
	const float DeltaSeconds,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	UpdateSway(DeltaSeconds);

	if (!bOn)
	{
		return;
	}

	UpdateBattery(DeltaSeconds);

	// Output falls away as the cell dies, then the flicker takes over: at
	// full charge it is a barely visible ripple, near flat it is a brown-out.
	const float ChargeCurve = FMath::Pow(FMath::Clamp(BatteryFraction, 0.0f, 1.0f), 0.35f);
	const float Flicker = SampleFlicker(DeltaSeconds);
	Beam->SetIntensity(BeamIntensity * ChargeCurve * Flicker);
	Spill->SetIntensity(220.0f * ChargeCurve * Flicker);
}

void UIGFlashlightComponent::UpdateSway(const float DeltaSeconds)
{
	// The torch is held in a hand, not bolted to the skull: the beam trails
	// the view by a few degrees and overshoots slightly when the view stops.
	const FRotator CurrentRotation = GetComponentRotation();
	const FRotator ViewDelta = (CurrentRotation - PreviousWorldRotation).GetNormalized();
	PreviousWorldRotation = CurrentRotation;

	const FRotator TargetSway(
		FMath::Clamp(-ViewDelta.Pitch * 1.4f, -6.0f, 6.0f),
		FMath::Clamp(-ViewDelta.Yaw * 1.4f, -7.0f, 7.0f),
		0.0f);
	SwayOffset = FMath::RInterpTo(SwayOffset, TargetSway, DeltaSeconds, SwayFollowSpeed);
	ImpulseOffset = FMath::RInterpTo(ImpulseOffset, FRotator::ZeroRotator, DeltaSeconds, 4.5f);

	Beam->SetRelativeRotation(SwayOffset + ImpulseOffset);
}

void UIGFlashlightComponent::UpdateBattery(const float DeltaSeconds)
{
	if (BatterySeconds <= 0.0f)
	{
		return;
	}

	BatteryFraction = FMath::Max(0.0f, BatteryFraction - DeltaSeconds / BatterySeconds);
	if (BatteryFraction <= 0.0f)
	{
		SetOn(false);
	}
}

float UIGFlashlightComponent::SampleFlicker(const float DeltaSeconds)
{
	FlickerTime += DeltaSeconds;

	// Hash-based value noise: deterministic, no allocation, and cheap enough
	// to run every frame. Two octaves give a ripple plus a slower wander.
	auto Hash01 = [this](const int32 Step)
	{
		uint32 Value = static_cast<uint32>(Step) * 2654435761u + NoiseCounter;
		Value ^= Value >> 15;
		Value *= 2246822519u;
		Value ^= Value >> 13;
		return (Value & 0xFFFF) / 65535.0f;
	};

	const float FastStep = FlickerTime * 18.0f;
	const float SlowStep = FlickerTime * 2.4f;
	const float Fast = FMath::Lerp(
		Hash01(FMath::FloorToInt(FastStep)),
		Hash01(FMath::FloorToInt(FastStep) + 1),
		FMath::Frac(FastStep));
	const float Slow = FMath::Lerp(
		Hash01(FMath::FloorToInt(SlowStep) + 7919),
		Hash01(FMath::FloorToInt(SlowStep) + 7920),
		FMath::Frac(SlowStep));

	// Ripple depth scales with how flat the cell is.
	const float Weakness = 1.0f - FMath::Clamp(BatteryFraction, 0.0f, 1.0f);
	const float Ripple = 1.0f - (0.03f + 0.30f * Weakness) * (0.6f * Fast + 0.4f * Slow);

	// Below a quarter charge the torch starts dropping out entirely.
	if (BrownOutTimer > 0.0f)
	{
		BrownOutTimer -= DeltaSeconds;
		return Ripple * 0.12f;
	}
	if (BatteryFraction < 0.25f && Fast > 0.985f)
	{
		BrownOutTimer = 0.05f + 0.22f * Slow;
	}
	FlickerValue = Ripple;
	return FlickerValue;
}
