#include "Interaction/IGTimeEntryPuzzle.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Sequence/IGSecondMorningDirector.h"

namespace IGTimeEntry
{
	constexpr bool DigitSegments[10][7] = {
		{true, true, true, true, true, true, false},
		{false, true, true, false, false, false, false},
		{true, true, false, true, true, false, true},
		{true, true, true, true, false, false, true},
		{false, true, true, false, false, true, true},
		{true, false, true, true, false, true, true},
		{true, false, true, true, true, true, true},
		{true, true, true, false, false, false, false},
		{true, true, true, true, true, true, true},
		{true, true, true, true, false, true, true}};

	const FVector DigitCenters[] = {
		FVector(2.32f, -12.0f, 2.0f),
		FVector(2.32f, -5.2f, 2.0f),
		FVector(2.32f, 5.2f, 2.0f),
		FVector(2.32f, 12.0f, 2.0f)};
}

AIGTimeEntryButton::AIGTimeEntryButton()
{
	PrimaryActorTick.bCanEverTick = false;

	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Button"));
	SetRootComponent(ButtonMesh);
	ButtonMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ButtonMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ButtonMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ButtonMesh->SetGenerateOverlapEvents(false);
	ButtonMesh->SetCanEverAffectNavigation(false);
	ButtonMesh->SetCastShadow(false);
	InteractionTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Interaction.Inspect")),
		false);
}

void AIGTimeEntryButton::Configure(
	AIGTimeEntryPuzzle* InPuzzle,
	const EIGTimeEntryButtonAction InAction,
	UStaticMesh* CubeMesh,
	UMaterialInterface* Material,
	const FText& Prompt)
{
	Puzzle = InPuzzle;
	Action = InAction;
	ButtonMesh->SetStaticMesh(CubeMesh);
	ButtonMesh->SetMaterial(0, Material);
	// The cube is 100 cm per axis. Keep the trace surface generous in Y/Z,
	// but make the press depth a believable 1.5 cm instead of a 5.5 cm block.
	ButtonMesh->SetRelativeScale3D(FVector(0.015f, 0.07f, 0.035f));
	InteractionPrompt = Prompt;
	SetAvailable(false);
}

void AIGTimeEntryButton::SetAvailable(const bool bInAvailable)
{
	SetInteractionEnabled(bInAvailable);
	ButtonMesh->SetCollisionEnabled(
		bInAvailable
			? ECollisionEnabled::QueryOnly
			: ECollisionEnabled::NoCollision);
}

void AIGTimeEntryButton::CompleteInteraction_Implementation(
	const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);
	if (Puzzle)
	{
		Puzzle->HandleButton(Action);
	}
}

AIGTimeEntryPuzzle::AIGTimeEntryPuzzle()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SetActorEnableCollision(false);
}

