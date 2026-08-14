#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/PlayerController.h"
#include "IGPlayerController.generated.h"

class UInputMappingContext;
class UIGAccessibilitySubsystem;
class UIGSaveGame;
class UIGSaveSubsystem;
struct FInputKeyEventArgs;

enum class EIGSystemMenuMode : uint8
{
	Hidden,
	Title,
	Pause,
	AudioCalibration,
	DisplaySettings,
	Credits
};

/** Owns local-player input context setup and future player-facing UI coordination. */
UCLASS()
class INDIEGAME_API AIGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AIGPlayerController();
	bool IsUsingGamepadForHud() const { return bUsingGamepadForHud; }
	/** Re-pushes menu state after the native HUD is constructed or replaced. */
	void RefreshMenuHud() const;

	/**
	 * Harness hook for §24's 즉시 차단 19. The sealed hour has to turn the
	 * evidence journal down, and a probe proves that by asking for it the way
	 * the key does and then reading the screen.
	 */
	void OpenMissingFloorJournalForTesting() { OpenMissingFloorJournal(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupInputComponent() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void ApplyDefaultInputMapping() const;
	void ToggleSystemMenu();
	void ToggleAccessibilityMenu();
	void CloseAccessibilityMenu();
	void BeginJournalInput();
	void EndJournalInput();
	void OpenMissingFloorJournal();
	void CloseMissingFloorJournal();
	void MoveMissingFloorJournalPage(int32 Direction);
	void MoveMissingFloorJournalPageLeft();
	void MoveMissingFloorJournalPageRight();
	bool IsMissingFloorNight() const;
	void PlayMissingFloorJournalPaperSound(float VolumeMultiplier = 1.0f) const;
	void MoveAccessibilitySelectionUp();
	void MoveAccessibilitySelectionDown();
	void AdjustAccessibilityLeft();
	void AdjustAccessibilityRight();
	void ConfirmAccessibilitySelection();
	void RequestManualHint();
	void ChangeAccessibilitySetting(int32 Direction, bool bConfirm);
	void MoveSystemMenuSelection(int32 Direction);
	void ConfirmSystemMenuSelection();
	void SetSystemMenuMode(EIGSystemMenuMode NewMode);
	void StartHeadphoneRecommendationIfNeeded();
	void DismissHeadphoneRecommendation();
	void LoadAudioCalibrationSettings();
	void OpenAudioCalibration(bool bFirstRun);
	void CancelAudioCalibration();
	void CompleteAudioCalibration();
	void MoveAudioCalibrationSelection(int32 Direction);
	void AdjustAudioCalibrationSetting(int32 Direction);
	void ConfirmAudioCalibrationSelection();
	void ApplyAudioCalibrationValues();
	void PlayAudioCalibrationKnock();
	void OpenDisplaySettings();
	void ReturnFromDisplaySettings();
	void RefreshStagedDisplaySettings();
	void MoveDisplaySettingsSelection(int32 Direction);
	void AdjustDisplaySetting(int32 Direction);
	void ConfirmDisplaySettingsSelection();
	void ApplyDisplaySettings();
	void ConfirmPendingDisplaySettings();
	void RevertPendingDisplaySettings();
	void ReturnFromCredits();
	void StartNewGame();
	void ContinueLatestAutosave();
	void QuitToDesktop();
	void BindSaveNotifications();
	void UnbindSaveNotifications();
	UFUNCTION()
	void HandleSaveCompleted(bool bSuccess, FString SlotName);
	UFUNCTION()
	void HandleLoadCompleted(
		bool bSuccess,
		FString SlotName,
		UIGSaveGame* SaveGame);
	void SetInputDevicePresentation(bool bUsingGamepad);
	void ApplyMenuInputMode();
	void UpdateMenuPointerHover();
	bool HandleMenuPointerClick();
	bool TryGetMenuRowFromPointer(
		int32 RowCount,
		float MinimumStartY,
		float StartYFraction,
		float MinimumSpacing,
		float MaximumSpacing,
		float SpacingFraction,
		int32& OutRow) const;
	/** Hit tests the exact responsive rectangles drawn by the native front end. */
	bool TryGetSystemMenuRowFromPointer(int32& OutRow) const;
	bool TryGetDisplaySettingsRowFromPointer(
		int32& OutRow,
		bool& bOutCategoryHit) const;
	bool TryGetAccessibilityRowFromPointer(
		int32& OutRow,
		bool& bOutCategoryHit) const;
	void StartFrontendShippingProbe();
	void TickFrontendShippingProbe();
	void DispatchFrontendProbeKey(const FKey& Key);
	void ReleaseFrontendProbeKey(const FKey& Key);
	void AwaitFrontendProbeFrame();
	bool TryCaptureFrontendProbeLayout(
		const TCHAR* PanelName,
		int32 MinimumElementCount,
		bool bIncludeInMinimumElementCoverage = true);
	bool TryVerifyFrontendDialogueLayout(
		const TCHAR* CaseName,
		int32 MinimumLineCount,
		int32 MaximumLineCount,
		bool bExpectedContinuation);
	void CompleteFrontendShippingProbe();
	void FailFrontendShippingProbe(const FString& Reason);
	void StartMissingFloorJournalPreviewProbe();
	void TickMissingFloorJournalPreviewProbe();
	void FailMissingFloorJournalPreviewProbe(const FString& Reason) const;
	void StartAudioCalibrationPreviewProbe();
	void TickAudioCalibrationPreviewProbe();
	void FailAudioCalibrationPreviewProbe(const FString& Reason) const;
	void StartMissingFloorEndingPreviewProbe();
	void TickMissingFloorEndingPreviewProbe();
	void FailMissingFloorEndingPreviewProbe(const FString& Reason) const;
	bool WriteFrontendShippingProbeReceipt(
		bool bSuccess,
		const FString& Reason) const;
	bool ShouldShowTitleMenu() const;
	bool HasCompatibleAutosave() const;
	bool HasEndingBAutosave() const;

	/**
	 * §9 「밤 5」 — 메타 개입은 본편 바깥, 엔딩 이후, 이 한 번이다.
	 *
	 * 엔딩 B를 본 세이브가 있으면 타이틀의 이어하기 밑에 행 하나가 조용히
	 * 늘어난다. 고르면 로딩 없이 검정 화면 30초: 그녀 자신의 둘-쉬고-하나,
	 * 긴 침묵, 그리고 B에서 복도 끝에서 돌아온 대답 둘이 같은 공간 잔향으로
	 * 다시 온다. 새 사건도, 갇힌 사람도 암시하지 않는다.
	 *
	 * 금지된 것을 명시한다: 세이브 파일을 만들거나 고치지 않고, 가짜 크래시를
	 * 내지 않고, 창 제목을 바꾸지 않는다. 신뢰를 깨는 메타는 전부 제외다.
	 */
	void PlayNightFive();
	bool IsNightFivePlaying() const { return bNightFivePlaying; }
	bool IsNightFiveAvailable() const { return bNightFiveAvailable; }
	bool IsNightFiveSpent() const { return bNightFiveSpent; }
	int32 GetNightFiveCuesPlayed() const { return NightFiveCuesPlayed; }
	/** 30초. 이 길이가 §9의 약속이다. */
	static constexpr float NightFiveTotalSeconds = 30.0f;
	bool IsSystemMenuRowEnabled(int32 Row) const;
	UIGAccessibilitySubsystem* GetAccessibilitySubsystem() const;
	UIGSaveSubsystem* GetSaveSubsystem() const;

	/** Mapping context assigned by the player Blueprint or data asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 DefaultMappingPriority = 0;

	int32 AccessibilitySelection = 0;
	int32 SystemMenuSelection = 0;
	int32 MissingFloorJournalPage = 0;
	EIGSystemMenuMode SystemMenuMode = EIGSystemMenuMode::Hidden;
	EIGSystemMenuMode CreditsReturnMode = EIGSystemMenuMode::Title;
	EIGSystemMenuMode AudioCalibrationReturnMode = EIGSystemMenuMode::Title;
	EIGSystemMenuMode DisplaySettingsReturnMode = EIGSystemMenuMode::Title;
	EIGSystemMenuMode AccessibilityReturnMode = EIGSystemMenuMode::Hidden;
	int32 DisplaySettingsSelection = 0;
	int32 AudioCalibrationSelection = 0;
	int32 AudioCalibrationVolumeStep = 6;
	int32 AudioCalibrationBrightnessStep = 2;
	int32 PreviousAudioCalibrationVolumeStep = 6;
	int32 PreviousAudioCalibrationBrightnessStep = 2;
	int32 DisplayWindowModeIndex = 0;
	int32 DisplayResolutionIndex = 1;
	int32 DisplayQualityIndex = 1;
	int32 DisplayFrameLimitIndex = 1;
	int32 PreviousDisplayQualityLevel = 2;
	int32 DisplayConfirmationSecondsRemaining = 0;
	bool bAccessibilityMenuVisible = false;
	bool bGameWasPausedBeforeAccessibility = false;
	bool bMissingFloorJournalVisible = false;
	bool bJournalInputHeld = false;
	bool bGameWasPausedBeforeJournal = false;
	bool bUsingGamepadForHud = false;
	bool bCompatibleAutosaveAvailable = false;

	/** §9 「밤 5」. 세션 상태다 — 재실행하면 다시 흐려지지 않은 채로 있다. */
	bool bNightFiveAvailable = false;
	bool bNightFiveSpent = false;
	bool bNightFivePlaying = false;
	float NightFiveSeconds = 0.0f;
	int32 NightFiveCuesPlayed = 0;
	/**
	 * 엔진 티커다. 월드 타이머가 아니다 — 타이틀 메뉴는 월드를 일시정지시키므로
	 * 월드 타이머로는 이 30초가 **한 프레임도 진행하지 않는다.** 코어 티커는
	 * 일시정지와 무관하게 돌고, 뒤의 프롤로그 월드를 깨우지도 않는다.
	 */
	FTSTicker::FDelegateHandle NightFiveTicker;
	bool AdvanceNightFive(float DeltaSeconds);
	void EndNightFive();
	FVector NightFiveListenPoint() const;
	/**
	 * -IGNightFiveProbe. 두 가지를 검사한다.
	 *
	 * 하나는 순수 논리다: 액션 인덱스와 화면 자리의 대응이 (이어하기 유무) ×
	 * (밤 5 유무) 네 조합 모두에서 전단사인지. 밤 5는 액션 목록의 마지막이면서
	 * 화면에서는 두 번째이므로, 이 대응이 깨지면 다른 행이 눌린다.
	 *
	 * 다른 하나는 30초 자체다. 소리 두 개가 저작된 시각에 나가고, 30초에
	 * 끝나고, 끝난 뒤 행이 흐려지지만 여전히 고를 수 있는지.
	 */
	void StartNightFiveProbe();
	bool AdvanceNightFiveProbe(float DeltaSeconds);
	void RequestNightFiveProbeExit(bool bFailed);
	bool bNightFiveProbeRequested = false;
	int32 NightFiveProbeStep = 0;
	float NightFiveProbeSeconds = 0.0f;
	FTSTicker::FDelegateHandle NightFiveProbeTicker;
	bool bNewGameConfirmationArmed = false;
	bool bHeadphoneRecommendationVisible = false;
	bool bAudioCalibrationCompleted = false;
	bool bAudioCalibrationFirstRun = false;
	bool bAudioCalibrationSessionActive = false;
	bool bDisplayVSync = true;
	bool bDisplaySettingsApplied = false;
	bool bDisplaySettingsAwaitingConfirmation = false;
	bool bSystemMenuStatusIsError = false;
	bool bPreviousDisplayVSync = true;
	bool bFrontendShippingProbe = false;
	bool bFrontendProbeCompilationDrained = false;
	bool bMissingFloorJournalPreviewProbe = false;
	bool bMissingFloorJournalPreviewScreenshotRequested = false;
	bool bMissingFloorJournalPreviewCompilationDrained = false;
	bool bAudioCalibrationPreviewProbe = false;
	bool bAudioCalibrationPreviewScreenshotRequested = false;
	bool bAudioCalibrationPreviewCompilationDrained = false;
	bool bMissingFloorEndingPreviewProbe = false;
	bool bMissingFloorEndingPreviewScreenshotRequested = false;
	bool bMissingFloorEndingPreviewCompilationDrained = false;
	bool bFrontendDialogueDefaultVerified = false;
	bool bFrontendDialogueVerified = false;
	bool bFrontendDialogueSpeakerVerified = false;
	bool bFrontendDialogueContinuationVerified = false;
	bool bFrontendAccessibilityScreenshotRequested = false;
	bool bFrontendDisplayScreenshotRequested = false;
	int32 FrontendProbeStep = 0;
	int32 FrontendProbeExpectedWidth = 0;
	int32 FrontendProbeExpectedHeight = 0;
	int32 FrontendProbeLayoutSampleCount = 0;
	int32 FrontendProbeMinimumElementCount = MAX_int32;
	int32 FrontendProbePressedEventCount = 0;
	int32 MissingFloorJournalPreviewExpectedWidth = 0;
	int32 MissingFloorJournalPreviewExpectedHeight = 0;
	int32 AudioCalibrationPreviewExpectedWidth = 0;
	int32 AudioCalibrationPreviewExpectedHeight = 0;
	int32 MissingFloorEndingPreviewExpectedWidth = 0;
	int32 MissingFloorEndingPreviewExpectedHeight = 0;
	uint64 FrontendProbeAwaitFrameSerial = 0;
	double FrontendProbeNextActionTime = 0.0;
	double FrontendProbeStepDeadline = 0.0;
	double MissingFloorJournalPreviewNextActionTime = 0.0;
	double MissingFloorJournalPreviewDeadline = 0.0;
	double AudioCalibrationPreviewNextActionTime = 0.0;
	double AudioCalibrationPreviewDeadline = 0.0;
	double MissingFloorEndingPreviewNextActionTime = 0.0;
	double MissingFloorEndingPreviewDeadline = 0.0;
	double NextAudioCalibrationKnockTime = -1.0;
	FVector2D FrontendProbeBoundsMin = FVector2D::ZeroVector;
	FVector2D FrontendProbeBoundsMax = FVector2D::ZeroVector;
	FString FrontendProbeDefaultScreenshotPath;
	FString FrontendProbeScreenshotPath;
	FString FrontendProbeTitleScreenshotPath;
	FString MissingFloorJournalPreviewScreenshotPath;
	FString AudioCalibrationPreviewScreenshotPath;
	FString MissingFloorEndingPreviewScreenshotPath;
	float MissingFloorEndingPreviewElapsedSeconds = 0.8f;
	float PreviousDisplayFrameLimit = 60.0f;
	double DisplayConfirmationDeadline = 0.0;
	double JournalInputPressedAt = 0.0;
	double HeadphoneRecommendationDeadline = 0.0;
	FText SystemMenuStatusText;
};
