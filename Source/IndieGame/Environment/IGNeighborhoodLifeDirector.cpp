#include "Environment/IGNeighborhoodLifeDirector.h"

#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

namespace IGNeighborhoodLife
{
	constexpr int32 VehiclePoolSize = 2;
	constexpr int32 LeafPoolSize = 24;
	constexpr float SoundSpeedCentimetersPerSecond = 34300.0f;

	constexpr uint32 NormalSeedSalt = 0x23A16E41u;
	constexpr uint32 UncannySeedSalt = 0x7819BC05u;
	constexpr uint32 AbsentSeedSalt = 0xD0044A11u;

	uint32 VariantSalt(const EIGNeighborhoodChapterVariant Variant)
	{
		switch (Variant)
		{
		case EIGNeighborhoodChapterVariant::ChapterTwoUncanny:
			return UncannySeedSalt;
		case EIGNeighborhoodChapterVariant::ChapterTwoAbsent:
			return AbsentSeedSalt;
		default:
			return NormalSeedSalt;
		}
	}
}

AIGNeighborhoodLifeDirector::AIGNeighborhoodLifeDirector()
{
	// Per-frame work exists only while a pooled pass, gust, leaf or cat trace
	// is active. Vehicles therefore stay smooth without an always-on tick.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("NeighborhoodLifeRoot"));
	SetRootComponent(SceneRoot);
}

void AIGNeighborhoodLifeDirector::BeginPlay()
{
	Super::BeginPlay();
	InitializePools();
	RestartDeterministicSchedule();
}

void AIGNeighborhoodLifeDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateRuntime(FMath::Clamp(DeltaSeconds, 0.0f, 0.1f));
}

void AIGNeighborhoodLifeDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	StopAllRuntimeEvents();
	Super::EndPlay(EndPlayReason);
}

void AIGNeighborhoodLifeDirector::ConfigureNeighborhood(
	const FVector InRoadStart,
	const FVector InRoadEnd,
	const int32 InDeterministicSeed)
{
	if (!InRoadStart.Equals(InRoadEnd, 10.0f))
	{
		RoadStart = InRoadStart;
		RoadEnd = InRoadEnd;
	}
	DeterministicSeed = InDeterministicSeed != 0 ? InDeterministicSeed : 4040444;

	if (HasActorBegunPlay())
	{
		InitializePools();
		RestartDeterministicSchedule();
	}
}

void AIGNeighborhoodLifeDirector::SetChapterVariant(
	const EIGNeighborhoodChapterVariant InVariant)
{
	if (ChapterVariant == InVariant)
	{
		return;
	}

	ChapterVariant = InVariant;
	if (HasActorBegunPlay())
	{
		RestartDeterministicSchedule();
	}
}

void AIGNeighborhoodLifeDirector::PrimeOutdoorSequence()
{
	// The street-level trigger may overlap again while the pawn is still
	// crossing its boundary.  Re-arming here would restart the cat/wind/car
	// sequence and turn ordinary neighbourhood sound into a noticeable loop.
	if (!GetWorld() || bOutdoorSequencePrimed ||
		ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoAbsent)
	{
		return;
	}

	bOutdoorSequencePrimed = true;
	GetWorldTimerManager().ClearTimer(VehicleScheduleHandle);
	GetWorldTimerManager().ClearTimer(CatScheduleHandle);
	GetWorldTimerManager().ClearTimer(GustScheduleHandle);
	GetWorldTimerManager().SetTimer(
		VehicleScheduleHandle,
		this,
		&ThisClass::LaunchVehicleEvent,
		6.8f,
		false);
	GetWorldTimerManager().SetTimer(
		CatScheduleHandle,
		this,
		&ThisClass::LaunchCatTrace,
		0.8f,
		false);
	GetWorldTimerManager().SetTimer(
		GustScheduleHandle,
		this,
		&ThisClass::LaunchWindGust,
		3.4f,
		false);
}

void AIGNeighborhoodLifeDirector::RegisterWindReactiveComponent(
	USceneComponent* Component,
	const float ResponseScale)
{
	if (!Component)
	{
		return;
	}

	for (FWindReactorRuntime& Reactor : WindReactors)
	{
		if (Reactor.Component == Component)
		{
			Reactor.NeutralRotation = Component->GetRelativeRotation();
			Reactor.ResponseScale = FMath::Clamp(ResponseScale, 0.0f, 4.0f);
			return;
		}
	}

	FWindReactorRuntime& Reactor = WindReactors.AddDefaulted_GetRef();
	Reactor.Component = Component;
	Reactor.NeutralRotation = Component->GetRelativeRotation();
	Reactor.ResponseScale = FMath::Clamp(ResponseScale, 0.0f, 4.0f);
}

void AIGNeighborhoodLifeDirector::UnregisterWindReactiveComponent(USceneComponent* Component)
{
	WindReactors.RemoveAllSwap(
		[Component](const FWindReactorRuntime& Reactor)
		{
			return !Reactor.Component.IsValid() || Reactor.Component == Component;
		},
		EAllowShrinking::No);
}