void AIGTimeEntryPuzzle::Configure(
	AIGSecondMorningDirector* InDirector,
	const EIGTimeEntryPuzzleId InPuzzleId,
	UStaticMesh* CubeMesh,
	UStaticMesh* AuthoredHousingMesh,
	UMaterialInterface* BodyMaterial,
	UMaterialInterface* InDisplayOffMaterial,
	UMaterialInterface* InDisplayGlassMaterial,
	UMaterialInterface* InAlarmDisplayOnMaterial,
	UMaterialInterface* InPosDisplayOnMaterial,
	UMaterialInterface* ButtonMaterial)
{
	if (PuzzleId != EIGTimeEntryPuzzleId::None || !CubeMesh)
	{
		return;
	}

	Director = InDirector;
	PuzzleId = InPuzzleId;
	DisplayOffMaterial = InDisplayOffMaterial;
	DisplayGlassMaterial = InDisplayGlassMaterial;
	DisplayOnMaterial = PuzzleId == EIGTimeEntryPuzzleId::P1Alarm
		? InAlarmDisplayOnMaterial
		: InPosDisplayOnMaterial;
	HourCandidates = {4, 5};
	MinuteCandidates = {44, 10, 31};
	TargetHour = PuzzleId == EIGTimeEntryPuzzleId::P1Alarm ? 5 : 4;
	TargetMinute = PuzzleId == EIGTimeEntryPuzzleId::P1Alarm ? 10 : 31;

	Tags.AddUnique(FName(TEXT("REBIRTH.CH02.TimeEntry")));
	Tags.AddUnique(
		PuzzleId == EIGTimeEntryPuzzleId::P1Alarm
			? FName(TEXT("REBIRTH.CH02.P1.TimeEntry"))
			: FName(TEXT("REBIRTH.CH02.P2.TimeEntry")));
	Tags.AddUnique(
		PuzzleId == EIGTimeEntryPuzzleId::P1Alarm
			? FName(TEXT("REBIRTH.CH02.P1.AlarmHousing"))
			: FName(TEXT("REBIRTH.CH02.P2.PosHousing")));

	if (PuzzleId == EIGTimeEntryPuzzleId::P1Alarm)
	{
		BuildAlarmHousing(
			CubeMesh,
			AuthoredHousingMesh,
			BodyMaterial,
			InDisplayOffMaterial);
	}
	else
	{
		BuildPosHousing(
			CubeMesh,
			AuthoredHousingMesh,
			BodyMaterial,
			InDisplayOffMaterial,
			ButtonMaterial);
	}
	DisplayBacking = AddPart(
		TEXT("DisplayBacking"),
		CubeMesh,
		InDisplayOffMaterial,
		FVector(2.12f, 0.0f, 2.0f),
		FVector(0.20f, 31.0f, 13.5f));
	BuildDisplay(CubeMesh, InDisplayOffMaterial);
	DisplayLens = AddPart(
		TEXT("DisplayLens"),
		CubeMesh,
		InDisplayGlassMaterial,
		FVector(2.45f, 0.0f, 2.0f),
		FVector(0.08f, 31.0f, 13.5f));
	if (DisplayLens)
	{
		DisplayLens->SetTranslucentSortPriority(2);
	}

	SpawnButton(
		EIGTimeEntryButtonAction::Hour,
		FVector(3.1f, -10.0f, -9.0f),
		CubeMesh,
		ButtonMaterial,
		NSLOCTEXT("IGTimeEntry", "HourPrompt", "시 조정"));
	SpawnButton(
		EIGTimeEntryButtonAction::Minute,
		FVector(3.1f, 0.0f, -9.0f),
		CubeMesh,
		ButtonMaterial,
		NSLOCTEXT("IGTimeEntry", "MinutePrompt", "분 조정"));
	SpawnButton(
		EIGTimeEntryButtonAction::Confirm,
		FVector(3.1f, 10.0f, -9.0f),
		CubeMesh,
		ButtonMaterial,
		NSLOCTEXT("IGTimeEntry", "ConfirmPrompt", "입력 확인"));
	UpdateDisplay();
	UpdateButtonPrompts();
}

void AIGTimeEntryPuzzle::RestoreState(
	const int32 InHour,
	const int32 InMinute,
	const int32 InWrongAttempts,
	const bool bInSolved)
{
	Hour = HourCandidates.Contains(InHour) ? InHour : 4;
	Minute = MinuteCandidates.Contains(InMinute) ? InMinute : 44;
	WrongAttempts = FMath::Clamp(InWrongAttempts, 0, 3);
	bSolved = bInSolved;
	UpdateDisplay();
	UpdateButtonPrompts();
	SetAvailable(bAvailable && !bSolved);
}

void AIGTimeEntryPuzzle::SetAvailable(const bool bInAvailable)
{
	bAvailable = bInAvailable && !bSolved;
	for (AIGTimeEntryButton* Button : Buttons)
	{
		if (Button)
		{
			Button->SetAvailable(bAvailable);
		}
	}
}

