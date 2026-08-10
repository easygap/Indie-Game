#include "Interaction/IGReadableNote.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Entity/IGNoiseSubsystem.h"
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
	const FVector& PaperSize,
	const bool bCastPresentationShadow)
{
	if (!CubeMesh)
	{
		return;
	}

	PaperMesh->SetStaticMesh(CubeMesh);
	PaperMesh->SetMaterial(0, PaperMaterial);
	PaperMesh->SetRelativeScale3D(PaperSize / 100.0f);
	// Loose wall paper stays shadowless to avoid acne; bound desk objects are
	// thick enough that their contact shadow is the cue preventing them from
	// reading as a card floating above the furniture.
	PaperMesh->SetCastShadow(bCastPresentationShadow);
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

	// Paper handled in a silent stairwell is barely a sound, but it is one.
	// Reported on open only; closing the panel moves nothing.
	if (UWorld* World = GetWorld())
	{
		if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
		{
			Noise->ReportNoise(GetActorLocation(), 0.08f, Context.Interactor);
		}
	}
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