void AIGNeighborhoodLifeDirector::InitializePools()
{
	if (bPoolsInitialized || !GetWorld())
	{
		return;
	}

	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	DarkMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_PlasticDark.M_PlasticDark"));
	MetalMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_MetalUV.M_MetalUV"));
	LeafMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_Cardboard.M_Cardboard"));

	if (!CubeMesh || !SphereMesh || !CylinderMesh)
	{
		return;
	}

	auto ConfigureVisual = [this](
		UStaticMeshComponent* Component,
		UStaticMesh* Mesh,
		UMaterialInterface* Material)
	{
		Component->SetupAttachment(SceneRoot);
		Component->SetStaticMesh(Mesh);
		if (Material)
		{
			Component->SetMaterial(0, Material);
		}
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(true);
		Component->SetHiddenInGame(true);
		Component->RegisterComponent();
	};

	for (int32 SlotIndex = 0; SlotIndex < IGNeighborhoodLife::VehiclePoolSize; ++SlotIndex)
	{
		UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(
			this, *FString::Printf(TEXT("VehicleBody_%d"), SlotIndex));
		ConfigureVisual(Body, CubeMesh, MetalMaterial);
		VehicleBodies.Add(Body);

		UStaticMeshComponent* Cabin = NewObject<UStaticMeshComponent>(
			this, *FString::Printf(TEXT("VehicleCabin_%d"), SlotIndex));
		ConfigureVisual(Cabin, CubeMesh, DarkMaterial);
		Cabin->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform);
		// Body scale authors its centimetre dimensions. Children inherit
		// translation/rotation but keep their own real-world dimensions.
		Cabin->SetAbsolute(false, false, true);
		VehicleCabins.Add(Cabin);

		UStaticMeshComponent* Cargo = NewObject<UStaticMeshComponent>(
			this, *FString::Printf(TEXT("VehicleCargo_%d"), SlotIndex));
		ConfigureVisual(Cargo, CubeMesh, LeafMaterial);
		Cargo->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform);
		Cargo->SetAbsolute(false, false, true);
		VehicleCargoBoxes.Add(Cargo);

		UStaticMeshComponent* RiderHead = NewObject<UStaticMeshComponent>(
			this, *FString::Printf(TEXT("VehicleRiderHead_%d"), SlotIndex));
		ConfigureVisual(RiderHead, SphereMesh, DarkMaterial);
		RiderHead->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform);
		RiderHead->SetAbsolute(false, false, true);
		VehicleRiderHeads.Add(RiderHead);

		for (int32 WheelIndex = 0; WheelIndex < 4; ++WheelIndex)
		{
			UStaticMeshComponent* Wheel = NewObject<UStaticMeshComponent>(
				this,
				*FString::Printf(TEXT("VehicleWheel_%d_%d"), SlotIndex, WheelIndex));
			ConfigureVisual(Wheel, CylinderMesh, DarkMaterial);
			Wheel->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform);
			Wheel->SetAbsolute(false, false, true);
			Wheel->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
			VehicleWheels.Add(Wheel);
		}

		UAudioComponent* Audio = CreateSpatialAudioComponent(
			Body,
			*FString::Printf(TEXT("VehicleAudio_%d"), SlotIndex),
			180.0f,
			2400.0f);
		VehicleAudio.Add(Audio);
		VehicleRuntime.AddDefaulted();
	}

	for (int32 LeafIndex = 0; LeafIndex < IGNeighborhoodLife::LeafPoolSize; ++LeafIndex)
	{
		UStaticMeshComponent* Leaf = NewObject<UStaticMeshComponent>(
			this, *FString::Printf(TEXT("WindLeaf_%d"), LeafIndex));
		// A paper-thin cube renders from both sides; Engine/BasicShapes/Plane
		// disappears whenever a one-sided material flips away from the camera.
		ConfigureVisual(Leaf, CubeMesh, LeafMaterial);
		Leaf->SetCastShadow(false);
		Leaf->SetRelativeScale3D(FVector(0.055f, 0.025f, 0.003f));
		LeafMeshes.Add(Leaf);
		LeafRuntime.AddDefaulted();
	}

	CatTraceRoot = NewObject<USceneComponent>(this, TEXT("FleeingCatTraceRoot"));
	CatTraceRoot->SetupAttachment(SceneRoot);
	CatTraceRoot->SetMobility(EComponentMobility::Movable);
	CatTraceRoot->RegisterComponent();

	auto AddCatPart = [this, &ConfigureVisual](
		const TCHAR* Name,
		UStaticMesh* Mesh,
		const FVector& Location,
		const FVector& Scale,
		const FRotator& Rotation)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, Name);
		ConfigureVisual(Part, Mesh, DarkMaterial);
		Part->AttachToComponent(CatTraceRoot, FAttachmentTransformRules::KeepRelativeTransform);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeRotation(Rotation);
		Part->SetRelativeScale3D(Scale);
		Part->SetCastShadow(false);
		CatSilhouetteParts.Add(Part);
	};

	// A readable cat only in silhouette and only for about a second. Separate
	// body/head/tail/leg masses avoid the "flattened debug sphere" look while
	// staying abstract enough that it never presents as a creature encounter.
	AddCatPart(
		TEXT("CatBody"), SphereMesh, FVector(0, 0, 18),
		FVector(0.42f, 0.14f, 0.14f), FRotator::ZeroRotator);
	AddCatPart(
		TEXT("CatHead"), SphereMesh, FVector(26, 0, 22),
		FVector(0.15f, 0.12f, 0.13f), FRotator::ZeroRotator);
	AddCatPart(
		TEXT("CatTail"), CubeMesh, FVector(-31, 0, 23), FVector(0.34f, 0.028f, 0.028f),
		FRotator(0, -18, 18));
	AddCatPart(
		TEXT("CatForeLeg"), CubeMesh, FVector(14, -5, 7), FVector(0.10f, 0.028f, 0.11f),
		FRotator(0, 0, -18));
	AddCatPart(
		TEXT("CatHindLeg"), CubeMesh, FVector(-14, 5, 7), FVector(0.11f, 0.028f, 0.10f),
		FRotator(0, 0, 24));
	AddCatPart(
		TEXT("CatFarLeg"), CubeMesh, FVector(-6, -5, 6), FVector(0.09f, 0.025f, 0.09f),
		FRotator(0, 0, -28));

	bPoolsInitialized = true;
}

UAudioComponent* AIGNeighborhoodLifeDirector::CreateSpatialAudioComponent(
	USceneComponent* Parent,
	const FName ComponentName,
	const float InnerRadius,
	const float FalloffDistance)
{
	UAudioComponent* Audio = NewObject<UAudioComponent>(this, ComponentName);
	Audio->SetupAttachment(Parent ? Parent : SceneRoot.Get());
	Audio->bAutoActivate = false;
	Audio->bAutoDestroy = false;
	Audio->bOverrideAttenuation = true;
	Audio->AttenuationOverrides.bAttenuate = true;
	Audio->AttenuationOverrides.bSpatialize = true;
	Audio->AttenuationOverrides.AttenuationShapeExtents =
		FVector(FMath::Max(1.0f, InnerRadius), 0.0f, 0.0f);
	Audio->AttenuationOverrides.FalloffDistance = FMath::Max(1.0f, FalloffDistance);
	Audio->AttenuationOverrides.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Audio->AttenuationOverrides.dBAttenuationAtMax = -60.0f;
	Audio->RegisterComponent();
	return Audio;
}

