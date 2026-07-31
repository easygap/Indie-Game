#include "Narrative/IGApartmentStoryDressing.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Interaction/IGInspectable.h"
#include "Interaction/IGReadableNote.h"
#include "Materials/MaterialInterface.h"

AIGApartmentStoryDressing::AIGApartmentStoryDressing()
{
	PrimaryActorTick.bCanEverTick = false;

	StoryRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StoryRoot"));
	SetRootComponent(StoryRoot);
}

UStaticMeshComponent* AIGApartmentStoryDressing::AddVisual(
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& RelativeLocation,
	const FVector& SizeCentimeters,
	const FRotator& RelativeRotation)
{
	if (!Mesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("StoryVisual_%d"), VisualCounter++));
	Visual->SetupAttachment(StoryRoot);
	Visual->SetStaticMesh(Mesh);
	Visual->SetMaterial(0, Material);
	Visual->SetRelativeLocation(RelativeLocation);
	Visual->SetRelativeRotation(RelativeRotation);
	Visual->SetRelativeScale3D(SizeCentimeters / 100.0f);
	Visual->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Visual->SetGenerateOverlapEvents(false);
	Visual->SetCanEverAffectNavigation(false);
	Visual->SetCastShadow(false);
	Visual->RegisterComponent();
	VisualComponents.Add(Visual);
	return Visual;
}

AIGInspectable* AIGApartmentStoryDressing::AddInspectable(
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& RelativeLocation,
	const FVector& SizeCentimeters,
	const FRotator& RelativeRotation,
	const FText& Prompt,
	const FText& Thought)
{
	UWorld* World = GetWorld();
	if (!World || !Mesh)
	{
		return nullptr;
	}

	const FTransform RelativeTransform(RelativeRotation, RelativeLocation);
	const FTransform WorldTransform = RelativeTransform * GetActorTransform();

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AIGInspectable* Inspectable = World->SpawnActor<AIGInspectable>(
		AIGInspectable::StaticClass(),
		WorldTransform,
		Parameters);
	if (!Inspectable)
	{
		return nullptr;
	}

	Inspectable->AttachToComponent(StoryRoot, FAttachmentTransformRules::KeepWorldTransform);
	Inspectable->ConfigurePrototypeVisuals(
		Mesh,
		Material,
		SizeCentimeters / 100.0f);
	Inspectable->SetInteractionPrompt(Prompt);
	Inspectable->ThoughtText = Thought;
	StoryInteractables.Add(Inspectable);
	return Inspectable;
}

AIGReadableNote* AIGApartmentStoryDressing::AddReadable(
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& RelativeLocation,
	const FVector& SizeCentimeters,
	const FRotator& RelativeRotation,
	const FText& Prompt,
	const FText& Title,
	TArray<FText>&& Lines)
{
	UWorld* World = GetWorld();
	if (!World || !Mesh)
	{
		return nullptr;
	}

	const FTransform RelativeTransform(RelativeRotation, RelativeLocation);
	const FTransform WorldTransform = RelativeTransform * GetActorTransform();

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AIGReadableNote* Readable = World->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		WorldTransform,
		Parameters);
	if (!Readable)
	{
		return nullptr;
	}

	Readable->AttachToComponent(StoryRoot, FAttachmentTransformRules::KeepWorldTransform);
	Readable->ConfigurePrototypeVisuals(Mesh, Material, SizeCentimeters);
	Readable->SetInteractionPrompt(Prompt);
	Readable->SetNoteText(Title, MoveTemp(Lines));
	StoryInteractables.Add(Readable);
	return Readable;
}

