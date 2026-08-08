#include "Entity/IGMissingFloorEvidence.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGHorrorHUD.h"

AIGMissingFloorEvidence::AIGMissingFloorEvidence()
{
	PresentationMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("Presentation"));
	SetRootComponent(PresentationMesh);
	// Small clues must not catch the player's capsule: block the visibility
	// trace the interaction uses and ignore the pawn entirely.
	PresentationMesh->SetCollisionProfileName(
		UCollisionProfile::BlockAllDynamic_ProfileName);
	PresentationMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	PresentationMesh->SetCanEverAffectNavigation(false);
	PresentationMesh->SetMobility(EComponentMobility::Static);
}

void AIGMissingFloorEvidence::Configure(
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& SizeCentimeters,
	const FText& Prompt,
	const FText& Thought,
	const EIGMissingFloorTruth Truth,
	const EIGMissingFloorSource Source,
	const float HoldSeconds,
	const float NoiseLoudness)
{
	if (Mesh)
	{
		PresentationMesh->SetStaticMesh(Mesh);
		// Engine primitives are 100 cm, so centimeters divide straight down.
		PresentationMesh->SetRelativeScale3D(SizeCentimeters / 100.0f);
	}
	if (Material)
	{
		PresentationMesh->SetMaterial(0, Material);
	}

	InteractionPrompt = Prompt;
	ExamineThought = Thought;
	EvidenceTruth = Truth;
	EvidenceSource = Source;
	InteractionHoldDuration = FMath::Max(HoldSeconds, 0.0f);
	ExamineNoiseLoudness = FMath::Clamp(NoiseLoudness, 0.0f, 1.0f);
}

void AIGMissingFloorEvidence::SetProgressiveStages(TArray<FText> InStageThoughts)
{
	StageThoughts = MoveTemp(InStageThoughts);
	CompletedStages = 0;
}

void AIGMissingFloorEvidence::CompleteInteraction_Implementation(
	const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);

	// Interim stages: work done, noise made, nothing filed yet. The sound
	// costs the same whether or not this pass finished the rubbing — that
	// is the risk the design charges for evidence (§7).
	if (CompletedStages < StageThoughts.Num())
	{
		const FText& InterimThought = StageThoughts[CompletedStages];
		++CompletedStages;
		if (UWorld* World = GetWorld())
		{
			if (ExamineNoiseLoudness > 0.0f)
			{
				if (UIGNoiseSubsystem* Noise =
					World->GetSubsystem<UIGNoiseSubsystem>())
				{
					Noise->ReportNoise(
						GetActorLocation(),
						ExamineNoiseLoudness,
						Context.Interactor);
				}
			}
		}
		if (!InterimThought.IsEmpty())
		{
			AIGHorrorHUD::PushThought(this, InterimThought, 3.2f);
		}
		return;
	}

	bExamined = true;

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UIGMissingFloorNarrativeSubsystem* Narrative =
				GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>())
			{
				// Repeat examinations are harmless: AddUnique means a record is
				// filed once, and confirmation is recomputed from the records.
				Narrative->RegisterTruthSource(EvidenceTruth, EvidenceSource);
			}
		}
		if (ExamineNoiseLoudness > 0.0f)
		{
			if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
			{
				Noise->ReportNoise(
					GetActorLocation(),
					ExamineNoiseLoudness,
					Context.Interactor);
			}
		}
	}

	if (!ExamineThought.IsEmpty())
	{
		AIGHorrorHUD::PushThought(this, ExamineThought, 3.6f);
	}

	OnExamined.Broadcast(this);
}
