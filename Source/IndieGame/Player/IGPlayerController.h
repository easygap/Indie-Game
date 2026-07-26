#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "IGPlayerController.generated.h"

class UInputMappingContext;

/** Owns local-player input context setup and future player-facing UI coordination. */
UCLASS()
class INDIEGAME_API AIGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AIGPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void ApplyDefaultInputMapping() const;
	void ToggleCursorMode();

	/** Mapping context assigned by the player Blueprint or data asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 DefaultMappingPriority = 0;
};
