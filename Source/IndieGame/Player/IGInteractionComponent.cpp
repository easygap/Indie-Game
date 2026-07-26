#include "Player/IGInteractionComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "Interaction/IGInteractable.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Templates/UnrealTemplate.h"

namespace IGInteraction
{
	constexpr float MinUpdateInterval = 1.0f / 15.0f;
	constexpr float MaxUpdateInterval = 1.0f / 10.0f;
}

UIGInteractionComponent::UIGInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bAutoActivate = true;
}

void UIGInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	FocusUpdateInterval = FMath::Clamp(
		FocusUpdateInterval,
		IGInteraction::MinUpdateInterval,
		IGInteraction::MaxUpdateInterval);

	RefreshFocus();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			UpdateTimerHandle,
			this,
			&ThisClass::HandleUpdateTimer,
			FocusUpdateInterval,
			true);
	}
}

void UIGInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	bInteractionPressed = false;
	FinishActiveInteraction(EIGInteractionEndReason::OwnerEndPlay, false);
	FocusedActor.Reset();
	FocusedHitResult = FHitResult();

	Super::EndPlay(EndPlayReason);
}

void UIGInteractionComponent::PressInteraction()
{
	if (!bInteractionInputEnabled || !IsActive() || bInteractionPressed || bFinalizingInteraction)
	{
		return;
	}

	bInteractionPressed = true;
	RefreshFocus();
	if (!bInteractionPressed || !bInteractionInputEnabled)
	{
		return;
	}

	AActor* Target = FocusedActor.Get();
	AActor* Interactor = GetOwner();
	const uint32 AttemptGeneration = ++InteractionGeneration;
	if (!IsValid(Target)
		|| !Target->GetClass()->ImplementsInterface(UIGInteractable::StaticClass())
		|| !IIGInteractable::Execute_CanInteract(Target, Interactor))
	{
		return;
	}

	const auto IsAttemptValid = [this, Target, AttemptGeneration]()
	{
		return bInteractionInputEnabled
			&& bInteractionPressed
			&& !bFinalizingInteraction
			&& InteractionGeneration == AttemptGeneration
			&& FocusedActor.Get() == Target
			&& IsValid(Target);
	};

	if (!IsAttemptValid())
	{
		return;
	}

	const FGameplayTag TargetInteractionTag =
		IIGInteractable::Execute_GetInteractionTag(Target, Interactor);
	if (!IsAttemptValid())
	{
		return;
	}

	const float TargetHoldDuration = FMath::Max(
		0.0f,
		IIGInteractable::Execute_GetInteractionHoldDuration(Target, Interactor));
	if (!IsAttemptValid())
	{
		return;
	}

	ActiveActor = Target;
	ActiveHitResult = FocusedHitResult;
	ActiveInteractionTag = TargetInteractionTag;
	ActiveHoldDuration = TargetHoldDuration;
	ActiveStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	bInteractionActive = true;

	const FIGInteractionContext Context = MakeContext(
		Target,
		ActiveHitResult,
		ActiveInteractionTag,
		0.0f,
		ActiveHoldDuration <= KINDA_SMALL_NUMBER ? 1.0f : 0.0f);
	IIGInteractable::Execute_BeginInteraction(Target, Context);

	if (!IsValid(Target))
	{
		FinishActiveInteraction(EIGInteractionEndReason::Cancelled, false);
		return;
	}

	if (!bInteractionActive || ActiveActor.Get() != Target)
	{
		return;
	}

	OnInteractionStarted.Broadcast(Target);

	// The callback is allowed to cancel or destroy its target.
	if (bInteractionActive
		&& ActiveActor.Get() == Target
		&& ActiveHoldDuration <= KINDA_SMALL_NUMBER)
	{
		CompleteActiveInteraction();
	}
}

void UIGInteractionComponent::ReleaseInteraction()
{
	if (!bInteractionPressed)
	{
		return;
	}

	++InteractionGeneration;
	bInteractionPressed = false;
	FinishActiveInteraction(EIGInteractionEndReason::Released, false);
}

