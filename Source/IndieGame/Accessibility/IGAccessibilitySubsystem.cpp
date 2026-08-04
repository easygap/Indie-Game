#include "Accessibility/IGAccessibilitySubsystem.h"

#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

namespace IGAccessibility
{
	const TCHAR* ConfigSection = TEXT("IndieGame.Accessibility");
	constexpr float MinimumHoldScale = 0.25f;
	constexpr float MaximumHoldScale = 1.0f;
	constexpr float MinimumCaptionScale = 0.85f;
	constexpr float MaximumCaptionScale = 1.25f;
	constexpr float MinimumCaptionSafeArea = 0.80f;
	constexpr float MaximumCaptionSafeArea = 1.0f;

	bool ParseHintMode(const FString& Value, EIGHintMode& OutMode)
	{
		if (Value.Equals(TEXT("Story"), ESearchCase::IgnoreCase))
		{
			OutMode = EIGHintMode::Story;
			return true;
		}
		if (Value.Equals(TEXT("Standard"), ESearchCase::IgnoreCase))
		{
			OutMode = EIGHintMode::Standard;
			return true;
		}
		if (Value.Equals(TEXT("Silent"), ESearchCase::IgnoreCase))
		{
			OutMode = EIGHintMode::Silent;
			return true;
		}
		return false;
	}
}

void UIGAccessibilitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadPersistedSettings();
	RebuildEffectiveSettings();
}

void UIGAccessibilitySubsystem::ApplySettings(
	const FIGAccessibilitySettings& NewSettings)
{
	PersistedSettings = Sanitize(NewSettings);
	SavePersistedSettings();
	RebuildEffectiveSettings();
}

void UIGAccessibilitySubsystem::ResetToDefaults()
{
	PersistedSettings = FIGAccessibilitySettings();
	SavePersistedSettings();
	RebuildEffectiveSettings();
}

FVector UIGAccessibilitySubsystem::GetP3HintThresholds() const
{
	switch (EffectiveSettings.HintMode)
	{
	case EIGHintMode::Story:
		return FVector(45.0f, 90.0f, 150.0f);
	case EIGHintMode::Silent:
		// Silent mode never advances the automatic clock. Keeping a stable
		// sentinel is useful to a future manual-hint UI and automated probes.
		return FVector(-1.0f, -1.0f, -1.0f);
	case EIGHintMode::Standard:
	default:
		return FVector(90.0f, 150.0f, 210.0f);
	}
}

FVector UIGAccessibilitySubsystem::GetP4HintThresholds() const
{
	switch (EffectiveSettings.HintMode)
	{
	case EIGHintMode::Story:
		return FVector(45.0f, 90.0f, 150.0f);
	case EIGHintMode::Silent:
		return FVector(-1.0f, -1.0f, -1.0f);
	case EIGHintMode::Standard:
	default:
		return FVector(45.0f, 100.0f, 150.0f);
	}
}

float UIGAccessibilitySubsystem::GetPressureRiseIntervalSeconds() const
{
	switch (EffectiveSettings.HintMode)
	{
	case EIGHintMode::Story:
		return 80.0f;
	case EIGHintMode::Silent:
		return 45.0f;
	case EIGHintMode::Standard:
	default:
		return 55.0f;
	}
}

FIGAccessibilitySettings UIGAccessibilitySubsystem::Sanitize(
	const FIGAccessibilitySettings& Candidate)
{
	FIGAccessibilitySettings Result = Candidate;
	if (Result.HintMode != EIGHintMode::Story
		&& Result.HintMode != EIGHintMode::Standard
		&& Result.HintMode != EIGHintMode::Silent)
	{
		Result.HintMode = EIGHintMode::Standard;
	}
	Result.HoldDurationScale = FMath::Clamp(
		FMath::IsFinite(Result.HoldDurationScale)
			? Result.HoldDurationScale
			: 1.0f,
		IGAccessibility::MinimumHoldScale,
		IGAccessibility::MaximumHoldScale);
	Result.CaptionSizeScale = FMath::Clamp(
		FMath::IsFinite(Result.CaptionSizeScale)
			? Result.CaptionSizeScale
			: 1.0f,
		IGAccessibility::MinimumCaptionScale,
		IGAccessibility::MaximumCaptionScale);
	Result.CaptionSafeAreaScale = FMath::Clamp(
		FMath::IsFinite(Result.CaptionSafeAreaScale)
			? Result.CaptionSafeAreaScale
			: 0.90f,
		IGAccessibility::MinimumCaptionSafeArea,
		IGAccessibility::MaximumCaptionSafeArea);
	return Result;
}