UIGToneSequenceSoundWave* AIGNeighborhoodLifeDirector::CreateVehicleLoop(
	UObject* Outer,
	const EIGPooledVehicleKind Kind) const
{
	UIGToneSequenceSoundWave* Wave = NewObject<UIGToneSequenceSoundWave>(Outer);
	TArray<FIGToneNote> Notes;

	if (Kind == EIGPooledVehicleKind::DeliveryMotorcycle)
	{
		// A small single-cylinder engine: uneven fundamental, bright exhaust
		// rasp and a little tyre noise. Spatial pan comes from the moving
		// component; the bounded runtime update supplies restrained Doppler.
		Notes.Add({0.00f, 0.48f, 104.0f, 0.110f, 0.03f, 0.45f, EIGToneWaveform::SoftSquare});
		Notes.Add({0.43f, 0.50f, 111.0f, 0.105f, 0.03f, 0.45f, EIGToneWaveform::SoftSquare});
		Notes.Add({0.00f, 0.98f, 760.0f, 0.035f, 0.03f, 0.60f, EIGToneWaveform::ValueNoise});
		Wave->ConfigureNotes(MoveTemp(Notes), true, 0.98f);
	}
	else
	{
		Notes.Add({0.00f, 1.20f, 57.0f, 0.100f, 0.02f, 0.35f, EIGToneWaveform::Sine});
		Notes.Add({0.00f, 1.20f, 114.0f, 0.042f, 0.02f, 0.40f, EIGToneWaveform::Triangle});
		Notes.Add({0.00f, 1.20f, 310.0f, 0.022f, 0.02f, 0.55f, EIGToneWaveform::ValueNoise});
		Wave->ConfigureNotes(MoveTemp(Notes), true, 1.20f);
	}
	return Wave;
}

UIGToneSequenceSoundWave* AIGNeighborhoodLifeDirector::CreateGustSound(
	UObject* Outer,
	const float Duration) const
{
	UIGToneSequenceSoundWave* Wave = NewObject<UIGToneSequenceSoundWave>(Outer);
	TArray<FIGToneNote> Notes;
	Notes.Add({0.0f, Duration, 420.0f, 0.075f, 0.24f, 1.8f, EIGToneWaveform::ValueNoise});
	Notes.Add({0.2f, FMath::Max(0.2f, Duration - 0.4f), 91.0f, 0.028f, 0.32f, 2.1f,
		EIGToneWaveform::ValueNoise});
	Wave->ConfigureNotes(MoveTemp(Notes), false);
	return Wave;
}

UIGToneSequenceSoundWave* AIGNeighborhoodLifeDirector::CreateCatCall(
	UObject* Outer,
	const bool bUncanny) const
{
	UIGToneSequenceSoundWave* Wave = NewObject<UIGToneSequenceSoundWave>(Outer);
	TArray<FIGToneNote> Notes;

	const float PitchScale = bUncanny ? 0.82f : 1.0f;
	const float Amplitude = bUncanny ? 0.075f : 0.105f;
	// Stepped glide is softened by overlapping sine partials. It reads as a
	// distant cat call without presenting an animal directly.
	const float Frequencies[] = {690.0f, 760.0f, 825.0f, 790.0f, 710.0f, 625.0f};
	for (int32 NoteIndex = 0; NoteIndex < static_cast<int32>(UE_ARRAY_COUNT(Frequencies)); ++NoteIndex)
	{
		const float Start = NoteIndex * 0.085f;
		Notes.Add({Start, 0.18f, Frequencies[NoteIndex] * PitchScale, Amplitude,
			0.18f, 1.15f, EIGToneWaveform::Sine});
		Notes.Add({Start, 0.16f, Frequencies[NoteIndex] * 2.01f * PitchScale,
			Amplitude * 0.18f, 0.20f, 1.35f, EIGToneWaveform::Sine});
	}
	if (bUncanny)
	{
		// The second call arrives too late and too low, implying a location the
		// fleeting ground trace cannot account for.
		Notes.Add({1.15f, 0.62f, 410.0f, 0.055f, 0.16f, 1.6f, EIGToneWaveform::Sine});
		Notes.Add({1.23f, 0.58f, 816.0f, 0.012f, 0.20f, 1.8f, EIGToneWaveform::Sine});
	}
	Wave->ConfigureNotes(MoveTemp(Notes), false);
	return Wave;
}

void AIGNeighborhoodLifeDirector::RestartDeterministicSchedule()
{
	if (!GetWorld())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(VehicleScheduleHandle);
	GetWorldTimerManager().ClearTimer(GustScheduleHandle);
	GetWorldTimerManager().ClearTimer(CatScheduleHandle);
	StopAllRuntimeEvents();

	const uint32 CombinedSeed =
		static_cast<uint32>(DeterministicSeed) ^
		IGNeighborhoodLife::VariantSalt(ChapterVariant);
	Random.Initialize(static_cast<int32>(CombinedSeed));
	bOutdoorSequencePrimed = false;
	ActiveVehicleOrdinal = 0;

	if (ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoAbsent)
	{
		// The return beat deliberately leaves the distant alarm as the sole
		// source.  Scheduling wind here would undercut that authored silence.
		return;
	}

	ScheduleNextGust();
	ScheduleNextVehicle();
	ScheduleNextCatTrace();
}

void AIGNeighborhoodLifeDirector::StopAllRuntimeEvents()
{
	for (int32 SlotIndex = 0; SlotIndex < VehicleRuntime.Num(); ++SlotIndex)
	{
		DeactivateVehicle(SlotIndex);
	}
	for (int32 LeafIndex = 0; LeafIndex < LeafRuntime.Num(); ++LeafIndex)
	{
		DeactivateLeaf(LeafIndex);
	}
	DeactivateCatTrace();
	for (UAudioComponent* Audio : TransientAudio)
	{
		if (IsValid(Audio))
		{
			Audio->Stop();
			Audio->DestroyComponent();
		}
	}
	TransientAudio.Reset();

	GustElapsed = -1.0f;
	CurrentWindSignal = FVector::ZeroVector;
	UpdateWindReactors();
	SetActorTickEnabled(false);
}

float AIGNeighborhoodLifeDirector::RandomRangeForVariant(
	const float NormalMin,
	const float NormalMax,
	const float UncannyMin,
	const float UncannyMax)
{
	return ChapterVariant == EIGNeighborhoodChapterVariant::ChapterOneNormal
		? Random.FRandRange(NormalMin, NormalMax)
		: Random.FRandRange(UncannyMin, UncannyMax);
}