void UIGInteractionComponent::CancelInteraction()
{
	if (!bInteractionPressed && !bInteractionActive)
	{
		return;
	}

	++InteractionGeneration;
	bInteractionPressed = false;
	FinishActiveInteraction(EIGInteractionEndReason::Cancelled, false);
}

void UIGInteractionComponent::SetInteractionInputEnabled(const bool bEnabled)
{
	if (bInteractionInputEnabled == bEnabled)
	{
		return;
	}

	bInteractionInputEnabled = bEnabled;
	++InteractionGeneration;

	if (!bEnabled)
	{
		bInteractionPressed = false;
		FinishActiveInteraction(EIGInteractionEndReason::Cancelled, false);
		SetFocusedActor(nullptr, FHitResult());
	}
	else
	{
		RefreshFocus();
	}
}

void UIGInteractionComponent::RefreshFocus()
{
	if (bFocusScanInProgress)
	{
		// The regular 10-15 Hz timer satisfies this deferred request without recursion.
		return;
	}

	TGuardValue<bool> FocusScanGuard(bFocusScanInProgress, true);

	if (!bInteractionInputEnabled || !IsActive())
	{
		SetFocusedActor(nullptr, FHitResult());
		return;
	}

	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !IsValid(OwnerActor))
	{
		SetFocusedActor(nullptr, FHitResult());
		return;
	}

	FVector TraceStart;
	FRotator ViewRotation;
	if (!GetInteractionViewPoint(TraceStart, ViewRotation))
	{
		SetFocusedActor(nullptr, FHitResult());
		return;
	}

	const uint32 ScanGeneration = InteractionGeneration;
	const FVector TraceEnd = TraceStart + ViewRotation.Vector() * FMath::Max(0.0f, TraceDistance);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(IGInteractionTrace), bTraceComplex, OwnerActor);
	QueryParams.AddIgnoredActor(OwnerActor);

	FHitResult HitResult;
	const bool bBlockingHit = World->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		TraceChannel,
		QueryParams);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebugTrace)
	{
		const FVector DebugEnd = bBlockingHit ? HitResult.ImpactPoint : TraceEnd;
		DrawDebugLine(
			World,
			TraceStart,
			DebugEnd,
			bBlockingHit ? FColor::Green : FColor::Red,
			false,
			FocusUpdateInterval,
			0,
			1.0f);
	}
#endif

	AActor* Candidate = bBlockingHit ? ResolveInteractable(HitResult, OwnerActor) : nullptr;
	FHitResult CandidateHit = HitResult;

	// Blueprint CanInteract may cancel input, destroy the actor, or trigger a nested scan.
	if (!bInteractionInputEnabled || !IsActive() || InteractionGeneration != ScanGeneration)
	{
		return;
	}

	// Forgiving pass: the centre ray alone makes small props (a bottle on a
	// wire shelf, a wallet on a desk) feel impossible to hit while moving.
	// Sweep a small sphere and take the interactable closest to the view ray.
	if (!Candidate && FocusSweepRadius > 0.0f)
	{
		TArray<FHitResult> SweepHits;
		World->SweepMultiByChannel(
			SweepHits,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			TraceChannel,
			FCollisionShape::MakeSphere(FocusSweepRadius),
			QueryParams);

		const FVector ViewDirection = ViewRotation.Vector();
		float BestDeviation = TNumericLimits<float>::Max();
		for (const FHitResult& SweepHit : SweepHits)
		{
			AActor* SweptActor = ResolveInteractable(SweepHit, OwnerActor);
			if (!SweptActor)
			{
				continue;
			}

			// Perpendicular distance from the aim ray: the nearer the centre
			// of the screen, the stronger the claim on focus.
			const FVector ToHit = SweepHit.ImpactPoint - TraceStart;
			const float AlongRay = FVector::DotProduct(ToHit, ViewDirection);
			const float Deviation = (ToHit - ViewDirection * AlongRay).Size();
			if (Deviation < BestDeviation)
			{
				BestDeviation = Deviation;
				Candidate = SweptActor;
				CandidateHit = SweepHit;
			}
		}

		if (!bInteractionInputEnabled || !IsActive() || InteractionGeneration != ScanGeneration)
		{
			return;
		}
	}

	if (!IsValid(Candidate))
	{
		Candidate = nullptr;
	}

	SetFocusedActor(Candidate, Candidate ? CandidateHit : FHitResult());
}

