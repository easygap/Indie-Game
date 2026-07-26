#include "Interaction/IGInspectable.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Player/IGHorrorHUD.h"

AIGInspectable::AIGInspectable()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InspectMesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCanEverAffectNavigation(false);

	InteractionPrompt = NSLOCTEXT("IGInspectable", "DefaultPrompt", "살펴보기");
}

void AIGInspectable::ConfigurePrototypeVisuals(
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& Scale)
{
	if (!Mesh)
	{
		return;
	}

	MeshComponent->SetStaticMesh(Mesh);
	MeshComponent->SetMaterial(0, Material);
	MeshComponent->SetRelativeScale3D(Scale);
}

void AIGInspectable::BeginPlay()
{
	Super::BeginPlay();

	if (!InteractionTag.IsValid())
	{
		InteractionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Interaction.Inspect")), false);
	}
}

void AIGInspectable::CompleteInteraction_Implementation(const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);

	if (!ThoughtText.IsEmpty())
	{
		AIGHorrorHUD::PushThought(this, ThoughtText, 3.4f);
	}
}
