#include "Entity/IGMissingFloorEvidence.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGHorrorHUD.h"

namespace IGMissingFloorEvidenceAudio
{
	/**
	 * The rub swells as the letters come up, but never loudly: §5.1 already
	 * charges 0.25 to the noise bus for this, and the cue's job is to let the
	 * player feel that charge, not to add a second one.
	 */
	constexpr float RubStartVolume = 0.42f;
	constexpr float RubEndVolume = 0.78f;
	constexpr float RubInnerRadius = 80.0f;
	constexpr float RubFalloff = 900.0f;
	constexpr float RubFadeSeconds = 0.12f;
}

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
	// Runtime-spawned evidence is configured after component registration.
	// Keep it movable only while assigning the mesh; SetStaticMesh on a Static
	// component emits a PIE warning and can leave render state stale.
	PresentationMesh->SetMobility(EComponentMobility::Movable);
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
	PresentationMesh->SetMobility(EComponentMobility::Static);

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

void AIGMissingFloorEvidence::SetSustainedRubCue(const bool bEnabled)
{
	bSustainedRubCue = bEnabled;
	if (!bEnabled)
	{
		StopRubCue();
	}
}

void AIGMissingFloorEvidence::BeginInteraction_Implementation(
	const FIGInteractionContext& Context)
{
	Super::BeginInteraction_Implementation(Context);
	StartRubCue();
}

void AIGMissingFloorEvidence::UpdateInteraction_Implementation(
	const FIGInteractionContext& Context)
{
	Super::UpdateInteraction_Implementation(Context);
	if (RubCueComponent)
	{
		// The stroke presses harder as the date surfaces. §21.3 asks for 입력
		// 속도 연동; the shipped interaction is a hold, so its own progress is
		// the only speed there is to follow.
		RubCueComponent->SetVolumeMultiplier(FMath::Lerp(
			IGMissingFloorEvidenceAudio::RubStartVolume,
			IGMissingFloorEvidenceAudio::RubEndVolume,
			FMath::Clamp(Context.HoldProgress, 0.0f, 1.0f)));
	}
}

void AIGMissingFloorEvidence::EndInteraction_Implementation(
	const FIGInteractionContext& Context,
	const EIGInteractionEndReason EndReason)
{
	Super::EndInteraction_Implementation(Context, EndReason);
	// Letting go stops the pencil. Releasing early is a real choice — the noise
	// already happened, and the sound stopping is how the player knows the
	// remaining cost is theirs to avoid.
	StopRubCue();
}

void AIGMissingFloorEvidence::StartRubCue()
{
	if (!bSustainedRubCue || RubCueComponent)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UIGToneSequenceSoundWave* Rub =
		UIGToneSequenceSoundWave::CreateFrottageRub(this);
	if (!Rub)
	{
		return;
	}
	RubCueComponent = NewObject<UAudioComponent>(this);
	RubCueComponent->RegisterComponent();
	RubCueComponent->AttachToComponent(
		GetRootComponent(),
		FAttachmentTransformRules::KeepWorldTransform);
	RubCueComponent->SetWorldLocation(GetActorLocation());
	RubCueComponent->SetSound(Rub);
	// The player's own hand: PLAYER bus, so §10.4 rings the room the ledger is
	// in rather than pretending the rubbing arrived through the building.
	RubCueComponent->AttenuationSettings = IGAudio::MakeAttenuation(
		this,
		IGMissingFloorEvidenceAudio::RubInnerRadius,
		IGMissingFloorEvidenceAudio::RubFalloff,
		EIGAudioBus::Player);
	RubCueComponent->bAllowSpatialization = true;
	RubCueComponent->SetVolumeMultiplier(
		IGMissingFloorEvidenceAudio::RubStartVolume);
	if (UIGMissingFloorAudioSubsystem* AudioDirector =
		World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
	{
		AudioDirector->RegisterComponent(RubCueComponent, EIGAudioBus::Player);
	}
	RubCueComponent->Play();
}

void AIGMissingFloorEvidence::StopRubCue()
{
	if (!RubCueComponent)
	{
		return;
	}
	// A short fade, not a cut: graphite lifting off paper has a tail, and a hard
	// stop on a loop clicks.
	RubCueComponent->FadeOut(IGMissingFloorEvidenceAudio::RubFadeSeconds, 0.0f);
	RubCueComponent = nullptr;
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
