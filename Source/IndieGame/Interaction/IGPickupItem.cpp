#include "Interaction/IGPickupItem.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/IGNoiseSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "IndieGame.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryHelpers.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "TimerManager.h"

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
		// Small pickups otherwise tunnel through thin shelves and keep skating
		// after a capsule touch. CCD is cheap for the handful of interactable
		// props in this scene and the stronger damping gives them household
		// rather than pinball behaviour.
		MeshComponent->SetUseCCD(true);
		MeshComponent->SetPhysicsMaxAngularVelocityInDegrees(720.0f);
		MeshComponent->SetAngularDamping(2.0f);
		MeshComponent->SetLinearDamping(0.65f);
	}
	bInitiallySimulatedPhysics = bSimulatePhysics;
}

void AIGPickupItem::BeginPlay()
{
	Super::BeginPlay();

	if (!InteractionTag.IsValid())
	{
		InteractionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Interaction.Pickup")), false);
	}

	InitialWorldTransform = GetActorTransform();
	bInitialTransformCaptured = true;

	// Runtime-spawned prototype actors are configured immediately after
	// SpawnActor returns, which is after BeginPlay in an already running world.
	// Reconcile on the next tick so StateTagOnPickup, pickup mode, and the
	// fixed purchase profile have reached their authored values first.
	bRestoreReconcilePending = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::ReconcileRestoredPickedUpState);
	}
	else
	{
		bRestoreReconcilePending = false;
	}
}

bool AIGPickupItem::CanInteract_Implementation(AActor* Interactor) const
{
	if (bRestoreReconcilePending
		|| !Super::CanInteract_Implementation(Interactor)
		|| bPickedUp)
	{
		return false;
	}

	// The hand can hold one item; remaining duplicates stop offering a prompt.
	if (PickupMode == EIGPickupMode::CarryInHand)
	{
		if (const AIGPlayerCharacter* Character = Cast<AIGPlayerCharacter>(Interactor);
			Character && Character->GetCarriedActor() != nullptr)
		{
			const AIGPickupItem* CarriedPickup =
				Cast<AIGPickupItem>(Character->GetCarriedActor());
			const bool bCanSwapProfile =
				CarriedPickup
				&& CarriedPickup != this
				&& RebirthPurchaseProfileOnPickup
					!= EIGRebirthPurchaseProfile::Unset
				&& CarriedPickup->RebirthPurchaseProfileOnPickup
					!= EIGRebirthPurchaseProfile::Unset
				&& !IsRebirthPurchaseCommitted();
			if (!bCanSwapProfile)
			{
				return false;
			}
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

	AIGPlayerCharacter* Character =
		Cast<AIGPlayerCharacter>(Context.Interactor);
	if (PickupMode == EIGPickupMode::CarryInHand
		&& Character
		&& Character->GetCarriedActor()
		&& Character->GetCarriedActor() != this)
	{
		AIGPickupItem* PreviousPickup =
			Cast<AIGPickupItem>(Character->GetCarriedActor());
		if (!PreviousPickup || !PreviousPickup->ReturnForProfileSwap(Character))
		{
			return;
		}
	}

	if (!FinishPickup(
			Character ? static_cast<AActor*>(Character) : Context.Interactor.Get(),
			false,
			true))
	{
		return;
	}

	// Commit only after a carry/pocket transition succeeded. Observers still
	// see the stable profile and story state before OnPickedUp is broadcast.
	CommitRebirthPurchaseProfile();
	if (StateTagOnPickup.IsValid())
	{
		IGStory::AddState(this, StateTagOnPickup);
	}
	OnPickedUp.Broadcast(this);
}

void AIGPickupItem::CommitRebirthPurchaseProfile()
{
	if (RebirthPurchaseProfileOnPickup == EIGRebirthPurchaseProfile::Unset)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!RebirthState)
	{
		return;
	}

	if (IsRebirthPurchaseCommitted())
	{
		return;
	}
	FIGRebirthChoiceState Choices = RebirthState->GetChoices();
	Choices.PurchaseProfile = RebirthPurchaseProfileOnPickup;
	RebirthState->SetChoices(Choices);
}

bool AIGPickupItem::IsRebirthPurchaseCommitted() const
{
	if (IGStory::HasState(
			this,
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("State.CH01.Morning.WaterPurchased")),
				false)))
	{
		return true;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	return RebirthState
		&& RebirthState->HasTruth(
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("Truth.Purchase0431")),
				false));
}

