#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "IGZoneTrigger.generated.h"

class AIGZoneTrigger;
class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FIGZoneTriggeredSignature,
	AIGZoneTrigger*, Zone);

/**
 * One-shot player volume. On first player entry it records an optional story
 * state, pushes an optional thought line and notifies listeners. If the state
 * already exists (checkpoint restore) the zone consumes itself silently.
 */
UCLASS(Blueprintable)
class INDIEGAME_API AIGZoneTrigger : public AActor
{
	GENERATED_BODY()

public:
	AIGZoneTrigger();

	UFUNCTION(BlueprintPure, Category = "Zone")
	bool WasTriggered() const { return bTriggered; }

	UFUNCTION(BlueprintCallable, Category = "Zone")
	void SetZoneExtent(const FVector& HalfExtent);

	UPROPERTY(BlueprintAssignable, Category = "Zone|Events")
	FIGZoneTriggeredSignature OnZoneTriggered;

	/** Optional story state recorded on first entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Story")
	FGameplayTag StateTagOnEnter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Story")
	FText ThoughtOnEnter;

	/** 해당 밤·선행 사건이 오기 전에는 들어가도 소모되지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Story")
	int32 RequiredNightIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Story")
	FName RequiredNarrativeBeat;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleStoryStateChanged(FGameplayTag StateTag, bool bAdded);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone|Components")
	TObjectPtr<UBoxComponent> ZoneBox;

private:
	void ReconcileWithStoryState();

	bool bTriggered = false;
};
