#include "Player/IGFlashlightComponent.h"

#include "IndieGame.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
#include "Components/SpotLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Player/IGBeamDustComponent.h"

UIGFlashlightComponent::UIGFlashlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// The torch is unavailable for most of CH01. Sway and flicker only need a
	// frame update while its beam is visible.
	PrimaryComponentTick.bStartWithTickEnabled = false;

	Beam = CreateDefaultSubobject<USpotLightComponent>(TEXT("FlashlightBeam"));
	Beam->SetupAttachment(this);
	Beam->SetMobility(EComponentMobility::Movable);
	// 싸구려 LED 손전등. 차갑고 희며, 가운데 핫스팟이 좁고 바깥은 부드럽게 죽는다.
	Beam->SetInnerConeAngle(13.0f);
	Beam->SetOuterConeAngle(34.0f);
	Beam->SetAttenuationRadius(2600.0f);
	Beam->SetLightColor(FLinearColor(0.90f, 0.95f, 1.0f));
	Beam->SetSourceRadius(1.4f);
	Beam->SetSoftSourceRadius(3.0f);
	Beam->SetCastShadows(true);
	// 실내 공기가 서 있을 때 원뿔이 보여야 한다. 손전등만 볼류메트릭 그림자를 진다.
	Beam->SetVolumetricScatteringIntensity(2.2f);
	Beam->bCastVolumetricShadow = true;
	Beam->SetVisibility(false);

	Spill = CreateDefaultSubobject<USpotLightComponent>(TEXT("FlashlightSpill"));
	Spill->SetupAttachment(Beam);
	Spill->SetMobility(EComponentMobility::Movable);
	Spill->SetRelativeRotation(FRotator(-12.0f, 0.0f, 0.0f));
	// 같은 광원에서 퍼지는 주변 빛도 벽과 문에 가려져야 한다.
	// 전 방향 점광원 대신 넓은 원뿔 하나로 발밑과 가까운 문틀만 비춘다.
	Spill->SetInnerConeAngle(50.0f);
	Spill->SetOuterConeAngle(78.0f);
	Spill->SetAttenuationRadius(300.0f);
	Spill->SetLightColor(FLinearColor(0.92f, 0.95f, 1.0f));
	Spill->SetSourceRadius(1.4f);
	Spill->SetSoftSourceRadius(3.0f);
	Spill->SetCastShadows(true);
	Spill->SetVisibility(false);

	// Volumetric scattering above already gives the beam a body in the air.
	// The motes are the readable half: they glitter, and they cluster where he
	// has just dragged himself past (§11 V1).
	BeamDust = CreateDefaultSubobject<UIGBeamDustComponent>(TEXT("BeamDust"));
	BeamDust->SetupAttachment(this);
}

void UIGFlashlightComponent::BeginPlay()
{
	Super::BeginPlay();
	PreviousWorldRotation = GetComponentRotation();
	SetComponentTickEnabled(false);
	if (BeamDust && Beam)
	{
		BeamDust->SetBeamCone(Beam->OuterConeAngle);
	}
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			AccessibilitySubsystem =
				GameInstance->GetSubsystem<UIGAccessibilitySubsystem>();
		}
	}
}

bool UIGFlashlightComponent::Toggle()
{
	SetOn(!bOn);
	return bOn;
}

void UIGFlashlightComponent::SetOn(const bool bNewOn)
{
	const bool bShouldBeOn = bNewOn && bAvailable;
	if (bOn == bShouldBeOn)
	{
		return;
	}

	bOn = bShouldBeOn;
	UE_LOG(LogIndieGame, Display, TEXT("Flashlight %s (available=%d)"), bOn ? TEXT("on") : TEXT("off"), bAvailable ? 1 : 0);
	Beam->SetVisibility(bOn);
	Spill->SetVisibility(bOn);
	if (bOn)
	{
		PreviousWorldRotation = GetComponentRotation();
		Beam->SetIntensity(BeamIntensity);
		Spill->SetIntensity(520.0f);
		SetComponentTickEnabled(true);
		// Kick the beam so switching on reads as a hand movement.
		if (!AccessibilitySubsystem
			|| !AccessibilitySubsystem->IsReducedCameraMotionEnabled())
		{
			AddImpulse(FRotator(-1.6f, 2.2f, 0.0f));
		}
		return;
	}

	// Do not carry a stale scare impulse into the next switch-on. Keeping the
	// component asleep here removes a permanent per-frame update in CH01.
	BrownOutTimer = 0.0f;
	SwayOffset = FRotator::ZeroRotator;
	ImpulseOffset = FRotator::ZeroRotator;
	Beam->SetRelativeRotation(FRotator::ZeroRotator);
	if (BeamDust)
	{
		// No beam, no motes. Dust that survived the switch-off would be the one
		// thing visible in a black corridor.
		BeamDust->ClearBeam();
	}
	SetComponentTickEnabled(false);
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
	const bool bReducedMotion = AccessibilitySubsystem
		&& AccessibilitySubsystem->IsReducedCameraMotionEnabled();
	if (!bOn || bReducedMotion)
	{
		return;
	}
	ImpulseOffset += Impulse;
	ImpulseOffset.Pitch = FMath::Clamp(ImpulseOffset.Pitch, -9.0f, 9.0f);
	ImpulseOffset.Yaw = FMath::Clamp(ImpulseOffset.Yaw, -9.0f, 9.0f);
}