void AIGTimeEntryPuzzle::HandleButton(
	const EIGTimeEntryButtonAction Action)
{
	if (!bAvailable || bSolved)
	{
		return;
	}

	if (Action == EIGTimeEntryButtonAction::Hour)
	{
		CycleHour();
		if (Director)
		{
			Director->HandleTimeEntrySelectionChanged(this);
		}
	}
	else if (Action == EIGTimeEntryButtonAction::Minute)
	{
		CycleMinute();
		if (Director)
		{
			Director->HandleTimeEntrySelectionChanged(this);
		}
	}
	else
	{
		const bool bCorrect = IsAnswerCorrect();
		if (bCorrect)
		{
			bSolved = true;
			SetAvailable(false);
		}
		else
		{
			WrongAttempts = FMath::Min(WrongAttempts + 1, 3);
		}
		UpdateDisplay();
		UpdateButtonPrompts();
		if (Director)
		{
			Director->HandleTimeEntryConfirmed(this, bCorrect);
		}
	}
}

bool AIGTimeEntryPuzzle::AdvancePressureStage()
{
	if (!bAvailable || bSolved || WrongAttempts >= 3)
	{
		return false;
	}
	++WrongAttempts;
	UpdateButtonPrompts();
	return true;
}

bool AIGTimeEntryPuzzle::RunAutomatedSolution()
{
	if (bSolved)
	{
		return IsAnswerCorrect() && HasPhysicalContract();
	}
	if (!bAvailable || !HasPhysicalContract())
	{
		return false;
	}

	FIGInteractionContext Context;
	while (Hour != TargetHour)
	{
		AIGTimeEntryButton* HourButton = FindButton(
			EIGTimeEntryButtonAction::Hour);
		if (!HourButton)
		{
			return false;
		}
		HourButton->CompleteInteraction_Implementation(Context);
	}
	while (Minute != TargetMinute)
	{
		AIGTimeEntryButton* MinuteButton = FindButton(
			EIGTimeEntryButtonAction::Minute);
		if (!MinuteButton)
		{
			return false;
		}
		MinuteButton->CompleteInteraction_Implementation(Context);
	}
	AIGTimeEntryButton* ConfirmButton = FindButton(
		EIGTimeEntryButtonAction::Confirm);
	if (!ConfirmButton)
	{
		return false;
	}
	ConfirmButton->CompleteInteraction_Implementation(Context);
	return bSolved && IsAnswerCorrect();
}

bool AIGTimeEntryPuzzle::IsAnswerCorrect() const
{
	return Hour == TargetHour && Minute == TargetMinute;
}

bool AIGTimeEntryPuzzle::HasPhysicalContract() const
{
	int32 LiveButtons = 0;
	for (const AIGTimeEntryButton* Button : Buttons)
	{
		LiveButtons += IsValid(Button) ? 1 : 0;
	}
	int32 LivePresentationParts = 0;
	for (const UStaticMeshComponent* Part : PresentationParts)
	{
		LivePresentationParts += IsValid(Part) ? 1 : 0;
	}
	const int32 RequiredPresentationParts =
		PuzzleId == EIGTimeEntryPuzzleId::P1Alarm ? 4 : 4;
	const bool bHasDisplayInstances =
		IsValid(DisplayOnInstances)
		&& IsValid(DisplayOffInstances)
		&& DisplaySegmentTransforms.Num() == 28
		&& DisplayOnInstances->GetInstanceCount()
			+ DisplayOffInstances->GetInstanceCount() == 30;
	const bool bHasPresentationInstances =
		PuzzleId == EIGTimeEntryPuzzleId::P1Alarm
		|| (IsValid(PosKeypadInstances)
			&& PosKeypadInstances->GetInstanceCount() == 12);
	return PuzzleId != EIGTimeEntryPuzzleId::None
		&& bHasDisplayInstances
		&& bHasPresentationInstances
		&& LiveButtons == 3
		&& LivePresentationParts >= RequiredPresentationParts
		&& bUsesAuthoredHousing
		&& UsesLayeredDisplay();
}

