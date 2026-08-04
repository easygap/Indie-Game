#include "Interaction/IGReadableNote.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"

TWeakObjectPtr<AIGReadableNote> AIGReadableNote::OpenNote;

AIGReadableNote* AIGReadableNote::GetOpenNote()
{
	return OpenNote.Get();
}

AIGReadableNote::AIGReadableNote()
{
	PrimaryActorTick.bCanEverTick = false;

	PaperMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Paper"));
	SetRootComponent(PaperMesh);
	PaperMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	PaperMesh->SetGenerateOverlapEvents(false);
	PaperMesh->SetCanEverAffectNavigation(false);

	OpenPrompt = NSLOCTEXT("IGReadableNote", "DefaultPrompt", "읽기");
}

void AIGReadableNote::ConfigurePrototypeVisuals(
	UStaticMesh* CubeMesh,
	UMaterialInterface* PaperMaterial,
	const FVector& PaperSize)
{
	if (!CubeMesh)
	{
		return;
	}

	PaperMesh->SetStaticMesh(CubeMesh);
	PaperMesh->SetMaterial(0, PaperMaterial);
	PaperMesh->SetRelativeScale3D(PaperSize / 100.0f);
	// Paper is too thin to cast a meaningful shadow and doing so only
	// produces shadow-map acne along the wall it is taped to.
	PaperMesh->SetCastShadow(false);
}

void AIGReadableNote::SetNoteText(const FText& InTitle, TArray<FText> InBodyLines)
{
	NoteTitle = InTitle;
	NoteBodyLines = MoveTemp(InBodyLines);
}

void AIGReadableNote::SetThermalReceiptData(FIGThermalReceiptData InReceiptData)
{
	ThermalReceiptData = MoveTemp(InReceiptData);
	bUsesThermalReceiptPresentation = true;
	bUsesPhoneNotificationPresentation = false;
}

void AIGReadableNote::SetPhoneNotificationPresentation()
{
	bUsesPhoneNotificationPresentation = true;
	bUsesThermalReceiptPresentation = false;
}

FText AIGReadableNote::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return OpenPrompt;
}

void AIGReadableNote::CompleteInteraction_Implementation(const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);

	if (bOpen)
	{
		Close();
		return;
	}

	// Opening a second note closes the first: the panel is singular.
	if (AIGReadableNote* Previous = OpenNote.Get())
	{
		if (Previous != this)
		{
			Previous->Close();
		}
	}

	bOpen = true;
	bEverRead = true;
	OpenNote = this;
	OnReadStateChanged.Broadcast(this, true);
}

void AIGReadableNote::Close()
{
	if (!bOpen)
	{
		return;
	}

	bOpen = false;
	if (OpenNote.Get() == this)
	{
		OpenNote = nullptr;
	}
	OnReadStateChanged.Broadcast(this, false);
}