void UIGFlashlightComponent::TriggerBrownOut(const float DurationSeconds)
{
	if (AccessibilitySubsystem
		&& AccessibilitySubsystem->IsReducedFlickerEnabled())
	{
		BrownOutTimer = 0.0f;
		return;
	}
	if (bOn)
	{
		BrownOutTimer = FMath::Max(
			BrownOutTimer,
			FMath::Max(0.0f, DurationSeconds));
		AddImpulse(FRotator(2.4f, -3.0f, 0.0f));
	}
}

void UIGFlashlightComponent::TickComponent(
	const float DeltaSeconds,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	if (!bOn)
	{
		// Defensive self-healing for state changes made by future callers.
		SetComponentTickEnabled(false);
		return;
	}

	UpdateSway(DeltaSeconds);
	const float Flicker = SampleFlicker(DeltaSeconds);
	Beam->SetIntensity(BeamIntensity * Flicker);
	Spill->SetIntensity(520.0f * Flicker);

	if (BeamDust)
	{
		// Drive from the beam, not from this component: the sway offset lives on
		// the light, and the dust has to hang in the cone that is actually lit.
		const FTransform BeamTransform = Beam->GetComponentTransform();
		BeamDust->UpdateBeam(
			BeamTransform.GetLocation(),
			BeamTransform.GetUnitAxis(EAxis::X),
			Flicker);
	}
}

void UIGFlashlightComponent::UpdateSway(const float DeltaSeconds)
{
	if (DeltaSeconds <= SMALL_NUMBER)
	{
		return;
	}
	// 프레임당 회전량 대신 초당 회전량을 쓴다. 같은 속도로 고개를 돌리면
	// 30fps와 120fps에서도 빛이 같은 만큼 뒤따라와야 한다.
	const FRotator CurrentRotation = GetComponentRotation();
	const FRotator ViewDelta = (CurrentRotation - PreviousWorldRotation).GetNormalized();
	PreviousWorldRotation = CurrentRotation;
	constexpr float LagSeconds = 1.4f / 60.0f;
	const float LagScale = LagSeconds / DeltaSeconds;

	const bool bReducedMotion = AccessibilitySubsystem
		&& AccessibilitySubsystem->IsReducedCameraMotionEnabled();
	const FRotator TargetSway = bReducedMotion
		? FRotator::ZeroRotator
		: FRotator(
			FMath::Clamp(-ViewDelta.Pitch * LagScale, -6.0f, 6.0f),
			FMath::Clamp(-ViewDelta.Yaw * LagScale, -7.0f, 7.0f),
			0.0f);
	SwayOffset = FMath::Lerp(
		SwayOffset, TargetSway, 1.0f - FMath::Exp(-SwayFollowSpeed * DeltaSeconds));
	if (bReducedMotion)
	{
		ImpulseOffset = FRotator::ZeroRotator;
	}
	ImpulseOffset = FMath::Lerp(
		ImpulseOffset, FRotator::ZeroRotator, 1.0f - FMath::Exp(-4.5f * DeltaSeconds));

	Beam->SetRelativeRotation(SwayOffset + ImpulseOffset);
}

float UIGFlashlightComponent::SampleFlicker(const float DeltaSeconds)
{
	FlickerTime += DeltaSeconds;
	if (AccessibilitySubsystem
		&& AccessibilitySubsystem->IsReducedFlickerEnabled())
	{
		BrownOutTimer = 0.0f;
		FlickerValue = 1.0f;
		return FlickerValue;
	}

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

	// A fixed cheap-torch ripple preserves unease without consuming a resource.
	constexpr float Weakness = 0.18f;
	const float Ripple = 1.0f - (0.03f + 0.30f * Weakness) * (0.6f * Fast + 0.4f * Slow);

	// Very rare, short brown-outs are presentation-only and always recover.
	if (BrownOutTimer > 0.0f)
	{
		BrownOutTimer -= DeltaSeconds;
		return Ripple * 0.12f;
	}
	if (Fast > 0.997f && Slow < 0.22f)
	{
		BrownOutTimer = 0.05f + 0.10f * Slow;
	}
	FlickerValue = Ripple;
	return FlickerValue;
}