AActor* UIGInteractionComponent::ResolveInteractable(
	const FHitResult& HitResult,
	AActor* Interactor) const
{
	AActor* HitActor = HitResult.GetActor();
	if (!IsValid(HitActor)
		|| !HitActor->GetClass()->ImplementsInterface(UIGInteractable::StaticClass()))
	{
		return nullptr;
	}

	return IIGInteractable::Execute_CanInteract(HitActor, Interactor) ? HitActor : nullptr;
}

AActor* UIGInteractionComponent::GetFocusedActor() const
{
	return FocusedActor.Get();
}

FText UIGInteractionComponent::GetFocusedPrompt() const
{
	AActor* Target = FocusedActor.Get();
	if (!IsValid(Target) || !Target->GetClass()->ImplementsInterface(UIGInteractable::StaticClass()))
	{
		return FText::GetEmpty();
	}

	return IIGInteractable::Execute_GetInteractionPrompt(Target, GetOwner());
}

FGameplayTag UIGInteractionComponent::GetFocusedInteractionTag() const
{
	AActor* Target = FocusedActor.Get();
	if (!IsValid(Target) || !Target->GetClass()->ImplementsInterface(UIGInteractable::StaticClass()))
	{
		return FGameplayTag();
	}

	return IIGInteractable::Execute_GetInteractionTag(Target, GetOwner());
}

bool UIGInteractionComponent::IsInteracting() const
{
	return bInteractionActive && ActiveActor.IsValid();
}

float UIGInteractionComponent::GetHoldProgress() const
{
	if (!bInteractionActive)
	{
		return 0.0f;
	}

	if (ActiveHoldDuration <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}

	return FMath::Clamp(GetActiveHeldDuration() / ActiveHoldDuration, 0.0f, 1.0f);
}

void UIGInteractionComponent::HandleUpdateTimer()
{
	if (!bInteractionInputEnabled || !IsActive())
	{
		return;
	}

	if (bInteractionActive && !ActiveActor.IsValid())
	{
		FinishActiveInteraction(EIGInteractionEndReason::Cancelled, false);
	}

	RefreshFocus();
	UpdateActiveInteraction();
}

bool UIGInteractionComponent::GetInteractionViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	if (const APlayerController* PlayerController = Cast<APlayerController>(OwnerActor))
	{
		PlayerController->GetPlayerViewPoint(OutLocation, OutRotation);
		return true;
	}

	if (const APawn* OwnerPawn = Cast<APawn>(OwnerActor))
	{
		if (const APlayerController* PlayerController = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			PlayerController->GetPlayerViewPoint(OutLocation, OutRotation);
			return true;
		}
	}

	OwnerActor->GetActorEyesViewPoint(OutLocation, OutRotation);
	return true;
}

void UIGInteractionComponent::SetFocusedActor(AActor* NewActor, const FHitResult& NewHitResult)
{
	AActor* PreviousActor = FocusedActor.Get();
	if (PreviousActor == NewActor)
	{
		FocusedHitResult = NewHitResult;
		return;
	}

	FocusedActor = NewActor;
	FocusedHitResult = NewHitResult;
	++InteractionGeneration;

	if (bInteractionActive && ActiveActor.Get() != NewActor)
	{
		FinishActiveInteraction(EIGInteractionEndReason::FocusLost, false);
	}

	OnFocusChanged.Broadcast(PreviousActor, NewActor);
}