int32 AIGTimeEntryPuzzle::GetPhysicalMeshComponentCount() const
{
	int32 Count = 0;
	for (const UStaticMeshComponent* Part : PresentationParts)
	{
		Count += IsValid(Part) ? 1 : 0;
	}
	Count += IsValid(DisplayBacking) ? 1 : 0;
	Count += IsValid(DisplayLens) ? 1 : 0;
	Count += IsValid(DisplayOnInstances) ? 1 : 0;
	Count += IsValid(DisplayOffInstances) ? 1 : 0;
	Count += IsValid(PosKeypadInstances) ? 1 : 0;
	for (const AIGTimeEntryButton* Button : Buttons)
	{
		Count += IsValid(Button) ? 1 : 0;
	}
	return Count;
}

bool AIGTimeEntryPuzzle::UsesLayeredDisplay() const
{
	return IsValid(DisplayBacking)
		&& IsValid(DisplayLens)
		&& IsValid(DisplayOnInstances)
		&& IsValid(DisplayOffInstances)
		&& IsValid(DisplayOffMaterial)
		&& IsValid(DisplayOnMaterial)
		&& IsValid(DisplayGlassMaterial);
}

void AIGTimeEntryPuzzle::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	for (AIGTimeEntryButton* Button : Buttons)
	{
		if (Button)
		{
			Button->Destroy();
		}
	}
	Buttons.Reset();
	Super::EndPlay(EndPlayReason);
}

UStaticMeshComponent* AIGTimeEntryPuzzle::AddPart(
	const TCHAR* BaseName,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& RelativeLocation,
	const FVector& SizeCentimeters)
{
	if (!Mesh || !SceneRoot)
	{
		return nullptr;
	}
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("%s_%02d"), BaseName, PartSerial++));
	Part->SetupAttachment(SceneRoot);
	Part->SetStaticMesh(Mesh);
	Part->SetMaterial(0, Material);
	Part->SetRelativeLocation(RelativeLocation);
	Part->SetRelativeScale3D(SizeCentimeters / 100.0f);
	Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetCastShadow(false);
	Part->RegisterComponent();
	return Part;
}

UStaticMeshComponent* AIGTimeEntryPuzzle::AddPresentationPart(
	const TCHAR* BaseName,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& RelativeLocation,
	const FVector& SizeCentimeters)
{
	UStaticMeshComponent* Part = AddPart(
		BaseName,
		Mesh,
		Material,
		RelativeLocation,
		SizeCentimeters);
	if (Part)
	{
		Part->SetCastShadow(true);
		PresentationParts.Add(Part);
	}
	return Part;
}

