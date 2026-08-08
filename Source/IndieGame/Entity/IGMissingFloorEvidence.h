#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGInteractableActor.h"
#include "Narrative/IGMissingFloorNarrativeTypes.h"
#include "IGMissingFloorEvidence.generated.h"

class AIGMissingFloorEvidence;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FIGEvidenceExaminedSignature,
	AIGMissingFloorEvidence* /*Evidence*/);

/**
 * A thing in the world worth looking at twice, in 없는 층 terms: examining it
 * files one evidence record against one truth, pushes one inner-voice line,
 * and makes one sound.
 *
 * Deliberately not AIGInspectable, which can only push a thought and owns no
 * state, and deliberately not the legacy CH03 action actor, whose enum is
 * 4시 44분 canon. The sound is the part that matters here: examining anything
 * during the hour is an act the one upstairs can hear, so loudness is authored
 * per prop — a dial is nearly silent, a breaker handle is not.
 */
UCLASS(NotBlueprintable)
class INDIEGAME_API AIGMissingFloorEvidence : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGMissingFloorEvidence();

	/**
	 * Builds the interaction surface and authors what examining it means.
	 * A truth of None files nothing — used for props whose only job is to make
	 * something happen in the world (the breaker).
	 */
	void Configure(
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& SizeCentimeters,
		const FText& Prompt,
		const FText& Thought,
		EIGMissingFloorTruth Truth,
		EIGMissingFloorSource Source,
		float HoldSeconds,
		float NoiseLoudness);

	/** True once this record has been filed at least once. */
	UFUNCTION(BlueprintPure, Category = "Evidence")
	bool HasBeenExamined() const { return bExamined; }

	UFUNCTION(BlueprintPure, Category = "Evidence")
	UStaticMeshComponent* GetPresentationMesh() const { return PresentationMesh; }

	/** Fires on every completed examination, including repeats. */
	FIGEvidenceExaminedSignature OnExamined;

	virtual void CompleteInteraction_Implementation(
		const FIGInteractionContext& Context) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Evidence")
	TObjectPtr<UStaticMeshComponent> PresentationMesh;

	UPROPERTY(EditAnywhere, Category = "Evidence")
	FText ExamineThought;

	UPROPERTY(EditAnywhere, Category = "Evidence")
	EIGMissingFloorTruth EvidenceTruth = EIGMissingFloorTruth::None;

	UPROPERTY(EditAnywhere, Category = "Evidence")
	EIGMissingFloorSource EvidenceSource = EIGMissingFloorSource::None;

	UPROPERTY(EditAnywhere, Category = "Evidence", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExamineNoiseLoudness = 0.05f;

	bool bExamined = false;
};
