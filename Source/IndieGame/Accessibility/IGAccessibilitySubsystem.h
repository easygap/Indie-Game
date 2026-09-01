#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IGAccessibilitySubsystem.generated.h"

UENUM(BlueprintType)
enum class EIGHintMode : uint8
{
	Story UMETA(DisplayName = "이야기"),
	Standard UMETA(DisplayName = "기본"),
	Silent UMETA(DisplayName = "침묵")
};

/** Settings that alter presentation and input without changing story truth. */
USTRUCT(BlueprintType)
struct INDIEGAME_API FIGAccessibilitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	EIGHintMode HintMode = EIGHintMode::Standard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bReducedCameraMotion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bReducedFlicker = false;

	/**
	 * 1인칭 시야각. 좁은 화면에서 멀미가 나는 사람이 있고, 반대로 넓히면
	 * 복도가 실제보다 멀어 보여 압박이 준다. 어느 쪽이든 손에 맡긴다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Accessibility",
		meta = (ClampMin = "68.0", ClampMax = "100.0"))
	float FieldOfViewDegrees = 78.0f;

	/**
	 * §18.3 멀미 완화 비네트. 시야각과 둘 중 하나를 고르는 것이 아니라
	 * 함께 쓴다 — 화각을 넓히면 주변부가 빨라져서 오히려 힘든 손도 있다.
	 * 기본은 0이다. 의도한 화면은 비네트가 없는 쪽이다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Accessibility",
		meta = (ClampMin = "0.00", ClampMax = "1.00"))
	float ComfortVignetteStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bDirectionalFearCues = false;

	/**
	 * §19.8 노크 진동 대체. 존재가 내는 소리를 패드 진동으로 옮긴다.
	 *
	 * §18.5는 벽에서 돌아오는 응답 노크에 진동을 붙이지 말라고 했고 그건
	 * 기본값으로 지켜진다. 이 설정은 그 규칙을 뒤집는 것이 아니라, 소리를
	 * 못 듣는 손에게 같은 정보를 다른 통로로 주는 것이다. 켠 사람만 받는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bKnockHapticSubstitute = false;

	/**
	 * §19.8 노크 시각 대체. 파문 링을 존재의 소리에도 그린다.
	 * 내 소리와는 **두께로** 구분한다 — 색만으로 나누면 색각에서 사라진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bKnockRippleSubstitute = false;

	/** §19.8 심박 경고. 스트레스가 임계에 닿으면 비네트가 맥동한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bHeartbeatWarning = false;

	/**
	 * §19.8 인지 지원. 퍼즐의 시간 압박을 풀고 노크 판정창을 넓힌다.
	 * 힌트 모드는 같은 묶음이지만 자기 행을 따로 가진다(§19.4).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bCognitiveAssist = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bAutoConnectEvidence = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bSubtitlesEnabled = true;

	/** Important non-speech sounds can be configured separately from dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bSoundCaptionsEnabled = true;

	/** Shared dialogue/caption scale; 2.0 preserves the 200% accessibility path. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Accessibility",
		meta = (ClampMin = "0.85", ClampMax = "2.00"))
	float CaptionSizeScale = 1.0f;

	/** Opacity of the high-contrast lower-third backing surface. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Accessibility",
		meta = (ClampMin = "0.00", ClampMax = "1.00"))
	float CaptionBackgroundOpacity = 0.82f;

	/**
	 * 자막이 화면에 머무는 시간의 배율. 읽는 속도는 사람마다 다르고,
	 * 이 게임은 소리를 글로 옮긴 줄이 정보의 전부인 구간이 있다.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Accessibility",
		meta = (ClampMin = "0.75", ClampMax = "2.00"))
	float CaptionDurationScale = 1.0f;

	/** Width/height fraction reserved as the caption-safe display rectangle. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Accessibility",
		meta = (ClampMin = "0.80", ClampMax = "1.00"))
	float CaptionSafeAreaScale = 0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bToggleHoldInteractions = false;

	/** Crouch is a tap toggle by default; disable for press-to-crouch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bToggleCrouch = true;

	/** Master gate for controller vibration, including audio replacement cues. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bHapticsEnabled = true;

	/**
	 * Optional real microphone-to-noise bridge. OFF is the balance baseline;
	 * enabling it never records, stores or transmits captured samples.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessibility")
	bool bMicrophoneNoiseEnabled = false;

	/** Multiplier applied only to non-zero hold interactions. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Accessibility",
		meta = (ClampMin = "0.25", ClampMax = "1.0"))
	float HoldDurationScale = 1.0f;
};

/**
 * Process-wide accessibility state backed by GameUserSettings.ini.
 *
 * Command-line overrides are deliberately effective-session only so QA runs
 * cannot silently change the player's persisted preferences.
 */