UStaticMeshComponent* AIGTimeEntryPuzzle::AddAuthoredPresentationMesh(
	const TCHAR* BaseName,
	UStaticMesh* Mesh,
	UMaterialInterface* MaterialOverride,
	const FVector& RelativeBoundsCenter,
	const FRotator& RelativeRotation,
	const float UniformScale)
{
	if (!Mesh || !SceneRoot || UniformScale <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(
		this,
		*FString::Printf(TEXT("%s_%02d"), BaseName, PartSerial++));
	Part->SetupAttachment(SceneRoot);
	Part->SetStaticMesh(Mesh);
	// Photo props keep every imported PBR slot. Generated meshes may opt into
	// the chapter body material through MaterialOverride.
	if (MaterialOverride)
	{
		Part->SetMaterial(0, MaterialOverride);
	}
	const FVector RotatedOrigin = RelativeRotation.RotateVector(
		Mesh->GetBounds().Origin * UniformScale);
	Part->SetRelativeLocation(RelativeBoundsCenter - RotatedOrigin);
	Part->SetRelativeRotation(RelativeRotation);
	Part->SetRelativeScale3D(FVector(UniformScale));
	Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetCastShadow(true);
	Part->RegisterComponent();
	PresentationParts.Add(Part);
	return Part;
}

UInstancedStaticMeshComponent* AIGTimeEntryPuzzle::AddInstancedPart(
	const TCHAR* BaseName,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const bool bCastShadow)
{
	if (!Mesh || !SceneRoot)
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* Part =
		NewObject<UInstancedStaticMeshComponent>(
			this,
			*FString::Printf(TEXT("%s_%02d"), BaseName, PartSerial++));
	Part->SetupAttachment(SceneRoot);
	Part->SetStaticMesh(Mesh);
	Part->SetMaterial(0, Material);
	Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetCastShadow(bCastShadow);
	Part->RegisterComponent();
	return Part;
}

void AIGTimeEntryPuzzle::BuildAlarmHousing(
	UStaticMesh* CubeMesh,
	UStaticMesh* AlarmMesh,
	UMaterialInterface* BodyMaterial,
	UMaterialInterface* InDisplayOffMaterial)
{
	bUsesAuthoredHousing = AddAuthoredPresentationMesh(
		TEXT("AlarmHousing"),
		AlarmMesh,
		BodyMaterial,
		FVector(-9.0f, 0.0f, 0.0f),
		FRotator(0.0f, -90.0f, 0.0f),
		2.65f) != nullptr;
	if (!bUsesAuthoredHousing)
	{
		AddPresentationPart(
			TEXT("AlarmFallbackHousing"),
			CubeMesh,
			BodyMaterial,
			FVector(-9.0f, 0.0f, 0.0f),
			FVector(22.0f, 36.0f, 22.0f));
	}

	// The authored mesh supplies the rounded shell and recess. These parts
	// strengthen the bedside-clock silhouette at a glance and cast into the
	// shelf lighting even when the display is dark.
	AddPresentationPart(
		TEXT("AlarmSnoozeBar"),
		CubeMesh,
		InDisplayOffMaterial,
		FVector(-5.0f, 0.0f, 10.0f),
		FVector(7.0f, 14.0f, 1.4f));
	AddPresentationPart(
		TEXT("AlarmLeftFoot"),
		CubeMesh,
		BodyMaterial,
		FVector(-8.0f, -12.0f, -10.0f),
		FVector(7.0f, 5.0f, 3.0f));
	AddPresentationPart(
		TEXT("AlarmRightFoot"),
		CubeMesh,
		BodyMaterial,
		FVector(-8.0f, 12.0f, -10.0f),
		FVector(7.0f, 5.0f, 3.0f));
}

void AIGTimeEntryPuzzle::BuildPosHousing(
	UStaticMesh* CubeMesh,
	UStaticMesh* PosMesh,
	UMaterialInterface* BodyMaterial,
	UMaterialInterface* InDisplayOffMaterial,
	UMaterialInterface* ButtonMaterial)
{
	// No material override: the Poly Haven scan keeps its imported base color,
	// roughness and normal response instead of being flattened to a grey proxy.
	constexpr float PosUniformScale = 0.96f;
	const FRotator PosRotation(0.0f, -90.0f, 0.0f);
	// Center the imported bounds rather than its arbitrary scan pivot, then
	// place the front face at X=2 cm. The glass at X=2.55 cm consequently
	// sits on the shell instead of floating in front of or clipping through it.
	const FVector PosRotatedExtent = PosMesh
		? PosRotation.RotateVector(
			PosMesh->GetBounds().BoxExtent * PosUniformScale).GetAbs()
		: FVector::ZeroVector;
	const FVector PosBoundsCenter(
		2.0f - PosRotatedExtent.X,
		0.0f,
		0.0f);
	bUsesAuthoredHousing = AddAuthoredPresentationMesh(
		TEXT("PosHousing"),
		PosMesh,
		nullptr,
		PosBoundsCenter,
		PosRotation,
		PosUniformScale) != nullptr;
	if (!bUsesAuthoredHousing)
	{
		AddPresentationPart(
			TEXT("PosFallbackHousing"),
			CubeMesh,
			BodyMaterial,
			FVector(-20.0f, 0.0f, 0.0f),
			FVector(45.0f, 50.0f, 36.0f));
	}

	AddPresentationPart(
		TEXT("PosCashDrawer"),
		CubeMesh,
		BodyMaterial,
		FVector(-20.0f, 0.0f, -20.0f),
		FVector(34.0f, 52.0f, 11.0f));
	AddPresentationPart(
		TEXT("PosDrawerHandle"),
		CubeMesh,
		InDisplayOffMaterial,
		FVector(-2.5f, 0.0f, -20.0f),
		FVector(1.0f, 15.0f, 2.0f));
	AddPresentationPart(
		TEXT("PosPrinterSlot"),
		CubeMesh,
		InDisplayOffMaterial,
		FVector(2.5f, -18.0f, 14.0f),
		FVector(0.8f, 15.0f, 1.5f));

	// The inert 3x4 keypad is one instanced draw. It preserves the familiar
	// Korean convenience-store POS silhouette without allocating 12 components.
	PosKeypadInstances = AddInstancedPart(
		TEXT("PosKeypadKeys"), CubeMesh, ButtonMaterial, true);
	if (PosKeypadInstances)
	{
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 3; ++Column)
			{
				PosKeypadInstances->AddInstance(FTransform(
					FRotator::ZeroRotator,
					FVector(
						3.0f,
						19.0f + Column * 4.0f,
						7.0f - Row * 4.0f),
					FVector(0.01f, 0.026f, 0.026f)));
			}
		}
	}
}