void AIGNeighborhoodLifeDirector::ScheduleNextVehicle()
{
	if (!GetWorld() || ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoAbsent)
	{
		return;
	}

	// Sparse enough for a residential alley before dawn: individual passes
	// register as events, not a continuous traffic loop.
	float Delay = RandomRangeForVariant(22.0f, 40.0f, 38.0f, 70.0f);
	if (bOutdoorSequencePrimed
		&& ActiveVehicleOrdinal == 1)
	{
		// The second authored life beat is the delivery motorcycle in both
		// loops; CH02 changes its sound/causality rather than replacing it.
		Delay = 5.4f;
	}
	GetWorldTimerManager().SetTimer(
		VehicleScheduleHandle,
		this,
		&ThisClass::LaunchVehicleEvent,
		Delay,
		false);
}

void AIGNeighborhoodLifeDirector::ScheduleNextGust()
{
	if (!GetWorld())
	{
		return;
	}

	float Delay = RandomRangeForVariant(5.0f, 13.0f, 9.0f, 22.0f);
	if (ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoAbsent)
	{
		Delay = Random.FRandRange(13.0f, 27.0f);
	}
	GetWorldTimerManager().SetTimer(
		GustScheduleHandle,
		this,
		&ThisClass::LaunchWindGust,
		Delay,
		false);
}

void AIGNeighborhoodLifeDirector::ScheduleNextCatTrace()
{
	if (!GetWorld() || ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoAbsent)
	{
		return;
	}

	const float Delay = RandomRangeForVariant(17.0f, 31.0f, 29.0f, 53.0f);
	GetWorldTimerManager().SetTimer(
		CatScheduleHandle,
		this,
		&ThisClass::LaunchCatTrace,
		Delay,
		false);
}

void AIGNeighborhoodLifeDirector::LaunchVehicleEvent()
{
	if (!bPoolsInitialized || !IsPlayerNearRoad(EventActivationDistance))
	{
		ScheduleNextVehicle();
		return;
	}

	const int32 SlotIndex = VehicleRuntime.IndexOfByPredicate(
		[](const FVehicleRuntime& Runtime) { return !Runtime.bActive; });
	if (SlotIndex == INDEX_NONE)
	{
		ScheduleNextVehicle();
		return;
	}

	// Establish both pieces of ordinary CH01 life before falling back to the
	// weighted grammar. Ordinal advances only for an event that actually got
	// an available pool slot while the listener was close enough.
	const bool bMotorcycle =
		ActiveVehicleOrdinal < 2
			? ActiveVehicleOrdinal == 1
			: Random.FRand() < 0.38f;
	++ActiveVehicleOrdinal;
	ScheduleNextVehicle();
	const bool bReverse = Random.FRand() < 0.46f;
	const FVector RoadDirection = (RoadEnd - RoadStart).GetSafeNormal();
	const FVector SideOffset =
		FVector::CrossProduct(FVector::UpVector, RoadDirection) *
		// Keep the pooled traffic in the carriageway. The player walks the
		// Y=-457 storefront edge; the old +38 cm motorcycle lane grazed the
		// capsule even though the visual has no collision.
		(bMotorcycle ? 0.0f : -55.0f);
	const FVector Start = (bReverse ? RoadEnd : RoadStart) + SideOffset;
	const FVector End = (bReverse ? RoadStart : RoadEnd) + SideOffset;
	const float Speed = bMotorcycle
		? RandomRangeForVariant(760.0f, 1040.0f, 520.0f, 710.0f)
		: RandomRangeForVariant(560.0f, 760.0f, 390.0f, 570.0f);

	FVehicleRuntime& Runtime = VehicleRuntime[SlotIndex];
	Runtime.bActive = true;
	Runtime.bAudioDropsOut =
		ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoUncanny &&
		Random.FRand() < 0.72f;
	Runtime.Kind = bMotorcycle
		? EIGPooledVehicleKind::DeliveryMotorcycle
		: EIGPooledVehicleKind::PassengerCar;
	Runtime.Start = Start;
	Runtime.End = End;
	Runtime.Duration = FVector::Distance(Start, End) / FMath::Max(1.0f, Speed);
	Runtime.Elapsed = 0.0f;
	Runtime.Velocity = (End - Start) / Runtime.Duration;
	Runtime.BasePitch = bMotorcycle ? 1.02f : 0.96f;
	Runtime.BaseVolume = bMotorcycle ? 0.32f : 0.42f;

	UStaticMeshComponent* Body = VehicleBodies[SlotIndex];
	UStaticMeshComponent* Cabin = VehicleCabins[SlotIndex];
	UStaticMeshComponent* Cargo = VehicleCargoBoxes[SlotIndex];
	const float TravelYaw = Runtime.Velocity.Rotation().Yaw;
	const float BodyCenterHeight = bMotorcycle ? 22.0f : 38.0f;
	Body->SetWorldLocationAndRotation(
		Start + FVector(0.0f, 0.0f, BodyCenterHeight),
		FRotator(0.0f, TravelYaw, 0.0f));

	if (bMotorcycle)
	{
		Body->SetRelativeScale3D(FVector(1.55f, 0.42f, 0.34f));
		Body->SetVisibility(true);
		Body->SetHiddenInGame(false);
		// The cabin primitive becomes the courier's torso for this pooled
		// variant. A riderless delivery bike reads as a physics bug.
		// Relative locations are composed through the chassis' 0.34 Z scale.
		Cabin->SetRelativeLocation(FVector(4.0f, 0.0f, 175.0f));
		Cabin->SetRelativeRotation(FRotator(0.0f, 0.0f, -12.0f));
		Cabin->SetRelativeScale3D(FVector(0.34f, 0.28f, 0.48f));
		Cabin->SetVisibility(true);
		Cabin->SetHiddenInGame(false);
		Cargo->SetRelativeLocation(FVector(-35.0f, 0.0f, 130.0f));
		Cargo->SetRelativeScale3D(FVector(0.58f, 0.56f, 0.55f));
		Cargo->SetVisibility(true);
		Cargo->SetHiddenInGame(false);
		if (VehicleRiderHeads.IsValidIndex(SlotIndex))
		{
			UStaticMeshComponent* RiderHead = VehicleRiderHeads[SlotIndex];
			RiderHead->SetRelativeLocation(FVector(14.0f, 0.0f, 290.0f));
			RiderHead->SetRelativeScale3D(FVector(0.18f));
			RiderHead->SetVisibility(true);
			RiderHead->SetHiddenInGame(false);
		}

		for (int32 WheelIndex = 0; WheelIndex < 4; ++WheelIndex)
		{
			UStaticMeshComponent* Wheel = VehicleWheels[SlotIndex * 4 + WheelIndex];
			const bool bUsed = WheelIndex < 2;
			Wheel->SetRelativeLocation(FVector(
				WheelIndex == 0 ? 42.0f : -42.0f,
				0.0f,
				14.7f));
			Wheel->SetRelativeScale3D(FVector(0.46f, 0.46f, 0.16f));
			Wheel->SetVisibility(bUsed);
			Wheel->SetHiddenInGame(!bUsed);
		}
	}
	else
	{
		Body->SetRelativeScale3D(FVector(4.15f, 1.72f, 0.66f));
		Body->SetVisibility(true);
		Body->SetHiddenInGame(false);
		Cabin->SetRelativeLocation(FVector(-3.6f, 0.0f, 100.0f));
		Cabin->SetRelativeRotation(FRotator::ZeroRotator);
		Cabin->SetRelativeScale3D(FVector(2.10f, 1.50f, 0.64f));
		Cabin->SetVisibility(true);
		Cabin->SetHiddenInGame(false);
		Cargo->SetVisibility(false);
		Cargo->SetHiddenInGame(true);
		if (VehicleRiderHeads.IsValidIndex(SlotIndex))
		{
			VehicleRiderHeads[SlotIndex]->SetVisibility(false);
			VehicleRiderHeads[SlotIndex]->SetHiddenInGame(true);
		}

		for (int32 WheelIndex = 0; WheelIndex < 4; ++WheelIndex)
		{
			UStaticMeshComponent* Wheel = VehicleWheels[SlotIndex * 4 + WheelIndex];
			const bool bFront = WheelIndex < 2;
			const bool bLeft = (WheelIndex % 2) == 0;
			Wheel->SetRelativeLocation(FVector(
				bFront ? 38.0f : -38.0f,
				bLeft ? 49.0f : -49.0f,
				-7.6f));
			Wheel->SetRelativeScale3D(FVector(0.56f, 0.56f, 0.20f));
			Wheel->SetVisibility(true);
			Wheel->SetHiddenInGame(false);
		}
	}

	UAudioComponent* Audio = VehicleAudio[SlotIndex];
	Audio->Stop();
	Audio->SetSound(CreateVehicleLoop(Audio, Runtime.Kind));
	Audio->SetVolumeMultiplier(Runtime.BaseVolume);
	Audio->SetPitchMultiplier(Runtime.BasePitch);
	Audio->Play();

	RefreshRuntimeUpdates();
}

