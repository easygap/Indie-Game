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
	constexpr float MaximumCaptionScale = 2.0f;
	constexpr float MinimumCaptionBackgroundOpacity = 0.0f;
	constexpr float MaximumCaptionBackgroundOpacity = 1.0f;
	constexpr float MinimumCaptionSafeArea = 0.80f;
	constexpr float MaximumCaptionSafeArea = 1.0f;
	constexpr float MinimumCaptionDuration = 0.75f;
	constexpr float MaximumCaptionDuration = 2.0f;
	// 68도는 검수 스틸이 이미 쓰고 있던 값이고, 100도를 넘기면 1인칭
	// 손전등 원뿔이 화면 밖으로 밀려 무엇을 비추는지 안 보인다.
	constexpr float MinimumFieldOfView = 68.0f;
	constexpr float MaximumFieldOfView = 100.0f;
	constexpr float MinimumComfortVignette = 0.0f;
	constexpr float MaximumComfortVignette = 1.0f;
	// §19.8 표의 두 배율. 문서에 적힌 숫자를 여기 한 번만 적는다.
	constexpr float KnockWindowAssistScale = 1.6f;
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

float UIGAccessibilitySubsystem::GetKnockWindowScale() const
{
	return EffectiveSettings.bCognitiveAssist
		? IGAccessibility::KnockWindowAssistScale
		: 1.0f;
}

