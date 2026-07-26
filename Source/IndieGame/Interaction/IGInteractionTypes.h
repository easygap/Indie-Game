#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "IGInteractionTypes.generated.h"

class AActor;
class UActorComponent;

/** Why an interaction ended. Completed is the only successful terminal state. */
UENUM(BlueprintType)
enum class EIGInteractionEndReason : uint8
{
	Released UMETA(DisplayName = "Released Early"),
	FocusLost UMETA(DisplayName = "Focus Lost"),
	Cancelled UMETA(DisplayName = "Cancelled"),
	Completed UMETA(DisplayName = "Completed"),
	OwnerEndPlay UMETA(DisplayName = "Owner End Play")
};

/**
 * Stable payload passed to interaction callbacks.
 * Add contextual data here instead of widening every interface function.
 */
USTRUCT(BlueprintType)
struct INDIEGAME_API FIGInteractionContext
{
	GENERATED_BODY()

	/** Actor initiating the interaction (normally the owning pawn). */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> Interactor = nullptr;

	/** Component coordinating the interaction. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UActorComponent> SourceComponent = nullptr;

	/** Actor receiving the interaction. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> TargetActor = nullptr;

	/** Trace result captured when the interaction started. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FHitResult HitResult;

	/** Semantic identifier supplied by the target. May be invalid. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTag InteractionTag;

	/** Game-time seconds for which the input has been held. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float HeldDuration = 0.0f;

	/** Normalized hold progress in the [0, 1] range. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float HoldProgress = 0.0f;
};