UCLASS()
class INDIEGAME_API UIGAccessibilitySubsystem final
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Accessibility")
	FIGAccessibilitySettings GetSettings() const { return EffectiveSettings; }

	UFUNCTION(BlueprintCallable, Category = "Accessibility")
	void ApplySettings(const FIGAccessibilitySettings& NewSettings);

	UFUNCTION(BlueprintCallable, Category = "Accessibility")
	void ResetToDefaults();

	UFUNCTION(BlueprintPure, Category = "Accessibility|Hints")
	bool ShouldAutoShowHints() const
	{
		return EffectiveSettings.HintMode != EIGHintMode::Silent;
	}

	/** X/Y/Z are the first, second, and third P3 dwell thresholds in seconds. */
	UFUNCTION(BlueprintPure, Category = "Accessibility|Hints")
	FVector GetP3HintThresholds() const;

	/** X/Y/Z are P4's relation, environmental, and explicit-route thresholds. */
	UFUNCTION(BlueprintPure, Category = "Accessibility|Hints")
	FVector GetP4HintThresholds() const;

	/** Story/Standard/Silent pressure cadence: 80/55/45 seconds. */
	UFUNCTION(BlueprintPure, Category = "Accessibility|Pressure")
	float GetPressureRiseIntervalSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Accessibility|Motion")
	bool IsReducedCameraMotionEnabled() const
	{
		return EffectiveSettings.bReducedCameraMotion;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Motion")
	bool IsReducedFlickerEnabled() const
	{
		return EffectiveSettings.bReducedFlicker;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Audio")
	bool UsesDirectionalFearCues() const
	{
		return EffectiveSettings.bDirectionalFearCues;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Evidence")
	bool UsesAutomaticEvidenceConnections() const
	{
		return EffectiveSettings.bAutoConnectEvidence;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Subtitles")
	bool AreSubtitlesEnabled() const
	{
		return EffectiveSettings.bSubtitlesEnabled;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Subtitles")
	bool AreSoundCaptionsEnabled() const
	{
		return EffectiveSettings.bSoundCaptionsEnabled;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Subtitles")
	float GetCaptionSizeScale() const
	{
		return EffectiveSettings.CaptionSizeScale;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Subtitles")
	float GetCaptionBackgroundOpacity() const
	{
		return EffectiveSettings.CaptionBackgroundOpacity;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Captions")
	float GetCaptionDurationScale() const
	{
		return EffectiveSettings.CaptionDurationScale;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Audio")
	bool UsesKnockHapticSubstitute() const
	{
		return EffectiveSettings.bKnockHapticSubstitute;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Audio")
	bool UsesKnockRippleSubstitute() const
	{
		return EffectiveSettings.bKnockRippleSubstitute;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Motion")
	bool UsesHeartbeatWarning() const
	{
		return EffectiveSettings.bHeartbeatWarning;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Guidance")
	bool UsesCognitiveAssist() const
	{
		return EffectiveSettings.bCognitiveAssist;
	}

	/** §19.8. 인지 지원에서 노크 판정창이 넓어지는 배율. */
	UFUNCTION(BlueprintPure, Category = "Accessibility|Guidance")
	float GetKnockWindowScale() const;

	UFUNCTION(BlueprintPure, Category = "Accessibility|Motion")
	float GetComfortVignetteStrength() const
	{
		return EffectiveSettings.ComfortVignetteStrength;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Motion")
	float GetFieldOfViewDegrees() const
	{
		return EffectiveSettings.FieldOfViewDegrees;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Subtitles")
	float GetCaptionSafeAreaScale() const
	{
		return EffectiveSettings.CaptionSafeAreaScale;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Input")
	bool UsesToggleHoldInteractions() const
	{
		return EffectiveSettings.bToggleHoldInteractions;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Input")
	bool UsesToggleCrouch() const
	{
		return EffectiveSettings.bToggleCrouch;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Input")
	bool AreHapticsEnabled() const
	{
		return EffectiveSettings.bHapticsEnabled;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Audio")
	bool IsMicrophoneNoiseEnabled() const
	{
		return EffectiveSettings.bMicrophoneNoiseEnabled;
	}

	UFUNCTION(BlueprintPure, Category = "Accessibility|Input")
	float GetHoldDurationScale() const
	{
		return EffectiveSettings.HoldDurationScale;
	}

private:
	static FIGAccessibilitySettings Sanitize(
		const FIGAccessibilitySettings& Candidate);
	void LoadPersistedSettings();
	void SavePersistedSettings() const;
	void RebuildEffectiveSettings();
	void ApplyCommandLineOverrides(FIGAccessibilitySettings& Settings) const;

	FIGAccessibilitySettings PersistedSettings;
	FIGAccessibilitySettings EffectiveSettings;
};
