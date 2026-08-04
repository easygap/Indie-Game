#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "IGPlayerController.generated.h"

class UInputMappingContext;
class UIGAccessibilitySubsystem;
struct FInputKeyEventArgs;

/** Owns local-player input context setup and future player-facing UI coordination. */
UCLASS()
class INDIEGAME_API AIGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AIGPlayerController();
	bool IsUsingGamepadForHud() const { return bUsingGamepadForHud; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void ApplyDefaultInputMapping() const;
	void ToggleCursorMode();
	void ToggleAccessibilityMenu();
	void CloseAccessibilityMenu();
	void MoveAccessibilitySelectionUp();
	void MoveAccessibilitySelectionDown();
	void AdjustAccessibilityLeft();
	void AdjustAccessibilityRight();
	void ConfirmAccessibilitySelection();
	void RequestManualHint();
	void ChangeAccessibilitySetting(int32 Direction, bool bConfirm);
	void RefreshAccessibilityHud() const;
	void SetInputDevicePresentation(bool bUsingGamepad);
	UIGAccessibilitySubsystem* GetAccessibilitySubsystem() const;

	/** Mapping context assigned by the player Blueprint or data asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 DefaultMappingPriority = 0;

	int32 AccessibilitySelection = 0;
	bool bAccessibilityMenuVisible = false;
	bool bGameWasPausedBeforeAccessibility = false;
	bool bUsingGamepadForHud = false;
};
