#include "Interaction/IGCheckoutCounter.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Narrative/IGStoryHelpers.h"
#include "Player/IGHorrorHUD.h"
#include "TimerManager.h"

AIGCheckoutCounter::AIGCheckoutCounter()
{
	PrimaryActorTick.bCanEverTick = false;

	CheckoutRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CheckoutRoot"));
	SetRootComponent(CheckoutRoot);

	RegisterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RegisterMesh"));
	RegisterMesh->SetupAttachment(CheckoutRoot);
	RegisterMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	RegisterMesh->SetGenerateOverlapEvents(false);
	RegisterMesh->SetCanEverAffectNavigation(false);

	ScreenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenMesh"));
	ScreenMesh->SetupAttachment(RegisterMesh);
	ScreenMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	ScreenMesh->SetGenerateOverlapEvents(false);
	ScreenMesh->SetCanEverAffectNavigation(false);

	// Chapter setup supplies the selected product and exact total. Keep the
	// constructor fallback neutral so an unconfigured frame never advertises
	// a stale profile or price.
	InteractionPrompt = NSLOCTEXT("IGCheckout", "PayPrompt", "계산하기");
	InteractionHoldDuration = 0.7f;
	PurchaseThought = NSLOCTEXT(
		"IGCheckout",
		"PaidThought",
		"물은 샀으니 됐다.");
}

void AIGCheckoutCounter::ConfigurePrototypeVisuals(
	UStaticMesh* CubeMesh,
	UMaterialInterface* BodyMaterial,
	UMaterialInterface* ScreenMaterial)
{
	if (!CubeMesh)
	{
		return;
	}

	RegisterMesh->SetStaticMesh(CubeMesh);
	RegisterMesh->SetMaterial(0, BodyMaterial);
	RegisterMesh->SetRelativeScale3D(FVector(0.30f, 0.34f, 0.24f));
	RegisterMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 12.0f));

	ScreenMesh->SetStaticMesh(CubeMesh);
	ScreenMesh->SetMaterial(0, ScreenMaterial);
	ScreenMesh->SetRelativeScale3D(FVector(0.12f, 0.75f, 0.66f));
	ScreenMesh->SetRelativeLocation(FVector(-14.0f, 0.0f, 26.0f));
	ScreenMesh->SetRelativeRotation(FRotator(-14.0f, 0.0f, 0.0f));
}

void AIGCheckoutCounter::SetVisualsHidden(const bool bInHidden)
{
	RegisterMesh->SetHiddenInGame(bInHidden);
	ScreenMesh->SetHiddenInGame(bInHidden);
}

void AIGCheckoutCounter::BeginPlay()
{
	Super::BeginPlay();

	if (!InteractionTag.IsValid())
	{
		InteractionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Interaction.Checkout")), false);
	}

	if (!RequiredStateTag.IsValid())
	{
		RequiredStateTag = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH01.Morning.HasWater")),
			false);
	}

	if (!PurchasedStateTag.IsValid())
	{
		PurchasedStateTag = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH01.Morning.WaterPurchased")),
			false);
	}
}

bool AIGCheckoutCounter::CanInteract_Implementation(AActor* Interactor) const
{
	return Super::CanInteract_Implementation(Interactor)
		&& IGStory::HasState(this, RequiredStateTag)
		&& !IGStory::HasState(this, PurchasedStateTag);
}

void AIGCheckoutCounter::CompleteInteraction_Implementation(const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);

	if (!IGStory::HasState(this, RequiredStateTag)
		|| IGStory::HasState(this, PurchasedStateTag))
	{
		return;
	}

	// Progression is committed immediately; the audio that follows is cosmetic.
	IGStory::AddState(this, PurchasedStateTag);

	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateScannerBeep(this),
		RegisterMesh->GetComponentLocation(),
		0.9f);
	GetWorldTimerManager().SetTimer(
		RegisterSoundTimerHandle,
		this,
		&ThisClass::PlayRegisterTimerElapsed,
		0.55f,
		false);

	if (!PurchaseThought.IsEmpty())
	{
		AIGHorrorHUD::PushThought(this, PurchaseThought, 4.6f);
	}

	OnPurchaseCompleted.Broadcast(this);
}

void AIGCheckoutCounter::PlayRegisterTimerElapsed()
{
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateRegisterSound(this),
		RegisterMesh->GetComponentLocation(),
		0.9f);
}