void AIGNeighborhoodLifeDirector::LaunchWindGust()
{
	if (ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoAbsent)
	{
		return;
	}

	ScheduleNextGust();
	if (!IsPlayerNearRoad(EventActivationDistance))
	{
		return;
	}

	GustElapsed = 0.0f;
	GustDuration = RandomRangeForVariant(2.2f, 4.2f, 3.4f, 6.2f);
	if (ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoAbsent)
	{
		GustDuration = Random.FRandRange(4.5f, 7.5f);
	}
	GustPeakStrength = RandomRangeForVariant(0.35f, 0.70f, 0.58f, 0.95f);
	const FVector RoadDirection = (RoadEnd - RoadStart).GetSafeNormal();
	const float DirectionSign = Random.FRand() < 0.5f ? -1.0f : 1.0f;
	GustDirection = (
		RoadDirection * DirectionSign +
		FVector(0.0f, Random.FRandRange(-0.22f, 0.22f), 0.04f)).GetSafeNormal();
	GustVisualPhase = Random.FRandRange(0.0f, 2.0f * UE_PI);

	OnWindGust.Broadcast(GustPeakStrength, GustDirection, GustDuration);

	UIGToneSequenceSoundWave* GustSound = CreateGustSound(this, GustDuration);
	TransientAudio.RemoveAllSwap(
		[](const TObjectPtr<UAudioComponent>& Component)
		{
			return !IsValid(Component.Get());
		},
		EAllowShrinking::No);
	UAudioComponent* GustAudio = CreateSpatialAudioComponent(
		SceneRoot, MakeUniqueObjectName(this, UAudioComponent::StaticClass(), TEXT("WindGustAudio")),
		400.0f, 2200.0f);
	GustAudio->bAutoDestroy = true;
	TransientAudio.Add(GustAudio);
	GustAudio->SetWorldLocation(FMath::Lerp(RoadStart, RoadEnd, Random.FRand()) + FVector(0, 0, 160));
	GustAudio->SetSound(GustSound);
	GustAudio->SetVolumeMultiplier(
		ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoAbsent ? 0.48f : 0.34f);
	GustAudio->Play();

	const FVector LeafOrigin =
		FMath::Lerp(RoadStart, RoadEnd, Random.FRandRange(0.12f, 0.88f)) +
		FVector(0.0f, Random.FRandRange(-85.0f, 85.0f), 3.0f);
	const int32 LeafCount =
		ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoAbsent
			? Random.RandRange(2, 5)
			: Random.RandRange(7, 13);
	ActivateLeaves(LeafOrigin, LeafCount, GustPeakStrength);
	RefreshRuntimeUpdates();
}

