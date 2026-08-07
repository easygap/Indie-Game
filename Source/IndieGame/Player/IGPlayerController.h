#pragma once

#include "CoreMinimal.h"
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
	bool WriteFrontendShippingProbeReceipt(
		bool bSuccess,
		const FString& Reason) const;
	bool ShouldShowTitleMenu() const;
	bool HasCompatibleAutosave() const;
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
	EIGSystemMenuMode SystemMenuMode = EIGSystemMenuMode::Hidden;
	EIGSystemMenuMode CreditsReturnMode = EIGSystemMenuMode::Title;
	EIGSystemMenuMode DisplaySettingsReturnMode = EIGSystemMenuMode::Title;
	EIGSystemMenuMode AccessibilityReturnMode = EIGSystemMenuMode::Hidden;
	int32 DisplaySettingsSelection = 0;
	int32 DisplayWindowModeIndex = 0;
	int32 DisplayResolutionIndex = 1;
	int32 DisplayQualityIndex = 1;
	int32 DisplayFrameLimitIndex = 1;
	int32 PreviousDisplayQualityLevel = 2;
	int32 DisplayConfirmationSecondsRemaining = 0;
	bool bAccessibilityMenuVisible = false;
	bool bGameWasPausedBeforeAccessibility = false;
	bool bUsingGamepadForHud = false;
	bool bCompatibleAutosaveAvailable = false;
	bool bNewGameConfirmationArmed = false;
	bool bDisplayVSync = true;
	bool bDisplaySettingsApplied = false;
	bool bDisplaySettingsAwaitingConfirmation = false;
	bool bSystemMenuStatusIsError = false;
	bool bPreviousDisplayVSync = true;
	bool bFrontendShippingProbe = false;
	bool bFrontendDialogueDefaultVerified = false;
	bool bFrontendDialogueVerified = false;
	bool bFrontendDialogueSpeakerVerified = false;
	bool bFrontendDialogueContinuationVerified = false;
	int32 FrontendProbeStep = 0;
	int32 FrontendProbeExpectedWidth = 0;
	int32 FrontendProbeExpectedHeight = 0;
	int32 FrontendProbeLayoutSampleCount = 0;
	int32 FrontendProbeMinimumElementCount = MAX_int32;
	int32 FrontendProbePressedEventCount = 0;
	uint64 FrontendProbeAwaitFrameSerial = 0;
	double FrontendProbeNextActionTime = 0.0;
	double FrontendProbeStepDeadline = 0.0;
	FVector2D FrontendProbeBoundsMin = FVector2D::ZeroVector;
	FVector2D FrontendProbeBoundsMax = FVector2D::ZeroVector;
	FString FrontendProbeDefaultScreenshotPath;
	FString FrontendProbeScreenshotPath;
	float PreviousDisplayFrameLimit = 60.0f;
	double DisplayConfirmationDeadline = 0.0;
	FText SystemMenuStatusText;
};