AIGTimeEntryButton* AIGTimeEntryPuzzle::SpawnButton(
	const EIGTimeEntryButtonAction Action,
	const FVector& RelativeLocation,
	UStaticMesh* CubeMesh,
	UMaterialInterface* Material,
	const FText& Prompt)
{
	if (!GetWorld())
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform RelativeTransform(FRotator::ZeroRotator, RelativeLocation);
	AIGTimeEntryButton* Button =
		GetWorld()->SpawnActor<AIGTimeEntryButton>(
			AIGTimeEntryButton::StaticClass(),
			RelativeTransform * GetActorTransform(),
			Parameters);
	if (Button)
	{
		Button->Configure(this, Action, CubeMesh, Material, Prompt);
		Buttons.Add(Button);
	}
	return Button;
}

void AIGTimeEntryPuzzle::BuildDisplay(
	UStaticMesh* CubeMesh,
	UMaterialInterface* InDisplayOffMaterial)
{
	DisplayOnInstances = AddInstancedPart(
		TEXT("DisplayOnSegments"), CubeMesh, DisplayOnMaterial, false);
	DisplayOffInstances = AddInstancedPart(
		TEXT("DisplayOffSegments"), CubeMesh, InDisplayOffMaterial, false);
	if (!DisplayOnInstances || !DisplayOffInstances)
	{
		return;
	}

	for (const FVector& Center : IGTimeEntry::DigitCenters)
	{
		const FVector SegmentLocations[] = {
			Center + FVector(0.0f, 0.0f, 4.4f),
			Center + FVector(0.0f, 2.0f, 2.2f),
			Center + FVector(0.0f, 2.0f, -2.2f),
			Center + FVector(0.0f, 0.0f, -4.4f),
			Center + FVector(0.0f, -2.0f, -2.2f),
			Center + FVector(0.0f, -2.0f, 2.2f),
			Center};
		for (int32 SegmentIndex = 0; SegmentIndex < 7; ++SegmentIndex)
		{
			const bool bHorizontal = SegmentIndex == 0
				|| SegmentIndex == 3
				|| SegmentIndex == 6;
			DisplaySegmentTransforms.Add(FTransform(
				FRotator::ZeroRotator,
				SegmentLocations[SegmentIndex],
				bHorizontal
					? FVector(0.001f, 0.04f, 0.0065f)
					: FVector(0.001f, 0.0065f, 0.04f)));
		}
	}
}