void AIGNeighborhoodLifeDirector::LaunchCatTrace()
{
	ScheduleNextCatTrace();
	if (!bPoolsInitialized || !IsPlayerNearRoad(900.0f))
	{
		return;
	}

	const bool bUncanny =
		ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoUncanny;
	const FVector SideAlleyMouth(1220.0f, -405.0f, 4.0f);
	const FVector CallLocation = bUncanny
		? SideAlleyMouth + FVector(0.0f, 80.0f, 420.0f)
		: SideAlleyMouth + FVector(0.0f, 35.0f, 38.0f);

	UIGToneSequenceSoundWave* CatCall = CreateCatCall(this, bUncanny);
	TransientAudio.RemoveAllSwap(
		[](const TObjectPtr<UAudioComponent>& Component)
		{
			return !IsValid(Component.Get());
		},
		EAllowShrinking::No);
	UAudioComponent* CatAudio = CreateSpatialAudioComponent(
		SceneRoot, MakeUniqueObjectName(this, UAudioComponent::StaticClass(), TEXT("CatCallAudio")),
		90.0f, 1700.0f);
	CatAudio->bAutoDestroy = true;
	TransientAudio.Add(CatAudio);
	CatAudio->SetWorldLocation(CallLocation);
	CatAudio->SetSound(CatCall);
	CatAudio->SetVolumeMultiplier(bUncanny ? 0.42f : 0.55f);
	CatAudio->Play();

	// CH02 deliberately withholds the source: the call is above the roofline,
	// while only disturbed litter remains at ground level.
	if (bUncanny)
	{
		ActivateLeaves(SideAlleyMouth, Random.RandRange(3, 6), 0.40f);
		RefreshRuntimeUpdates();
		return;
	}

	const bool bReverse = Random.FRand() < 0.5f;
	CatRuntime.bActive = true;
	CatRuntime.Start = SideAlleyMouth + FVector(0.0f, bReverse ? -250.0f : 85.0f, 0.0f);
	CatRuntime.End = SideAlleyMouth + FVector(0.0f, bReverse ? 85.0f : -250.0f, 0.0f);
	CatRuntime.Elapsed = 0.0f;
	CatRuntime.Duration = Random.FRandRange(0.9f, 1.35f);
	CatTraceRoot->SetWorldLocation(CatRuntime.Start);
	CatTraceRoot->SetWorldRotation((CatRuntime.End - CatRuntime.Start).Rotation());
	CatTraceRoot->SetWorldScale3D(FVector::OneVector);
	for (UStaticMeshComponent* Part : CatSilhouetteParts)
	{
		if (Part)
		{
			Part->SetVisibility(true);
			Part->SetHiddenInGame(false);
		}
	}
	ActivateLeaves(CatRuntime.End, Random.RandRange(3, 6), 0.55f);
	RefreshRuntimeUpdates();
}

void AIGNeighborhoodLifeDirector::PlayAuthoredReturnIncident(
	const FVector CatStart,
	const FVector CatEnd)
{
	if (ChapterVariant != EIGNeighborhoodChapterVariant::ChapterOneNormal
		|| CatStart.Equals(CatEnd, 10.0f))
	{
		return;
	}
	InitializePools();
	GetWorldTimerManager().ClearTimer(VehicleScheduleHandle);
	GetWorldTimerManager().ClearTimer(GustScheduleHandle);
	GetWorldTimerManager().ClearTimer(CatScheduleHandle);

	// The first two gusts belong to the ambient baseline. This authored third
	// gust always follows the road toward the villa and is never duplicated by
	// a newly scheduled ambient event.
	GustElapsed = 0.0f;
	GustDuration = 3.1f;
	GustPeakStrength = 0.72f;
	GustDirection = (CatEnd - CatStart).GetSafeNormal();
	GustVisualPhase = 0.35f * UE_PI;
	OnWindGust.Broadcast(GustPeakStrength, GustDirection, GustDuration);

	UAudioComponent* GustAudio = CreateSpatialAudioComponent(
		SceneRoot,
		MakeUniqueObjectName(
			this,
			UAudioComponent::StaticClass(),
			TEXT("AuthoredReturnGust")),
		320.0f,
		1900.0f);
	GustAudio->bAutoDestroy = true;
	TransientAudio.Add(GustAudio);
	GustAudio->SetWorldLocation(FMath::Lerp(CatStart, CatEnd, 0.35f));
	GustAudio->SetSound(CreateGustSound(this, GustDuration));
	GustAudio->SetVolumeMultiplier(0.42f);
	GustAudio->Play();
	ActivateLeaves(CatStart, 11, GustPeakStrength);

	UAudioComponent* CatAudio = CreateSpatialAudioComponent(
		SceneRoot,
		MakeUniqueObjectName(
			this,
			UAudioComponent::StaticClass(),
			TEXT("AuthoredReturnCat")),
		90.0f,
		1500.0f);
	CatAudio->bAutoDestroy = true;
	TransientAudio.Add(CatAudio);
	CatAudio->SetWorldLocation(CatStart + FVector(0, 0, 35));
	CatAudio->SetSound(CreateCatCall(this, false));
	CatAudio->SetVolumeMultiplier(0.58f);
	CatAudio->Play();

	CatRuntime.bActive = true;
	CatRuntime.Start = CatStart;
	CatRuntime.End = CatEnd;
	CatRuntime.Elapsed = 0.0f;
	CatRuntime.Duration = 0.95f;
	CatTraceRoot->SetWorldLocation(CatRuntime.Start);
	CatTraceRoot->SetWorldRotation((CatRuntime.End - CatRuntime.Start).Rotation());
	CatTraceRoot->SetWorldScale3D(FVector::OneVector);
	for (UStaticMeshComponent* Part : CatSilhouetteParts)
	{
		if (Part)
		{
			Part->SetVisibility(true);
			Part->SetHiddenInGame(false);
		}
	}
	ActivateLeaves(CatRuntime.End, 5, 0.58f);
	RefreshRuntimeUpdates();
}

void AIGNeighborhoodLifeDirector::PlayAuthoredCatCall(
	const FVector WorldLocation,
	const bool bUncanny)
{
	if (!GetWorld() || !SceneRoot)
	{
		return;
	}

	TransientAudio.RemoveAllSwap(
		[](const TObjectPtr<UAudioComponent>& Component)
		{
			return !IsValid(Component.Get());
		},
		EAllowShrinking::No);
	UAudioComponent* CatAudio = CreateSpatialAudioComponent(
		SceneRoot,
		MakeUniqueObjectName(
			this,
			UAudioComponent::StaticClass(),
			TEXT("AuthoredCatCall")),
		85.0f,
		1250.0f);
	CatAudio->bAutoDestroy = true;
	TransientAudio.Add(CatAudio);
	CatAudio->SetWorldLocation(WorldLocation);
	CatAudio->SetSound(CreateCatCall(this, bUncanny));
	CatAudio->SetVolumeMultiplier(bUncanny ? 0.38f : 0.50f);
	CatAudio->Play();
}