void AIGApartmentStoryDressing::ConfigurePrototypeVisuals(
	UStaticMesh* CubeMesh,
	UMaterialInterface* PaperMaterial,
	UMaterialInterface* DarkPlasticMaterial,
	UMaterialInterface* BookCoverMaterial,
	UMaterialInterface* VestMaterial,
	UMaterialInterface* ReflectiveMaterial,
	UMaterialInterface* PhoneScreenMaterial,
	UMaterialInterface* WaterNoteMaterial,
	USceneComponent* FridgeDoorPivot)
{
	if (bConfigured || !CubeMesh)
	{
		return;
	}
	bConfigured = true;

	// Four dog-eared 9급 books on the desk. The pages are inset between
	// slightly oversized covers so the stack reads as books, not four boxes.
	const FVector BookCenter(-103.0f, -190.0f, 0.0f);
	const float BookYaws[] = {-5.0f, 2.0f, -2.5f};
	for (int32 BookIndex = 0; BookIndex < 3; ++BookIndex)
	{
		const float BookBaseZ = 78.0f + BookIndex * 2.45f;
		const FRotator BookRotation(0.0f, BookYaws[BookIndex], 0.0f);
		AddVisual(
			CubeMesh,
			BookCoverMaterial,
			FVector(BookCenter.X, BookCenter.Y, BookBaseZ + 0.15f),
			FVector(30.0f, 19.0f, 0.3f),
			BookRotation);
		AddVisual(
			CubeMesh,
			PaperMaterial,
			FVector(BookCenter.X, BookCenter.Y, BookBaseZ + 1.15f),
			FVector(28.5f, 17.5f, 1.7f),
			BookRotation);
		AddVisual(
			CubeMesh,
			BookCoverMaterial,
			FVector(BookCenter.X, BookCenter.Y, BookBaseZ + 2.15f),
			FVector(30.0f, 19.0f, 0.3f),
			BookRotation);
	}
	// One corner of the result notice is still being used as a bookmark.
	AddVisual(
		CubeMesh,
		PaperMaterial,
		FVector(BookCenter.X, -181.0f, 85.25f),
		FVector(18.0f, 12.0f, 0.35f),
		FRotator(0.0f, 10.0f, 0.0f));

	AddInspectable(
		CubeMesh,
		BookCoverMaterial,
		FVector(BookCenter.X, BookCenter.Y, 86.4f),
		FVector(30.0f, 19.0f, 2.2f),
		FRotator(0.0f, 3.5f, 0.0f),
		NSLOCTEXT("IGApartmentStory", "BooksPrompt", "9급 수험서"),
		NSLOCTEXT(
			"IGApartmentStory",
			"BooksThought",
			"2차 불합격 통지서. …아직 책갈피처럼 끼워 놨다."));

	// The planner is the quiet proof that 04:44 was never his alarm time.
	PlannerNote = AddReadable(
		CubeMesh,
		PaperMaterial,
		FVector(-136.0f, -169.0f, 79.0f),
		FVector(24.0f, 17.0f, 1.2f),
		FRotator(0.0f, -4.0f, 0.0f),
		NSLOCTEXT("IGApartmentStory", "PlannerPrompt", "수험 플래너 펼치기"),
		NSLOCTEXT("IGApartmentStory", "PlannerTitle", "수험 플래너 · 7월"),
		{
			NSLOCTEXT("IGApartmentStory", "PlannerL1", "월–금"),
			NSLOCTEXT("IGApartmentStory", "PlannerL2", "기상          05:10"),
			NSLOCTEXT("IGApartmentStory", "PlannerL3", "캠프 집결     05:30"),
			NSLOCTEXT("IGApartmentStory", "PlannerL4", "저녁          기출 2회분"),
			FText::GetEmpty(),
			NSLOCTEXT("IGApartmentStory", "PlannerL5", "7/25  생수"),
			NSLOCTEXT("IGApartmentStory", "PlannerL6", "7/26"),
		});

	// A cracked, old phone. Only the lock-screen notification is available
	// now; the full message thread can still be reserved for CH03.
	const FVector PhoneCenter(-174.0f, -174.0f, 79.0f);
	const FRotator PhoneRotation(0.0f, 8.0f, 0.0f);
	AddInspectable(
		CubeMesh,
		DarkPlasticMaterial,
		PhoneCenter,
		FVector(7.2f, 14.2f, 1.2f),
		PhoneRotation,
		NSLOCTEXT("IGApartmentStory", "PhonePrompt", "휴대폰 알림"),
		NSLOCTEXT(
			"IGApartmentStory",
			"PhoneThought",
			"엄마 — ‘주말에 내려오니’. …아직 답장을 못 했다."));
	AddVisual(
		CubeMesh,
		PhoneScreenMaterial,
		PhoneCenter + FVector(0.0f, 0.0f, 0.72f),
		FVector(5.9f, 11.8f, 0.22f),
		PhoneRotation);

	const FVector CrackOffsets[] = {
		FVector(-0.7f, 2.1f, 0.86f),
		FVector(0.4f, 0.7f, 0.87f),
		FVector(0.7f, -1.1f, 0.88f),
	};
	const FVector CrackSizes[] = {
		FVector(0.18f, 5.0f, 0.10f),
		FVector(0.16f, 3.6f, 0.10f),
		FVector(0.16f, 3.0f, 0.10f),
	};
	const float CrackYaws[] = {-24.0f, 38.0f, -50.0f};
	for (int32 CrackIndex = 0; CrackIndex < UE_ARRAY_COUNT(CrackOffsets); ++CrackIndex)
	{
		const FVector RotatedOffset = PhoneRotation.RotateVector(CrackOffsets[CrackIndex]);
		AddVisual(
			CubeMesh,
			PaperMaterial,
			PhoneCenter + RotatedOffset,
			CrackSizes[CrackIndex],
			FRotator(0.0f, PhoneRotation.Yaw + CrackYaws[CrackIndex], 0.0f));
	}

	// Fluorescent logistics vest on a hook just inside the entrance. It is
	// flat against the east wall and has no navigational collision.
	const FVector VestCenter(187.0f, -145.0f, 145.0f);
	const FRotator VestRotation(0.0f, 90.0f, 0.0f);
	AddInspectable(
		CubeMesh,
		VestMaterial,
		VestCenter,
		FVector(38.0f, 1.6f, 52.0f),
		VestRotation,
		NSLOCTEXT("IGApartmentStory", "VestPrompt", "작업조끼"),
		NSLOCTEXT(
			"IGApartmentStory",
			"VestThought",
			"05:30 캠프 집결. 늦으면 오늘 자리는 없다."));
	for (const float StripeZ : {139.0f, 150.0f})
	{
		AddVisual(
			CubeMesh,
			ReflectiveMaterial,
			FVector(186.0f, VestCenter.Y, StripeZ),
			FVector(34.0f, 0.5f, 3.0f),
			VestRotation);
	}
	for (const float ShoulderX : {-11.0f, 11.0f})
	{
		const FVector ShoulderOffset = VestRotation.RotateVector(
			FVector(ShoulderX, -0.9f, 18.0f));
		AddVisual(
			CubeMesh,
			ReflectiveMaterial,
			VestCenter + ShoulderOffset,
			FVector(5.0f, 0.5f, 18.0f),
			VestRotation);
	}
	AddVisual(
		CubeMesh,
		DarkPlasticMaterial,
		FVector(188.0f, VestCenter.Y, 177.0f),
		FVector(45.0f, 1.0f, 2.0f),
		VestRotation);

	// Make the existing fridge memo itself traceable. It attaches to the
	// moving door, so opening the fridge cannot leave an interaction target
	// hanging in mid-air.
	if (FridgeDoorPivot)
	{
		UWorld* World = GetWorld();
		FActorSpawnParameters Parameters;
		Parameters.Owner = this;
		Parameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AIGInspectable* WaterNote = World->SpawnActor<AIGInspectable>(
			AIGInspectable::StaticClass(),
			FridgeDoorPivot->GetComponentTransform(),
			Parameters);
		if (WaterNote)
		{
			WaterNote->AttachToComponent(
				FridgeDoorPivot,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			WaterNote->SetActorRelativeLocation(FVector(-7.8f, 36.0f, 18.0f));
			WaterNote->ConfigurePrototypeVisuals(
				CubeMesh,
				WaterNoteMaterial,
				FVector(0.010f, 0.20f, 0.20f));
			WaterNote->SetInteractionPrompt(
				NSLOCTEXT("IGApartmentStory", "WaterNotePrompt", "메모"));
			WaterNote->ThoughtText = NSLOCTEXT(
				"IGApartmentStory",
				"WaterNoteThought",
				"‘생수.’ 어젯밤의 내가 냉장고에 붙여 놨다.");
			StoryInteractables.Add(WaterNote);
		}
	}
}