FIGAccessibilitySettings UIGAccessibilitySubsystem::Sanitize(
	const FIGAccessibilitySettings& Candidate)
{
	FIGAccessibilitySettings Result = Candidate;
	Result.HoldDurationScale = FMath::Clamp(
		FMath::IsFinite(Result.HoldDurationScale)
			? Result.HoldDurationScale
			: 1.0f,
		IGAccessibility::MinimumHoldScale,
		IGAccessibility::MaximumHoldScale);
	Result.CaptionDurationScale = FMath::Clamp(
		FMath::IsFinite(Result.CaptionDurationScale)
			? Result.CaptionDurationScale
			: 1.0f,
		IGAccessibility::MinimumCaptionDuration,
		IGAccessibility::MaximumCaptionDuration);
	Result.ComfortVignetteStrength = FMath::Clamp(
		FMath::IsFinite(Result.ComfortVignetteStrength)
			? Result.ComfortVignetteStrength
			: 0.0f,
		IGAccessibility::MinimumComfortVignette,
		IGAccessibility::MaximumComfortVignette);
	Result.FieldOfViewDegrees = FMath::Clamp(
		FMath::IsFinite(Result.FieldOfViewDegrees)
			? Result.FieldOfViewDegrees
			: 78.0f,
		IGAccessibility::MinimumFieldOfView,
		IGAccessibility::MaximumFieldOfView);
	Result.CaptionSizeScale = FMath::Clamp(
		FMath::IsFinite(Result.CaptionSizeScale)
			? Result.CaptionSizeScale
			: 1.0f,
		IGAccessibility::MinimumCaptionScale,
		IGAccessibility::MaximumCaptionScale);
	Result.CaptionBackgroundOpacity = FMath::Clamp(
		FMath::IsFinite(Result.CaptionBackgroundOpacity)
			? Result.CaptionBackgroundOpacity
			: 0.82f,
		IGAccessibility::MinimumCaptionBackgroundOpacity,
		IGAccessibility::MaximumCaptionBackgroundOpacity);
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
		TEXT("SubtitlesEnabled"),
		PersistedSettings.bSubtitlesEnabled,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("SoundCaptionsEnabled"),
		PersistedSettings.bSoundCaptionsEnabled,
		GGameUserSettingsIni);
	GConfig->GetFloat(
		IGAccessibility::ConfigSection,
		TEXT("CaptionSizeScale"),
		PersistedSettings.CaptionSizeScale,
		GGameUserSettingsIni);
	GConfig->GetFloat(
		IGAccessibility::ConfigSection,
		TEXT("CaptionBackgroundOpacity"),
		PersistedSettings.CaptionBackgroundOpacity,
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
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("ToggleCrouch"),
		PersistedSettings.bToggleCrouch,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("HapticsEnabled"),
		PersistedSettings.bHapticsEnabled,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("MicrophoneNoiseEnabled"),
		PersistedSettings.bMicrophoneNoiseEnabled,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("KnockHapticSubstitute"),
		PersistedSettings.bKnockHapticSubstitute,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("KnockRippleSubstitute"),
		PersistedSettings.bKnockRippleSubstitute,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("HeartbeatWarning"),
		PersistedSettings.bHeartbeatWarning,
		GGameUserSettingsIni);
	GConfig->GetBool(
		IGAccessibility::ConfigSection,
		TEXT("CognitiveAssist"),
		PersistedSettings.bCognitiveAssist,
		GGameUserSettingsIni);
	GConfig->GetFloat(
		IGAccessibility::ConfigSection,
		TEXT("CaptionDurationScale"),
		PersistedSettings.CaptionDurationScale,
		GGameUserSettingsIni);
	GConfig->GetFloat(
		IGAccessibility::ConfigSection,
		TEXT("FieldOfViewDegrees"),
		PersistedSettings.FieldOfViewDegrees,
		GGameUserSettingsIni);
	GConfig->GetFloat(
		IGAccessibility::ConfigSection,
		TEXT("ComfortVignetteStrength"),
		PersistedSettings.ComfortVignetteStrength,
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
		TEXT("SubtitlesEnabled"),
		PersistedSettings.bSubtitlesEnabled,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("SoundCaptionsEnabled"),
		PersistedSettings.bSoundCaptionsEnabled,
		GGameUserSettingsIni);
	GConfig->SetFloat(
		IGAccessibility::ConfigSection,
		TEXT("CaptionSizeScale"),
		PersistedSettings.CaptionSizeScale,
		GGameUserSettingsIni);
	GConfig->SetFloat(
		IGAccessibility::ConfigSection,
		TEXT("CaptionBackgroundOpacity"),
		PersistedSettings.CaptionBackgroundOpacity,
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
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("ToggleCrouch"),
		PersistedSettings.bToggleCrouch,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("HapticsEnabled"),
		PersistedSettings.bHapticsEnabled,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("MicrophoneNoiseEnabled"),
		PersistedSettings.bMicrophoneNoiseEnabled,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("KnockHapticSubstitute"),
		PersistedSettings.bKnockHapticSubstitute,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("KnockRippleSubstitute"),
		PersistedSettings.bKnockRippleSubstitute,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("HeartbeatWarning"),
		PersistedSettings.bHeartbeatWarning,
		GGameUserSettingsIni);
	GConfig->SetBool(
		IGAccessibility::ConfigSection,
		TEXT("CognitiveAssist"),
		PersistedSettings.bCognitiveAssist,
		GGameUserSettingsIni);
	GConfig->SetFloat(
		IGAccessibility::ConfigSection,
		TEXT("CaptionDurationScale"),
		PersistedSettings.CaptionDurationScale,
		GGameUserSettingsIni);
	GConfig->SetFloat(
		IGAccessibility::ConfigSection,
		TEXT("FieldOfViewDegrees"),
		PersistedSettings.FieldOfViewDegrees,
		GGameUserSettingsIni);
	GConfig->SetFloat(
		IGAccessibility::ConfigSection,
		TEXT("ComfortVignetteStrength"),
		PersistedSettings.ComfortVignetteStrength,
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
	Settings.bReducedCameraMotion |=
		FParse::Param(CommandLine, TEXT("IGReducedMotion"));
	Settings.bReducedFlicker |=
		FParse::Param(CommandLine, TEXT("IGReducedFlicker"));
	Settings.bDirectionalFearCues |=
		FParse::Param(CommandLine, TEXT("IGFearDirection"));
	Settings.bToggleHoldInteractions |=
		FParse::Param(CommandLine, TEXT("IGToggleHolds"));
	Settings.bMicrophoneNoiseEnabled |=
		FParse::Param(CommandLine, TEXT("IGMicrophoneMode"));
	if (FParse::Param(CommandLine, TEXT("IGHoldCrouch")))
	{
		Settings.bToggleCrouch = false;
	}
	if (FParse::Param(CommandLine, TEXT("IGNoHaptics")))
	{
		Settings.bHapticsEnabled = false;
	}
	if (FParse::Param(CommandLine, TEXT("IGNoSubtitles")))
	{
		Settings.bSubtitlesEnabled = false;
	}
	if (FParse::Param(CommandLine, TEXT("IGNoSoundCaptions")))
	{
		Settings.bSoundCaptionsEnabled = false;
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
	float CaptionBackgroundOpacity = Settings.CaptionBackgroundOpacity;
	if (FParse::Value(
		CommandLine,
		TEXT("IGCaptionBackground="),
		CaptionBackgroundOpacity))
	{
		Settings.CaptionBackgroundOpacity = CaptionBackgroundOpacity;
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