void UIGAccessibilitySubsystem::LoadPersistedSettings()
{
	PersistedSettings = FIGAccessibilitySettings();
	if (!GConfig)
	{
		return;
	}

	int32 HintMode = static_cast<int32>(PersistedSettings.HintMode);
	GConfig->GetInt(
		IGAccessibility::ConfigSection,
		TEXT("HintMode"),
		HintMode,
		GGameUserSettingsIni);
	PersistedSettings.HintMode = static_cast<EIGHintMode>(HintMode);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("ReducedCameraMotion"),
		PersistedSettings.bReducedCameraMotion,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("ReducedFlicker"),
		PersistedSettings.bReducedFlicker,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("DirectionalFearCues"),
		PersistedSettings.bDirectionalFearCues,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("AutoConnectEvidence"),
		PersistedSettings.bAutoConnectEvidence,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("SubtitlesEnabled"),
		PersistedSettings.bSubtitlesEnabled,
		GGameUserSettingsIni);
	GConfig->GetFloat(
		IGAccessibility::ConfigSection,
		TEXT("CaptionSizeScale"),
		PersistedSettings.CaptionSizeScale,
		GGameUserSettingsIni);
	GConfig->GetFloat(
		IGAccessibility::ConfigSection,
		TEXT("CaptionSafeAreaScale"),
		PersistedSettings.CaptionSafeAreaScale,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("ToggleHoldInteractions"),
		PersistedSettings.bToggleHoldInteractions,
		GGameUserSettingsIni);
	GConfig->GetFloat(
		IGAccessibility::ConfigSection,
		TEXT("HoldDurationScale"),
		PersistedSettings.HoldDurationScale,
		GGameUserSettingsIni);
	PersistedSettings = Sanitize(PersistedSettings);
}

void UIGAccessibilitySubsystem::SavePersistedSettings() const
{
	if (!GConfig)
	{
		return;
	}

	GConfig->SetInt(
		IGAccessibility::ConfigSection,
		TEXT("HintMode"),
		static_cast<int32>(PersistedSettings.HintMode),
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("ReducedCameraMotion"),
		PersistedSettings.bReducedCameraMotion,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("ReducedFlicker"),
		PersistedSettings.bReducedFlicker,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("DirectionalFearCues"),
		PersistedSettings.bDirectionalFearCues,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("AutoConnectEvidence"),
		PersistedSettings.bAutoConnectEvidence,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("SubtitlesEnabled"),
		PersistedSettings.bSubtitlesEnabled,
		GGameUserSettingsIni);
	GConfig->SetFloat(
		IGAccessibility::ConfigSection,
		TEXT("CaptionSizeScale"),
		PersistedSettings.CaptionSizeScale,
		GGameUserSettingsIni);
	GConfig->SetFloat(
		IGAccessibility::ConfigSection,
		TEXT("CaptionSafeAreaScale"),
		PersistedSettings.CaptionSafeAreaScale,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("ToggleHoldInteractions"),
		PersistedSettings.bToggleHoldInteractions,
		GGameUserSettingsIni);
	GConfig->SetFloat(
		IGAccessibility::ConfigSection,
		TEXT("HoldDurationScale"),
		PersistedSettings.HoldDurationScale,
		GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UIGAccessibilitySubsystem::RebuildEffectiveSettings()
{
	EffectiveSettings = PersistedSettings;
	ApplyCommandLineOverrides(EffectiveSettings);
	EffectiveSettings = Sanitize(EffectiveSettings);
}

void UIGAccessibilitySubsystem::ApplyCommandLineOverrides(
	FIGAccessibilitySettings& Settings) const
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FString Preset;
	if (FParse::Value(CommandLine, TEXT("IGAccessibilityPreset="), Preset))
	{
		IGAccessibility::ParseHintMode(Preset, Settings.HintMode);
	}
	Settings.bReducedCameraMotion |=
		FParse::Param(CommandLine, TEXT("IGReducedMotion"));
	Settings.bReducedFlicker |=
		FParse::Param(CommandLine, TEXT("IGReducedFlicker"));
	Settings.bDirectionalFearCues |=
		FParse::Param(CommandLine, TEXT("IGFearDirection"));
	Settings.bAutoConnectEvidence |=
		FParse::Param(CommandLine, TEXT("IGAutoConnectEvidence"));
	Settings.bToggleHoldInteractions |=
		FParse::Param(CommandLine, TEXT("IGToggleHolds"));
	if (FParse::Param(CommandLine, TEXT("IGNoSubtitles")))
	{
		Settings.bSubtitlesEnabled = false;
	}

	float HoldScale = Settings.HoldDurationScale;
	if (FParse::Value(CommandLine, TEXT("IGHoldScale="), HoldScale))
	{
		Settings.HoldDurationScale = HoldScale;
	}
	float CaptionScale = Settings.CaptionSizeScale;
	if (FParse::Value(CommandLine, TEXT("IGCaptionScale="), CaptionScale))
	{
		Settings.CaptionSizeScale = CaptionScale;
	}
	float CaptionSafeArea = Settings.CaptionSafeAreaScale;
	if (FParse::Value(
			CommandLine,
			TEXT("IGCaptionSafeArea="),
			CaptionSafeArea))
	{
		Settings.CaptionSafeAreaScale = CaptionSafeArea;
	}
}
