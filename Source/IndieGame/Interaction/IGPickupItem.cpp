#include "Interaction/IGPickupItem.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Narrative/IGStoryHelpers.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"

AIGPickupItem::AIGPickupItem()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCanEverAffectNavigation(false);
	MeshComponent->SetMobility(EComponentMobility::Movable);

	InteractionPrompt = NSLOCTEXT("IGPickupItem", "DefaultPrompt", "집기");
}

void AIGPickupItem::ConfigurePrototypeVisuals(
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& Scale,
	const bool bSimulatePhysics,
	const float MassKg)
{
	if (!Mesh)
	{
		return;
	}

	MeshComponent->SetStaticMesh(Mesh);
	MeshComponent->SetMaterial(0, Material);
	MeshComponent->SetRelativeScale3D(Scale);

	if (bSimulatePhysics)
	{
		MeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		MeshComponent->SetSimulatePhysics(true);
		MeshComponent->SetMassOverrideInKg(NAME_None, FMath::Max(0.05f, MassKg));
		MeshComponent->SetAngularDamping(0.6f);
		MeshComponent->SetLinearDamping(0.15f);
	}
}

void AIGPickupItem::BeginPlay()
{
	Super::BeginPlay();

	if (!InteractionTag.IsValid())
	{
		InteractionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Interaction.Pickup")), false);
	}

	// A restart after the checkpoint should not offer the same item twice.
	if (StateTagOnPickup.IsValid() && IGStory::HasState(this, StateTagOnPickup))
	{
		ApplyRestoredPickedUpState(PickupMode == EIGPickupMode::CarryInHand);
	}
}

bool AIGPickupItem::CanInteract_Implementation(AActor* Interactor) const
{
	if (!Super::CanInteract_Implementation(Interactor) || bPickedUp)
	{
		return false;
	}

	// The hand can hold one item; remaining duplicates stop offering a prompt.
	if (PickupMode == EIGPickupMode::CarryInHand)
	{
		if (const AIGPlayerCharacter* Character = Cast<AIGPlayerCharacter>(Interactor);
			Character && Character->GetCarriedActor() != nullptr)
		{
			return false;
		}
	}

	return true;
}

void AIGPickupItem::CompleteInteraction_Implementation(const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);

	if (bPickedUp)
	{
		return;
	}

	if (StateTagOnPickup.IsValid())
	{
		IGStory::AddState(this, StateTagOnPickup);
	}

	FinishPickup(Context.Interactor, true, true);
}

void AIGPickupItem::ApplyRestoredPickedUpState(const bool bAttachToPlayer)
{
	if (bPickedUp)
	{
		return;
	}

	if (bAttachToPlayer)
	{
		// Defer to the first tick-safe moment; the pawn may not be spawned yet.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick([WeakThis = TWeakObjectPtr<AIGPickupItem>(this)]()
			{
				if (AIGPickupItem* Item = WeakThis.Get(); Item && !Item->bPickedUp)
				{
					Item->FinishPickup(nullptr, false, false);
				}
			});
			return;
		}
	}

	FinishPickup(nullptr, false, false);
}

void AIGPickupItem::FinishPickup(AActor* Interactor, const bool bBroadcast, const bool bPlayFeedback)
{
	bPickedUp = true;
	SetInteractionEnabled(false);

	MeshComponent->SetSimulatePhysics(false);

	if (bPlayFeedback)
	{
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateFootstep(this, 1.6f, 0.55f),
			GetActorLocation());
		if (!ThoughtOnPickup.IsEmpty())
		{
			AIGHorrorHUD::PushThought(this, ThoughtOnPickup, 3.4f);
		}
	}

	AIGPlayerCharacter* Character = Cast<AIGPlayerCharacter>(Interactor);
	if (!Character && GetWorld())
	{
		const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
		Character = PlayerController
			? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
			: nullptr;
	}

	// Only one item can occupy the hand; duplicates (e.g. the two other water
	// bottles consumed by a checkpoint restore) disappear into the pocket path.
	if (PickupMode == EIGPickupMode::CarryInHand
		&& Character
		&& Character->GetCarriedActor() == nullptr)
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Character->CarryActor(this, CarryOffset, CarryRotation);
	}
	else
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorHiddenInGame(true);
	}

	if (bBroadcast)
	{
		OnPickedUp.Broadcast(this);
	}
}
