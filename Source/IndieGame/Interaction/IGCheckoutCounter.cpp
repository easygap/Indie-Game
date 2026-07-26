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

	InteractionPrompt = NSLOCTEXT("IGCheckout", "PayPrompt", "계산하기 (생수 1,100원)");
	InteractionHoldDuration = 0.7f;
	PurchaseThought = NSLOCTEXT(
		"IGCheckout",
		"PaidThought",
		"바코드를 찍고 카드로 결제했다. …점원은 어디 갔지?");
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

void AIGCheckoutCounter::ConfigureChapterPurchase(
	const FGameplayTag InRequiredStateTag,
	const FGameplayTag InPurchasedStateTag,
	const FText& InInteractionPrompt,
	const FText& InPurchaseThought)
{
	if (InRequiredStateTag.IsValid())
	{
		RequiredStateTag = InRequiredStateTag;
	}
	if (InPurchasedStateTag.IsValid())
	{
		PurchasedStateTag = InPurchasedStateTag;
	}
	InteractionPrompt = InInteractionPrompt;
	PurchaseThought = InPurchaseThought;
}

void AIGCheckoutCounter::ResetForNewChapter()
{
	GetWorldTimerManager().ClearTimer(RegisterSoundTimerHandle);
	SetInteractionEnabled(true);
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

	const FVector RegisterLocation = RegisterMesh->GetComponentLocation();
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateScannerBeep(this),
		RegisterLocation,
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