void AIGNeighborhoodLifeDirector::ActivateLeaves(
	const FVector& Origin,
	const int32 Count,
	const float ImpulseScale)
{
	int32 Remaining = FMath::Max(0, Count);
	for (int32 LeafIndex = 0; LeafIndex < LeafRuntime.Num() && Remaining > 0; ++LeafIndex)
	{
		FLeafRuntime& Runtime = LeafRuntime[LeafIndex];
		if (Runtime.bActive)
		{
			continue;
		}

		Runtime.bActive = true;
		Runtime.Age = 0.0f;
		Runtime.Lifetime = Random.FRandRange(2.0f, 4.8f);
		Runtime.Phase = Random.FRandRange(0.0f, 2.0f * UE_PI);
		Runtime.Position = Origin + FVector(
			Random.FRandRange(-90.0f, 90.0f),
			Random.FRandRange(-60.0f, 60.0f),
			Random.FRandRange(1.0f, 15.0f));

		const FVector EffectiveDirection =
			ChapterVariant == EIGNeighborhoodChapterVariant::ChapterTwoUncanny &&
			(LeafIndex % 3) == 0
				? -GustDirection
				: GustDirection;
		Runtime.Velocity =
			EffectiveDirection * Random.FRandRange(55.0f, 145.0f) * FMath::Max(0.25f, ImpulseScale) +
			FVector(
				Random.FRandRange(-22.0f, 22.0f),
				Random.FRandRange(-18.0f, 18.0f),
				Random.FRandRange(20.0f, 72.0f));

		UStaticMeshComponent* Leaf = LeafMeshes[LeafIndex];
		Leaf->SetWorldLocation(Runtime.Position);
		Leaf->SetWorldRotation(FRotator(
			Random.FRandRange(-50.0f, 50.0f),
			Random.FRandRange(-180.0f, 180.0f),
			Random.FRandRange(-70.0f, 70.0f)));
		Leaf->SetVisibility(true);
		Leaf->SetHiddenInGame(false);
		--Remaining;
	}
}

void AIGNeighborhoodLifeDirector::UpdateRuntime(const float DeltaSeconds)
{
	if (!GetWorld())
	{
		return;
	}

	FVector ListenerLocation = FVector::ZeroVector;
	TryGetListenerLocation(ListenerLocation);
	UpdateGust(DeltaSeconds);
	UpdateVehicles(DeltaSeconds, ListenerLocation);
	UpdateLeaves(DeltaSeconds, ListenerLocation);
	UpdateCatTrace(DeltaSeconds);
	UpdateWindReactors();
	RefreshRuntimeUpdates();
}

void AIGNeighborhoodLifeDirector::UpdateVehicles(
	const float DeltaSeconds,
	const FVector& ListenerLocation)
{
	for (int32 SlotIndex = 0; SlotIndex < VehicleRuntime.Num(); ++SlotIndex)
	{
		FVehicleRuntime& Runtime = VehicleRuntime[SlotIndex];
		if (!Runtime.bActive)
		{
			continue;
		}

		Runtime.Elapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(Runtime.Elapsed / Runtime.Duration, 0.0f, 1.0f);
		UStaticMeshComponent* Body = VehicleBodies[SlotIndex];
		const FVector Position = FMath::Lerp(Runtime.Start, Runtime.End, Alpha);
		const float BodyCenterHeight =
			Runtime.Kind == EIGPooledVehicleKind::DeliveryMotorcycle ? 22.0f : 38.0f;
		Body->SetWorldLocation(Position + FVector(0.0f, 0.0f, BodyCenterHeight));

		const FVector ListenerToSource = (Position - ListenerLocation).GetSafeNormal();
		const float RadialVelocity = FVector::DotProduct(Runtime.Velocity, ListenerToSource);
		const float PhysicalDoppler = FMath::Clamp(
			1.0f - RadialVelocity / IGNeighborhoodLife::SoundSpeedCentimetersPerSecond,
			0.93f,
			1.08f);
		UAudioComponent* Audio = VehicleAudio[SlotIndex];
		Audio->SetPitchMultiplier(Runtime.BasePitch * PhysicalDoppler);

		float Volume = Runtime.BaseVolume;
		const float EdgeFade = FMath::Min(
			FMath::Clamp(Alpha / 0.12f, 0.0f, 1.0f),
			FMath::Clamp((1.0f - Alpha) / 0.12f, 0.0f, 1.0f));
		Volume *= EdgeFade;
		if (Runtime.bAudioDropsOut && Alpha > 0.52f)
		{
			// CH02's image continues through the frame after its engine vanishes.
			Volume *= FMath::Clamp(1.0f - (Alpha - 0.52f) / 0.08f, 0.0f, 1.0f);
		}
		Audio->SetVolumeMultiplier(Volume);

		if (Alpha >= 1.0f)
		{
			DeactivateVehicle(SlotIndex);
		}
	}
}

void AIGNeighborhoodLifeDirector::UpdateGust(const float DeltaSeconds)
{
	if (GustElapsed < 0.0f)
	{
		return;
	}

	GustElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(GustElapsed / FMath::Max(0.01f, GustDuration), 0.0f, 1.0f);
	const float Envelope = FMath::Sin(Alpha * UE_PI);
	CurrentWindSignal = GustDirection * GustPeakStrength * Envelope;
	if (Alpha >= 1.0f)
	{
		GustElapsed = -1.0f;
		CurrentWindSignal = FVector::ZeroVector;
		OnWindGust.Broadcast(0.0f, GustDirection, 0.0f);
	}
}

void AIGNeighborhoodLifeDirector::UpdateLeaves(
	const float DeltaSeconds,
	const FVector& ListenerLocation)
{
	const float MaxDistanceSquared = FMath::Square(LeafSimulationDistance);
	for (int32 LeafIndex = 0; LeafIndex < LeafRuntime.Num(); ++LeafIndex)
	{
		FLeafRuntime& Runtime = LeafRuntime[LeafIndex];
		if (!Runtime.bActive)
		{
			continue;
		}

		if (FVector::DistSquared2D(Runtime.Position, ListenerLocation) > MaxDistanceSquared)
		{
			DeactivateLeaf(LeafIndex);
			continue;
		}

		Runtime.Age += DeltaSeconds;
		if (Runtime.Age >= Runtime.Lifetime)
		{
			DeactivateLeaf(LeafIndex);
			continue;
		}

		const float Flutter = FMath::Sin(Runtime.Phase + Runtime.Age * 8.0f);
		Runtime.Velocity += CurrentWindSignal * (80.0f * DeltaSeconds);
		Runtime.Velocity.Z += (-35.0f + Flutter * 24.0f) * DeltaSeconds;
		Runtime.Velocity *= FMath::Pow(0.82f, DeltaSeconds);
		Runtime.Position += Runtime.Velocity * DeltaSeconds;
		if (Runtime.Position.Z < 2.0f)
		{
			Runtime.Position.Z = 2.0f;
			Runtime.Velocity.Z = FMath::Abs(Runtime.Velocity.Z) * 0.18f;
			Runtime.Velocity.X *= 0.72f;
			Runtime.Velocity.Y *= 0.72f;
		}

		UStaticMeshComponent* Leaf = LeafMeshes[LeafIndex];
		Leaf->SetWorldLocation(Runtime.Position);
		Leaf->AddWorldRotation(FRotator(
			Flutter * 95.0f * DeltaSeconds,
			135.0f * DeltaSeconds,
			(90.0f + Flutter * 80.0f) * DeltaSeconds));
	}
}