bool AIGPickupItem::ReturnForProfileSwap(AIGPlayerCharacter* Character)
{
	if (!Character
		|| !bPickedUp
		|| RebirthPurchaseProfileOnPickup == EIGRebirthPurchaseProfile::Unset
		|| IsRebirthPurchaseCommitted()
		|| !Character->ReleaseCarriedActor(this))
	{
		return false;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (bInitialTransformCaptured)
	{
		SetActorTransform(
			InitialWorldTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	SetActorHiddenInGame(false);
	MeshComponent->SetCollisionProfileName(
		bInitiallySimulatedPhysics
			? UCollisionProfile::PhysicsActor_ProfileName
			: UCollisionProfile::BlockAll_ProfileName);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetSimulatePhysics(bInitiallySimulatedPhysics);
	SetInteractionEnabled(true);
	bPickedUp = false;
	bRestoreReconcilePending = false;
	return true;
}

bool AIGPickupItem::MatchesRestoredRebirthPurchaseProfile() const
{
	if (RebirthPurchaseProfileOnPickup == EIGRebirthPurchaseProfile::Unset)
	{
		return true;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!RebirthState)
	{
		return true;
	}

	const EIGRebirthPurchaseProfile SelectedProfile =
		RebirthState->GetChoices().PurchaseProfile;
	return SelectedProfile == EIGRebirthPurchaseProfile::Unset
		|| SelectedProfile == RebirthPurchaseProfileOnPickup;
}

void AIGPickupItem::ReconcileRestoredPickedUpState()
{
	if (bPickedUp)
	{
		bRestoreReconcilePending = false;
		return;
	}

	// A restart after the checkpoint should not offer the same item twice.
	// Clearing the pending gate without touching bInteractionEnabled preserves
	// callers that deliberately parked a future-chapter pickup.
	if (!StateTagOnPickup.IsValid()
		|| !IGStory::HasState(this, StateTagOnPickup))
	{
		bRestoreReconcilePending = false;
		return;
	}

	if (PickupMode == EIGPickupMode::CarryInHand
		&& RebirthPurchaseProfileOnPickup
			!= EIGRebirthPurchaseProfile::Unset
		&& !MatchesRestoredRebirthPurchaseProfile()
		&& !IsRebirthPurchaseCommitted())
	{
		// A pre-checkout save restores the selected bottle to the hand while
		// leaving the other authored profiles on the shelf for another choice.
		bRestoreReconcilePending = false;
		return;
	}

	ApplyRestoredPickedUpState(
		PickupMode == EIGPickupMode::CarryInHand
		&& MatchesRestoredRebirthPurchaseProfile());
}

void AIGPickupItem::ApplyRestoredPickedUpState(const bool bAttachToPlayer)
{
	if (bPickedUp)
	{
		bRestoreReconcilePending = false;
		return;
	}

	if (bAttachToPlayer)
	{
		bRestoreReconcilePending = true;
		RestoreAttachAttemptCount = 0;
		TryAttachRestoredPickup();
		return;
	}

	bRestoreReconcilePending = false;
	FinishPickup(nullptr, false, false, false);
}

void AIGPickupItem::TryAttachRestoredPickup()
{
	if (bPickedUp)
	{
		bRestoreReconcilePending = false;
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController =
		World ? World->GetFirstPlayerController() : nullptr;
	AIGPlayerCharacter* Character = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;
	if (Character)
	{
		if (FinishPickup(Character, false, false))
		{
			bRestoreReconcilePending = false;
			return;
		}
	}

	++RestoreAttachAttemptCount;
	if (World && RestoreAttachAttemptCount < MaxRestoreAttachAttempts)
	{
		World->GetTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::TryAttachRestoredPickup);
		return;
	}

	// Do not consume or hide the only copy when no pawn ever becomes ready.
	// The actor returns to its caller-owned enabled/parked state, so a later
	// chapter reveal or direct player interaction remains a safe recovery.
	bRestoreReconcilePending = false;
	UE_LOG(
		LogIndieGame,
		Error,
		TEXT(
			"Pickup restore could not find the player pawn after %d attempts; "
			"leaving %s recoverable in the world."),
		RestoreAttachAttemptCount,
		*GetName());
}

bool AIGPickupItem::FinishPickup(
	AActor* Interactor,
	const bool bBroadcast,
	const bool bPlayFeedback,
	const bool bAllowCarry)
{
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
	const bool bShouldCarry =
		bAllowCarry
		&& PickupMode == EIGPickupMode::CarryInHand
		&& Character
		&& Character->GetCarriedActor() == nullptr;
	if (bAllowCarry
		&& PickupMode == EIGPickupMode::CarryInHand
		&& !bShouldCarry)
	{
		return false;
	}

	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bShouldCarry
		&& !Character->CarryActor(this, CarryOffset, CarryRotation))
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComponent->SetSimulatePhysics(bInitiallySimulatedPhysics);
		return false;
	}

	bPickedUp = true;
	bRestoreReconcilePending = false;
	SetInteractionEnabled(false);
	if (!bShouldCarry)
	{
		SetActorHiddenInGame(true);
	}

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
		// Only a real pickup sounds. Checkpoint restores come through here
		// with feedback suppressed and must stay silent, or every load would
		// ring the building's ear.
		if (UWorld* World = GetWorld())
		{
			if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
			{
				Noise->ReportNoise(GetActorLocation(), 0.14f, Character);
			}
		}
	}

	if (bBroadcast)
	{
		OnPickedUp.Broadcast(this);
	}
	return true;
}