void AIGTimeEntryPuzzle::UpdateDisplay()
{
	if (DisplaySegmentTransforms.Num() != 28
		|| !DisplayOnInstances
		|| !DisplayOffInstances)
	{
		return;
	}
	DisplayOnInstances->ClearInstances();
	DisplayOffInstances->ClearInstances();
	const int32 Digits[] = {
		Hour / 10,
		Hour % 10,
		Minute / 10,
		Minute % 10};
	for (int32 DigitIndex = 0; DigitIndex < 4; ++DigitIndex)
	{
		for (int32 SegmentIndex = 0; SegmentIndex < 7; ++SegmentIndex)
		{
			const FTransform& SegmentTransform =
				DisplaySegmentTransforms[DigitIndex * 7 + SegmentIndex];
			UInstancedStaticMeshComponent* TargetInstances =
				IGTimeEntry::DigitSegments[Digits[DigitIndex]][SegmentIndex]
					? DisplayOnInstances.Get()
					: DisplayOffInstances.Get();
			TargetInstances->AddInstance(SegmentTransform);
		}
	}
	for (const float DotZ : {-1.7f, 3.7f})
	{
		DisplayOnInstances->AddInstance(FTransform(
			FRotator::ZeroRotator,
			FVector(2.32f, 0.0f, DotZ),
			FVector(0.001f, 0.007f, 0.007f)));
	}
}

void AIGTimeEntryPuzzle::UpdateButtonPrompts()
{
	if (AIGTimeEntryButton* HourButton = FindButton(
		EIGTimeEntryButtonAction::Hour))
	{
		HourButton->SetInteractionPrompt(FText::Format(
			PuzzleId == EIGTimeEntryPuzzleId::P1Alarm
				? NSLOCTEXT(
					"IGTimeEntry",
					"AlarmHourValuePrompt",
					"알람 시 조정  [{0}]")
				: NSLOCTEXT(
					"IGTimeEntry",
					"TransactionHourValuePrompt",
					"원거래 시 조정  [{0}]"),
			FText::FromString(FString::Printf(TEXT("%02d"), Hour))));
	}
	if (AIGTimeEntryButton* MinuteButton = FindButton(
		EIGTimeEntryButtonAction::Minute))
	{
		MinuteButton->SetInteractionPrompt(FText::Format(
			PuzzleId == EIGTimeEntryPuzzleId::P1Alarm
				? NSLOCTEXT(
					"IGTimeEntry",
					"AlarmMinuteValuePrompt",
					"알람 분 조정  [{0}]")
				: NSLOCTEXT(
					"IGTimeEntry",
					"TransactionMinuteValuePrompt",
					"원거래 분 조정  [{0}]"),
			FText::FromString(FString::Printf(TEXT("%02d"), Minute))));
	}
	if (AIGTimeEntryButton* ConfirmButton = FindButton(
		EIGTimeEntryButtonAction::Confirm))
	{
		ConfirmButton->SetInteractionPrompt(FText::Format(
			PuzzleId == EIGTimeEntryPuzzleId::P1Alarm
				? NSLOCTEXT(
					"IGTimeEntry",
					"AlarmConfirmValuePrompt",
					"알람 시각 확인  [{0}:{1}]")
				: NSLOCTEXT(
					"IGTimeEntry",
					"TransactionConfirmValuePrompt",
					"원거래 시각 복원  [{0}:{1}]"),
			FText::FromString(FString::Printf(TEXT("%02d"), Hour)),
			FText::FromString(FString::Printf(TEXT("%02d"), Minute))));
	}
}

void AIGTimeEntryPuzzle::CycleHour()
{
	const int32 CurrentIndex = HourCandidates.IndexOfByKey(Hour);
	Hour = HourCandidates[(CurrentIndex + 1) % HourCandidates.Num()];
	UpdateDisplay();
	UpdateButtonPrompts();
}

void AIGTimeEntryPuzzle::CycleMinute()
{
	const int32 CurrentIndex = MinuteCandidates.IndexOfByKey(Minute);
	Minute = MinuteCandidates[(CurrentIndex + 1) % MinuteCandidates.Num()];
	UpdateDisplay();
	UpdateButtonPrompts();
}

AIGTimeEntryButton* AIGTimeEntryPuzzle::FindButton(
	const EIGTimeEntryButtonAction Action) const
{
	for (AIGTimeEntryButton* Button : Buttons)
	{
		if (Button && Button->GetAction() == Action)
		{
			return Button;
		}
	}
	return nullptr;
}