void AIGNeighborhoodLifeDirector::UpdateCatTrace(const float DeltaSeconds)
{
	if (!CatRuntime.bActive)
	{
		return;
	}

	CatRuntime.Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(
		CatRuntime.Elapsed / FMath::Max(0.01f, CatRuntime.Duration), 0.0f, 1.0f);
	// Ease in/out keeps the trace peripheral; a linear grey blob reads like
	// an unfinished enemy pawn.
	const float SmoothedAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	CatTraceRoot->SetWorldLocation(FMath::Lerp(CatRuntime.Start, CatRuntime.End, SmoothedAlpha));
	const float FleetingScale = FMath::Sin(Alpha * UE_PI);
	CatTraceRoot->SetWorldScale3D(FVector(FMath::Max(0.04f, FleetingScale)));
	if (Alpha >= 1.0f)
	{
		DeactivateCatTrace();
	}
}

void AIGNeighborhoodLifeDirector::UpdateWindReactors()
{
	WindReactors.RemoveAllSwap(
		[](const FWindReactorRuntime& Reactor) { return !Reactor.Component.IsValid(); },
		EAllowShrinking::No);

	const float Strength = CurrentWindSignal.Size();
	for (FWindReactorRuntime& Reactor : WindReactors)
	{
		if (USceneComponent* Component = Reactor.Component.Get())
		{
			const float Oscillation =
				FMath::Sin(GustVisualPhase + GustElapsed * 6.3f) *
				Strength * Reactor.ResponseScale;
			const FRotator Offset(
				CurrentWindSignal.X * 2.0f * Reactor.ResponseScale,
				Oscillation * 0.9f,
				CurrentWindSignal.Y * 4.0f * Reactor.ResponseScale + Oscillation * 1.8f);
			Component->SetRelativeRotation(Reactor.NeutralRotation + Offset);
		}
	}
}

void AIGNeighborhoodLifeDirector::DeactivateVehicle(const int32 SlotIndex)
{
	if (!VehicleRuntime.IsValidIndex(SlotIndex))
	{
		return;
	}

	VehicleRuntime[SlotIndex].bActive = false;
	if (VehicleBodies.IsValidIndex(SlotIndex))
	{
		VehicleBodies[SlotIndex]->SetVisibility(false);
		VehicleBodies[SlotIndex]->SetHiddenInGame(true);
	}
	if (VehicleCabins.IsValidIndex(SlotIndex))
	{
		VehicleCabins[SlotIndex]->SetVisibility(false);
		VehicleCabins[SlotIndex]->SetHiddenInGame(true);
	}
	if (VehicleCargoBoxes.IsValidIndex(SlotIndex))
	{
		VehicleCargoBoxes[SlotIndex]->SetVisibility(false);
		VehicleCargoBoxes[SlotIndex]->SetHiddenInGame(true);
	}
	if (VehicleRiderHeads.IsValidIndex(SlotIndex))
	{
		VehicleRiderHeads[SlotIndex]->SetVisibility(false);
		VehicleRiderHeads[SlotIndex]->SetHiddenInGame(true);
	}
	for (int32 WheelIndex = 0; WheelIndex < 4; ++WheelIndex)
	{
		const int32 FlatWheelIndex = SlotIndex * 4 + WheelIndex;
		if (VehicleWheels.IsValidIndex(FlatWheelIndex))
		{
			VehicleWheels[FlatWheelIndex]->SetVisibility(false);
			VehicleWheels[FlatWheelIndex]->SetHiddenInGame(true);
		}
	}
	if (VehicleAudio.IsValidIndex(SlotIndex))
	{
		VehicleAudio[SlotIndex]->Stop();
	}
}

void AIGNeighborhoodLifeDirector::DeactivateLeaf(const int32 LeafIndex)
{
	if (!LeafRuntime.IsValidIndex(LeafIndex))
	{
		return;
	}
	LeafRuntime[LeafIndex].bActive = false;
	if (LeafMeshes.IsValidIndex(LeafIndex))
	{
		LeafMeshes[LeafIndex]->SetVisibility(false);
		LeafMeshes[LeafIndex]->SetHiddenInGame(true);
	}
}

void AIGNeighborhoodLifeDirector::DeactivateCatTrace()
{
	CatRuntime.bActive = false;
	if (CatTraceRoot)
	{
		CatTraceRoot->SetWorldScale3D(FVector::OneVector);
	}
	for (UStaticMeshComponent* Part : CatSilhouetteParts)
	{
		if (Part)
		{
			Part->SetVisibility(false);
			Part->SetHiddenInGame(true);
		}
	}
}

bool AIGNeighborhoodLifeDirector::TryGetListenerLocation(FVector& OutLocation) const
{
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* Controller = World->GetFirstPlayerController())
		{
			if (const APawn* Pawn = Controller->GetPawn())
			{
				OutLocation = Pawn->GetActorLocation();
				return true;
			}
		}
	}
	return false;
}

bool AIGNeighborhoodLifeDirector::IsPlayerNearRoad(const float MaxDistance) const
{
	FVector ListenerLocation;
	if (!TryGetListenerLocation(ListenerLocation))
	{
		return false;
	}
	const FVector Closest = FMath::ClosestPointOnSegment(ListenerLocation, RoadStart, RoadEnd);
	return FVector::DistSquared(ListenerLocation, Closest) <= FMath::Square(MaxDistance);
}

void AIGNeighborhoodLifeDirector::RefreshRuntimeUpdates()
{
	bool bHasActiveWork = GustElapsed >= 0.0f || CatRuntime.bActive;
	bHasActiveWork |= VehicleRuntime.ContainsByPredicate(
		[](const FVehicleRuntime& Runtime) { return Runtime.bActive; });
	bHasActiveWork |= LeafRuntime.ContainsByPredicate(
		[](const FLeafRuntime& Runtime) { return Runtime.bActive; });

	if (!bHasActiveWork)
	{
		UpdateWindReactors();
		SetActorTickEnabled(false);
		return;
	}

	SetActorTickEnabled(true);
}