void UIGInteractionComponent::UpdateActiveInteraction()
{
	AActor* Target = ActiveActor.Get();
	if (!bInteractionActive || !bInteractionPressed || !IsValid(Target))
	{
		return;
	}

	const float HeldDuration = GetActiveHeldDuration();
	const float HoldProgress = ActiveHoldDuration <= KINDA_SMALL_NUMBER
		? 1.0f
		: FMath::Clamp(HeldDuration / ActiveHoldDuration, 0.0f, 1.0f);
	const FIGInteractionContext Context = MakeContext(
		Target,
		ActiveHitResult,
		ActiveInteractionTag,
		HeldDuration,
		HoldProgress);

	IIGInteractable::Execute_UpdateInteraction(Target, Context);

	if (!IsValid(Target))
	{
		FinishActiveInteraction(EIGInteractionEndReason::Cancelled, false);
		return;
	}

	if (!bInteractionActive || ActiveActor.Get() != Target)
	{
		return;
	}

	OnInteractionProgress.Broadcast(Target, HoldProgress);

	if (bInteractionActive
		&& ActiveActor.Get() == Target
		&& HoldProgress >= 1.0f)
	{
		CompleteActiveInteraction();
	}
}

void UIGInteractionComponent::CompleteActiveInteraction()
{
	FinishActiveInteraction(EIGInteractionEndReason::Completed, true);
}

void UIGInteractionComponent::FinishActiveInteraction(
	const EIGInteractionEndReason EndReason,
	const bool bCompleted)
{
	if (!bInteractionActive || bFinalizingInteraction)
	{
		return;
	}

	bFinalizingInteraction = true;
	AActor* Target = ActiveActor.Get();
	const float HeldDuration = GetActiveHeldDuration();
	const float HoldProgress = bCompleted
		? 1.0f
		: (ActiveHoldDuration <= KINDA_SMALL_NUMBER
			? 0.0f
			: FMath::Clamp(HeldDuration / ActiveHoldDuration, 0.0f, 1.0f));
	const FIGInteractionContext Context = MakeContext(
		Target,
		ActiveHitResult,
		ActiveInteractionTag,
		HeldDuration,
		HoldProgress);

	// Clear first so callbacks cannot complete or end the same interaction twice.
	ResetActiveState();

	if (IsValid(Target) && Target->GetClass()->ImplementsInterface(UIGInteractable::StaticClass()))
	{
		if (bCompleted)
		{
			IIGInteractable::Execute_CompleteInteraction(Target, Context);
		}

		if (IsValid(Target))
		{
			IIGInteractable::Execute_EndInteraction(Target, Context, EndReason);
		}
	}

	OnInteractionEnded.Broadcast(IsValid(Target) ? Target : nullptr, EndReason);
	bFinalizingInteraction = false;
}

void UIGInteractionComponent::ResetActiveState()
{
	bInteractionActive = false;
	ActiveActor.Reset();
	ActiveHitResult = FHitResult();
	ActiveInteractionTag = FGameplayTag();
	ActiveHoldDuration = 0.0f;
	ActiveStartTime = 0.0f;
}

float UIGInteractionComponent::GetActiveHeldDuration() const
{
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.0f, World->GetTimeSeconds() - ActiveStartTime) : 0.0f;
}

FIGInteractionContext UIGInteractionComponent::MakeContext(
	AActor* TargetActor,
	const FHitResult& HitResult,
	const FGameplayTag& Tag,
	const float HeldDuration,
	const float HoldProgress)
{
	FIGInteractionContext Context;
	Context.Interactor = GetOwner();
	Context.SourceComponent = this;
	Context.TargetActor = TargetActor;
	Context.HitResult = HitResult;
	Context.InteractionTag = Tag;
	Context.HeldDuration = FMath::Max(0.0f, HeldDuration);
	Context.HoldProgress = FMath::Clamp(HoldProgress, 0.0f, 1.0f);
	return Context;
}
