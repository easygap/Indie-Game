#include "Entity/IGListenerGreyboxDirector.h"

#include "AssetCompilingManager.h"
#include "IndieGame.h"
#include "Audio/IGAmbienceSoundWave.h"
#include "Audio/IGAudioHelpers.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGSwingDoor.h"
#include "Interaction/IGElevator.h"
#include "Misc/Paths.h"
#include "Player/IGHorrorHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Player/IGPlayerController.h"
#include "Sequence/IGWakeUpDirector.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/IGStressComponent.h"
#include "ShaderCompiler.h"
#include "Sound/SoundWave.h"
#include "UnrealClient.h"
#include "Entity/IGListenerEntity.h"
#include "Entity/IGNightLoopDirector.h"
#include "Entity/IGMissingFloorEpilogueDirector.h"
#include "Entity/IGMissingFloorEvidence.h"
#include "Entity/IGMissingFloorFifthDawnDirector.h"
#include "Entity/IGMissingFloorNightFourDirector.h"
#include "Entity/IGMissingFloorMercyDirector.h"
#include "Entity/IGMissingFloorNightThreeDirector.h"
#include "Entity/IGMissingFloorNightTwoBeatDirector.h"
#include "Entity/IGMissingFloorPuzzleOneDirector.h"
#include "Entity/IGMissingFloorPuzzleTwoDirector.h"
#include "Entity/IGNightOneBeatDirector.h"
#include "Entity/IGNightPhaseDirector.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Environment/IGCctvChannelFive.h"
#include "Environment/IGDustSubsystem.h"
#include "Environment/IGSettledDustComponent.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Player/IGBeamDustComponent.h"
#include "Player/IGFlashlightComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Narrative/IGRecordingSubsystem.h"
#include "Player/IGPlayerCharacter.h"
#include "Save/IGSaveSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

namespace IGListenerGreybox
{
	// 복도를 세운 것과 같은 상수를 쓴다. 이제 주석이 아니라 코드가 그렇다.
	constexpr float FourthFloorZ = AIGPrologueWorldScene::FourthFloorZ;
	constexpr float EntityHalfHeight = 58.0f;
	// 404 냉장고 험. §5.1의 첫 마스킹 주머니다. 자리는 냉장고에게
	// 묻는다 — 좌표를 여기 다시 적으면 냉장고만 옮겨지고 험은 옛
	// 자리에 남는다. 기계 몸통 한가운데 높이만 여기서 정한다.
	constexpr float FridgeHumHeightOffset = 60.0f;
	constexpr float FridgeHumRadius = 200.0f;
	constexpr float FridgeHumMasking = 0.2f;

	// §5.1의 세 번째 험. 복도 동쪽 끝 설비 벽장이고, 자리는 벽장에게 묻는다.
	// 반경과 마스킹은 다른 둘과 같아야 한다 — 기계마다 다르면 플레이어가
	// 배운 「기계 옆이 안전하다」가 기계마다 다른 규칙이 된다.
	constexpr float BoilerHumRadius = 200.0f;
	constexpr float BoilerHumMasking = 0.2f;

	constexpr float SetupRetryIntervalSeconds = 0.3f;
	constexpr float SetupGiveUpSeconds = 6.0f;
	constexpr float ProbePollSeconds = 0.25f;
}

AIGListenerGreyboxDirector::AIGListenerGreyboxDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AIGListenerGreyboxDirector::BeginPlay()
{
	Super::BeginPlay();
	bProductionMode = GetWorld()
		&& (GetWorld()->URL.HasOption(TEXT("IGMissingFloor"))
			|| FParse::Param(FCommandLine::Get(), TEXT("IGMissingFloor")));

	bProbeRequested =
		FParse::Param(FCommandLine::Get(), TEXT("IGListenerGreyboxProbe"));
	bArrivalProbeRequested =
		FParse::Param(FCommandLine::Get(), TEXT("IGArrivalProbe"));
	bArrivalCaptureRequested =
		FParse::Param(FCommandLine::Get(), TEXT("IGArrivalCapture"));
	bNightCaptureRequested =
		FParse::Param(FCommandLine::Get(), TEXT("IGNightCapture"));
	bCaptureMetricsOnly = FParse::Param(FCommandLine::Get(), TEXT("IGCaptureMetricsOnly"));
	bMercyNoteProbeRequested =
		FParse::Param(FCommandLine::Get(), TEXT("IGM65MercyNoteProbe"));
	bHistogramRequested =
		FParse::Param(FCommandLine::Get(), TEXT("IGNightHistogram"));
	bHistogramReportOnly =
		FParse::Param(FCommandLine::Get(), TEXT("IGNightHistogramReport"));
	bCctvFeedProbeRequested =
		FParse::Param(FCommandLine::Get(), TEXT("IGCctvFeedProbe"));

	// The procedural villa and the player pawn appear over the first frames;
	// poll briefly instead of assuming a build order.
	GetWorldTimerManager().SetTimer(
		SetupTimer,
		this,
		&AIGListenerGreyboxDirector::TrySetupStage,
		IGListenerGreybox::SetupRetryIntervalSeconds,
		true);
}

void AIGListenerGreyboxDirector::TrySetupStage()
{
	SetupRetrySeconds += IGListenerGreybox::SetupRetryIntervalSeconds;
	if (SetupStage())
	{
		GetWorldTimerManager().ClearTimer(SetupTimer);
		bStageReady = true;
		// The capture tour, the V5 sweep and the probe are mutually exclusive
		// drivers of the same stage. The two that need real pixels win.
		if (bNightCaptureRequested)
		{
			StartNightCapture();
		}
		else if (bHistogramRequested)
		{
			StartHistogramSweep();
		}
		else if (bArrivalCaptureRequested)
		{
			StartArrivalCapture();
		}
		else if (bArrivalProbeRequested)
		{
			RunArrivalProbe();
		}
		else if (bProbeRequested)
		{
			StartProbe();
		}
		return;
	}
	if (SetupRetrySeconds >= IGListenerGreybox::SetupGiveUpSeconds)
	{
		GetWorldTimerManager().ClearTimer(SetupTimer);
		if (bArrivalCaptureRequested || bArrivalProbeRequested)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("MISSINGFLOOR_ARRIVAL FAIL: stage setup timed out capture=%d"),
				bArrivalCaptureRequested ? 1 : 0);
			RequestExit(true);
		}
		else if (bProbeRequested)
		{
			FailProbe(TEXT("stage setup timed out (world scene or player missing)"));
		}
	}
}

void AIGListenerGreyboxDirector::SpawnNightAmbienceBeds()
{
	UWorld* World = GetWorld();
	AIGPrologueWorldScene* Scene = WorldScene.Get();
	if (!World || !Scene || NightAmbienceBeds.Num() > 0)
	{
		return;
	}
	UIGMissingFloorAudioSubsystem* AudioDirector =
		World->GetSubsystem<UIGMissingFloorAudioSubsystem>();

	// 복도 베드는 조명 기구들의 한가운데. 계단실은 반층 참, 5층은 복도 위 한 층.
	FVector CorridorCenter = FVector::ZeroVector;
	const int32 FixtureCount = Scene->GetCorridorFixtureCount();
	for (int32 Index = 0; Index < FixtureCount; ++Index)
	{
		CorridorCenter += Scene->GetCorridorFixtureLocation(Index);
	}
	if (FixtureCount > 0)
	{
		CorridorCenter /= static_cast<float>(FixtureCount);
	}
	else
	{
		CorridorCenter = FVector(200.0f, -305.0f, IGListenerGreybox::FourthFloorZ + 120.0f);
	}
	struct FBedSpec
	{
		const TCHAR* Name;
		EIGAmbienceMode Mode;
		FVector Location;
		float Volume;
		float InnerRadius;
		float Falloff;
		uint32 Seed;
		const TCHAR* SampleName;
		float SampleVolume;
	};
	const FBedSpec Specs[] = {
		{TEXT("NightBedCorridor"), EIGAmbienceMode::CorridorNight,
			FVector(CorridorCenter.X, CorridorCenter.Y, IGListenerGreybox::FourthFloorZ + 140.0f),
			0.55f, 700.0f, 1900.0f, 0x7A11C0DEu, TEXT("Bed_Corridor"), 0.12f},
		{TEXT("NightBedStairwell"), EIGAmbienceMode::Stairwell,
			FVector(-445.0f, -305.0f, IGListenerGreybox::FourthFloorZ - 30.0f),
			0.60f, 260.0f, 1100.0f, 0x51A1B2C3u, TEXT("Wind_Gap"), 0.055f},
		{TEXT("NightBedUpperFloor"), EIGAmbienceMode::UpperFloor,
			FVector(CorridorCenter.X, CorridorCenter.Y, IGListenerGreybox::FourthFloorZ + 420.0f),
			0.50f, 500.0f, 1500.0f, 0x9C0FFEE1u, TEXT("Bed_Corridor"), 0.065f},
	};
	for (const FBedSpec& Spec : Specs)
	{
		USoundBase* Wave = IGAudio::Sample(Spec.SampleName);
		const bool bRecorded = Wave != nullptr;
		if (!Wave)
		{
			UIGAmbienceSoundWave* Fallback = NewObject<UIGAmbienceSoundWave>(this);
			Fallback->Configure(Spec.Mode, Spec.Seed);
			Wave = Fallback;
		}
		UAudioComponent* Bed = NewObject<UAudioComponent>(this, Spec.Name);
		Bed->RegisterComponent();
		Bed->SetWorldLocation(Spec.Location);
		Bed->SetSound(Wave);
		// 녹음은 합성 베드보다 원음이 크다. 노크의 빈자리를 덮지 않게 섞는다.
		Bed->SetVolumeMultiplier(bRecorded ? Spec.SampleVolume : Spec.Volume);
		Bed->bAutoActivate = false;
		Bed->AttenuationSettings = IGAudio::MakeAttenuation(
			this, Spec.InnerRadius, Spec.Falloff, EIGAudioBus::World);
		Bed->bAllowSpatialization = true;
		if (AudioDirector)
		{
			AudioDirector->PrepareSound(Wave, EIGAudioBus::World);
			AudioDirector->RegisterPersistentBed(Bed, EIGAudioBus::World);
		}
		// 같은 녹음을 층마다 같은 위치에서 재생하면 위아래 소리가 달라붙는다.
		const USoundWave* RecordedWave = Cast<USoundWave>(Wave);
		const float ClipSeconds = RecordedWave ? RecordedWave->Duration : 0.0f;
		const float StartOffset = bRecorded
			? FMath::Fmod(static_cast<float>(Spec.Seed % 1100u) * 0.01f,
				FMath::Max(0.1f, ClipSeconds)) : 0.0f;
		Bed->Play(StartOffset);
		UE_LOG(LogIndieGame, Display, TEXT("NIGHT_BED %s recorded=%d sound=%s volume=%.3f"),
			Spec.Name, bRecorded, *Wave->GetName(), Bed->VolumeMultiplier);
		NightAmbienceBeds.Add(Bed);
	}
}

void AIGListenerGreyboxDirector::ScheduleNextSettle()
{
	// 45~110초. 규칙적이면 시계가 되고 시계는 무섭지 않다.
	const float Delay = FMath::FRandRange(45.0f, 110.0f);
	GetWorldTimerManager().SetTimer(
		SettleTimerHandle, this, &AIGListenerGreyboxDirector::PlaySettleEvent, Delay, false);
}

void AIGListenerGreyboxDirector::PlaySettleEvent()
{
	ScheduleNextSettle();
	UWorld* World = GetWorld();
	AIGPlayerCharacter* PlayerCharacter = Player.Get();
	// 밤에만. 낮에 그는 자고 건물도 잔다.
	if (!World || !PlayerCharacter || !Entity || Entity->IsDormant())
	{
		return;
	}
	if (UIGMissingFloorAudioSubsystem* AudioDirector =
		World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
	{
		if (AudioDirector->IsAuthoredSilence())
		{
			return;
		}
	}
	// 위에서 난다. 훅이 「위에서 나는 소리」다(§10.5). 방위는 매번 다르게.
	const FVector Location = PlayerCharacter->GetActorLocation()
		+ FVector(FMath::FRandRange(-260.0f, 260.0f), FMath::FRandRange(-260.0f, 260.0f), FMath::FRandRange(240.0f, 330.0f));
	USoundBase* Wave = nullptr;
	FText Caption;
	switch (SettleCounter++ % 4)
	{
	case 0:
		Wave = UIGToneSequenceSoundWave::CreateSettlePipeKnock(this);
		Caption = NSLOCTEXT("IGNight", "SettlePipe", "위 — 배관이 튄다");
		break;
	case 1:
		Wave = IGAudio::SampleVariantOr(
			TEXT("Settle_Creak"), 2, static_cast<uint32>(SettleCounter) * 2654435761u,
			[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreateSettleTimberCreak(this); });
		Caption = NSLOCTEXT("IGNight", "SettleCreak", "위 — 나무가 뒤틀린다");
		break;
	case 2:
		Wave = UIGToneSequenceSoundWave::CreateSettlePlasterTick(this);
		Caption = NSLOCTEXT("IGNight", "SettleTick", "위 — 석고가 갈라진다");
		break;
	default:
		Wave = UIGToneSequenceSoundWave::CreateSettleFarDoorSlam(this);
		Caption = NSLOCTEXT("IGNight", "SettleSlam", "멀리 — 문이 닫힌다");
		break;
	}
	IGAudio::SpawnOneShotAt(this, Wave, Location, 0.62f, 1.0f, 180.0f, 1700.0f, EIGAudioBus::World);
	AIGHorrorHUD::PushAudioCaptionAt(this, Caption, 2.0f, Location);
}

void AIGListenerGreyboxDirector::DestroyPartialStage()
{
	// 세우는 순서의 역순일 필요는 없다. 서로를 붙들고 있지 않고, 각자
	// EndPlay에서 자기 것만 정리한다.
	AActor* const Built[] = {
		Entity.Get(), NightLoop.Get(), NightPhase.Get(), PuzzleOne.Get(),
		NightOneBeats.Get(), NightTwoBeats.Get(), PuzzleTwo.Get(),
		NightThree.Get(), Mercy.Get(), FifthDawn.Get(), Epilogue.Get(),
		NightFour.Get(), SleepTarget.Get(), Unit401Door.Get(),
		UsedListingNote.Get(), NeighborhoodDeliveryNote.Get(),
		// SpawnOptionalWitnesses가 세우는 다섯. 지금은 마지막 실패 경로보다
		// 뒤에 있어 새어 나갈 수 없지만, 그 사이에 실패가 하나 생기면 이름이
		// 살아남아 재시도를 막는다.
		WaterBowl.Get(), SleepingPills.Get(), CigarettePack.Get(),
		StoreRoster.Get(), Unit401Radio.Get(),
		// SpawnArrivalInteractables가 세우는 일곱. 그 안의 람다도 이름을
		// 붙여 스폰하므로 남으면 재시도가 같은 이름에 막힌다.
		ArrivalContract.Get(), ArrivalParcelBox.Get(), ArrivalNotebookBox.Get(),
		ArrivalVoicemailBox.Get(), ArrivalStoreBell.Get(),
		ArrivalUnit402Note.Get(), ArrivalRoofLock.Get()};
	for (AActor* Actor : Built)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	Entity = nullptr;
	NightLoop = nullptr;
	NightPhase = nullptr;
	PuzzleOne = nullptr;
	NightOneBeats = nullptr;
	NightTwoBeats = nullptr;
	PuzzleTwo = nullptr;
	NightThree = nullptr;
	Mercy = nullptr;
	FifthDawn = nullptr;
	Epilogue = nullptr;
	NightFour = nullptr;
	SleepTarget = nullptr;
	Unit401Door = nullptr;
	UsedListingNote = nullptr;
	NeighborhoodDeliveryNote = nullptr;
	GetWorldTimerManager().ClearTimer(NeighborhoodSoundTimer);
	WaterBowl = nullptr;
	SleepingPills = nullptr;
	CigarettePack = nullptr;
	StoreRoster = nullptr;
	Unit401Radio = nullptr;
	ArrivalContract = nullptr;
	ArrivalParcelBox = nullptr;
	ArrivalNotebookBox = nullptr;
	ArrivalVoicemailBox = nullptr;
	ArrivalStoreBell = nullptr;
	ArrivalUnit402Note = nullptr;
	ArrivalRoofLock = nullptr;

	// 험은 액터가 아니라 구독이다. 지우지 않으면 시도마다 하나씩 쌓인다.
	if (UWorld* World = GetWorld())
	{
		if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
		{
			for (int32* Handle : {&FridgeHumHandle, &BoilerHumHandle})
			{
				if (*Handle != INDEX_NONE)
				{
					Noise->UnregisterHumSource(*Handle);
				}
			}
		}
	}
	FridgeHumHandle = INDEX_NONE;
	BoilerHumHandle = INDEX_NONE;

	// 밤 베드 셋과 건물 소리 시계. 이름 붙인 컴포넌트라 남기면 재시도의 NewObject가
	// 같은 이름에 막힌다.
	GetWorldTimerManager().ClearTimer(SettleTimerHandle);
	for (UAudioComponent* Bed : NightAmbienceBeds)
	{
		if (IsValid(Bed))
		{
			Bed->Stop();
			Bed->DestroyComponent();
		}
	}
	NightAmbienceBeds.Reset();
	// 소리도 같이 걷는다. 남겨 두면 다음 시도에서 같은 자리에 하나 더 얹혀
	// 마스킹은 그대로인데 소리만 두 배가 된다.
	for (TObjectPtr<UAudioComponent>* Loop : {&FridgeHumLoop, &BoilerHumLoop})
	{
		if (*Loop)
		{
			(*Loop)->Stop();
			(*Loop)->DestroyComponent();
			*Loop = nullptr;
		}
	}
}

bool AIGListenerGreyboxDirector::SetupStage()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 이 함수는 중간 어디서든 false로 빠진다. 빠진 자리까지 세운 것을
	// 남겨 두면 다음 시도가 같은 이름으로 스폰하다 실패한다.
	DestroyPartialStage();

	if (!WorldScene.IsValid())
	{
		for (TActorIterator<AIGPrologueWorldScene> It(World); It; ++It)
		{
			WorldScene = *It;
			break;
		}
	}
	if (!Player.IsValid())
	{
		for (TActorIterator<AIGPlayerCharacter> It(World); It; ++It)
		{
			Player = *It;
			break;
		}
	}
	const AIGPrologueWorldScene* Scene = WorldScene.Get();
	AIGPlayerCharacter* PlayerCharacter = Player.Get();
	if (!Scene || !PlayerCharacter
		|| Scene->GetCorridorFixtureCount() < 2)
	{
		return false;
	}

	NoiseSubsystem = World->GetSubsystem<UIGNoiseSubsystem>();
	if (!NoiseSubsystem)
	{
		return false;
	}

	// Patrol stops ride the corridor light fixtures, west to east: the
	// authored corridor is the route, no coordinates duplicated here.
	const float WalkZ =
		IGListenerGreybox::FourthFloorZ + IGListenerGreybox::EntityHalfHeight + 2.0f;
	TArray<FVector> PatrolPoints;
	for (int32 Index = 0; Index < Scene->GetCorridorFixtureCount(); ++Index)
	{
		const FVector Fixture = Scene->GetCorridorFixtureLocation(Index);
		PatrolPoints.Add(FVector(Fixture.X, Fixture.Y, WalkZ));
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.Name = TEXT("ListenerEntityGreybox");
	Entity = World->SpawnActor<AIGListenerEntity>(
		AIGListenerEntity::StaticClass(),
		FTransform(FRotator::ZeroRotator, PatrolPoints[0]),
		SpawnParameters);
	if (!Entity)
	{
		return false;
	}
	Entity->SetPatrolPoints(PatrolPoints);

	FActorSpawnParameters LoopParameters;
	LoopParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	LoopParameters.Name = TEXT("NightLoopDirectorGreybox");
	NightLoop = World->SpawnActor<AIGNightLoopDirector>(
		AIGNightLoopDirector::StaticClass(),
		FTransform::Identity,
		LoopParameters);
	if (!NightLoop)
	{
		return false;
	}
	NightLoop->RegisterEntity(Entity);
	// The half-past-four bed is wherever the player actually woke.
	NightLoop->SetWakeTransform(PlayerCharacter->GetActorTransform());
	if (bMercyNoteProbeRequested)
	{
		NightLoop->PrimeMercyNoteCaptureProbe();
	}
	ExpectedWakeLocation = PlayerCharacter->GetActorLocation();

	// 자리를 먼저 잡아 두고 마스킹과 소리가 같은 값을 읽는다. 각자 계산하면
	// 한쪽만 옮겨도 조용히 어긋난다.
	const FVector FridgeHumLocation =
		Scene->GetFridgeLocation()
			+ FVector(0.0f, 0.0f, IGListenerGreybox::FridgeHumHeightOffset);
	FridgeHumHandle = NoiseSubsystem->RegisterHumSource(
		FridgeHumLocation,
		IGListenerGreybox::FridgeHumRadius,
		IGListenerGreybox::FridgeHumMasking);
	FridgeHumLoop = IGAudio::SpawnHumLoopAt(
		this,
		TEXT("GreyboxFridgeHum"),
		FridgeHumLocation,
		IGListenerGreybox::FridgeHumRadius);

	const FVector BoilerHumLocation =
		AIGPrologueWorldScene::GetBoilerCupboardLocation();
	BoilerHumHandle = NoiseSubsystem->RegisterHumSource(
		BoilerHumLocation,
		IGListenerGreybox::BoilerHumRadius,
		IGListenerGreybox::BoilerHumMasking);
	BoilerHumLoop = IGAudio::SpawnHumLoopAt(
		this,
		TEXT("GreyboxBoilerHum"),
		BoilerHumLocation,
		IGListenerGreybox::BoilerHumRadius);

	// 험 반경 2m 밖의 복도는 통째로 무음이었다. 복도·계단실·5층에 베드를 깔고
	// 건물이 밤 사이 한 번씩 소리를 내게 한다.
	SpawnNightAmbienceBeds();
	ScheduleNextSettle();

	// A resumed session hands the pursuer back at the impatience it had earned.
	if (const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Entity->SetAggressionTier(Narrative->GetAggressionTier());
	}

	// The hour itself. Capture and demo tours must stay finite walks, so they
	// never get sealed in — the same exemption SpawnReturnBoundary already
	// makes for the CH01 return boundary.
	const bool bCaptureTour =
		FParse::Param(FCommandLine::Get(), TEXT("IGCapture"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGDemo"))
		|| FParse::Param(FCommandLine::Get(), TEXT("IGDemoFrames"));
	if (!bCaptureTour)
	{
		FActorSpawnParameters PhaseParameters;
		PhaseParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PhaseParameters.Name = TEXT("NightPhaseDirectorGreybox");
		NightPhase = World->SpawnActor<AIGNightPhaseDirector>(
			AIGNightPhaseDirector::StaticClass(),
			FTransform::Identity,
			PhaseParameters);
		if (!NightPhase)
		{
			return false;
		}
		NightPhase->Configure(const_cast<AIGPrologueWorldScene*>(Scene), PlayerCharacter);
		// Bind the hour boundary BEFORE the first seal so the initial
		// broadcast reaches every listener this director wires up.
		NightPhase->OnHourActiveChanged.AddUObject(
			this, &AIGListenerGreyboxDirector::HandleHourActiveChanged);
	}

	// P1 lives in the lobby, which the scene has finished building by now.
	FActorSpawnParameters PuzzleParameters;
	PuzzleParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PuzzleParameters.Name = TEXT("MissingFloorPuzzleOneDirector");
	PuzzleOne = World->SpawnActor<AIGMissingFloorPuzzleOneDirector>(
		AIGMissingFloorPuzzleOneDirector::StaticClass(),
		FTransform::Identity,
		PuzzleParameters);
	if (!PuzzleOne
		|| !PuzzleOne->Configure(const_cast<AIGPrologueWorldScene*>(Scene)))
	{
		return false;
	}

	// 밤1 scripted beats: the stair-landing sighting and the extinguisher
	// tutorial. Handed the corridor route so the cameo can put the entity
	// back exactly where the night stage runs it.
	FActorSpawnParameters BeatParameters;
	BeatParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BeatParameters.Name = TEXT("NightOneBeatDirector");
	NightOneBeats = World->SpawnActor<AIGNightOneBeatDirector>(
		AIGNightOneBeatDirector::StaticClass(),
		FTransform::Identity,
		BeatParameters);
	if (!NightOneBeats
		|| !NightOneBeats->Configure(
			const_cast<AIGPrologueWorldScene*>(Scene),
			Entity,
			PlayerCharacter,
			PatrolPoints))
	{
		return false;
	}

	// 밤2 비트 2-1: the knock at 403's own front door. Handed the same corridor
	// route, because the figure at the peephole is the real entity on loan and
	// has to go back to its patrol when the beat lets go of it.
	FActorSpawnParameters NightTwoBeatParameters;
	NightTwoBeatParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	NightTwoBeatParameters.Name = TEXT("MissingFloorNightTwoBeatDirector");
	NightTwoBeats = World->SpawnActor<AIGMissingFloorNightTwoBeatDirector>(
		AIGMissingFloorNightTwoBeatDirector::StaticClass(),
		FTransform::Identity,
		NightTwoBeatParameters);
	if (!NightTwoBeats
		|| !NightTwoBeats->Configure(
			const_cast<AIGPrologueWorldScene*>(Scene),
			Entity,
			PlayerCharacter,
			PatrolPoints))
	{
		return false;
	}

	// 밤2: the management booth and P2. Spawned for every night so free
	// exploration is never fenced off; the narrative, not the walls, decides
	// which night the evidence matters.
	FActorSpawnParameters PuzzleTwoParameters;
	PuzzleTwoParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PuzzleTwoParameters.Name = TEXT("MissingFloorPuzzleTwoDirector");
	PuzzleTwo = World->SpawnActor<AIGMissingFloorPuzzleTwoDirector>(
		AIGMissingFloorPuzzleTwoDirector::StaticClass(),
		FTransform::Identity,
		PuzzleTwoParameters);
	if (!PuzzleTwo
		|| !PuzzleTwo->Configure(const_cast<AIGPrologueWorldScene*>(Scene)))
	{
		return false;
	}

	// 밤3: the gate, the annex, and the two puzzles that end in an answer.
	FActorSpawnParameters NightThreeParameters;
	NightThreeParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	NightThreeParameters.Name = TEXT("MissingFloorNightThreeDirector");
	NightThree = World->SpawnActor<AIGMissingFloorNightThreeDirector>(
		AIGMissingFloorNightThreeDirector::StaticClass(),
		FTransform::Identity,
		NightThreeParameters);
	if (!NightThree
		|| !NightThree->Configure(
			const_cast<AIGPrologueWorldScene*>(Scene),
			Entity,
			PlayerCharacter,
			PatrolPoints))
	{
		return false;
	}

	// §20.3's two automatic safety nets. Spawned after night three so it can be
	// handed the one puzzle whose key wall the entity can be seen listening at.
	FActorSpawnParameters MercyParameters;
	MercyParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MercyParameters.Name = TEXT("MissingFloorMercyDirector");
	Mercy = World->SpawnActor<AIGMissingFloorMercyDirector>(
		AIGMissingFloorMercyDirector::StaticClass(),
		FTransform::Identity,
		MercyParameters);
	if (!Mercy)
	{
		return false;
	}
	Mercy->Configure(Entity, NightThree);

	FActorSpawnParameters FifthDawnParameters;
	FifthDawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FifthDawnParameters.Name = TEXT("MissingFloorFifthDawnDirector");
	FifthDawn = World->SpawnActor<AIGMissingFloorFifthDawnDirector>(
		AIGMissingFloorFifthDawnDirector::StaticClass(),
		FTransform::Identity,
		FifthDawnParameters);
	if (!FifthDawn || !FifthDawn->ValidateTimeline())
	{
		return false;
	}

	FActorSpawnParameters EpilogueParameters;
	EpilogueParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	EpilogueParameters.Name = TEXT("MissingFloorEpilogueDirector");
	Epilogue = World->SpawnActor<AIGMissingFloorEpilogueDirector>(
		AIGMissingFloorEpilogueDirector::StaticClass(),
		FTransform::Identity,
		EpilogueParameters);
	if (!Epilogue || !AIGMissingFloorEpilogueDirector::ValidateTimelines())
	{
		return false;
	}
	Epilogue->OnCompleted.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleEpilogueCompleted);

	// Night 4: the persisted three-control cleaning circuit, five physical
	// wall strikes and the two spatial mourning choices.
	FActorSpawnParameters NightFourParameters;
	NightFourParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	NightFourParameters.Name = TEXT("MissingFloorNightFourDirector");
	NightFour = World->SpawnActor<AIGMissingFloorNightFourDirector>(
		AIGMissingFloorNightFourDirector::StaticClass(),
		FTransform::Identity,
		NightFourParameters);
	if (!NightFour
		|| !NightFour->Configure(const_cast<AIGPrologueWorldScene*>(Scene)))
	{
		return false;
	}

	// Night goals: each puzzle announces itself once; the hour decides
	// whether that ends the night.
	PuzzleOne->OnSolved.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleNightOneSolved);
	PuzzleTwo->OnSolved.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleNightTwoSolved);
	NightTwoBeats->OnReturnedHome.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleNightTwoReturnedHome);
	NightThree->OnSolved.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleNightThreeSolved);
	NightThree->OnReturnedHome.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleNightThreeReturnedHome);
	FifthDawn->OnCompleted.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleFifthDawnCompleted);
	NightFour->SetFifthDawn(FifthDawn);
	NightFour->OnResolved.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleNightFourResolved);

	// Day verbs. The bed advances the cycle; 401's door answers it.
	UStaticMesh* CubeMesh =
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh)
	{
		FActorSpawnParameters DayParameters;
		DayParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		DayParameters.Name = TEXT("MissingFloorSleepTarget");
		SleepTarget = World->SpawnActor<AIGMissingFloorEvidence>(
			AIGMissingFloorEvidence::StaticClass(),
			FTransform(
				FRotator::ZeroRotator,
				FVector(-140.0f, 183.0f, 952.0f)),
			DayParameters);
		if (SleepTarget)
		{
			SleepTarget->Configure(
				CubeMesh,
				nullptr,
				FVector(60.0f, 40.0f, 14.0f),
				NSLOCTEXT("IGMissingFloor", "SleepPrompt", "눕는다"),
				FText::GetEmpty(),
				EIGMissingFloorTruth::None,
				EIGMissingFloorSource::None,
				1.2f,
				0.05f,
			/*bPresentationVisible=*/false);
			SleepTarget->OnExamined.AddUObject(
				this, &AIGListenerGreyboxDirector::HandleSleepRequested);
		}

		DayParameters.Name = TEXT("MissingFloorUnit401Door");
		Unit401Door = World->SpawnActor<AIGMissingFloorEvidence>(
			AIGMissingFloorEvidence::StaticClass(),
			FTransform(
				FRotator::ZeroRotator,
				FVector(-150.0f, -237.0f, 1000.0f)),
			DayParameters);
		if (Unit401Door)
		{
			Unit401Door->Configure(
				CubeMesh,
				nullptr,
				FVector(12.0f, 3.0f, 40.0f),
				NSLOCTEXT("IGMissingFloor", "Unit401Prompt", "401호 — 문을 두드린다"),
				FText::GetEmpty(),
				EIGMissingFloorTruth::None,
				EIGMissingFloorSource::None,
				0.0f,
				0.15f,
				/*bPresentationVisible=*/false);
			Unit401Door->OnExamined.AddUObject(
				this, &AIGListenerGreyboxDirector::HandleUnit401Knocked);
		}

		SpawnOptionalWitnesses(CubeMesh);

	// §13 13행. 낮에 폰으로 보는 글이라 종이가 아니라 알림 화면으로 띄운다.
	// 진실 표에 들어가지 않는다 — 이건 출처가 아니라 심기다.
	{
		FActorSpawnParameters ListingParameters;
		ListingParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ListingParameters.Name = TEXT("MissingFloorUsedListingNote");
		UsedListingNote = World->SpawnActor<AIGReadableNote>(
			AIGReadableNote::StaticClass(),
			FTransform(
				FRotator(0.0f, -12.0f, 0.0f),
				FVector(-64.0f, -180.0f, 974.8f)),
			ListingParameters);
		if (UsedListingNote)
		{
			UMaterialInterface* PhoneMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/Prototype/Materials/M_PlasticDark.M_PlasticDark"));
			UsedListingNote->ConfigurePrototypeVisuals(
				CubeMesh, PhoneMaterial, FVector(7.0f, 14.0f, 1.6f));
			UsedListingNote->SetPhoneNotificationPresentation();
			UsedListingNote->SetInteractionPrompt(
				NSLOCTEXT("IGMissingFloor", "UsedListingPrompt", "휴대전화 — 중고 거래"));
			UsedListingNote->SetNoteText(
				NSLOCTEXT("IGMissingFloor", "UsedListingTitle", "달빛  ·  동네 중고 거래"),
				{
					NSLOCTEXT("IGMissingFloor", "UsedListing1", "피아노 조율 공구 일괄 (튜닝해머 외 11점)"),
					NSLOCTEXT("IGMissingFloor", "UsedListing2", "무영동  ·  직거래만  ·  30,000원"),
					FText::GetEmpty(),
					NSLOCTEXT("IGMissingFloor", "UsedListing3", "「세입자가 두고 간 짐 정리합니다. 상태 좋아요.」"),
					FText::GetEmpty(),
					NSLOCTEXT("IGMissingFloor", "UsedListing4", "작년 8월 12일  ·  거래 완료"),
				});
			UsedListingNote->OnReadStateChanged.AddDynamic(
				this, &AIGListenerGreyboxDirector::HandleUsedListingRead);
		}
	}

	if (bProductionMode)
		{
			SpawnArrivalInteractables(CubeMesh);
		}
	}

	// The initial hour state fired before these actors existed; apply it to
	// them now that they do. Without a night phase (capture tours) everything
	// keeps its natural default and nothing is put to sleep.
	if (NightPhase)
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (bProductionMode && Narrative)
		{
			if (Narrative->GetNightIndex() <= 0)
			{
				HandleHourActiveChanged(false);
				InitializeArrivalSequence();
			}
			else if (Narrative->IsHourSealed())
			{
				NightPhase->ResumeTheHour(
					Narrative->GetNightIndex(),
					Narrative->GetNightElapsedSeconds());
			}
			else
			{
				HandleHourActiveChanged(false);
				if (APlayerController* Controller = World->GetFirstPlayerController())
				{
					if (AIGHorrorHUD* Hud = Cast<AIGHorrorHUD>(Controller->GetHUD()))
					{
						Hud->SetNightPresentation(false);
						Hud->SetObjectiveProvider(NightPhase);
					}
				}
			}
		}
		else
		{
			NightPhase->BeginTheHour(/*NightIndex=*/1);
		}
	}

	UE_LOG(LogTemp, Display,
		TEXT("MISSINGFLOOR_GREYBOX stage ready: %d patrol stops, entity at %s, "
			"hour_sealed=%d"),
		PatrolPoints.Num(),
		*PatrolPoints[0].ToCompactString(),
		Scene->IsTheHourSealed() ? 1 : 0);
	return true;
}

void AIGListenerGreyboxDirector::SpawnOptionalWitnesses(UStaticMesh* CubeMesh)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh)
	{
		return;
	}

	UMaterialInterface* CeramicMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_StainlessUV.M_StainlessUV"));
	UMaterialInterface* PaperMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_PaperClean.M_PaperClean"));
	UMaterialInterface* CardboardMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_PaperOld.M_PaperOld"));

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 뒤편 통로에서도 입주 조사를 시작할 수 있다. 안내문은 본 줄거리의 필수 조건이 아니다.
	Parameters.Name = TEXT("NeighborhoodDeliveryNote");
	NeighborhoodDeliveryNote = World->SpawnActor<AIGReadableNote>(AIGReadableNote::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(2045, -1468.5f, 151)), Parameters);
	if (NeighborhoodDeliveryNote)
	{
		NeighborhoodDeliveryNote->ConfigurePrototypeVisuals(CubeMesh,
			LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Prototype/Materials/M_NeighborhoodDelivery.M_NeighborhoodDelivery")),
			FVector(36, 0.5, 48));
		NeighborhoodDeliveryNote->SetInteractionPrompt(NSLOCTEXT("IGMissingFloor", "DeliveryPrompt", "택배 배송 안내"));
		NeighborhoodDeliveryNote->SetNoteText(NSLOCTEXT("IGMissingFloor", "DeliveryTitle", "택배 기사님께"), {
			NSLOCTEXT("IGMissingFloor", "Delivery1", "달빛빌라 옥탑"),
			NSLOCTEXT("IGMissingFloor", "Delivery2", "부재 시 앞쪽 새벽24 무영로점에 맡겨 주세요."),
			NSLOCTEXT("IGMissingFloor", "Delivery3", "장기 보관은 어렵습니다. 수령인에게 연락 부탁드립니다."),
			NSLOCTEXT("IGMissingFloor", "Delivery4", "달빛빌라 관리실  목한수") });
		NeighborhoodDeliveryNote->OnReadStateChanged.AddDynamic(this, &AIGListenerGreyboxDirector::HandleNeighborhoodDeliveryRead);
	}

	// 401호 문선 서쪽 복도 바닥. 4층 슬래브는 Z=900이고 그릇 높이는 7 cm이라
	// 중심은 903.5다. X는 401호 문선(X -200..-192)과 계단 개구부(X -230까지)
	// 사이의 빈 30 cm에 넣고, Y는 굽도리 앞면(-238.5)에서 1.5 cm 띄운다 —
	// 처음에는 문선을 1 cm 물고 벽 안에 들어가 있었다.
	Parameters.Name = TEXT("MissingFloorWitnessWaterBowl");
	WaterBowl = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(-212.0f, -249.0f, 903.5f)),
		Parameters);
	if (WaterBowl)
	{
		WaterBowl->Configure(
			CubeMesh,
			CeramicMaterial,
			FVector(18.0f, 18.0f, 7.0f),
			NSLOCTEXT("IGMissingFloor", "WitnessBowlPrompt", "물그릇"),
			NSLOCTEXT(
				"IGMissingFloor",
				"WitnessBowlThought",
				"물그릇 밑에 신문지를 깔아 뒀다. 누가 여기서 고양이를 돌보나 보다."),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			0.9f,
			0.04f);
		WaterBowl->OnExamined.AddUObject(
			this, &AIGListenerGreyboxDirector::HandleWaterBowlExamined);
	}

	// 골목 이쪽 보도. 서일영은 Y=-835 건너편에 한 번 서고, 이것은 그가
	// 서 있던 자리가 아니라 지나가며 떨어뜨린 것이다. 노면은 Z=0이고
	// 봉투는 눕혀 4 cm이므로 중심은 2다.
	Parameters.Name = TEXT("MissingFloorWitnessSleepingPills");
	SleepingPills = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator(0.0f, 18.0f, 0.0f), FVector(-40.0f, -400.0f, 2.0f)),
		Parameters);
	if (SleepingPills)
	{
		SleepingPills->Configure(
			CubeMesh,
			PaperMaterial,
			FVector(15.0f, 9.0f, 4.0f),
			NSLOCTEXT("IGMissingFloor", "WitnessPillsPrompt", "떨어진 약봉투"),
			NSLOCTEXT(
				"IGMissingFloor",
				"WitnessPillsThought",
				"같은 병원 약봉지다. 작년 여름 것부터 모아 두셨네."),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			0.9f,
			0.05f);
		SleepingPills->OnExamined.AddUObject(
			this, &AIGListenerGreyboxDirector::HandleSleepingPillsExamined);
	}

	// 옥상 탱크 기초 위. 기초는 (0,-25) 중심에 360×360×100이라 윗면이
	// Z=1300이고 X는 -180..180이다. 탱크 몸통(X -153..153)을 벗어난
	// 동쪽 턱 27 cm 위에 눕힌다 — 앉으면 딱 무릎 옆이 되는 자리다.
	Parameters.Name = TEXT("MissingFloorWitnessCigarettePack");
	CigarettePack = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator(0.0f, -34.0f, 0.0f), FVector(168.0f, -25.0f, 1301.1f)),
		Parameters);
	if (CigarettePack)
	{
		CigarettePack->Configure(
			CubeMesh,
			CardboardMaterial,
			FVector(8.5f, 5.5f, 2.2f),
			NSLOCTEXT("IGMissingFloor", "WitnessPackPrompt", "눌러 끈 담배"),
			NSLOCTEXT(
				"IGMissingFloor",
				"WitnessPackThought",
				"재떨이가 없어서 바닥에 끈 건가. 끝마다 눌린 자국이 있다."),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			0.9f,
			0.04f);
		CigarettePack->OnExamined.AddUObject(
			this, &AIGListenerGreyboxDirector::HandleCigarettePackExamined);
	}

	// 편의점 계산대 상판. 몸통(Z 6..96) 위에 강판 상판이 Z 96..99로 얹혀
	// 있다. 금전등록기(X 2596..2644)와 서쪽 소품(X 2494..2506) 사이의 빈
	// 자리에 클립보드를 눕힌다.
	Parameters.Name = TEXT("MissingFloorWitnessStoreRoster");
	StoreRoster = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator(0.0f, -9.0f, 0.0f), FVector(2548.0f, -193.0f, 99.6f)),
		Parameters);
	if (StoreRoster)
	{
		StoreRoster->Configure(
			CubeMesh,
			PaperMaterial,
			FVector(21.0f, 30.0f, 1.2f),
			NSLOCTEXT("IGMissingFloor", "WitnessRosterPrompt", "야간 근무표"),
			NSLOCTEXT(
				"IGMissingFloor",
				"WitnessRosterThought",
				"나린 씨 이름은 전부 야간에 적혀 있다."),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			0.9f,
			0.04f);
		StoreRoster->OnExamined.AddUObject(
			this, &AIGListenerGreyboxDirector::HandleStoreRosterExamined);
	}

	// 401호 문 너머. 부피만 세우고 그림은 두지 않는다 — 여기 있는 것은
	// 물건이 아니라 소리다. 문 판정(Y -238.5..-235.5)보다 복도 쪽에 둔다.
	Parameters.Name = TEXT("MissingFloorWitnessUnit401Radio");
	Unit401Radio = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(-150.0f, -246.0f, 1010.0f)),
		Parameters);
	if (Unit401Radio)
	{
		Unit401Radio->Configure(
			CubeMesh,
			nullptr,
			FVector(30.0f, 12.0f, 40.0f),
			NSLOCTEXT(
				"IGMissingFloor", "WitnessRadioPrompt", "401호 문 — 귀를 기울인다"),
			FText::GetEmpty(),
			EIGMissingFloorTruth::None,
			EIGMissingFloorSource::None,
			1.0f,
			0.03f,
			/*bPresentationVisible=*/false);
		Unit401Radio->Tags.AddUnique(FName(TEXT("MissingFloor.Verb.Listen")));
		Unit401Radio->OnExamined.AddUObject(
			this, &AIGListenerGreyboxDirector::HandleUnit401RadioExamined);
	}

	// 402호 문 너머. 401호와 같은 오프셋으로 세운다 — 두 문 앞에서 같은
	// 동작을 하고 다른 것을 듣는 것이 이 목격의 전부다.
	Unit402Listen = SpawnListeningVolume(
		CubeMesh,
		TEXT("MissingFloorWitnessUnit402Listen"),
		FVector(-30.0f, -246.0f, 1010.0f),
		FVector(30.0f, 12.0f, 40.0f),
		NSLOCTEXT(
			"IGMissingFloor", "WitnessUnit402Prompt", "402호 문 — 귀를 기울인다"));
	if (Unit402Listen)
	{
		Unit402Listen->OnExamined.AddUObject(
			this, &AIGListenerGreyboxDirector::HandleUnit402ListenExamined);
	}

	// 옥상 철문 안쪽. 계단탑 문(힌지 -320, 220)을 지나 옥상으로 나선 자리다.
	RoofDoorListen = SpawnListeningVolume(
		CubeMesh,
		TEXT("MissingFloorWitnessRoofDoorListen"),
		FVector(-300.0f, 250.0f, 1290.0f),
		FVector(40.0f, 40.0f, 60.0f),
		NSLOCTEXT(
			"IGMissingFloor", "WitnessRoofWindPrompt", "옥상 문 — 바람을 듣는다"));
	if (RoofDoorListen)
	{
		RoofDoorListen->OnExamined.AddUObject(
			this, &AIGListenerGreyboxDirector::HandleRoofDoorListenExamined);
	}
}

AIGMissingFloorEvidence* AIGListenerGreyboxDirector::SpawnListeningVolume(
	UStaticMesh* CubeMesh,
	const TCHAR* ActorName,
	const FVector& Location,
	const FVector& Extent,
	const FText& Prompt)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.Name = ActorName;
	AIGMissingFloorEvidence* Volume = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, Location),
		Parameters);
	if (!Volume)
	{
		return nullptr;
	}
	// 소음 0.03은 귀를 대는 값이다(§5.1의 엿듣기). 그림은 두지 않는다 —
	// 여기 있는 것은 물건이 아니라 소리다.
	Volume->Configure(
		CubeMesh,
		nullptr,
		Extent,
		Prompt,
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		1.0f,
		0.03f,
		/*bPresentationVisible=*/false);
	Volume->Tags.AddUnique(FName(TEXT("MissingFloor.Verb.Listen")));
	return Volume;
}

void AIGListenerGreyboxDirector::HandleStoreRosterExamined(
	AIGMissingFloorEvidence* Evidence)
{
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RecordWitness(EIGMissingFloorWitness::StoreNightRoster);
	}
}

void AIGListenerGreyboxDirector::HandleUnit402ListenExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	Narrative->RecordWitness(EIGMissingFloorWitness::Unit402Silence);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateVacantUnitTone(this),
		Evidence ? Evidence->GetActorLocation() : GetActorLocation(),
		0.58f,
		1.0f,
		90.0f,
		380.0f,
		EIGAudioBus::World);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"WitnessUnit402Caption",
			"[문 안은 조용하다]"),
		2.8f);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"WitnessUnit402Thought",
			"벨이 안 울린다. 문틈에 끼워 둔 우편도 그대로고."),
		4.4f);
}

void AIGListenerGreyboxDirector::HandleRoofDoorListenExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	Narrative->RecordWitness(EIGMissingFloorWitness::RoofDoorWind);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateRoofDoorGust(this),
		Evidence ? Evidence->GetActorLocation() : GetActorLocation(),
		0.66f,
		1.0f,
		150.0f,
		900.0f,
		EIGAudioBus::World);
	AIGHorrorHUD::PushAudioCaption(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"WitnessRoofWindCaption",
			"[바깥] 바람이 느리게 부풀었다 죽는다"),
		3.0f);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"WitnessRoofWindThought",
			"바람은 창틀에서 난다. 어젯밤 천장에서 들은 소리하고는 다르다."),
		4.6f);
}

void AIGListenerGreyboxDirector::HandleUnit401RadioExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	Narrative->RecordWitness(EIGMissingFloorWitness::Unit401DoorRadio);

	// 문 너머의 소리는 이미 §10.3에 있다. 말이 되지 않는 대역이라 무엇을
	// 트는지는 끝까지 알 수 없고, 그것이 이 큐의 요점이다.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateMuffledPrayerRadio(this),
		Evidence ? Evidence->GetActorLocation() : GetActorLocation(),
		0.46f,
		1.0f,
		90.0f,
		420.0f,
		EIGAudioBus::World);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"WitnessRadioThought",
			"문 안에서 라디오가 들린다. 주무시는 줄 알았는데."),
		4.2f);
}

void AIGListenerGreyboxDirector::HandleWaterBowlExamined(
	AIGMissingFloorEvidence* Evidence)
{
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RecordWitness(EIGMissingFloorWitness::HwangWaterBowl);
	}
}

void AIGListenerGreyboxDirector::HandleSleepingPillsExamined(
	AIGMissingFloorEvidence* Evidence)
{
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RecordWitness(EIGMissingFloorWitness::SeoSleepingPills);
	}
}

void AIGListenerGreyboxDirector::HandleCigarettePackExamined(
	AIGMissingFloorEvidence* Evidence)
{
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RecordWitness(EIGMissingFloorWitness::RooftopCigarettePack);
	}
}

void AIGListenerGreyboxDirector::SpawnArrivalInteractables(UStaticMesh* CubeMesh)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh)
	{
		return;
	}

	UMaterialInterface* Paper = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Prototype/Materials/M_PaperClean.M_PaperClean"));
	UMaterialInterface* Contract = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Prototype/Materials/M_ArrivalContract.M_ArrivalContract"));
	UMaterialInterface* Cardboard = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Prototype/Materials/M_MovingBoxCardboardUV.M_MovingBoxCardboardUV"));
	UMaterialInterface* Metal = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Prototype/Materials/M_StainlessUV.M_StainlessUV"));
	UStaticMesh* MovingBoxMesh = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Game/Photo/Props/cardboard_box_01/cardboard_box_01_1k/StaticMeshes/cardboard_box_01_1k.cardboard_box_01_1k"),
		nullptr,
		LOAD_NoWarn);

	auto SpawnEvidence = [this, World, CubeMesh](
		const TCHAR* Name,
		const FVector& Location,
		const FVector& Size,
		UStaticMesh* PresentationMesh,
		UMaterialInterface* Material,
		const FText& Prompt,
		const FText& Thought,
		const float HoldSeconds,
		const float Loudness)
	{
		FActorSpawnParameters Parameters;
		Parameters.Name = FName(Name);
		Parameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AIGMissingFloorEvidence* Evidence =
			World->SpawnActor<AIGMissingFloorEvidence>(
				AIGMissingFloorEvidence::StaticClass(),
				FTransform(FRotator::ZeroRotator, Location),
				Parameters);
		if (Evidence)
		{
			Evidence->Configure(
				PresentationMesh ? PresentationMesh : CubeMesh,
				Material,
				Size,
				Prompt,
				Thought,
				EIGMissingFloorTruth::None,
				EIGMissingFloorSource::None,
				HoldSeconds,
				Loudness);
			Evidence->OnExamined.AddUObject(
				this,
				&AIGListenerGreyboxDirector::HandleArrivalEvidence);
		}
		return Evidence;
	};

	ArrivalContract = SpawnEvidence(
		TEXT("MissingFloorArrivalContract"),
		FVector(-85.0f, -178.0f, 978.0f),
		FVector(21.0f, 29.7f, 0.7f),
		CubeMesh,
		Contract ? Contract : Paper,
		NSLOCTEXT("IGMissingFloor", "ArrivalContractPrompt", "임대차계약서를 확인한다"),
		NSLOCTEXT(
			"IGMissingFloor",
			"ArrivalContractThought",
			"403호. 계약서에 적힌 건 여기까지다. 옥상은 같이 쓴다고 했고."),
		0.7f,
		0.02f);
	ArrivalParcelBox = SpawnEvidence(
		TEXT("MissingFloorArrivalParcelBox"),
		FVector(20.0f, 70.0f, 924.0f),
		FVector(48.0f, 38.0f, 48.0f),
		MovingBoxMesh,
		Cardboard,
		NSLOCTEXT("IGMissingFloor", "ArrivalParcelPrompt", "반송된 소포 상자를 연다"),
		NSLOCTEXT(
			"IGMissingFloor",
			"ArrivalParcelThought",
			"백도하, 달빛빌라 501호. 오빠가 쓰던 주소가 맞다."),
		0.9f,
		0.12f);
	ArrivalNotebookBox = SpawnEvidence(
		TEXT("MissingFloorArrivalNotebookBox"),
		FVector(82.0f, 145.0f, 918.0f),
		FVector(56.0f, 42.0f, 36.0f),
		MovingBoxMesh,
		Cardboard,
		NSLOCTEXT("IGMissingFloor", "ArrivalNotebookPrompt", "악기 상자를 연다"),
		NSLOCTEXT(
			"IGMissingFloor",
			"ArrivalNotebookThought",
			"오빠 수첩이다. 마지막 며칠은 조율 예약 대신 새벽에 들은 소리를 적어 놨다."),
		1.0f,
		0.13f);
	ArrivalVoicemailBox = SpawnEvidence(
		TEXT("MissingFloorArrivalVoicemailBox"),
		FVector(125.0f, 62.0f, 913.0f),
		FVector(42.0f, 34.0f, 26.0f),
		MovingBoxMesh,
		Cardboard,
		NSLOCTEXT("IGMissingFloor", "ArrivalVoicemailPrompt", "휴대전화 상자를 연다"),
		NSLOCTEXT(
			"IGMissingFloor",
			"ArrivalVoicemailThought",
			"마지막으로 온 음성메시지다. 「문 두드리면 알지? 두 번, 쉬고 한 번.」 뒤에서 뭔가 긁힌다."),
		0.8f,
		0.09f);
	ArrivalStoreBell = SpawnEvidence(
		TEXT("MissingFloorArrivalStoreBell"),
		FVector(2690.0f, -216.0f, 101.55f),
		FVector(8.6f, 8.6f, 5.1f),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_ServiceBell.SM_ServiceBell")),
		nullptr,
		NSLOCTEXT("IGMissingFloor", "ArrivalStoreBellPrompt", "계산대 호출벨을 누른다"),
		FText::GetEmpty(),
		0.0f,
		0.10f);
	ArrivalUnit402Note = SpawnEvidence(
		TEXT("MissingFloorArrivalUnit402Note"),
		FVector(-30.0f, -238.0f, 1018.0f),
		FVector(14.8f, 0.6f, 10.5f),
		CubeMesh,
		Paper,
		NSLOCTEXT("IGMissingFloor", "Arrival402Prompt", "402호 문에 붙은 메모를 읽는다"),
		NSLOCTEXT(
			"IGMissingFloor",
			"Arrival402Thought",
			"「밤에 위에서 소리가 나도 올라가지 마세요.」 402호 사람이 붙였나?"),
		0.5f,
		0.01f);
	ArrivalRoofLock = SpawnEvidence(
		TEXT("MissingFloorArrivalRoofLock"),
		FVector(-277.5f, 213.0f, 1300.0f),
		FVector(12.0f, 4.0f, 20.0f),
		CubeMesh,
		Metal,
		NSLOCTEXT("IGMissingFloor", "ArrivalRoofLockPrompt", "옥상 잠금장치를 확인한다"),
		NSLOCTEXT(
			"IGMissingFloor",
			"ArrivalRoofLockThought",
			"자물쇠가 채워져 있다. 옥상은 같이 쓴다더니."),
		0.8f,
		0.08f);
}

void AIGListenerGreyboxDirector::InitializeArrivalSequence()
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return;
	}
	const bool bFirstEntry = Narrative->MarkBeatPlayed(FName(TEXT("Arrival.Started")));
	if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
	{
		PlayerCharacter->SetCameraMotionEnabled(true);
	}
	if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
	{
		if (AIGHorrorHUD* Hud = Cast<AIGHorrorHUD>(Controller->GetHUD()))
		{
			Hud->SetNightPresentation(false);
			Hud->SetObjectiveProvider(this);
		}
	}
	if (bFirstEntry)
	{
		AIGHorrorHUD::ShowChapterCard(
			this,
			NSLOCTEXT("IGMissingFloor", "ArrivalEyebrow", "입주 첫날 · 20:47"),
			NSLOCTEXT("IGMissingFloor", "ArrivalTitle", "없는 층"),
			NSLOCTEXT("IGMissingFloor", "ArrivalSubtitle", "오빠가 마지막으로 보낸 소포의 주소"),
			4.6f);
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateCardboardDrag(this),
			Player.IsValid()
				? Player->GetActorLocation() + FVector(0.0f, 0.0f, 310.0f)
				: FVector(-40.0f, 60.0f, 1280.0f),
			0.58f,
			0.92f,
			90.0f,
			900.0f,
			EIGAudioBus::World);
		AIGHorrorHUD::PushAudioCaption(
			this,
			NSLOCTEXT("IGMissingFloor", "ArrivalDragCaption", "[위층에서 상자를 끄는 소리]"),
			2.7f);
		RequestArrivalAutosave();
	}
	UpdateArrivalSequence();
}

bool AIGListenerGreyboxDirector::AreArrivalBoxesOpened() const
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	return Narrative
		&& Narrative->HasBeatPlayed(FName(TEXT("Arrival.Box.Parcel")))
		&& Narrative->HasBeatPlayed(FName(TEXT("Arrival.Box.Notebook")))
		&& Narrative->HasBeatPlayed(FName(TEXT("Arrival.Box.Voicemail")));
}

void AIGListenerGreyboxDirector::UpdateArrivalSequence()
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || Narrative->GetNightIndex() != 0)
	{
		return;
	}
	const bool bContract = Narrative->HasBeatPlayed(FName(TEXT("Arrival.Contract")));
	const bool bBoxes = AreArrivalBoxesOpened();
	const bool bStore = Narrative->HasBeatPlayed(FName(TEXT("Arrival.Store")));
	const bool bUnit401 = Narrative->HasBeatPlayed(FName(TEXT("Arrival.Unit401")));
	const bool bUnit402 = Narrative->HasBeatPlayed(FName(TEXT("Arrival.Unit402")));
	const bool bRoof = Narrative->HasBeatPlayed(FName(TEXT("Arrival.RoofDoor")));
	if (ArrivalContract)
	{
		ArrivalContract->SetInteractionEnabled(!bContract);
	}
	if (ArrivalParcelBox)
	{
		ArrivalParcelBox->SetInteractionEnabled(
			!Narrative->HasBeatPlayed(FName(TEXT("Arrival.Box.Parcel"))));
	}
	if (ArrivalNotebookBox)
	{
		ArrivalNotebookBox->SetInteractionEnabled(
			!Narrative->HasBeatPlayed(FName(TEXT("Arrival.Box.Notebook"))));
	}
	if (ArrivalVoicemailBox)
	{
		ArrivalVoicemailBox->SetInteractionEnabled(
			!Narrative->HasBeatPlayed(FName(TEXT("Arrival.Box.Voicemail"))));
	}
	if (ArrivalStoreBell)
	{
		ArrivalStoreBell->SetInteractionEnabled(true);
	}
	if (Unit401Door)
	{
		Unit401Door->SetInteractionEnabled(!bUnit401);
	}
	if (ArrivalUnit402Note)
	{
		ArrivalUnit402Note->SetInteractionEnabled(!bUnit402);
	}
	if (ArrivalRoofLock)
	{
		ArrivalRoofLock->SetInteractionEnabled(!bRoof);
	}

	if (bContract && bBoxes && bStore && bUnit401 && bUnit402 && bRoof
		&& Narrative->MarkBeatPlayed(FName(TEXT("Arrival.Complete"))))
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"ArrivalReadyForBed",
				"내일 관리인부터 만나 봐야겠다. 오늘은 좀 자자."),
			3.8f);
		RequestArrivalAutosave();
	}
	if (SleepTarget)
	{
		SleepTarget->SetInteractionEnabled(
			Narrative->HasBeatPlayed(FName(TEXT("Arrival.Complete"))));
	}
}

void AIGListenerGreyboxDirector::HandleArrivalEvidence(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Evidence)
	{
		return;
	}
	if (Evidence == ArrivalStoreBell)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now - LastCounterTalkAt < 7.0) { return; }
		LastCounterTalkAt = Now;
		IGAudio::SpawnOneShotAt(this,
			UIGToneSequenceSoundWave::CreateDoorbellChime(this), Evidence->GetActorLocation(), 0.22f);
		const bool bDeliveryQuestion = Narrative->HasBeatPlayed(FName(TEXT("Neighborhood.Delivery")))
			&& !Narrative->HasBeatPlayed(FName(TEXT("Neighborhood.DeliveryDiscussed")));
		AIGHorrorHUD::PushDialogue(this,
			NSLOCTEXT("IGMissingFloor", "YudamCounterSpeaker", "백유담"),
			bDeliveryQuestion
				? NSLOCTEXT("IGMissingFloor", "NarinDeliveryQuestion", "뒤편 안내문 보고 왔어요. 백도하 이름으로 온 택배도 여기 맡긴 적 있나요?")
				: (Narrative->GetNightIndex() == 0
					? (Narrative->HasBeatPlayed(FName(TEXT("Arrival.Store")))
						? NSLOCTEXT("IGMissingFloor", "NarinRepeatQuestion", "혹시 생각나는 게 더 있으세요?")
						: NSLOCTEXT("IGMissingFloor", "NarinFirstQuestion", "달빛빌라 403호에 이사 왔어요. 백도하라는 사람 아세요? 제 오빠예요."))
					: NSLOCTEXT("IGMissingFloor", "NarinLaterQuestion", "저 왔어요. 혹시 그 뒤로 뭐 들으신 건 없어요?")),
			EIGDialogueChannel::Conversation, 0.0f, EIGDialoguePriority::Story);
		AIGHorrorHUD::PushDialogue(this,
			NSLOCTEXT("IGMissingFloor", "NarinSpeaker", "한나린"),
			GetNarinCounterLine(), EIGDialogueChannel::Conversation, 0.0f, EIGDialoguePriority::Story);
		if (bDeliveryQuestion)
		{
			Narrative->MarkBeatPlayed(FName(TEXT("Neighborhood.DeliveryDiscussed")));
			RequestArrivalAutosave();
		}
	}
	if (Narrative->GetNightIndex() != 0)
	{
		return;
	}
	FName Beat;
	if (Evidence == ArrivalContract)
	{
		Beat = FName(TEXT("Arrival.Contract"));
	}
	else if (Evidence == ArrivalParcelBox)
	{
		Beat = FName(TEXT("Arrival.Box.Parcel"));
	}
	else if (Evidence == ArrivalNotebookBox)
	{
		Beat = FName(TEXT("Arrival.Box.Notebook"));
	}
	else if (Evidence == ArrivalVoicemailBox)
	{
		Beat = FName(TEXT("Arrival.Box.Voicemail"));
	}
	else if (Evidence == ArrivalStoreBell)
	{
		Beat = FName(TEXT("Arrival.Store"));
	}
	else if (Evidence == ArrivalUnit402Note)
	{
		Beat = FName(TEXT("Arrival.Unit402"));
	}
	else if (Evidence == ArrivalRoofLock)
	{
		Beat = FName(TEXT("Arrival.RoofDoor"));
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateHatchOpenMetal(this),
			Evidence->GetActorLocation() + FVector(0.0f, 35.0f, 15.0f),
			0.44f,
			0.72f,
			70.0f,
			1200.0f,
			EIGAudioBus::World);
		AIGHorrorHUD::PushAudioCaption(
			this,
			NSLOCTEXT("IGMissingFloor", "ArrivalHammerCaption", "[문 너머, 둔탁한 망치 소리]"),
			2.5f);
	}
	if (!Beat.IsNone() && Narrative->MarkBeatPlayed(Beat))
	{
		RequestArrivalAutosave();
	}
	UpdateArrivalSequence();
}

void AIGListenerGreyboxDirector::HandleUsedListingRead(
	AIGReadableNote* Note,
	const bool bOpened)
{
	if (!bOpened)
	{
		return;
	}
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative
		|| !Narrative->MarkBeatPlayed(FName(TEXT("Day.UsedListing"))))
	{
		return;
	}
	// 진실을 열지 않는다. 열두 점이 팔렸다는 사실은 5층에 하나만 남은
	// 렌치를 만났을 때 비로소 뜻이 생긴다(§13 13행).
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGMissingFloor",
			"UsedListingThought",
			"오빠 공구랑 같은 모델이다. 작년 여름에 팔렸네."),
		4.4f);
}

void AIGListenerGreyboxDirector::HandleNeighborhoodDeliveryRead(AIGReadableNote* Note, bool bOpened)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative) { return; }
	if (bOpened)
	{
		if (Narrative->MarkBeatPlayed(FName(TEXT("Neighborhood.Delivery")))) { RequestArrivalAutosave(); }
		return;
	}
	if (!Narrative->MarkBeatPlayed(FName(TEXT("Neighborhood.RearDoor")))) { return; }
	RequestArrivalAutosave();
	GetWorldTimerManager().SetTimer(NeighborhoodSoundTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		const FVector Door(1510, -1475, 108);
		if (!Player.IsValid() || FVector::DistSquared(Player->GetActorLocation(), Door) > FMath::Square(1200.f)) { return; }
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/S_Door_Steel_Close.S_Door_Steel_Close"));
		IGAudio::SpawnOneShotAt(this, Sound, Door, 0.36f, 0.87f, 100.f, 1700.f, EIGAudioBus::World);
		AIGHorrorHUD::PushAudioCaptionAt(this,
			NSLOCTEXT("IGMissingFloor", "RearDoorCaption", "[뒤편 건물, 철문 닫히는 소리]"), 2.5f, Door);
	}), 1.4f, false);
}

FText AIGListenerGreyboxDirector::GetNarinCounterLine() const
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	const int32 NightIndex = Narrative ? Narrative->GetNightIndex() : 0;
	if (Narrative && Narrative->HasBeatPlayed(FName(TEXT("Neighborhood.Delivery")))
		&& !Narrative->HasBeatPlayed(FName(TEXT("Neighborhood.DeliveryDiscussed"))))
	{
		return NSLOCTEXT("IGMissingFloor", "NarinDeliveryAnswer",
			"작년 여름에 몇 번 맡겼어요. 마지막 건 건물주 아저씨가 찾아가셨고요. 오빠분이 부탁했다고 하던데요.");
	}


	// 마지막 방문의 대화는 이후 나린이 진술하게 되는 계기다.
	if (NightIndex >= 3
		|| (Narrative && Narrative->HasTruth(EIGMissingFloorTruth::StillCoveringIt)))
	{
		return NSLOCTEXT(
			"IGMissingFloor",
			"NarinLineLastDay",
			"아직은요. 저 오늘도 밤새 있으니까, 무슨 일 있으면 바로 이쪽으로 오세요.");
	}
	if (NightIndex >= 1)
	{
		return NSLOCTEXT(
			"IGMissingFloor",
			"NarinLineMidWeek",
			"아직 소식 없으세요? 저도 단골들한테 물어봤는데, 최근에 봤다는 사람은 없더라고요.");
	}
	if (Narrative && Narrative->HasBeatPlayed(FName(TEXT("Arrival.Store"))))
	{
		return NSLOCTEXT("IGMissingFloor", "NarinRepeatAnswer", "지금은 그 정도예요. 다른 손님들한테도 한번 물어볼게요.");
	}
	// 이전 세입자들도 오래 살지 못했다는 사실만 먼저 알린다.
	return NSLOCTEXT(
		"IGMissingFloor",
		"ArrivalNarinLine",
		"아, 늘 큰 가방 들고 오시던 분요? 한동안 안 보이시던데. 403호는 올해 벌써 세 번째 이사네요.");
}

void AIGListenerGreyboxDirector::RequestArrivalAutosave()
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GetWorld();
	UIGSaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UIGSaveSubsystem>()
		: nullptr;
	if (SaveSubsystem && World)
	{
		SaveSubsystem->RequestAutosave(
			FGameplayTag::RequestGameplayTag(FName(TEXT("Chapter.MissingFloor")), false),
			World->GetOutermost()->GetFName(),
			FGameplayTag::RequestGameplayTag(
				FName(TEXT("Checkpoint.MissingFloor.Arrival")),
				false));
	}
}

FText AIGListenerGreyboxDirector::GetObjectiveText() const
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!bProductionMode || !Narrative || Narrative->GetNightIndex() != 0)
	{
		return FText::GetEmpty();
	}
	if (GetObjectiveProgress() < 0.5f)
	{
		return NSLOCTEXT("IGMissingFloor", "ArrivalObjectiveExplore", "짐을 풀고, 빌라와 편의점을 둘러본다");
	}
	if (!Narrative->HasBeatPlayed(FName(TEXT("Arrival.Contract"))))
	{
		return NSLOCTEXT("IGMissingFloor", "ArrivalObjectiveContract", "책상 위 임대차계약서를 확인한다");
	}
	if (!AreArrivalBoxesOpened())
	{
		return NSLOCTEXT("IGMissingFloor", "ArrivalObjectiveBoxes", "이삿짐 상자 세 개를 확인한다");
	}
	if (!Narrative->HasBeatPlayed(FName(TEXT("Arrival.Store"))))
	{
		return NSLOCTEXT("IGMissingFloor", "ArrivalObjectiveStore", "골목 편의점에서 이 건물 이야기를 묻는다");
	}
	if (!Narrative->HasBeatPlayed(FName(TEXT("Arrival.Unit401")))
		|| !Narrative->HasBeatPlayed(FName(TEXT("Arrival.Unit402"))))
	{
		return NSLOCTEXT("IGMissingFloor", "ArrivalObjectiveNeighbors", "401호와 402호 앞을 확인한다");
	}
	if (!Narrative->HasBeatPlayed(FName(TEXT("Arrival.RoofDoor"))))
	{
		return NSLOCTEXT("IGMissingFloor", "ArrivalObjectiveRoof", "계약서에 적힌 옥상 출입문을 확인한다");
	}
	return NSLOCTEXT("IGMissingFloor", "ArrivalObjectiveSleep", "403호로 돌아가 잠든다");
}

FString AIGListenerGreyboxDirector::GetObjectiveTextAscii() const
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!bProductionMode || !Narrative || Narrative->GetNightIndex() != 0)
	{
		return FString();
	}
	if (GetObjectiveProgress() < 0.5f) return TEXT("UNPACK AND EXPLORE THE NEIGHBORHOOD");
	if (!Narrative->HasBeatPlayed(FName(TEXT("Arrival.Contract")))) return TEXT("CHECK THE RENTAL CONTRACT");
	if (!AreArrivalBoxesOpened()) return TEXT("OPEN ALL THREE MOVING BOXES");
	if (!Narrative->HasBeatPlayed(FName(TEXT("Arrival.Store")))) return TEXT("ASK AT THE CONVENIENCE STORE");
	if (!Narrative->HasBeatPlayed(FName(TEXT("Arrival.Unit401")))
		|| !Narrative->HasBeatPlayed(FName(TEXT("Arrival.Unit402")))) return TEXT("CHECK UNITS 401 AND 402");
	if (!Narrative->HasBeatPlayed(FName(TEXT("Arrival.RoofDoor")))) return TEXT("CHECK THE ROOFTOP ACCESS DOOR");
	return TEXT("RETURN TO UNIT 403 AND SLEEP");
}

float AIGListenerGreyboxDirector::GetObjectiveProgress() const
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!bProductionMode || !Narrative || Narrative->GetNightIndex() != 0)
	{
		return 0.0f;
	}
	int32 Completed = 0;
	for (const TCHAR* Beat : {
		TEXT("Arrival.Contract"), TEXT("Arrival.Box.Parcel"),
		TEXT("Arrival.Box.Notebook"), TEXT("Arrival.Box.Voicemail"),
		TEXT("Arrival.Store"), TEXT("Arrival.Unit401"),
		TEXT("Arrival.Unit402"), TEXT("Arrival.RoofDoor")})
	{
		Completed += Narrative->HasBeatPlayed(FName(Beat)) ? 1 : 0;
	}
	return static_cast<float>(Completed) / 8.0f;
}

UIGMissingFloorNarrativeSubsystem* AIGListenerGreyboxDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}

void AIGListenerGreyboxDirector::HandleHourActiveChanged(const bool bActive)
{
	if (!bActive)
	{
		// 05:30 on any route: the goal, the timeout, or the walk home.
		MakeNightThreeFirstReport();
	}
	// One boundary, every consequence, in one place: the entity sleeps by
	// day, the booth locks by day, and the day verbs vanish by night.
	if (Entity)
	{
		Entity->SetDormant(!bActive);
	}
	if (PuzzleOne)
	{
		PuzzleOne->SetHourActive(bActive);
	}
	if (NightTwoBeats)
	{
		NightTwoBeats->SetHourActive(bActive);
	}
	if (PuzzleTwo)
	{
		PuzzleTwo->SetHourActive(bActive);
	}
	if (NightThree)
	{
		NightThree->SetHourActive(bActive);
	}
	if (NightFour)
	{
		NightFour->SetHourActive(bActive);
	}
	if (Mercy)
	{
		Mercy->SetHourActive(bActive);
	}
	if (SleepTarget)
	{
		SleepTarget->SetInteractionEnabled(!bActive);
	}
	if (Unit401Door)
	{
		Unit401Door->SetInteractionEnabled(!bActive);
	}
	if (ArrivalStoreBell)
	{
		// 프롤로그가 끝난 뒤에도 편의점은 낮마다 열려 있다. 입주 시퀀스가
		// 밤 0에서만 돌기 때문에 그 뒤로는 아무도 이 문을 다시 켜 주지
		// 않았고, 나린은 첫날 한 마디만 하고 사라진 사람이 되어 있었다.
		const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (Narrative && Narrative->GetNightIndex() >= 1)
		{
			ArrivalStoreBell->SetInteractionEnabled(!bActive);
		}
	}
}

void AIGListenerGreyboxDirector::HandleNightOneSolved()
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (NightPhase && Narrative && Narrative->GetNightIndex() == 1)
	{
		NightPhase->CompleteNightGoal();
	}
}

void AIGListenerGreyboxDirector::HandleNightTwoSolved()
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!NightPhase || !Narrative || Narrative->GetNightIndex() != 2)
	{
		return;
	}
	// §8 밤2 does not end where the paper contradicts itself; it ends at 403's
	// door. T7 arms 비트 2-5 instead of releasing her to dawn from inside the
	// booth, and the walk home decides the night.
	if (NightTwoBeats)
	{
		NightTwoBeats->ArmReturnChase();
		return;
	}
	// No beat director: complete rather than trap her in a night with no exit.
	NightPhase->CompleteNightGoal();
}

void AIGListenerGreyboxDirector::HandleNightTwoReturnedHome()
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (NightPhase && Narrative && Narrative->GetNightIndex() == 2)
	{
		NightPhase->CompleteNightGoal();
	}
}

void AIGListenerGreyboxDirector::HandleNightThreeSolved()
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!NightPhase || !Narrative || Narrative->GetNightIndex() != 3)
	{
		return;
	}
	// §8 밤3 does not end at the wall. T9 arms 비트 3-7 and he is standing in
	// the corridor between the stair core and her door; the walk past him is the
	// rest of the night.
	if (NightThree)
	{
		NightThree->ArmReturnPass();
		return;
	}
	// No director: complete rather than trap her on a floor with no exit.
	MakeNightThreeFirstReport();
	NightPhase->CompleteNightGoal();
}

void AIGListenerGreyboxDirector::HandleNightThreeReturnedHome()
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (NightPhase && Narrative && Narrative->GetNightIndex() == 3)
	{
		NightPhase->CompleteNightGoal();
	}
}

void AIGListenerGreyboxDirector::MakeNightThreeFirstReport()
{
	// The call happens as soon as signal returns at 05:30, so it belongs to dawn
	// rather than to arriving home — a player who runs the hour out instead of
	// getting past him still reported a voice behind a wall. Night 4 may disobey
	// a scene-preservation warning, but it never exists because the protagonist
	// simply forgot to make this call.
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative
		|| Narrative->GetNightIndex() != 3
		|| Narrative->WasFirstReportMade())
	{
		return;
	}
	Narrative->SetFirstReportMade(true);
	Narrative->MarkBeatPlayed(FName(TEXT("Night3.FirstReport")));
	// 신고는 불리언이 아니다. 새벽 독백이 지나간 뒤 폰이 한 번 울리고 접수
	// 문자가 온다 — 밤4의 두 번째 신고와 엔딩 A의 근거가 이 한 줄이다.
	if (!bProbeRequested)
	{
		GetWorldTimerManager().SetTimer(
			ReportTimer,
			this,
			&AIGListenerGreyboxDirector::PlayFirstReportReceipt,
			5.5f,
			false);
	}
}

void AIGListenerGreyboxDirector::PlayFirstReportReceipt()
{
	if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
	{
		IGAudio::SpawnOneShotAt(
			this,
			IGAudio::SampleOr(
				TEXT("Phone_Vibrate"),
				[this]() -> USoundBase* { return UIGToneSequenceSoundWave::CreatePhoneVibrationUnfinished(this); }),
			PlayerCharacter->GetActorLocation(),
			0.5f,
			1.0f,
			60.0f,
			400.0f,
			EIGAudioBus::Player);
	}
	AIGHorrorHUD::PushDialogue(
		this,
		NSLOCTEXT("IGMissingFloor", "ReportSpeaker", "112"),
		NSLOCTEXT(
			"IGMissingFloor",
			"FirstReportReceipt",
			"[문자신고 접수] 접수되었습니다. 담당자 확인 후 연락드리겠습니다."),
		EIGDialogueChannel::Device,
		0.0f,
		EIGDialoguePriority::Story);
}

void AIGListenerGreyboxDirector::HandleNightFourResolved()
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!NightPhase || !Narrative || Narrative->GetNightIndex() != 4)
	{
		return;
	}
	// 성공 엔딩 A/B는 같은 세계의 발견 사실로 돌아온다. C는 신고하거나
	// 새벽에 건물을 여는 대신 시간을 멈추고 밤 4 재시도를 제공하므로
	// 이 완료 경로에서 의도적으로 제외한다.
	Narrative->SetSecondReportMade(true);
	Narrative->MarkBeatPlayed(FName(TEXT("Night4.SecondReport")));
	// 시간을 먼저 푼다. 에필로그는 87초 동안 화면과 이동을 가져가므로,
	// 그 사이에 추격이 살아 있으면 애도 장면 뒤에서 포획이 일어난다.
	NightPhase->SuppressNextMorningPresentation();
	NightPhase->CompleteNightGoal();

	if (bProbeRequested)
	{
		StartEpilogueAfterGesture();
		return;
	}
	// 렌치가 제자리에 놓이는 0.85초와 그 독백 한 줄은 카메라가 살아 있을 때
	// 봐야 한다. 같은 프레임에 에필로그가 화면을 검게 칠하면 선택은 했는데
	// 한 것을 못 본다.
	GetWorldTimerManager().SetTimer(
		EpilogueStartTimer,
		this,
		&AIGListenerGreyboxDirector::StartEpilogueAfterGesture,
		3.4f,
		false);
}

void AIGListenerGreyboxDirector::StartEpilogueAfterGesture()
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (Epilogue && Narrative)
	{
		Epilogue->StartEpilogue(Player.Get(), Narrative->GetEndingChoice());
	}
}

void AIGListenerGreyboxDirector::HandleEpilogueCompleted()
{
	AIGPlayerCharacter* PlayerCharacter = Player.Get();
	APlayerController* Controller = PlayerCharacter
		? Cast<APlayerController>(PlayerCharacter->GetController())
		: nullptr;
	if (!Controller)
	{
		if (UWorld* World = GetWorld())
		{
			Controller = World->GetFirstPlayerController();
		}
	}
	if (AIGPlayerController* IGController = Cast<AIGPlayerController>(Controller))
	{
		// 타이틀이 자기 배경을 그리므로 암전은 여기서 걷는다. 이동 잠금은
		// 남겨 둔다 — 뒤에서 걸어 다니는 사람이 있으면 타이틀이 아니다.
		if (IGController->PlayerCameraManager)
		{
			IGController->PlayerCameraManager->StopCameraFade();
		}
		IGController->ShowTitleAfterEnding();
	}
}

void AIGListenerGreyboxDirector::HandleFifthDawnCompleted()
{
	// 막간은 밤4의 벽 안에서 돈다(§8 막간, 2026-09-10). 여기서는 밤4에
	// 넘겨줄 뿐이다 — 밤3의 잠자리는 보통 밤처럼 밤4로 간다.
	if (NightFour)
	{
		NightFour->HandleInterludeCompleted();
	}
}

void AIGListenerGreyboxDirector::HandleSleepRequested(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!NightPhase || !Narrative || NightPhase->IsHourActive()
		|| GetWorldTimerManager().IsTimerActive(NightStartTimer))
	{
		return;
	}
	if (bProductionMode && Narrative->GetNightIndex() == 0)
	{
		if (!Narrative->HasBeatPlayed(FName(TEXT("Arrival.Complete"))))
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT("IGMissingFloor", "ArrivalSleepBlocked", "아직 확인할 게 남았다."),
				2.6f);
			return;
		}
		Narrative->MarkBeatPlayed(FName(TEXT("Arrival.Slept")));
		BeginNightAfterSleep(1);
		return;
	}
	if (Narrative->GetNightIndex() == 3
		&& !Narrative->WasFifthDawnInterludeCompleted()
		&& bProbeRequested)
	{
		// 계약 연습. 막간 자체는 밤4의 벽 안에서 돈다(§8 막간). 프로브는
		// 여기서 한 번 돌려 시각표·입력·즉시 종료를 확인하고 완료 표시를
		// 남긴다 — 그래서 밤4의 리빌은 프로브에서 막간을 건너뛴다.
		if (!FifthDawn
			|| !FifthDawn->ValidateTimeline()
			|| !FifthDawn->StartInterlude(Player.Get())
			|| !FifthDawn->RegisterPlayerKnock()
			|| !FifthDawn->SetPlayerListening(true)
			|| !FifthDawn->SetPlayerListening(false)
			|| !FifthDawn->CompleteImmediatelyForProbe())
		{
			FailProbe(TEXT("fifth-dawn start/input/finish contract failed"));
			return;
		}
	}
	const int32 CurrentNight = Narrative->GetNightIndex();
	int32 NextNight = FMath::Clamp(CurrentNight + 1, 1, 4);
	const bool bGoalMet = CurrentNight < 1
		|| Narrative->HasBeatPlayed(AIGNightPhaseDirector::GoalBeatId(CurrentNight));
	if (!bGoalMet)
	{
		// 못 채운 밤은 다음 저녁에 같은 밤이 온다. 그는 한 티어 더 급하다 —
		// 잡히는 값이 죽음이 아니라 시간이려면 시간이 정말로 사라져야 한다(§5.4).
		// 밤4의 05:30은 여기 오지 않는다. 벽이 닫힌 새벽은 엔딩 C가 갖는다.
		NextNight = CurrentNight;
		Narrative->SetAggressionTier(Narrative->GetAggressionTier() + 1);
	}
	BeginNightAfterSleep(NextNight);
}

void AIGListenerGreyboxDirector::BeginNightAfterSleep(const int32 NightIndex)
{
	PendingNightIndex = NightIndex;
	if (bProbeRequested)
	{
		// 프로브는 잠든 프레임에 밤이 서 있어야 한다.
		WakeIntoNight();
		return;
	}
	// 눕는 순간 눈을 감긴다. 눕고 나서도 서 있던 자리에 그대로 서서 카드를
	// 보는 것은 잠이 아니라 로딩이었다. 눈을 뜨면 침대이고 알람이 울린다.
	AIGPlayerCharacter* PlayerCharacter = Player.Get();
	APlayerController* Controller = PlayerCharacter
		? Cast<APlayerController>(PlayerCharacter->GetController())
		: nullptr;
	if (Controller && Controller->PlayerCameraManager)
	{
		Controller->PlayerCameraManager->StartCameraFade(
			0.0f,
			1.0f,
			0.6f,
			FLinearColor::Black,
			/*bShouldFadeAudio=*/false,
			/*bHoldWhenFinished=*/true);
	}
	GetWorldTimerManager().SetTimer(
		NightStartTimer,
		this,
		&AIGListenerGreyboxDirector::WakeIntoNight,
		0.75f,
		false);
}

void AIGListenerGreyboxDirector::WakeIntoNight()
{
	if (!NightPhase)
	{
		return;
	}
	AIGPlayerCharacter* PlayerCharacter = Player.Get();
	APlayerController* Controller = PlayerCharacter
		? Cast<APlayerController>(PlayerCharacter->GetController())
		: nullptr;
	if (!bProbeRequested && PlayerCharacter && NightLoop
		&& NightLoop->HasWakeTransform())
	{
		// 포획 뒤에 깨는 자리와 같은 자리다. 밤은 언제나 침대에서 시작한다.
		const FTransform& Wake = NightLoop->GetWakeTransform();
		PlayerCharacter->TeleportTo(
			Wake.GetLocation(), Wake.Rotator(), false, true);
		if (UCharacterMovementComponent* Movement =
			PlayerCharacter->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
		if (Controller)
		{
			Controller->SetControlRotation(Wake.Rotator());
		}
	}
	NightPhase->BeginTheHour(PendingNightIndex);
	const FVector At = PlayerCharacter
		? PlayerCharacter->GetActorLocation()
		: GetActorLocation();
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateAlarmFirstNote(this),
		At,
		0.58f,
		0.94f,
		80.0f,
		650.0f,
		EIGAudioBus::Player);
	AIGHorrorHUD::PushAudioCaption(
		this,
		PendingNightIndex == 1
			? NSLOCTEXT("IGMissingFloor", "ArrivalAlarmCaption", "[04:30 알람 — 위층에서 세 번 두드린다]")
			: NSLOCTEXT("IGMissingFloor", "NightAlarmCaption", "[04:30 알람]"),
		2.7f);
	if (bProbeRequested)
	{
		return;
	}
	if (Controller && Controller->PlayerCameraManager)
	{
		Controller->PlayerCameraManager->StartCameraFade(
			1.0f,
			0.0f,
			1.4f,
			FLinearColor::Black,
			/*bShouldFadeAudio=*/false,
			/*bHoldWhenFinished=*/false);
	}
	if (PendingNightIndex == 1)
	{
		// 첫 밤에만. 이 게임에서 손전등이 있다는 것을 배우는 자리는 여기 하나다.
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"NightTorchThought",
				"손전등은 머리맡에 꺼내 뒀다."),
			4.5f);
	}
	// 밤은 방향으로 시작한다. 밤2만 여섯 초 뒤에 문을 두드렸고 나머지는
	// 카드 뒤에 아무것도 없었다.
	GetWorldTimerManager().SetTimer(
		NightSettleTimer,
		this,
		&AIGListenerGreyboxDirector::PlayNightOpeningSettle,
		6.0f,
		false);
	if (PendingNightIndex == 1)
	{
		// 밤1, 위에서 누가 일한다. 옥상 탱크 매니폴드에서 전동 드릴이 다섯 번
		// 돌다 멈춘다 — 목한수의 첫 흔적. 밤4의 한 마디를 두 밤의 노동으로 번다.
		GetWorldTimerManager().SetTimer(
			RoofDriverTimer,
			this,
			&AIGListenerGreyboxDirector::PlayRoofDriverBeat,
			42.0f,
			false);
	}
}

void AIGListenerGreyboxDirector::PlayRoofDriverBeat()
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!NightPhase || !NightPhase->IsHourActive() || !Narrative
		|| !Narrative->MarkBeatPlayed(FName(TEXT("Night1.RoofDriver"))))
	{
		return;
	}
	// 옥상 매니폴드(5, 140, 1300) 위. 4층 복도에서 3미터 위다.
	const FVector RoofManifold(5.0f, 140.0f, 1330.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateCordlessDriverRun(this),
		RoofManifold,
		0.7f,
		1.0f,
		400.0f,
		4200.0f,
		EIGAudioBus::World);
	AIGHorrorHUD::PushAudioCaptionAt(
		this,
		NSLOCTEXT("IGMissingFloor", "RoofDriverCaption", "전동 드릴. 다섯 번 돌다 멈춘다"),
		3.0f,
		RoofManifold);
}

void AIGListenerGreyboxDirector::PlayNightOpeningSettle()
{
	if (!NightPhase || !NightPhase->IsHourActive())
	{
		return;
	}
	// 플레이어 기준이 아니라 403호 천장의 정해진 자리다. 걸어가 볼 수 있는
	// 소리라야 장소가 된다.
	const FVector Bed = NightLoop && NightLoop->HasWakeTransform()
		? NightLoop->GetWakeTransform().GetLocation()
		: (Player.IsValid() ? Player->GetActorLocation() : GetActorLocation());
	const FVector Above = Bed + FVector(40.0f, 0.0f, 300.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateSettleTimberCreak(this),
		Above,
		0.62f,
		1.0f,
		160.0f,
		1400.0f,
		EIGAudioBus::World);
	AIGHorrorHUD::PushAudioCaptionAt(
		this,
		NSLOCTEXT("IGMissingFloor", "NightOpeningCreak", "나무가 뒤틀린다"),
		2.2f,
		Above);
}

FText AIGListenerGreyboxDirector::GetHwangDoorLine() const
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative)
	{
		return NSLOCTEXT(
			"IGMissingFloor",
			"Hwang401Greeting",
			"403호 새로 왔어요? 신발장 문은 살살 닫아 줘요. 밤에는 여기 다 울려.");
	}
	// §7 난이도 보정의 낮 1단계. 퍼즐마다 한 줄씩, 답이 아니라 어디를 볼지만.
	// 가장 앞선 상태부터 본다 — 뒤의 밤에 앞의 밤 힌트를 되풀이하지 않도록.
	const int32 NightIndex = Narrative->GetNightIndex();
	const bool bAlive = Narrative->HasTruth(EIGMissingFloorTruth::WasStillAlive);
	const bool bInWall = Narrative->HasTruth(EIGMissingFloorTruth::SomeoneInTheWall);
	const bool bAnswered = Narrative->HasTruth(EIGMissingFloorTruth::WaitingForAnAnswer);
	if (bAnswered && !bAlive)
	{
		// 대답은 받았는데 날짜를 안 닫았다. 밤4의 망치는 T7 없이 안 열린다.
		return NSLOCTEXT(
			"IGMissingFloor",
			"Hwang401HintT7",
			"부동산에서 짐 뺐다고 연락한 날이 있을 거예요. 관리인이 문자까지 인쇄해 두더라고.");
	}
	if (bAnswered && !Narrative->IsNightFourWallOpened())
	{
		return NSLOCTEXT(
			"IGMissingFloor",
			"Hwang401HintP5",
			"내일 아침 공사한대요. 또 막기 전에 안에 뭐가 있나 봐야 할 것 아니에요.");
	}
	if (bInWall && !bAnswered)
	{
		return NSLOCTEXT(
			"IGMissingFloor",
			"Hwang401HintP4",
			"벽 두드렸더니 한 번 대답이 왔어요. 사람이 있나 싶어서 다시 했는데, 그 뒤로는 조용하고.");
	}
	if (!bInWall && NightIndex >= 2
		&& Narrative->IsPuzzleSolved(FName(TEXT("P2"))))
	{
		return NSLOCTEXT(
			"IGMissingFloor",
			"Hwang401HintP3",
			"관리인 없어요? 열쇠는 늘 책상 옆에 걸어 둬요. 못 들어가게 하면 나한테 말해요.");
	}
	if (bAlive)
	{
		return NSLOCTEXT(
			"IGMissingFloor",
			"Hwang401AfterT7",
			"그날도 들었어요. 사람이 없다는데, 나는 분명히 들었다니까.");
	}
	if (NightIndex >= 2)
	{
		return NSLOCTEXT(
			"IGMissingFloor",
			"Hwang401HintP2",
			"소리 난다고 전화한 게 몇 번인데, 배관이라고만 해요. 대장에 뭐라고 썼는지 좀 봐 줘요.");
	}
	if (NightIndex == 1
		&& !Narrative->HasTruth(EIGMissingFloorTruth::LivedUpstairs))
	{
		return NSLOCTEXT(
			"IGMissingFloor",
			"Hwang401HintP1",
			"전기 나갔으면 1층 계량기함 봐요. 검침표도 거기 붙어 있어요.");
	}
	if (NightIndex >= 1)
	{
		return NSLOCTEXT(
			"IGMissingFloor",
			"Hwang401Neutral",
			"잠은 좀 잤어요? 물이라도 한 잔 마시고 가요.");
	}
	return NSLOCTEXT(
		"IGMissingFloor",
		"Hwang401Greeting",
		"403호 새로 왔어요? 신발장 문은 살살 닫아 줘요. 밤에는 여기 다 울려.");
}

void AIGListenerGreyboxDirector::HandleUnit401Knocked(
	AIGMissingFloorEvidence* Evidence)
{
	// Two soft knocks from the player's side of 401. She answers through the
	// door — the daytime hint channel the puzzles lean on (§7 난이도 보정).
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWallKnockReply(this),
		Evidence ? Evidence->GetActorLocation() : GetActorLocation(),
		0.7f);

	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	const FText Speaker =
		NSLOCTEXT("IGMissingFloor", "HwangSpeaker", "황순금");
	AIGHorrorHUD::PushDialogue(
		this,
		Speaker,
		GetHwangDoorLine(),
		EIGDialogueChannel::Conversation,
		0.0f,
		EIGDialoguePriority::Story);

	if (bProductionMode && Narrative && Narrative->GetNightIndex() == 0
		&& Narrative->MarkBeatPlayed(FName(TEXT("Arrival.Unit401"))))
	{
		RequestArrivalAutosave();
		UpdateArrivalSequence();
	}
}

// -- probe -----------------------------------------------------------------

void AIGListenerGreyboxDirector::MeasureCctvFeed(
	const AIGCctvChannelFive* Channel)
{
	// Read the channel's own render target rather than a screenshot of the
	// monitor. It proves the thing that can actually fail — that the scene
	// capture rendered the annex — without depending on where the probe's camera
	// happens to be pointing, and it is the only reading available for a beat
	// that plays once.
	UTextureRenderTarget2D* Target = Channel
		? Channel->GetFeedForTesting()
		: nullptr;
	if (!Target)
	{
		return;
	}
	TArray<FColor> Samples;
	if (!UKismetRenderingLibrary::ReadRenderTarget(this, Target, Samples, false)
		|| Samples.Num() == 0)
	{
		return;
	}

	// Rec. 709 luma, and "lit" is any pixel above a twentieth — the annex has one
	// bulb, so most of this frame is legitimately dark and an average would hide
	// a black capture behind a correctly dark one.
	int32 LitPixels = 0;
	float Brightest = 0.0f;
	for (const FColor& Sample : Samples)
	{
		const float Luma =
			(0.2126f * Sample.R + 0.7152f * Sample.G + 0.0722f * Sample.B)
			/ 255.0f;
		Brightest = FMath::Max(Brightest, Luma);
		LitPixels += Luma > 0.05f ? 1 : 0;
	}
	CctvFeedBrightestLuma = Brightest;
	CctvFeedLitFraction =
		static_cast<float>(LitPixels) / static_cast<float>(Samples.Num());
	bCctvFeedMeasured = true;

	// Numbers say the frame is not black; they cannot say whether the shot reads.
	// The frame is written out so the composition can be judged by eye later
	// without re-running an engine for a beat that plays once.
	UKismetRenderingLibrary::ExportRenderTarget(
		this,
		Target,
		FPaths::ProjectDir() / TEXT("Docs/Media"),
		TEXT("cctv5-feed.png"));
}

void AIGListenerGreyboxDirector::StartProbe()
{
	ProbeStep = EProbeStep::AudioVisualContract;
	StepDeadlineSeconds = 0.0f;
	GetWorldTimerManager().SetTimer(
		ProbeTimer,
		this,
		&AIGListenerGreyboxDirector::AdvanceProbe,
		IGListenerGreybox::ProbePollSeconds,
		true);
}

void AIGListenerGreyboxDirector::AdvanceProbe()
{
	if (!Entity || !NightLoop || !NoiseSubsystem)
	{
		FailProbe(TEXT("stage actors disappeared mid-probe"));
		return;
	}
	StepDeadlineSeconds += IGListenerGreybox::ProbePollSeconds;

	switch (ProbeStep)
	{
	case EProbeStep::AudioVisualContract:
	{
		UIGMissingFloorAudioSubsystem* AudioDirector =
			GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>();
		FString Failure;
		if (!AudioDirector || !AudioDirector->ValidateContract(Failure))
		{
			FailProbe(FString::Printf(
				TEXT("M6 audio graph invalid: %s"),
				AudioDirector ? *Failure : TEXT("subsystem missing")));
			return;
		}
		const bool bVoiceCapsMatch =
			AudioDirector->GetVoiceCap(EIGAudioBus::Entity) == 4
			&& AudioDirector->GetVoiceCap(EIGAudioBus::Player) == 6
			&& AudioDirector->GetVoiceCap(EIGAudioBus::Puzzle) == 6
			&& AudioDirector->GetVoiceCap(EIGAudioBus::World) == 12;
		const bool bTitleWindowMatches =
			UIGMissingFloorAudioSubsystem::IsTitleReplyTime(
				FDateTime(2026, 8, 11, 4, 30))
			&& UIGMissingFloorAudioSubsystem::IsTitleReplyTime(
				FDateTime(2026, 8, 11, 5, 30))
			&& !UIGMissingFloorAudioSubsystem::IsTitleReplyTime(
				FDateTime(2026, 8, 11, 5, 31));
		if (!bVoiceCapsMatch || !bTitleWindowMatches)
		{
			FailProbe(TEXT("M6 voice caps or title reply window drifted"));
			return;
		}
		AudioDirector->SetAuthoredSilence(true);
		const bool bSilenceMixMatches = FMath::IsNearlyEqual(
			AudioDirector->GetEffectiveBusDecibels(EIGAudioBus::Score),
			-96.0f,
			0.01f)
			&& FMath::IsNearlyEqual(
				AudioDirector->GetEffectiveBusDecibels(EIGAudioBus::World),
				-24.0f,
				0.01f);
		AudioDirector->SetAuthoredSilence(false);
		AudioDirector->SetPlayerListening(true);
		const bool bListeningDuckMatches = FMath::IsNearlyEqual(
			AudioDirector->GetEffectiveBusDecibels(EIGAudioBus::World),
			-14.0f,
			0.01f);
		AudioDirector->SetPlayerListening(false);
		AudioDirector->SetEntityDistance(500.0f);
		const bool bNearDuckMatches = FMath::IsNearlyEqual(
			AudioDirector->GetEffectiveBusDecibels(EIGAudioBus::Player),
			-7.0f,
			0.01f);
		AudioDirector->SetEntityDistance(MAX_flt);
		if (!bSilenceMixMatches || !bListeningDuckMatches || !bNearDuckMatches)
		{
			FailProbe(TEXT("M6 silence or dynamic ducking values drifted"));
			return;
		}
		ProbeStep = EProbeStep::DifficultyContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::DifficultyContract:
	{
		// §20.2 is a table of authored numbers, so the only honest test is to
		// resolve it and compare. A static text assertion can read the literals
		// but cannot prove the three axes multiply in the right order.
		const float ExpectedSensitivity[] = {1.0f, 1100.0f / 900.0f, 1100.0f / 900.0f, 1300.0f / 900.0f};
		const float ExpectedListen[] = {8.0f, 8.0f, 7.0f, 6.0f};
		const int32 ExpectedNodes[] = {6, 11, 14, 18};
		const float ExpectedChase[] = {330.0f, 462.0f, 462.0f, 506.0f};
		const float ExpectedHold[] = {6.0f, 6.0f, 5.0f, 5.0f};
		const float ExpectedHeat[] = {0.0f, 0.3f, 0.5f, 0.7f};
		const bool ExpectedAmbush[] = {false, false, true, true};
		for (int32 Night = 1; Night <= 4; ++Night)
		{
			const FIGListenerTuning Base = IGListenerTuning::Resolve(
				Night,
				EIGNightDifficulty::Standard,
				0);
			const int32 Index = Night - 1;
			const bool bRowMatches =
				FMath::IsNearlyEqual(Base.HearingSensitivity, ExpectedSensitivity[Index], 0.001f)
				&& FMath::IsNearlyEqual(Base.ListenWindowSeconds, ExpectedListen[Index], 0.001f)
				&& Base.PatrolNodeCount == ExpectedNodes[Index]
				&& FMath::IsNearlyEqual(Base.ChaseSpeed, ExpectedChase[Index], 0.01f)
				&& FMath::IsNearlyEqual(Base.InvestigateHoldSeconds, ExpectedHold[Index], 0.001f)
				&& FMath::IsNearlyEqual(Base.HeatmapWeight, ExpectedHeat[Index], 0.001f)
				&& Base.bTierThreeAmbushAllowed == ExpectedAmbush[Index]
				&& Base.bCaptureEnabled
				&& Base.bChaseEnabled;
			if (!bRowMatches)
			{
				FailProbe(FString::Printf(
					TEXT("§20.2 tuning row for night %d drifted"),
					Night));
				return;
			}
		}

		// The tier is a separate axis that multiplies the night value, not a
		// replacement for it: night 3 at tier 3 is 7 × 0.5, not 4.
		const FIGListenerTuning Night3Tier3 = IGListenerTuning::Resolve(
			3,
			EIGNightDifficulty::Standard,
			3);
		if (!FMath::IsNearlyEqual(Night3Tier3.ListenWindowSeconds, 3.5f, 0.001f))
		{
			FailProbe(FString::Printf(
				TEXT("tier axis is not multiplying the night value: %.2fs"),
				Night3Tier3.ListenWindowSeconds));
			return;
		}

		const FIGListenerTuning Quiet = IGListenerTuning::Resolve(
			4,
			EIGNightDifficulty::Quiet,
			0);
		const FIGListenerTuning Hasty = IGListenerTuning::Resolve(
			2,
			EIGNightDifficulty::Hasty,
			0);
		const FIGListenerTuning ListenOnly = IGListenerTuning::Resolve(
			4,
			EIGNightDifficulty::ListenOnly,
			3);
		const bool bQuietMatches =
			FMath::IsNearlyEqual(Quiet.HearingSensitivity, (1300.0f / 900.0f) * 0.75f, 0.001f)
			&& FMath::IsNearlyEqual(Quiet.ChaseSpeed, 506.0f * 0.8f, 0.01f)
			&& FMath::IsNearlyEqual(Quiet.WaitScale, 1.5f, 0.001f);
		const bool bHastyMatches =
			FMath::IsNearlyEqual(Hasty.ListenWindowSeconds, 7.0f, 0.001f)
			&& FMath::IsNearlyEqual(Hasty.HeatmapWeight, 0.5f, 0.001f);
		const bool bListenOnlyMatches =
			!ListenOnly.bChaseEnabled
			&& !ListenOnly.bCaptureEnabled
			&& !ListenOnly.bTierThreeAmbushAllowed;
		if (!bQuietMatches || !bHastyMatches || !bListenOnlyMatches)
		{
			FailProbe(TEXT("§20.4 difficulty modifiers drifted"));
			return;
		}

		// §5.6 is pure statistics, so it has to be reproducible: same reports in,
		// same hottest zone out, halved by a night, gone on reset.
		UIGNoiseSubsystem* Noise = NoiseSubsystem;
		if (!Noise)
		{
			FailProbe(TEXT("noise subsystem missing for the heatmap contract"));
			return;
		}
		Noise->ResetHeatmap();
		const float PreviousMasking = Noise->GetGlobalMasking();
		Noise->SetGlobalMasking(0.0f);
		const FVector ColdSpot = ProbeNoiseLocation + FVector(4000.0f, 0.0f, 0.0f);
		const FVector WarmSpot = ProbeNoiseLocation;
		const FVector HotSpot =
			ProbeNoiseLocation + FVector(UIGNoiseSubsystem::HeatZoneSize * 3.0f, 0.0f, 0.0f);
		Noise->ReportNoise(WarmSpot, 0.30f, nullptr);
		// Deliberately past the saturation point: a zone the player has been loud
		// in fifty times must not out-weigh one they were loud in fifteen times,
		// or the ambush would chase an outlier instead of a habit.
		const int32 SaturatingReports = FMath::CeilToInt(
			UIGNoiseSubsystem::HeatSaturation / 0.5f) + 2;
		for (int32 Repeat = 0; Repeat < SaturatingReports; ++Repeat)
		{
			Noise->ReportNoise(HotSpot, 0.50f, nullptr);
		}
		const float WarmHeat = Noise->GetHeatAt(WarmSpot);
		const float HotHeat = Noise->GetHeatAt(HotSpot);
		const float ColdHeat = Noise->GetHeatAt(ColdSpot);
		FVector HottestCenter = FVector::ZeroVector;
		float HottestHeat = 0.0f;
		const bool bFoundHottest =
			Noise->GetHottestZone(HottestCenter, HottestHeat);
		// The hottest zone must be the one that was hammered, not the first one
		// touched, or an ambush would sit where the player merely walked once.
		const bool bHottestIsHot = bFoundHottest
			&& FVector::Dist2D(HottestCenter, HotSpot)
				< UIGNoiseSubsystem::HeatZoneSize;
		const bool bSaturates = HotHeat >= 0.99f;
		const int32 ZonesBeforeDecay = Noise->GetHeatZoneCount();
		Noise->DecayHeatmapForNewNight();
		const float DecayedHot = Noise->GetHeatAt(HotSpot);
		Noise->ResetHeatmap();
		const int32 ZonesAfterReset = Noise->GetHeatZoneCount();
		Noise->SetGlobalMasking(PreviousMasking);
		if (ColdHeat > 0.0f || WarmHeat <= 0.0f || HotHeat <= WarmHeat
			|| !bHottestIsHot || !bSaturates
			|| ZonesBeforeDecay != 2 || ZonesAfterReset != 0
			|| !FMath::IsNearlyEqual(DecayedHot, HotHeat * 0.5f, 0.01f))
		{
			FailProbe(FString::Printf(
				TEXT("§5.6 heatmap drifted: cold=%.2f warm=%.2f hot=%.2f "
					"decayed=%.2f zones=%d reset=%d hottest=%d"),
				ColdHeat,
				WarmHeat,
				HotHeat,
				DecayedHot,
				ZonesBeforeDecay,
				ZonesAfterReset,
				bHottestIsHot ? 1 : 0));
			return;
		}

		// And the entity honours the mode it was given, not just the table.
		AIGListenerEntity* EntityActor = Entity.Get();
		if (!EntityActor)
		{
			FailProbe(TEXT("entity missing for the difficulty contract"));
			return;
		}
		const EIGNightDifficulty RestoreDifficulty =
			EntityActor->GetDifficulty();
		EntityActor->SetDifficultyForTesting(EIGNightDifficulty::ListenOnly);
		const bool bEntityListenOnly =
			!EntityActor->GetTuning().bCaptureEnabled
			&& !EntityActor->GetTuning().bChaseEnabled;
		EntityActor->SetDifficultyForTesting(RestoreDifficulty);
		const bool bEntityRestored =
			EntityActor->GetTuning().bCaptureEnabled
			&& EntityActor->GetDifficulty() == RestoreDifficulty;
		if (!bEntityListenOnly || !bEntityRestored)
		{
			FailProbe(TEXT("the entity does not follow §20.4 at runtime"));
			return;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_DIFFICULTY PASS: §20.2 four nights, "
				"tier axis multiplies (night3 tier3 = %.2fs), "
				"quiet/hasty/listen-only honoured, heatmap saturates and halves"),
			Night3Tier3.ListenWindowSeconds);

		ProbeStep = EProbeStep::PerceptionContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::PerceptionContract:
	{
		UIGMissingFloorAudioSubsystem* AudioDirector =
			GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>();
		UIGDustSubsystem* Dust = GetWorld()->GetSubsystem<UIGDustSubsystem>();
		AIGPlayerCharacter* PlayerCharacter = Player.Get();
		UIGFlashlightComponent* Torch = PlayerCharacter
			? PlayerCharacter->GetFlashlight()
			: nullptr;
		if (!AudioDirector || !Dust || !Torch)
		{
			FailProbe(TEXT("perception subsystems or the torch are missing"));
			return;
		}

		// §10.4: only stair treads are the stairwell, only the roof slab is
		// outside, and everything else in the building is corridor.
		const bool bSpaceMapMatches =
			UIGMissingFloorAudioSubsystem::ClassifyAcousticSpace(
				EIGFootstepSurface::MetalStair) == EIGAcousticSpace::Stairwell
			&& UIGMissingFloorAudioSubsystem::ClassifyAcousticSpace(
				EIGFootstepSurface::Rooftop) == EIGAcousticSpace::Open
			&& UIGMissingFloorAudioSubsystem::ClassifyAcousticSpace(
				EIGFootstepSurface::Concrete) == EIGAcousticSpace::Corridor
			&& UIGMissingFloorAudioSubsystem::ClassifyAcousticSpace(
				EIGFootstepSurface::Vinyl) == EIGAcousticSpace::Corridor
			&& UIGMissingFloorAudioSubsystem::ClassifyAcousticSpace(
				EIGFootstepSurface::GypsumDebris) == EIGAcousticSpace::Corridor;
		AudioDirector->SetAcousticSpace(EIGAcousticSpace::Stairwell);
		const bool bStairwellApplied =
			AudioDirector->GetAcousticSpace() == EIGAcousticSpace::Stairwell;
		AudioDirector->SetAcousticSpace(EIGAcousticSpace::Open);
		const bool bOpenIsDry =
			AudioDirector->GetAcousticSpace() == EIGAcousticSpace::Open
			&& AudioDirector->GetAcousticPreset(EIGAcousticSpace::Open) == nullptr;
		AudioDirector->SetAcousticSpace(EIGAcousticSpace::Corridor);
		const bool bPresetsBuilt =
			AudioDirector->GetAcousticPreset(EIGAcousticSpace::Corridor) != nullptr
			&& AudioDirector->GetAcousticPreset(EIGAcousticSpace::Stairwell)
				!= nullptr;
		if (!bSpaceMapMatches || !bStairwellApplied || !bOpenIsDry
			|| !bPresetsBuilt)
		{
			FailProbe(TEXT("§10.4 acoustic space routing drifted"));
			return;
		}

		// §11 V1: one stir doubles the air where it happened, changes nothing a
		// room away, and merges rather than piling up along a slow crawl.
		Dust->ClearDisturbances();
		const FVector Stir = ProbeNoiseLocation;
		const bool bStartsClean = Dust->GetLiveDisturbanceCount() == 0
			&& FMath::IsNearlyEqual(
				Dust->GetDensityMultiplierAt(Stir), 1.0f, 0.001f);
		Dust->ReportDisturbance(Stir, 1.0f);
		const bool bDoublesAtStir = FMath::IsNearlyEqual(
			Dust->GetDensityMultiplierAt(Stir),
			UIGDustSubsystem::MaxDensityMultiplier,
			0.02f);
		const bool bOrdinaryAwayFromStir = FMath::IsNearlyEqual(
			Dust->GetDensityMultiplierAt(
				Stir + FVector(UIGDustSubsystem::DisturbanceRadius * 2.0f, 0, 0)),
			1.0f,
			0.001f);
		Dust->ReportDisturbance(
			Stir + FVector(UIGDustSubsystem::MergeDistance * 0.5f, 0.0f, 0.0f),
			1.0f);
		const bool bMergesNearby = Dust->GetLiveDisturbanceCount() == 1;
		Dust->ReportDisturbance(Stir + FVector(320.0f, 0.0f, 0.0f), 1.0f);
		const bool bKeepsSeparateLane = Dust->GetLiveDisturbanceCount() == 2;
		Dust->ClearDisturbances();
		const bool bResetForgets = Dust->GetLiveDisturbanceCount() == 0;
		if (!bStartsClean || !bDoublesAtStir || !bOrdinaryAwayFromStir
			|| !bMergesNearby || !bKeepsSeparateLane || !bResetForgets)
		{
			FailProbe(TEXT("§11 V1 airborne dust model drifted"));
			return;
		}

		// §7 P3 is decided by one audible fact: 속이 찬 벽은 짧게 죽고, 빈 벽은
		// 길게 운다. Render both answers and compare the energy left in the last
		// third of each. A thought bubble claiming the difference while the two
		// walls sound alike would be the puzzle failing silently, and no static
		// assertion can catch that — only the samples can.
		// Both answers are measured over the same absolute window — half a second
		// to one second after the ear lands. A ratio of the two would be
		// meaningless here: the solid wall has stopped producing samples by then,
		// so the denominator is zero and any ratio reads as a fake number. What
		// matters is a fact in two parts. At half a second the cavity must still
		// be plainly audible, and the solid wall must already be gone.
		constexpr int32 SampleRateHz = 48000;
		constexpr int32 WindowStartSample = SampleRateHz / 2;
		constexpr int32 WindowEndSample = SampleRateHz;
		// About -44 dBFS: quiet, but unmistakably a note rather than a floor.
		constexpr float AudibleFloor = 200.0f;
		float HollowLevel = 0.0f;
		float SolidLevel = 0.0f;
		float HollowLength = 0.0f;
		float SolidLength = 0.0f;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const bool bHollow = Pass == 0;
			UIGToneSequenceSoundWave* Response =
				UIGToneSequenceSoundWave::CreateWallCavityResponse(this, bHollow);
			if (!Response)
			{
				FailProbe(TEXT("wall cavity response failed to synthesize"));
				return;
			}
			const float Length = Response->GetConfiguredDurationSeconds();
			TArray<uint8> Pcm;
			Response->OnGeneratePCMAudio(Pcm, WindowEndSample);
			const int32 SampleCount =
				Pcm.Num() / static_cast<int32>(sizeof(int16));
			const int16* Samples =
				reinterpret_cast<const int16*>(Pcm.GetData());
			double WindowSum = 0.0;
			int32 WindowSamples = 0;
			for (int32 Index = WindowStartSample; Index < SampleCount; ++Index)
			{
				WindowSum += FMath::Abs(static_cast<double>(Samples[Index]));
				++WindowSamples;
			}
			// A wave that ended before the window contributes no samples, which
			// is itself the answer: it is silent there.
			const float Level = WindowSamples > 0
				? static_cast<float>(WindowSum / WindowSamples)
				: 0.0f;
			if (bHollow)
			{
				HollowLevel = Level;
				HollowLength = Length;
			}
			else
			{
				SolidLevel = Level;
				SolidLength = Length;
			}
		}
		if (HollowLevel < AudibleFloor)
		{
			FailProbe(FString::Printf(
				TEXT("cavity is not still ringing at half a second: level=%.1f"),
				HollowLevel));
			return;
		}
		if (SolidLevel >= AudibleFloor)
		{
			FailProbe(FString::Printf(
				TEXT("solid wall has not died by half a second: level=%.1f"),
				SolidLevel));
			return;
		}
		if (SolidLength > 0.40f || HollowLength < 1.40f)
		{
			FailProbe(FString::Printf(
				TEXT("wall ring lengths drifted: hollow=%.2fs solid=%.2fs"),
				HollowLength,
				SolidLength));
			return;
		}
		ProbeHollowRingLevel = HollowLevel;
		ProbeHollowRingSeconds = HollowLength;
		ProbeSolidRingSeconds = SolidLength;

		// Arm the beam over a fresh lane and let it tick once before asserting.
		const UCameraComponent* Camera = PlayerCharacter->GetFirstPersonCamera();
		const FVector CameraLocation = Camera
			? Camera->GetComponentLocation()
			: PlayerCharacter->GetActorLocation();
		const FVector CameraForward = Camera
			? Camera->GetForwardVector()
			: PlayerCharacter->GetActorForwardVector();
		ProbeDustTrailLocation = CameraLocation + CameraForward * 260.0f;
		Dust->ReportDisturbance(ProbeDustTrailLocation, 1.0f);
		// 밤 시작부터 켜져 있을 수 있다. 새 빔 검사 전에 이전 앵커를 비운다.
		Torch->SetOn(false);
		Torch->SetAvailable(true);
		Torch->SetOn(true);
		ProbeStep = EProbeStep::BeamDustContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::BeamDustContract:
	{
		AIGPlayerCharacter* PlayerCharacter = Player.Get();
		UIGFlashlightComponent* Torch = PlayerCharacter
			? PlayerCharacter->GetFlashlight()
			: nullptr;
		UIGBeamDustComponent* BeamDust = Torch ? Torch->GetBeamDust() : nullptr;
		UIGDustSubsystem* Dust = GetWorld()->GetSubsystem<UIGDustSubsystem>();
		if (!Torch || !BeamDust || !Dust)
		{
			FailProbe(TEXT("beam dust component is missing from the torch"));
			return;
		}
		if (!Torch->IsOn())
		{
			FailProbe(TEXT("the torch would not stay lit for the dust contract"));
			return;
		}

		// Ordinary air already carries a cloud; his lane carries twice as much,
		// and the extra motes are anchored to the lane rather than sprinkled.
		const int32 LitMotes = BeamDust->GetActiveMoteCount();
		const float LaneDensity = BeamDust->GetBeamDensityMultiplier();
		if (LitMotes < UIGBeamDustComponent::BaseMoteCount)
		{
			FailProbe(FString::Printf(
				TEXT("lit beam carries only %d motes"),
				LitMotes));
			return;
		}
		if (LaneDensity < 1.5f
			|| LitMotes < UIGBeamDustComponent::BaseMoteCount
				+ UIGBeamDustComponent::TrailMoteCount / 2)
		{
			FailProbe(FString::Printf(
				TEXT("his lane did not thicken the beam: x%.2f, %d motes"),
				LaneDensity,
				LitMotes));
			return;
		}

		Torch->SetOn(false);
		if (BeamDust->GetActiveMoteCount() != 0)
		{
			FailProbe(TEXT("motes survived the torch going out"));
			return;
		}
		// §11 V2 분진 퇴적. The field owns its own extent, so the two facts worth
		// proving are that a mark inside the fifth-floor slab is drawn and a mark
		// outside it is silently discarded. Getting that backwards would either
		// litter the whole building with footprints or draw none at all, and both
		// look like "the feature is off" from a screenshot.
		const AIGPrologueWorldScene* SceneActor = WorldScene.Get();
		const UIGSettledDustComponent* DustField = SceneActor
			? SceneActor->FindComponentByClass<UIGSettledDustComponent>()
			: nullptr;
		if (!DustField || !DustField->IsFieldReady())
		{
			FailProbe(TEXT("the fifth-floor settled dust field is missing"));
			return;
		}
		Dust->ClearSettledPrints();
		const FVector InsideField(0.0f, 700.0f, 1200.0f);
		const FVector OutsideField(190.0f, -305.0f, 900.0f);
		Dust->ReportSettledPrint(InsideField, 0.0f, EIGDustPrintKind::Footfall);
		Dust->ReportSettledPrint(
			InsideField + FVector(120.0f, 0.0f, 0.0f),
			90.0f,
			EIGDustPrintKind::Drag);
		Dust->ReportSettledPrint(OutsideField, 0.0f, EIGDustPrintKind::Footfall);
		// Standing still must not evict the trail: a repeat inside the merge
		// radius replaces its neighbour instead of stacking.
		Dust->ReportSettledPrint(
			InsideField + FVector(UIGDustSubsystem::PrintMergeDistance * 0.4f, 0, 0),
			12.0f,
			EIGDustPrintKind::Footfall);
		const int32 ReportedPrints = Dust->GetSettledPrintCount();
		Dust->ClearSettledPrints();
		const int32 PrintsAfterReset = Dust->GetSettledPrintCount();
		if (ReportedPrints != 3 || PrintsAfterReset != 0)
		{
			FailProbe(FString::Printf(
				TEXT("settled dust bookkeeping drifted: reported=%d reset=%d"),
				ReportedPrints,
				PrintsAfterReset));
			return;
		}

		// §11 V2 403호 3단계 노화: cumulative, and clean in the prologue.
		AIGPrologueWorldScene* MutableScene = WorldScene.Get();
		if (!MutableScene)
		{
			FailProbe(TEXT("world scene missing for the aging contract"));
			return;
		}
		const int32 RestoreAgeStage = MutableScene->GetUnit403AgeStage();
		MutableScene->SetUnit403AgeStage(0);
		const int32 CleanPlanes = MutableScene->GetUnit403AgingPlaneCount();
		MutableScene->SetUnit403AgeStage(1);
		const int32 StageOnePlanes = MutableScene->GetUnit403AgingPlaneCount();
		MutableScene->SetUnit403AgeStage(2);
		const int32 StageTwoPlanes = MutableScene->GetUnit403AgingPlaneCount();
		MutableScene->SetUnit403AgeStage(RestoreAgeStage);
		if (CleanPlanes != 0 || StageOnePlanes <= 0
			|| StageTwoPlanes <= StageOnePlanes)
		{
			FailProbe(FString::Printf(
				TEXT("403 aging is not cumulative: %d/%d/%d planes"),
				CleanPlanes,
				StageOnePlanes,
				StageTwoPlanes));
			return;
		}

		Torch->SetAvailable(false);
		Dust->ClearDisturbances();

		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_PERCEPTION PASS: corridor/stairwell reverb, "
				"cavity still ringing at 0.5s (level=%.0f, %.2fs vs solid %.2fs), "
				"settled dust bounded and 403 aging cumulative, "
				"dust x%.2f over his lane, %d/%d motes lit, dry when dark"),
			ProbeHollowRingLevel,
			ProbeHollowRingSeconds,
			ProbeSolidRingSeconds,
			LaneDensity,
			LitMotes,
			UIGBeamDustComponent::MaxMoteCount);

		ProbeStep = EProbeStep::PuzzleOneContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::PuzzleOneContract:
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (!PuzzleOne || !Narrative)
		{
			FailProbe(TEXT("P1 director or narrative subsystem is missing"));
			return;
		}
		if (!PuzzleOne->ValidateFixtures())
		{
			FailProbe(TEXT("P1 fixtures were not all placed"));
			return;
		}
		// 회로가 꺼졌을 때 멈추고, 켜졌을 때 수평 원판만 도는지 확인한다.
		const FVector MeterProbeSavedLocation = Player->GetActorLocation();
		Player->SetActorLocation(FVector(505, -300, 98));
		UStaticMeshComponent* Rotor = WorldScene->GetFifthMeterDisc();
		const FRotator RotorBefore = Rotor->GetRelativeRotation();
		WorldScene->AdvanceUtilityMeters(12.f, false, true);
		const bool bOffStopped = RotorBefore.Equals(Rotor->GetRelativeRotation(), .01f);
		WorldScene->AdvanceUtilityMeters(12.f, true, true);
		const FRotator RotorAfter = Rotor->GetRelativeRotation();
		Player->SetActorLocation(FVector(505, -300, 1098));
		WorldScene->AdvanceUtilityMeters(12.f, true, true);
		const bool bFarStopped = RotorAfter.Equals(Rotor->GetRelativeRotation(), .01f);
		Player->SetActorLocation(MeterProbeSavedLocation);
		if (!bOffStopped || !bFarStopped || RotorBefore.Equals(RotorAfter, .01f)
			|| FMath::Abs(Rotor->GetUpVector().Z) < .99f
			|| PuzzleOne->GetMeterAction()->GetInteractionHoldDuration_Implementation(Player.Get()) < 1.f)
		{
			FailProbe(TEXT("P1 rotor power, horizontal axis, distance or observation hold failed")); return;
		}
		UE_LOG(LogTemp, Display, TEXT("UTILITY_METER PASS stopped_off=1 rotates_on=1 horizontal=1 distance_cull=1 observation_hold=1"));
		// 공용 회로를 분리하고 전원 전후를 비교해야 계량기를 증거로 남긴다.
		const FIGMissingFloorNarrativeSnapshot BeforeExperiment = Narrative->GetSnapshot();
		const float CommonSwitchZ = WorldScene->GetCommonBreakerToggle()->GetRelativeLocation().Z;
		const float UnnamedSwitchZ = WorldScene->GetUnnamedBreakerToggle()->GetRelativeLocation().Z;
		FIGInteractionContext Experiment;
		Experiment.Interactor = Player.Get();
		PuzzleOne->SetHourActive(true);
		PuzzleOne->GetMeterAction()->CompleteInteraction_Implementation(Experiment);
		if (Narrative->HasSource(EIGMissingFloorTruth::LivedUpstairs, EIGMissingFloorSource::MeterFifthDial))
		{
			FailProbe(TEXT("P1 filed meter evidence without separating the common circuit")); return;
		}
		PuzzleOne->GetCommonLightAction()->CompleteInteraction_Implementation(Experiment);
		PuzzleOne->GetMeterAction()->CompleteInteraction_Implementation(Experiment);
		PuzzleOne->GetBreakerAction()->CompleteInteraction_Implementation(Experiment);
		if (!FMath::IsNearlyEqual(WorldScene->GetCommonBreakerToggle()->GetRelativeLocation().Z, CommonSwitchZ - 3.f)
			|| !FMath::IsNearlyEqual(WorldScene->GetUnnamedBreakerToggle()->GetRelativeLocation().Z, UnnamedSwitchZ + 3.f)
			|| WorldScene->GetFifthMeterDisc()->Mobility != EComponentMobility::Movable)
		{
			FailProbe(TEXT("P1 switch or meter presentation cannot move")); return;
		}
		if (Narrative->HasSource(EIGMissingFloorTruth::LivedUpstairs, EIGMissingFloorSource::MeterFifthDial))
		{
			FailProbe(TEXT("P1 switch alone completed the experiment")); return;
		}
		PuzzleOne->GetMeterAction()->CompleteInteraction_Implementation(Experiment);
		if (!Narrative->HasSource(EIGMissingFloorTruth::LivedUpstairs, EIGMissingFloorSource::MeterFifthDial)
			|| Narrative->HasTruth(EIGMissingFloorTruth::LivedUpstairs))
		{
			FailProbe(TEXT("P1 requires both observations and an independent reading sheet")); return;
		}
		PuzzleOne->GetBreakerAction()->CompleteInteraction_Implementation(Experiment);
		PuzzleOne->GetCommonLightAction()->CompleteInteraction_Implementation(Experiment);
		PuzzleOne->SetHourActive(false);
		Narrative->RestoreSnapshot(BeforeExperiment);
		UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_P1_EXPERIMENT PASS isolation=1 off_on=1 reversible=1"));
		// 기록 하나만으로는 결론을 내릴 수 없다.
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::LivedUpstairs,
			EIGMissingFloorSource::MeterFifthDial);
		if (Narrative->HasTruth(EIGMissingFloorTruth::LivedUpstairs))
		{
			FailProbe(TEXT("T1 confirmed from a single evidence record"));
			return;
		}
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::LivedUpstairs,
			EIGMissingFloorSource::MeterReadingSheet);
		if (!Narrative->HasTruth(EIGMissingFloorTruth::LivedUpstairs))
		{
			FailProbe(TEXT("T1 did not confirm after crossing both records"));
			return;
		}
		// The final choice must not open on an unrelated truth.
		if (Narrative->IsFinalChoiceUnlocked())
		{
			FailProbe(TEXT("final choice unlocked without T6/T7/T9"));
			return;
		}
		// Confirmation is derived, never latched: dropping the records must
		// drop the truth with them.
		FIGMissingFloorNarrativeSnapshot Stripped = Narrative->GetSnapshot();
		for (FIGMissingFloorTruthRecord& Record : Stripped.Truths)
		{
			Record.SourceIds.Reset();
		}
		Narrative->RestoreSnapshot(Stripped);
		if (Narrative->HasTruth(EIGMissingFloorTruth::LivedUpstairs))
		{
			FailProbe(TEXT("T1 survived a snapshot with no evidence records"));
			return;
		}
		ProbeStep = EProbeStep::SealContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::SealContract:
	{
		const AIGPrologueWorldScene* SealedScene = WorldScene.Get();
		if (!SealedScene || !NightPhase)
		{
			FailProbe(TEXT("night phase did not arm with the stage"));
			return;
		}
		// The hour must be holding, the shutter must exist, and the release
		// tag must actually be registered — an unregistered tag would leave
		// the sealed entrance silently openable.
		if (!NightPhase->IsHourActive() || !SealedScene->IsTheHourSealed())
		{
			FailProbe(TEXT("the hour did not seal the building"));
			return;
		}
		if (!SealedScene->HasNightSealGeometry())
		{
			FailProbe(TEXT("connector night gate geometry is missing"));
			return;
		}
		if (!FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.MissingFloor.Night.MorningCame")), false).IsValid())
		{
			FailProbe(TEXT("night release tag is not registered in DefaultGameplayTags"));
			return;
		}
		// Morning has to be the same exit whichever way it arrives.
		NightPhase->CompleteNightGoal();
		if (NightPhase->IsHourActive() || SealedScene->IsTheHourSealed())
		{
			FailProbe(TEXT("completing the night goal did not release the seal"));
			return;
		}
		// Re-seal for the hunting steps: the entity's rules are what the rest
		// of this probe measures.
		NightPhase->BeginTheHour(1);
		if (!SealedScene->IsTheHourSealed())
		{
			FailProbe(TEXT("the hour could not be re-armed after dawn"));
			return;
		}
		ProbeStep = EProbeStep::MaskingContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::MaskingContract:
	{
		// Synchronous contract: global masking swallows a quiet sound whole,
		// the fridge pocket masks its surroundings, and an unmasked report
		// carries loudness into radius.
		NoiseSubsystem->SetGlobalMasking(0.4f);
		const FIGNoiseEvent Masked = NoiseSubsystem->ReportNoise(
			Entity->GetActorLocation() + FVector(120.0f, 0.0f, 0.0f), 0.3f);
		NoiseSubsystem->SetGlobalMasking(0.0f);
		if (Masked.Loudness > 0.0f)
		{
			FailProbe(TEXT("global masking failed to swallow a 0.3 sound"));
			return;
		}
		// 험의 중심이 아니라 냉장고에 서서 잰다. 플레이어는 좌표를
		// 보고 숨지 않는다 — 기계를 보고 숨는다. 둘이 갈라지면
		// 여기서 걸린다.
		if (!WorldScene.IsValid()
			|| NoiseSubsystem->GetMaskingAt(
				WorldScene->GetFridgeLocation()) <= 0.0f)
		{
			FailProbe(TEXT("fridge hum pocket does not cover the fridge"));
			return;
		}

		// First bait: a single modest sound a short crawl from the entity.
		ProbeNoiseLocation =
			Entity->GetActorLocation() + FVector(0.0f, -40.0f, 0.0f)
			+ FVector(260.0f, 0.0f, 0.0f);
		EmitProbeNoise();
		ProbeStep = EProbeStep::InvestigateOnFirstSound;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::InvestigateOnFirstSound:
		if (Entity->GetListenerState() == EIGListenerState::Investigating
			|| Entity->GetListenerState() == EIGListenerState::Holding)
		{
			const UIGMissingFloorAudioSubsystem* AudioDirector =
				GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>();
			if (!AudioDirector
				|| AudioDirector->GetThreatState()
					!= EIGAudioThreatState::Investigating)
			{
				FailProbe(TEXT("M6 score did not follow investigation state"));
				return;
			}
			EmitProbeNoise();
			ProbeStep = EProbeStep::ChaseOnSecondSound;
			StepDeadlineSeconds = 0.0f;
			break;
		}
		if (StepDeadlineSeconds > 3.0f)
		{
			FailProbe(TEXT("first sound did not trigger Investigating"));
		}
		break;

	case EProbeStep::ChaseOnSecondSound:
		if (Entity->GetListenerState() == EIGListenerState::Chasing)
		{
			const UIGMissingFloorAudioSubsystem* AudioDirector =
				GetWorld()->GetSubsystem<UIGMissingFloorAudioSubsystem>();
			if (!AudioDirector
				|| AudioDirector->GetThreatState()
					!= EIGAudioThreatState::Chasing)
			{
				FailProbe(TEXT("M6 score did not follow chase state"));
				return;
			}
			// Touch: hand the player to the pursuer.
			if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
			{
				PlayerCharacter->TeleportTo(
					Entity->GetActorLocation()
						+ Entity->GetActorForwardVector() * 70.0f,
					PlayerCharacter->GetActorRotation(),
					false,
					true);
			}
			ProbeStep = EProbeStep::CaptureOnTouch;
			StepDeadlineSeconds = 0.0f;
			break;
		}
		if (StepDeadlineSeconds > 3.0f)
		{
			FailProbe(TEXT("second sound did not escalate to Chasing"));
		}
		break;

	case EProbeStep::CaptureOnTouch:
		if (NightLoop->GetCaptureCount() >= 1)
		{
			ProbeStep = EProbeStep::ResetAfterCapture;
			StepDeadlineSeconds = 0.0f;
			break;
		}
		if (StepDeadlineSeconds > 5.0f)
		{
			FailProbe(TEXT("touch did not capture the player"));
		}
		break;

	case EProbeStep::ResetAfterCapture:
	{
		AIGPlayerCharacter* PlayerCharacter = Player.Get();
		const bool bPlayerBackAtBed =
			PlayerCharacter
			&& FVector::Dist(
				PlayerCharacter->GetActorLocation(),
				ExpectedWakeLocation) <= 200.0f;
		const bool bTierRaised = Entity->GetAggressionTier() == 1;
		const bool bCaptureHandprintLeft =
			NightLoop->GetCaptureHandprintCount() >= 1;
		const bool bWakeRecoveryFinished =
			!NightLoop->IsCaptureResetInFlight();
		const bool bInputRestored =
			PlayerCharacter && PlayerCharacter->InputEnabled();
		const bool bMercyNoteReady =
			!bMercyNoteProbeRequested
			|| (NightLoop->IsMercyNoteVisible()
				&& !NightLoop->IsMercyNoteSliding()
				&& FVector::Dist(
					NightLoop->GetMercyNoteLocation(),
					AIGNightLoopDirector::GetMercyNoteRestLocation()) <= 1.0f);
		if (bPlayerBackAtBed
			&& bTierRaised
			&& bCaptureHandprintLeft
			&& bWakeRecoveryFinished
			&& bInputRestored
			&& bMercyNoteReady)
		{
			if (bMercyNoteProbeRequested)
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("MISSINGFLOOR_M65_MERCY_NOTE PASS: "
						"capture=5 slide=1 world_note=1 ui=0"));
			}
			ProbeStep = EProbeStep::MercyNetContract;
			StepDeadlineSeconds = 0.0f;
			break;
		}
		if (StepDeadlineSeconds > 6.0f)
		{
			if (bMercyNoteProbeRequested)
			{
				FailProbe(FString::Printf(
					TEXT("reset incomplete (atBed=%d tier=%d handprints=%d recovery=%d input=%d mercy=%d)"),
					bPlayerBackAtBed ? 1 : 0,
					Entity->GetAggressionTier(),
					NightLoop->GetCaptureHandprintCount(),
					bWakeRecoveryFinished ? 1 : 0,
					bInputRestored ? 1 : 0,
					bMercyNoteReady ? 1 : 0));
			}
			else
			{
				FailProbe(FString::Printf(
					TEXT("reset incomplete (atBed=%d tier=%d handprints=%d recovery=%d input=%d)"),
					bPlayerBackAtBed ? 1 : 0,
					Entity->GetAggressionTier(),
					NightLoop->GetCaptureHandprintCount(),
					bWakeRecoveryFinished ? 1 : 0,
					bInputRestored ? 1 : 0));
			}
		}
		break;
	}

	case EProbeStep::MercyNetContract:
	{
		// §20.3's two automatic nets. The properties worth proving are the ones
		// that make them mercy rather than noise: they key off learning, not
		// walking; they stand down the moment something is learned; they never
		// repeat the same nudge twice running; and they never say the answer.
		AIGMissingFloorMercyDirector* MercyActor = Mercy.Get();
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (!MercyActor || !Narrative)
		{
			FailProbe(TEXT("mercy director or narrative missing"));
			return;
		}
		if (!FMath::IsNearlyEqual(
				AIGMissingFloorMercyDirector::StuckResponseSeconds,
				90.0f,
				0.01f)
			|| AIGMissingFloorMercyDirector::ResetsForEnvironmentHint != 2)
		{
			FailProbe(TEXT("§20.3 thresholds drifted from 90 s and 2 resets"));
			return;
		}

		// The paper takes about a second to come out from under the door, so the
		// separation question can only be asked once it has settled. Measuring
		// mid-slide compares two notes that are both still at the threshold.
		if (bMercyNetsFired)
		{
			if (MercyActor->IsNoteSliding())
			{
				if (StepDeadlineSeconds > 6.0f)
				{
					FailProbe(TEXT("the note never finished sliding"));
				}
				break;
			}
			// Both notes at rest: the five-capture note's authored resting spot
			// is the one the M6.5 contract pins, so compare against that.
			const float SettledSeparation = FVector::Dist2D(
				MercyActor->GetNoteLocation(),
				AIGNightLoopDirector::GetMercyNoteRestLocation());
			if (SettledSeparation < 20.0f)
			{
				FailProbe(FString::Printf(
					TEXT("the two notes rest %.1f cm apart and overlap"),
					SettledSeparation));
				return;
			}
			UE_LOG(
				LogTemp,
				Display,
				TEXT("MISSINGFLOOR_MERCY PASS: 90s clock, 2-reset hint, "
					"responses=%d never repeating, note rests %.1f cm clear of "
					"the five-capture note, stands down on a new source"),
				MercyActor->GetResponseCount(),
				SettledSeparation);
			if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
			{
				PlayerCharacter->TeleportTo(
					AIGNightOneBeatDirector::GetSightingZoneCenter(),
					PlayerCharacter->GetActorRotation(),
					false,
					true);
			}
			ProbeStep = EProbeStep::RecordingRuleContract;
			StepDeadlineSeconds = 0.0f;
			break;
		}

		// The reset that got us here was the first; §20.3-1 wants two in a row
		// with nothing learned in between, so exactly one more must fire it.
		const int32 HintsBefore = MercyActor->GetResetHintCount();
		MercyActor->NotifyCaptureReset();
		const int32 HintsAfter = MercyActor->GetResetHintCount();
		if (HintsAfter != HintsBefore + 1)
		{
			FailProbe(FString::Printf(
				TEXT("two consecutive resets did not add observation material "
					"(%d -> %d)"),
				HintsBefore,
				HintsAfter));
			return;
		}

		// Alternation: the same nudge twice running would train the player to
		// ignore it. Asking three times in a row must never repeat, and on a
		// night where only some responses are available the rotation has to fall
		// through rather than stall.
		EIGMercyResponse PreviousKind = MercyActor->GetLastResponse();
		for (int32 Attempt = 0; Attempt < 3; ++Attempt)
		{
			const int32 ResponsesBefore = MercyActor->GetResponseCount();
			if (!MercyActor->ForceWorldResponseForTesting()
				|| MercyActor->GetResponseCount() != ResponsesBefore + 1
				|| MercyActor->GetLastResponse() == EIGMercyResponse::None)
			{
				FailProbe(FString::Printf(
					TEXT("the world would not respond on attempt %d"),
					Attempt));
				return;
			}
			if (Attempt > 0 && MercyActor->GetLastResponse() == PreviousKind)
			{
				FailProbe(TEXT("the same nudge fired twice running"));
				return;
			}
			PreviousKind = MercyActor->GetLastResponse();
		}

		// The note is once a night: asking again must not produce a second sheet.
		if (!MercyActor->IsNoteDelivered())
		{
			FailProbe(TEXT("the note never came under the door"));
			return;
		}

		// And the load-bearing property: learning one thing stands both nets
		// down. Without this a player making progress would still be nudged,
		// which reads as the game not watching them.
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::LivedUpstairs,
			EIGMissingFloorSource::MeterReadingSheet);
		MercyActor->NotifyCaptureReset();
		const bool bStandsDownOnProgress =
			MercyActor->GetResetHintCount() == HintsAfter;
		if (!bStandsDownOnProgress)
		{
			FailProbe(TEXT("a new source did not stand the reset net down"));
			return;
		}

		// Everything synchronous is proven. The paper is still moving, so the
		// step re-enters until it settles and then measures the separation.
		bMercyNetsFired = true;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::RecordingRuleContract:
	{
		// §5.5. The rule has one job and one exception, and both have to be true
		// or the whole climax stops meaning anything: her own sounds survive, his
		// do not, and the gap is exactly as long as what it replaced.
		UIGRecordingSubsystem* Recording =
			GetWorld()->GetSubsystem<UIGRecordingSubsystem>();
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (!Recording || !Narrative)
		{
			FailProbe(TEXT("recording subsystem or narrative missing"));
			return;
		}
		if (Narrative->IsNightFourWallOpened())
		{
			FailProbe(TEXT("the wall is already open before night four"));
			return;
		}

		// Two of her sounds either side of one of his.
		Recording->ClearTake();
		Recording->RecordForTesting(0.5f, 0.15f, /*bFromEntity=*/false);
		Recording->RecordForTesting(2.0f, 1.00f, /*bFromEntity=*/true);
		Recording->RecordForTesting(4.0f, 0.15f, /*bFromEntity=*/false);
		const bool bHerSoundsKept = Recording->GetSurvivingCount() == 2;
		const bool bHisSoundRefused = Recording->GetSuppressedCount() == 1;
		// 정확히 그 길이만큼의 무음: a full-loudness knock leaves the longest gap
		// the table allows, and the tape must account for every second of it.
		const float Gap = Recording->GetSuppressedSeconds();
		const bool bGapIsExact = FMath::IsNearlyEqual(Gap, 2.10f, 0.01f);
		const bool bPlaysBack =
			Recording->PlayBack(FVector(0.0f, 0.0f, 1000.0f));
		if (!bHerSoundsKept || !bHisSoundRefused || !bGapIsExact || !bPlaysBack)
		{
			FailProbe(FString::Printf(
				TEXT("§5.5 rule drifted: kept=%d refused=%d gap=%.2fs played=%d"),
				Recording->GetSurvivingCount(),
				Recording->GetSuppressedCount(),
				Gap,
				bPlaysBack ? 1 : 0));
			return;
		}

		// The one exception. Opening the wall in night four lifts the rule, and
		// the first sound the machine keeps is what ending A reports.
		Narrative->SetNightIndex(4);
		Narrative->SetNightFourWallOpened(true);
		if (!Recording->IsRuleLifted())
		{
			FailProbe(TEXT("the wall opened and the rule did not lift"));
			return;
		}
		Recording->ClearTake();
		Recording->RecordForTesting(0.5f, 1.00f, /*bFromEntity=*/true);
		const bool bLiftedKeepsHim =
			Recording->GetSuppressedCount() == 0
			&& Recording->GetSurvivingCount() == 1;
		// Put the night back the way the probe found it; later steps own it.
		Narrative->ResetNightFourForRetry();
		Narrative->SetNightIndex(1);
		Recording->ClearTake();
		if (!bLiftedKeepsHim)
		{
			FailProbe(TEXT("the lifted rule still refused his sound"));
			return;
		}
		if (Recording->IsRuleLifted())
		{
			FailProbe(TEXT("the rule stayed lifted after the night was reset"));
			return;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_RECORDING PASS: her sounds kept, his refused, "
				"%.2fs of exact silence, lifted once by the night-four wall"),
			Gap);

		ProbeStep = EProbeStep::Night1SightingStage;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::Night1SightingStage:
	{
		const bool bStaged = NightOneBeats && NightOneBeats->IsSightingStaged();
		const bool bOnLanding =
			FVector::Dist(
				Entity->GetActorLocation(),
				AIGNightOneBeatDirector::GetSightingStagePoint()) <= 250.0f;
		if (bStaged && bOnLanding)
		{
			// Descend past the figure: step into the moved portal line.
			if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
			{
				PlayerCharacter->TeleportTo(
					FVector(-435.0f, -305.0f, 890.0f),
					PlayerCharacter->GetActorRotation(),
					false,
					true);
			}
			ProbeStep = EProbeStep::Night1SightingRestore;
			StepDeadlineSeconds = 0.0f;
			break;
		}
		if (StepDeadlineSeconds > 4.0f)
		{
			FailProbe(FString::Printf(
				TEXT("sighting did not stage (staged=%d onLanding=%d)"),
				bStaged ? 1 : 0,
				bOnLanding ? 1 : 0));
		}
		break;
	}

	case EProbeStep::Night1SightingRestore:
	{
		const bool bCompleted =
			NightOneBeats && NightOneBeats->HasSightingCompleted();
		const bool bBackOnRoute =
			FVector::Dist(
				Entity->GetActorLocation(),
				FVector(-180.0f, -305.0f, 960.0f)) <= 320.0f;
		if (bCompleted && bBackOnRoute)
		{
			// The forced tutorial: stand at the fire cabinet.
			if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
			{
				PlayerCharacter->TeleportTo(
					FVector(232.0f, -290.0f, 1010.0f),
					PlayerCharacter->GetActorRotation(),
					false,
					true);
			}
			ProbeStep = EProbeStep::Night1Extinguisher;
			StepDeadlineSeconds = 0.0f;
			break;
		}
		if (StepDeadlineSeconds > 8.0f)
		{
			FailProbe(FString::Printf(
				TEXT("sighting cameo did not end (completed=%d back=%d)"),
				bCompleted ? 1 : 0,
				bBackOnRoute ? 1 : 0));
		}
		break;
	}

	case EProbeStep::Night1Extinguisher:
	{
		const AIGPrologueWorldScene* SceneNow = WorldScene.Get();
		const bool bDropped =
			SceneNow && SceneNow->IsCorridorExtinguisherDropped();
		const bool bBeatFired =
			NightOneBeats && NightOneBeats->HasExtinguisherBeatFired();
		// CaptureHold also proves it heard the clatter — being caught while
		// standing at the noise is the tutorial's other legitimate outcome.
		const EIGListenerState State = Entity->GetListenerState();
		const bool bReacted =
			State == EIGListenerState::Investigating
			|| State == EIGListenerState::Holding
			|| State == EIGListenerState::Chasing
			|| State == EIGListenerState::CaptureHold;
		if (bDropped && bBeatFired && bReacted)
		{
			if (!NightPhase)
			{
				FailProbe(TEXT("night phase missing for the cycle contract"));
				return;
			}
			// End night 1 through the goal exit and verify the day.
			NightPhase->CompleteNightGoal();
			ProbeStep = EProbeStep::DayNightCycle;
			StepDeadlineSeconds = 0.0f;
			break;
		}
		if (StepDeadlineSeconds > 6.0f)
		{
			FailProbe(FString::Printf(
				TEXT("extinguisher beat incomplete (dropped=%d fired=%d state=%d)"),
				bDropped ? 1 : 0,
				bBeatFired ? 1 : 0,
				static_cast<int32>(State)));
		}
		break;
	}

	case EProbeStep::DayNightCycle:
	{
		const AIGPrologueWorldScene* SceneNow = WorldScene.Get();
		const bool bDay = NightPhase && !NightPhase->IsHourActive();
		const bool bUnsealed = SceneNow && !SceneNow->IsTheHourSealed();
		const bool bEntityAsleep = Entity->IsDormant();
		if (bDay && bUnsealed && bEntityAsleep)
		{
			// The day holds. Go to bed and expect night 2 to begin with the
			// pursuer awake again.
			if (!SleepTarget)
			{
				FailProbe(TEXT("sleep target missing"));
				return;
			}
			FIGInteractionContext SleepContext;
			SleepContext.Interactor = Player.Get();
			SleepContext.TargetActor = SleepTarget;
			SleepContext.HoldProgress = 1.0f;
			IIGInteractable::Execute_CompleteInteraction(
				SleepTarget, SleepContext);

			UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
			const bool bNightTwo =
				NightPhase->IsHourActive()
				&& Narrative
				&& Narrative->GetNightIndex() == 2
				&& !Entity->IsDormant();
			if (!bNightTwo)
			{
				FailProbe(TEXT("sleeping did not begin night 2"));
				return;
			}
			ProbeStep = EProbeStep::NightTwoDoorBeatContract;
			StepDeadlineSeconds = 0.0f;
			break;
		}
		if (StepDeadlineSeconds > 4.0f)
		{
			FailProbe(FString::Printf(
				TEXT("dawn incomplete (day=%d unsealed=%d asleep=%d)"),
				bDay ? 1 : 0,
				bUnsealed ? 1 : 0,
				bEntityAsleep ? 1 : 0));
		}
		break;
	}

	case EProbeStep::NightTwoDoorBeatContract:
	{
		// §8 비트 2-1. The beat has to arm itself on night two without being
		// asked, put a figure outside 403, and leave the three knocks on the
		// tape as the refusal §5.5's morning playback is built on.
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		UIGRecordingSubsystem* Recording =
			GetWorld()->GetSubsystem<UIGRecordingSubsystem>();
		AIGMissingFloorEvidence* Peephole =
			NightTwoBeats ? NightTwoBeats->GetPeephole() : nullptr;
		if (!NightTwoBeats || !Narrative || !Recording || !Peephole)
		{
			FailProbe(TEXT("§8 비트 2-1 director or its peephole is missing"));
			return;
		}
		if (NightTwoBeats->HasPlayed())
		{
			FailProbe(TEXT("the door beat played before night two armed it"));
			return;
		}

		// The phone is the player's verb, so the probe plays the player: arm the
		// recording, then let the beat run without waiting out its patience.
		Recording->ClearTake();
		Recording->StartRecording();
		NightTwoBeats->AdvanceForTesting();

		if (!NightTwoBeats->HasPlayed()
			|| NightTwoBeats->GetStage() != EIGNightTwoBeatStage::Spent)
		{
			FailProbe(FString::Printf(
				TEXT("§8 비트 2-1 did not finish: stage=%d knocks=%d"),
				static_cast<int32>(NightTwoBeats->GetStage()),
				NightTwoBeats->GetKnockCount()));
			return;
		}
		// One knock to bring her to the door, the triple through it, the drag.
		if (NightTwoBeats->GetKnockCount() != 3)
		{
			FailProbe(FString::Printf(
				TEXT("§8 비트 2-1 played %d of its 3 cues"),
				NightTwoBeats->GetKnockCount()));
			return;
		}
		if (!NightTwoBeats->WasRecordingDuringAnswer())
		{
			FailProbe(TEXT("the armed phone was not running for the answer"));
			return;
		}
		// The figure was on loan. It must be back on its corridor route, or
		// night 2's patrol runs a two-point shuffle outside one door all hour.
		if (NightTwoBeats->IsFigureAtDoor())
		{
			FailProbe(TEXT("the figure stayed at the door after the beat"));
			return;
		}
		// §5.5's payoff: his knocks are on the log and every one is refused.
		// 2.10 s is what the loudness table gives a full-loudness triple, and
		// the morning gap is exactly that long.
		const int32 Suppressed = Recording->GetSuppressedCount();
		const float SuppressedSeconds = Recording->GetSuppressedSeconds();
		if (Suppressed < 2 || Suppressed != Recording->GetRecordedCount())
		{
			FailProbe(FString::Printf(
				TEXT("the door beat left %d of %d events on the tape"),
				Recording->GetRecordedCount() - Suppressed,
				Recording->GetRecordedCount()));
			return;
		}
		if (SuppressedSeconds < 2.10f)
		{
			FailProbe(FString::Printf(
				TEXT("the triple knock left only %.2fs of silence"),
				SuppressedSeconds));
			return;
		}
		// Once per run. A capture reset must not replay it as a jump scare.
		if (!Narrative->HasBeatPlayed(FName(TEXT("Night2.DoorKnock"))))
		{
			FailProbe(TEXT("the door beat did not book itself"));
			return;
		}
		Recording->StopRecording();
		Recording->ClearTake();

		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_N2DOOR PASS: knock at 403, figure staged and "
				"released, 3 cues, %d refused events, %.2fs of silence"),
			Suppressed,
			SuppressedSeconds);

		// Into the booth, whose door the hour has opened.
		if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
		{
			PlayerCharacter->TeleportTo(
				FVector(170.0f, -150.0f, 92.0f),
				PlayerCharacter->GetActorRotation(),
				false,
				true);
		}
		ProbeStep = EProbeStep::PuzzleTwoContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::PuzzleTwoContract:
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (!PuzzleTwo || !Narrative || !PuzzleTwo->ValidateFixtures())
		{
			FailProbe(TEXT("P2 fixtures were not all placed"));
			return;
		}

		AIGMissingFloorEvidence* Carbon = PuzzleTwo->GetCarbonLedger();
		AIGReadableNote* AgentNote = PuzzleTwo->GetAgentMessageNote();
		AIGMissingFloorEvidence* Cctv = PuzzleTwo->GetCctvSelector();
		if (!Carbon || !AgentNote || !Cctv)
		{
			FailProbe(TEXT("P2 interactables unresolved"));
			return;
		}

		FIGInteractionContext Context;
		Context.Interactor = Player.Get();
		Context.HoldProgress = 1.0f;

		// The booth's own valve: water on the riser masks the desk, which is
		// the verb night 3 and night 4 build on. Open it first, the way a
		// careful player would, and prove the rub is swallowed where she sits.
		AIGMissingFloorEvidence* BoothValve = PuzzleTwo->GetBoothRiserValve();
		if (!BoothValve || !BoothValve->IsInteractionEnabled())
		{
			FailProbe(TEXT("the booth riser valve was not available on night 2"));
			return;
		}
		Context.TargetActor = BoothValve;
		IIGInteractable::Execute_CompleteInteraction(BoothValve, Context);
		if (!PuzzleTwo->IsBoothValveOpen()
			|| NoiseSubsystem->GetMaskingAt(Carbon->GetActorLocation()) < 0.29f)
		{
			FailProbe(TEXT("the booth valve did not put water over the desk"));
			return;
		}

		// Two passes of frottage restore nothing yet...
		Context.TargetActor = Carbon;
		IIGInteractable::Execute_CompleteInteraction(Carbon, Context);
		IIGInteractable::Execute_CompleteInteraction(Carbon, Context);
		if (Narrative->HasSource(
			EIGMissingFloorTruth::WasStillAlive,
			EIGMissingFloorSource::CarbonLedgerOriginal))
		{
			FailProbe(TEXT("carbon original filed before the final pass"));
			return;
		}
		// ...and the third files the original.
		IIGInteractable::Execute_CompleteInteraction(Carbon, Context);
		if (!Narrative->HasSource(
			EIGMissingFloorTruth::WasStillAlive,
			EIGMissingFloorSource::CarbonLedgerOriginal))
		{
			FailProbe(TEXT("three frottage passes did not restore the original"));
			return;
		}
		if (Narrative->HasTruth(EIGMissingFloorTruth::WasStillAlive))
		{
			FailProbe(TEXT("T7 confirmed from the carbon record alone"));
			return;
		}

		if (Narrative->IsPuzzleSolved(FName(TEXT("P2"))))
		{
			FailProbe(TEXT("P2 completed before comparing the move-out date"));
			return;
		}
		if (AgentNote->IsHidden() || !AgentNote->IsInteractionEnabled())
		{
			FailProbe(TEXT("the printed message must already be on the desk"));
			return;
		}
		Context.TargetActor = AgentNote;
		IIGInteractable::Execute_CompleteInteraction(AgentNote, Context);
		if (!Narrative->HasTruth(EIGMissingFloorTruth::WasStillAlive)
			|| !Narrative->IsPuzzleSolved(FName(TEXT("P2"))))
		{
			FailProbe(TEXT("P2 date comparison did not complete")); return;
		}
		IIGInteractable::Execute_CompleteInteraction(AgentNote, Context);
		// §8 밤2 ends at 403's door, not here. The restored original arms 비트
		// 2-5 and the hour has to still be running, or the return chase never
		// happens.
		if (!NightPhase->IsHourActive())
		{
			FailProbe(TEXT("restoring the original released her to dawn from the booth"));
			return;
		}
		if (!NightTwoBeats
			|| NightTwoBeats->GetReturnStage()
				!= EIGNightTwoReturnStage::AwaitingExit)
		{
			FailProbe(FString::Printf(
				TEXT("§8 비트 2-5 did not arm on the restored original: stage=%d"),
				NightTwoBeats
					? static_cast<int32>(NightTwoBeats->GetReturnStage())
					: -1));
			return;
		}

		// §14 상시 렌더 금지. Before the press the channel must cost the frame
		// nothing at all: no render target, no capture, no picture. Pressed while
		// she is still at the desk, because that is where the monitor is.
		AIGCctvChannelFive* Channel = PuzzleTwo->GetCctvChannelFive();
		if (!Channel)
		{
			FailProbe(TEXT("§14 channel five was never built with the booth"));
			return;
		}
		if (Channel->GetState() != EIGCctvChannelState::Idle
			|| Channel->HasFeed()
			|| Channel->IsOnScreen()
			|| Channel->GetCaptureCount() != 0)
		{
			FailProbe(FString::Printf(
				TEXT("§14 상시 렌더 금지 broken before the press: "
					"state=%d feed=%d onscreen=%d captures=%d"),
				static_cast<int32>(Channel->GetState()),
				Channel->HasFeed() ? 1 : 0,
				Channel->IsOnScreen() ? 1 : 0,
				Channel->GetCaptureCount()));
			return;
		}

		// The one-shot CCTV beat books itself exactly once.
		Context.TargetActor = Cctv;
		IIGInteractable::Execute_CompleteInteraction(Cctv, Context);
		if (!Narrative->HasBeatPlayed(FName(TEXT("Night2.CCTV"))))
		{
			FailProbe(TEXT("CCTV channel-five beat did not book"));
			return;
		}
		if (!Channel->HasFeed() || !Channel->IsOnScreen())
		{
			FailProbe(TEXT("the fifth button did not put a picture on the monitor"));
			return;
		}
		// CIF, 또는 진단 배율을 곱한 CIF. 배율이 없는 실행에서는 정확히 352×288.
		const FIntPoint Resolution = Channel->GetFeedResolution();
		const FIntPoint ExpectedResolution = Channel->GetExpectedFeedResolution();
		if (Resolution != ExpectedResolution
			|| ExpectedResolution.X % 352 != 0
			|| ExpectedResolution.Y % 288 != 0)
		{
			FailProbe(FString::Printf(
				TEXT("channel five is not a CIF channel: %dx%d expected %dx%d"),
				Resolution.X,
				Resolution.Y,
				ExpectedResolution.X,
				ExpectedResolution.Y));
			return;
		}

		CctvCapturesAtLive = 0;
		CctvCapturesAtDeath = 0;
		bCctvLiveSoundHeard = false;
		bCctvFeedMeasured = false;
		CctvFeedBrightestLuma = 0.0f;
		CctvFeedLitFraction = 0.0f;
		ProbeStep = EProbeStep::CctvChannelContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::NightTwoReturnChaseContract:
	{
		// §8 비트 2-5. Leaving the booth has to drop the stack, the building has
		// to hear it twice — which is what makes the real AI commit to CHASE —
		// and only arriving back inside 403 may end the night.
		if (!NightTwoBeats || !NightPhase)
		{
			FailProbe(TEXT("§8 비트 2-5 director disappeared"));
			return;
		}
		if (!NightTwoBeats->HasReturnChaseFired())
		{
			if (StepDeadlineSeconds > 4.0f)
			{
				FailProbe(TEXT("leaving the booth did not drop the material"));
				return;
			}
			break;
		}
		if (NightTwoBeats->GetReturnStage() == EIGNightTwoReturnStage::Chased
			&& !NightPhase->IsHourActive())
		{
			FailProbe(TEXT("night 2 ended while she was still out of 403"));
			return;
		}
		// The chase is the real AI reacting to two sounds. Give it a moment to
		// commit, then check it is hunting rather than still patrolling.
		if (StepDeadlineSeconds < 1.5f)
		{
			break;
		}
		const bool bHunting = !Entity->IsDormant()
			&& Entity->GetListenerState() != EIGListenerState::Patrolling;
		if (!bHunting)
		{
			FailProbe(FString::Printf(
				TEXT("the collapse did not move the building: state=%d"),
				static_cast<int32>(Entity->GetListenerState())));
			return;
		}

		// Home. Being teleported here by a capture reset would not have counted;
		// that path owes the night another trip out and back.
		if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
		{
			PlayerCharacter->TeleportTo(
				FVector(60.0f, -120.0f, 992.0f),
				PlayerCharacter->GetActorRotation(),
				false,
				true);
		}
		ProbeStep = EProbeStep::NightTwoHomeContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::NightTwoHomeContract:
	{
		if (!NightTwoBeats || !NightPhase)
		{
			FailProbe(TEXT("§8 비트 2-5 director disappeared before dawn"));
			return;
		}
		const bool bHome =
			NightTwoBeats->GetReturnStage() == EIGNightTwoReturnStage::Home;
		if (!bHome || NightPhase->IsHourActive() || !Entity->IsDormant())
		{
			if (StepDeadlineSeconds > 6.0f)
			{
				FailProbe(FString::Printf(
					TEXT("§8 비트 2-5 did not close: home=%d hour=%d dormant=%d"),
					bHome ? 1 : 0,
					NightPhase->IsHourActive() ? 1 : 0,
					Entity->IsDormant() ? 1 : 0));
				return;
			}
			break;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_N2CHASE PASS: T7 armed the return, the booth exit "
				"dropped the stack, two sounds moved the building, and 403 "
				"ended the night"));

		ProbeStep = EProbeStep::DayTwoContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::CctvChannelContract:
	{
		AIGCctvChannelFive* Channel =
			PuzzleTwo ? PuzzleTwo->GetCctvChannelFive() : nullptr;
		if (!Channel)
		{
			FailProbe(TEXT("channel five disappeared mid-beat"));
			return;
		}

		// 화면이 살아 있는 동안 복도가 한 번 운다 — latched, because the poll
		// must not have to land on the frame it fired.
		bCctvLiveSoundHeard = bCctvLiveSoundHeard || Channel->HasLiveSoundPlayed();
		if (Channel->GetState() == EIGCctvChannelState::Live)
		{
			CctvCapturesAtLive = Channel->GetCaptureCount();
			// Read the target while there is still a picture in it. Once is
			// enough, and the beat is not repeatable so there is no second chance.
			// 2.80 s past the press is 0.44 through the live window: the bulb has
			// settled and the first dropout is over, so the reading and the
			// exported frame are the corridor as the player sees it.
			if (bCctvFeedProbeRequested && !bCctvFeedMeasured
				&& StepDeadlineSeconds >= 2.80f)
			{
				MeasureCctvFeed(Channel);
			}
		}
		if (Channel->GetState() == EIGCctvChannelState::Collapsing
			&& CctvCapturesAtDeath == 0)
		{
			CctvCapturesAtDeath = Channel->GetCaptureCount();
		}
		if (!Channel->IsSpent())
		{
			// Acquire 0.32 + live 5.60 + collapse 0.86 = 6.78 s of channel.
			if (StepDeadlineSeconds > 12.0f)
			{
				FailProbe(FString::Printf(
					TEXT("channel five never died: state=%d after %.1fs"),
					static_cast<int32>(Channel->GetState()),
					StepDeadlineSeconds));
				return;
			}
			break;
		}

		// Spent. Everything the beat allocated has to be gone again (§14).
		if (Channel->HasFeed() || Channel->IsOnScreen())
		{
			FailProbe(FString::Printf(
				TEXT("§14 the dead channel is still allocated: "
					"feed=%d onscreen=%d"),
				Channel->HasFeed() ? 1 : 0,
				Channel->IsOnScreen() ? 1 : 0));
			return;
		}
		if (!bCctvLiveSoundHeard)
		{
			FailProbe(TEXT("the corridor never sounded while the picture was up"));
			return;
		}
		// 12 fps over the 5.92 s the picture is up is about 71 renders. The band
		// is wide enough for frame pacing and narrow enough to catch either
		// failure that matters: a capture stuck off, or one running every frame.
		const int32 Captures = Channel->GetCaptureCount();
		if (Captures < 40 || Captures > 110)
		{
			FailProbe(FString::Printf(
				TEXT("channel five captured %d frames; expected about 71 "
					"(12 fps for 5.92 s)"),
				Captures));
			return;
		}
		if (CctvCapturesAtDeath != 0 && Captures != CctvCapturesAtDeath)
		{
			FailProbe(FString::Printf(
				TEXT("the capture kept rendering through the collapse: %d -> %d"),
				CctvCapturesAtDeath,
				Captures));
			return;
		}
		// 1회 한정, 반복 재생 불가 — enforced by the actor, not only by the beat.
		if (Channel->Play())
		{
			FailProbe(TEXT("channel five played a second time"));
			return;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_CCTV5 PASS: 352x288 allocated on the press, "
				"%d captures, the corridor sounded once, released on death, "
				"second press refused"),
			Captures);

		if (bCctvFeedProbeRequested)
		{
			// The picture itself. Reported separately because it needs a real RHI,
			// and reported as FAIL rather than silence when the read comes back
			// black — an all-black capture satisfies every structural check above.
			const bool bNullRhi =
				FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
			if (bNullRhi)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("MISSINGFLOOR_CCTV5_FEED SKIP: -nullrhi renders no "
						"scene capture. Re-run with -RenderOffScreen and no "
						"-nullrhi to measure the picture."));
			}
			else if (!bCctvFeedMeasured)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("MISSINGFLOOR_CCTV5_FEED FAIL: the render target could "
						"not be read while the channel was live"));
			}
			else if (CctvFeedBrightestLuma < 0.08f || CctvFeedLitFraction < 0.02f)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("MISSINGFLOOR_CCTV5_FEED FAIL: the channel rendered "
						"black. brightest=%.4f lit=%.4f"),
					CctvFeedBrightestLuma,
					CctvFeedLitFraction);
			}
			else
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("MISSINGFLOOR_CCTV5_FEED PASS: brightest=%.4f "
						"lit=%.4f of the frame"),
					CctvFeedBrightestLuma,
					CctvFeedLitFraction);
			}
		}

		// Step out of the booth into the connector. §8 비트 2-5's collapse fires
		// on the player's own position, so this is the walk home starting, not a
		// poke at the beat.
		if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
		{
			PlayerCharacter->TeleportTo(
				FVector(190.0f, -300.0f, 92.0f),
				PlayerCharacter->GetActorRotation(),
				false,
				true);
		}
		ProbeStep = EProbeStep::NightTwoReturnChaseContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::DayTwoContract:
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (!NightThree || !Narrative || !NightThree->ValidateFixtures())
		{
			FailProbe(TEXT("night-3 fixtures were not all placed"));
			return;
		}

		FIGInteractionContext Context;
		Context.Interactor = Player.Get();
		Context.HoldProgress = 1.0f;

		// The day papers feed the truth board: the labels alone name no one
		// (T2 needs the notebook), the printout carries both the noise war
		// and its last morning, and the journal is earned by T7 and daylight.
		AIGReadableNote* Labels = NightThree->GetLabelsNote();
		AIGReadableNote* Forum = NightThree->GetForumNote();
		AIGReadableNote* Journal = NightThree->GetJournalNote();
		if (!Labels || !Forum || !Journal)
		{
			FailProbe(TEXT("day papers unresolved"));
			return;
		}
		// 날짜 대조를 마친 다음 낮에는 황순금의 기록을 받을 수 있다.
		if (Journal->IsHidden() == Narrative->HasTruth(EIGMissingFloorTruth::WasStillAlive))
		{
			FailProbe(TEXT("day journal visibility did not follow the date evidence"));
			return;
		}

		Context.TargetActor = Labels;
		IIGInteractable::Execute_CompleteInteraction(Labels, Context);
		IIGInteractable::Execute_CompleteInteraction(Labels, Context);
		Context.TargetActor = Forum;
		IIGInteractable::Execute_CompleteInteraction(Forum, Context);
		IIGInteractable::Execute_CompleteInteraction(Forum, Context);

		if (Narrative->HasTruth(EIGMissingFloorTruth::TenantIdentity))
		{
			FailProbe(TEXT("T2 confirmed from the labels alone"));
			return;
		}

		// To bed: night 3 begins.
		if (SleepTarget)
		{
			Context.TargetActor = SleepTarget;
			IIGInteractable::Execute_CompleteInteraction(SleepTarget, Context);
		}
		if (!NightPhase || !NightPhase->IsHourActive()
			|| Narrative->GetNightIndex() != 3)
		{
			FailProbe(TEXT("sleeping did not begin night 3"));
			return;
		}
		ProbeStep = EProbeStep::AnswerReachContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::NightThreeContract:
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (!NightThree || !Narrative)
		{
			FailProbe(TEXT("night-3 stage lost mid-contract"));
			return;
		}

		FIGInteractionContext Context;
		Context.Interactor = Player.Get();
		Context.HoldProgress = 1.0f;

		// The keyring in the open booth unlocks the gate as a saved fact.
		AIGMissingFloorEvidence* Key = NightThree->GetKeyring();
		AIGSwingDoor* Gate = NightThree->GetStairGate();
		AIGSwingDoor* AnnexGate = NightThree->GetAnnexGate();
		if (!Key || !Gate || !AnnexGate)
		{
			FailProbe(TEXT("keyring or either physical gate unresolved"));
			return;
		}
		if (!Gate->IsLocked() || !AnnexGate->IsLocked())
		{
			FailProbe(TEXT("a rooftop gate stood open before the keyring"));
			return;
		}
		// 실제 조준에 쓰는 단순 충돌로도 열쇠에 닿아야 집을 수 있다.
		const FVector KeyCenter = Key->GetComponentsBoundingBox().GetCenter();
		FHitResult KeyHit;
		FCollisionQueryParams KeyQuery(SCENE_QUERY_STAT(KeyPickupProbe), false, Player.Get());
		if (!GetWorld()->LineTraceSingleByChannel(KeyHit,
			KeyCenter + FVector(0, -70, 70), KeyCenter - FVector(0, 0, 1), ECC_Visibility, KeyQuery)
			|| KeyHit.GetActor() != Key)
		{
			FailProbe(TEXT("keyring cannot be targeted through the gameplay visibility trace")); return;
		}
		Context.TargetActor = Key;
		IIGInteractable::Execute_CompleteInteraction(Key, Context);
		if (Gate->IsLocked() || AnnexGate->IsLocked() || !Key->IsHidden()
			|| Key->IsInteractionEnabled() || Key->GetActorEnableCollision())
		{
			FailProbe(TEXT("key pickup did not release the gates and remove the keyring"));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("UTILITY_KEYRING PASS visibility_trace=1 picked_up=1 hidden=1 collision_off=1 gates_unlocked=2"));
		// The realtor's message waits beside the keyring. Two reads: open, close.
		AIGReadableNote* AgentNote =
			PuzzleTwo ? PuzzleTwo->GetAgentMessageNote() : nullptr;
		if (!AgentNote || AgentNote->IsHidden()
			|| !AgentNote->IsInteractionEnabled())
		{
			FailProbe(TEXT("the realtor's message did not appear on night 3"));
			return;
		}
		Context.TargetActor = AgentNote;
		IIGInteractable::Execute_CompleteInteraction(AgentNote, Context);
		IIGInteractable::Execute_CompleteInteraction(AgentNote, Context);
		if (!Narrative->HasTruth(EIGMissingFloorTruth::WasStillAlive))
		{
			FailProbe(TEXT("T7 did not confirm after crossing both records"));
			return;
		}

		// The topology contract above owns the full walk. The probe jumps only
		// after proving both leaves and the 640 cm collision receipt, so content
		// interactions can remain deterministic and fast in headless CI.
		if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
		{
			PlayerCharacter->TeleportTo(
				FVector(-280.0f, 700.0f, 1292.0f),
				PlayerCharacter->GetActorRotation(),
				false,
				true);
		}
		AIGReadableNote* Notebook = NightThree->GetTunerNotebook();
		Context.TargetActor = Notebook;
		IIGInteractable::Execute_CompleteInteraction(Notebook, Context);
		IIGInteractable::Execute_CompleteInteraction(Notebook, Context);
		if (!Narrative->HasTruth(EIGMissingFloorTruth::TenantIdentity)
			|| !Narrative->HasTruth(EIGMissingFloorTruth::NoiseWasHomecoming))
		{
			FailProbe(TEXT("notebook did not cross T2/T3 with the day papers"));
			return;
		}
		AIGMissingFloorEvidence* Mark = NightThree->GetImpactMark();
		Context.TargetActor = Mark;
		IIGInteractable::Execute_CompleteInteraction(Mark, Context);
		if (!Narrative->HasTruth(EIGMissingFloorTruth::LandingStruggle))
		{
			FailProbe(TEXT("impact mark did not cross T4 with the final post"));
			return;
		}

		// P3, the patient route: silence first, then water behind one bay.
		AIGMissingFloorEvidence* CavityListen = NightThree->GetWallListen(1);
		if (!NightThree->TryPlayerListen(CavityListen, Player.Get()))
		{
			FailProbe(TEXT("dedicated listen verb rejected the cavity wall"));
			return;
		}
		if (Narrative->HasSource(
			EIGMissingFloorTruth::SomeoneInTheWall,
			EIGMissingFloorSource::PipeWaterComparison))
		{
			FailProbe(TEXT("a dry wall filed the water comparison"));
			return;
		}
		AIGMissingFloorEvidence* Valve = NightThree->GetRiserValve();
		Context.TargetActor = Valve;
		IIGInteractable::Execute_CompleteInteraction(Valve, Context);
		if (!NightThree->IsValveOpen())
		{
			FailProbe(TEXT("valve did not open"));
			return;
		}
		if (!NightThree->TryPlayerListen(CavityListen, Player.Get()))
		{
			FailProbe(TEXT("dedicated listen verb dropped after valve open"));
			return;
		}
		if (!Narrative->HasTruth(EIGMissingFloorTruth::SomeoneInTheWall))
		{
			FailProbe(TEXT("criterion plus water did not confirm T6"));
			return;
		}

		// The answer surface arms only now.
		AIGMissingFloorEvidence* Answer = NightThree->GetAnswerTarget();
		if (!Answer || Answer->IsHidden() || !Answer->IsInteractionEnabled())
		{
			FailProbe(TEXT("answer target did not arm after T6"));
			return;
		}
		if (!NightThree->TryPlayerKnock(Answer, Player.Get()))
		{
			FailProbe(TEXT("dedicated knock verb rejected the armed answer wall"));
			return;
		}
		ProbeStep = EProbeStep::AnswerPairTap;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::AnswerReachContract:
	{
		// §8 비트 3-7. P4 teaches 둘-쉬고-하나 on an authored wall; the corridor
		// asks her to use it with nothing under the cursor. Before this existed
		// the entity's Waiting state and NotifyAnswerKnock had no caller at all,
		// so the answer could not leave P4's surface.
		AIGPlayerCharacter* PlayerCharacter = Player.Get();
		if (!PlayerCharacter)
		{
			FailProbe(TEXT("no pawn to answer with"));
			return;
		}
		// Stand him next to her so the taps are within a knock's earshot, and
		// make sure he is awake and merely patrolling first. His dormancy is
		// remembered rather than assumed: this check runs right after the sleep
		// that begins night three, so "it is still day" is not true here.
		bAnswerReachWasDormant = Entity->IsDormant();
		Entity->SetDormant(false);
		Entity->TeleportTo(
			PlayerCharacter->GetActorLocation() + FVector(180.0f, 0.0f, 0.0f),
			Entity->GetActorRotation(),
			false,
			true);
		if (Entity->GetListenerState() == EIGListenerState::Waiting)
		{
			FailProbe(TEXT("he was already waiting before she answered"));
			return;
		}

		// Out of earshot the cadence must do nothing at all. Two floors up is
		// the case the guard exists for.
		const FVector FarAway =
			PlayerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 1800.0f);
		if (PlayerCharacter->OfferAnswerKnock(FarAway))
		{
			FailProbe(TEXT("an answer from two floors up reached him"));
			return;
		}

		// 둘 — 쉬고 — 하나, at the authored windows. The taps are offered
		// through the same entry point the knock verb uses.
		const FVector Here = PlayerCharacter->GetActorLocation();
		const bool bFirst = PlayerCharacter->OfferAnswerKnock(Here);
		AnswerReachTapTwoAt =
			GetWorld()->GetTimeSeconds() + AIGListenerEntity::AnswerPairMinSeconds;
		if (!bFirst)
		{
			FailProbe(TEXT("the first tap of the answer was not taken"));
			return;
		}
		ProbeStep = EProbeStep::AnswerReachCadence;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::AnswerReachCadence:
	{
		AIGPlayerCharacter* PlayerCharacter = Player.Get();
		if (!PlayerCharacter)
		{
			FailProbe(TEXT("no pawn to finish the answer with"));
			return;
		}
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now < AnswerReachTapTwoAt)
		{
			break;
		}
		const FVector Here = PlayerCharacter->GetActorLocation();
		if (AnswerReachTapsSent == 0)
		{
			PlayerCharacter->OfferAnswerKnock(Here);
			AnswerReachTapsSent = 1;
			// The rest: longer than the pair, inside the authored window.
			AnswerReachTapTwoAt = Now + AIGListenerEntity::AnswerRestMinSeconds
				+ 0.10;
			break;
		}
		PlayerCharacter->OfferAnswerKnock(Here);
		if (Entity->GetListenerState() != EIGListenerState::Waiting)
		{
			FailProbe(FString::Printf(
				TEXT("둘-쉬고-하나 did not reach him: state=%d"),
				static_cast<int32>(Entity->GetListenerState())));
			return;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_ANSWERREACH PASS: the learned answer knocked at "
				"nothing froze him into Waiting, and the same cadence from two "
				"floors up did not"));

		// Put the day back exactly as it was: P4's own tap sequence runs later
		// and its pair interval is 0.65 s at the outside, so nothing of this
		// check may still be standing between his first and second knock.
		Entity->ResetToPatrolStart(/*bRaiseAggression=*/false);
		Entity->SetDormant(bAnswerReachWasDormant);
		AnswerReachTapsSent = 0;
		ProbeStep = EProbeStep::NightThreeContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::AnswerPairTap:
	{
		// Second beat at 0.42 s: inside the accepted 0.18..0.65 pair.
		if (StepDeadlineSeconds < 0.42f)
		{
			break;
		}
		AIGMissingFloorEvidence* Answer = NightThree
			? NightThree->GetAnswerTarget()
			: nullptr;
		if (!Answer || !Answer->IsInteractionEnabled())
		{
			FailProbe(TEXT("answer surface dropped before the second tap"));
			return;
		}
		if (!NightThree->TryPlayerKnock(Answer, Player.Get()))
		{
			FailProbe(TEXT("dedicated knock verb rejected the second tap"));
			return;
		}
		ProbeStep = EProbeStep::AnswerFinalTap;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::AnswerFinalTap:
	{
		// The 0.82 s rest is deliberately not the shortest accepted value, so
		// timer jitter cannot accidentally collapse the family rhythm.
		if (StepDeadlineSeconds < 0.82f)
		{
			break;
		}
		AIGMissingFloorEvidence* Answer = NightThree
			? NightThree->GetAnswerTarget()
			: nullptr;
		if (!Answer || !Answer->IsInteractionEnabled())
		{
			FailProbe(TEXT("answer surface dropped before the final tap"));
			return;
		}
		if (!NightThree->TryPlayerKnock(Answer, Player.Get()))
		{
			FailProbe(TEXT("dedicated knock verb rejected the final tap"));
			return;
		}
		ProbeStep = EProbeStep::AnswerContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::AnswerContract:
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (!Narrative || !NightPhase)
		{
			FailProbe(TEXT("stage lost while waiting on the wall"));
			return;
		}
		// Eight seconds of nothing, then the reply, T9, and dawn. The final
		// choice must stand unlocked afterwards: T6, T7 and T9 are all in.
		const bool bAnswered =
			Narrative->HasTruth(EIGMissingFloorTruth::WaitingForAnAnswer);
		if (bAnswered)
		{
			// §8 밤3 does not end at the wall either. T9 arms 비트 3-7 and the
			// hour has to still be running, with him standing in the corridor.
			if (!NightPhase->IsHourActive())
			{
				FailProbe(TEXT("the answer released her to dawn from the annex"));
				return;
			}
			if (!NightThree
				|| NightThree->GetReturnStage()
					!= EIGNightThreeReturnStage::Passing
				|| !NightThree->IsFigureInCorridor())
			{
				FailProbe(FString::Printf(
					TEXT("§8 비트 3-7 did not arm on T9: stage=%d corridor=%d"),
					NightThree
						? static_cast<int32>(NightThree->GetReturnStage())
						: -1,
					NightThree && NightThree->IsFigureInCorridor() ? 1 : 0));
				return;
			}
			if (!Narrative->IsFinalChoiceUnlocked())
			{
				FailProbe(TEXT("T6+T7+T9 did not unlock the final choice"));
				return;
			}
			if (!Narrative->IsPuzzleSolved(FName(TEXT("P3"))))
			{
				FailProbe(TEXT("P3 was not booked after the wall was identified"));
				return;
			}
			// Walk her to the west side of him and hand off; the pass itself is
			// the next step, because freezing him needs three taps in real time.
			if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
			{
				const FVector His = NightThree->GetReturnPassPoint();
				PlayerCharacter->TeleportTo(
					His - FVector(150.0f, 0.0f, 0.0f) + FVector(0.0f, 0.0f, 92.0f),
					PlayerCharacter->GetActorRotation(),
					false,
					true);
			}
			AnswerReachTapsSent = 0;
			AnswerReachTapTwoAt = 0.0;
			ProbeStep = EProbeStep::NightThreePassContract;
			StepDeadlineSeconds = 0.0f;
			break;
		}
		if (StepDeadlineSeconds > 12.0f)
		{
			FailProbe(TEXT("the wall never answered"));
			return;
		}
		break;
	}

	case EProbeStep::NightThreePassContract:
	{
		// §8 비트 3-7. 「그가 멈춰 기다리는 옆을 걸어 지나가는」 — the answer she
		// was taught minutes ago, used on a thing standing in her way, and then
		// the walk past it. Nothing forces this; slipping by unheard was always
		// allowed. It simply is not the beat.
		AIGPlayerCharacter* PlayerCharacter = Player.Get();
		if (!PlayerCharacter || !NightThree || !NightPhase)
		{
			FailProbe(TEXT("stage lost during the corridor pass"));
			return;
		}
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now < AnswerReachTapTwoAt)
		{
			break;
		}
		const FVector Here = PlayerCharacter->GetActorLocation();
		if (AnswerReachTapsSent < 3)
		{
			const bool bTaken = PlayerCharacter->OfferAnswerKnock(Here);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("MISSINGFLOOR_N3PASS_TAP tap=%d taken=%d dist=%.0f state=%d"),
				AnswerReachTapsSent + 1,
				bTaken ? 1 : 0,
				FVector::Dist(Here, Entity->GetActorLocation()),
				static_cast<int32>(Entity->GetListenerState()));
			++AnswerReachTapsSent;
			// 둘 — 쉬고 — 하나, at the authored windows.
			AnswerReachTapTwoAt = Now
				+ (AnswerReachTapsSent == 1
					? AIGListenerEntity::AnswerPairMinSeconds
					: AIGListenerEntity::AnswerRestMinSeconds + 0.10);
			break;
		}
		if (Entity->GetListenerState() != EIGListenerState::Waiting)
		{
			FailProbe(FString::Printf(
				TEXT("the answer did not stop him in the corridor: state=%d"),
				static_cast<int32>(Entity->GetListenerState())));
			return;
		}

		// Past him, while he is still listening for the next knock.
		const FVector His = NightThree->GetReturnPassPoint();
		PlayerCharacter->TeleportTo(
			His + FVector(150.0f, 0.0f, 92.0f),
			PlayerCharacter->GetActorRotation(),
			false,
			true);
		ProbeStep = EProbeStep::NightThreeHomeContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::NightThreeHomeContract:
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		AIGPlayerCharacter* PlayerCharacter = Player.Get();
		if (!Narrative || !PlayerCharacter || !NightThree || !NightPhase)
		{
			FailProbe(TEXT("stage lost on the way home from the annex"));
			return;
		}
		if (!NightThree->HasPassedWhileWaiting())
		{
			if (StepDeadlineSeconds > 4.0f)
			{
				FailProbe(TEXT("walking past him while he waited did not book 3-7"));
				return;
			}
			break;
		}
		if (!Narrative->HasBeatPlayed(FName(TEXT("Night3.PassBy"))))
		{
			FailProbe(TEXT("비트 3-7 was not booked once"));
			return;
		}
		// Only her own floor ends the night.
		if (NightThree->GetReturnStage() != EIGNightThreeReturnStage::Home)
		{
			if (!NightPhase->IsHourActive())
			{
				FailProbe(TEXT("night 3 ended while she was still in the corridor"));
				return;
			}
			PlayerCharacter->TeleportTo(
				FVector(60.0f, -120.0f, 992.0f),
				PlayerCharacter->GetActorRotation(),
				false,
				true);
			if (StepDeadlineSeconds > 6.0f)
			{
				FailProbe(TEXT("403 did not close night 3"));
				return;
			}
			break;
		}
		if (NightPhase->IsHourActive() || NightThree->IsFigureInCorridor())
		{
			if (StepDeadlineSeconds > 6.0f)
			{
				FailProbe(FString::Printf(
					TEXT("§8 비트 3-7 did not close: hour=%d corridor=%d"),
					NightPhase->IsHourActive() ? 1 : 0,
					NightThree->IsFigureInCorridor() ? 1 : 0));
				return;
			}
			break;
		}
		if (!Narrative->WasFirstReportMade())
		{
			FailProbe(TEXT("night 3 ended without the 05:30 first report"));
			return;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_N3PASS PASS: T9 armed the walk home, the learned "
				"answer stopped him in the corridor, she passed him while he "
				"waited, and 403 ended the night"));

		if (!NightFour || !NightFour->ValidateFixtures())
		{
			FailProbe(TEXT("night-4 fixtures were not all placed"));
			return;
		}

		// Day three: T7 closed last night, so 황순금 hands the journal over now.
		AIGReadableNote* Journal = NightThree->GetJournalNote();
		if (!Journal || Journal->IsHidden())
		{
			FailProbe(TEXT("journal stayed hidden after T7 by day"));
			return;
		}
		FIGInteractionContext DayContext;
		DayContext.Interactor = Player.Get();
		DayContext.HoldProgress = 1.0f;
		DayContext.TargetActor = Journal;
		IIGInteractable::Execute_CompleteInteraction(Journal, DayContext);
		IIGInteractable::Execute_CompleteInteraction(Journal, DayContext);
		if (!Narrative->HasSource(
			EIGMissingFloorTruth::FiveNightsOfThirst,
			EIGMissingFloorSource::KnockTallyJournal))
		{
			FailProbe(TEXT("journal read did not file the tally record"));
			return;
		}

		// Day after the first report: read Mok's repair/eviction notice,
		// then sleep into night 4. T10 still needs the breaker cut later.
		AIGMissingFloorEvidence* Eviction = NightFour->GetEvictionNotice();
		if (!Eviction || Eviction->IsHidden()
			|| !Eviction->IsInteractionEnabled())
		{
			FailProbe(TEXT("the day-four eviction notice was not available"));
			return;
		}
		FIGInteractionContext Context;
		Context.Interactor = Player.Get();
		Context.TargetActor = Eviction;
		Context.HoldProgress = 1.0f;
		IIGInteractable::Execute_CompleteInteraction(Eviction, Context);
		if (Narrative->HasTruth(EIGMissingFloorTruth::StillCoveringIt))
		{
			FailProbe(TEXT("eviction notice alone confirmed T10"));
			return;
		}
		if (SleepTarget)
		{
			Context.TargetActor = SleepTarget;
			IIGInteractable::Execute_CompleteInteraction(SleepTarget, Context);
		}
		if (!NightPhase->IsHourActive() || Narrative->GetNightIndex() != 4)
		{
			FailProbe(TEXT("sleeping did not begin night 4"));
			return;
		}
		ProbeStep = EProbeStep::SealedHourUiContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::SealedHourUiContract:
	{
		// §24 즉시 차단 19. 봉쇄된 한 시간 동안 F9 즉시 로드도, 증거 기록
		// 화면도 열리지 않는다. 둘 다 구현돼 있었지만 아무것도 그것을 잠그지
		// 않았고, 잠금 없는 규칙은 다음 사람이 지우면 그만이다.
		//
		// 거부했다는 사실만 보지 않는다. 「아무 일도 일어나지 않았다」는 것은
		// 입력이 끊어졌을 때의 모습이기도 해서, 어떤 거부를 했는지까지 읽는다.
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		AIGPlayerCharacter* PlayerCharacter = Player.Get();
		AIGPlayerController* PlayerController = PlayerCharacter
			? Cast<AIGPlayerController>(PlayerCharacter->GetController())
			: nullptr;
		AIGHorrorHUD* HorrorHUD = PlayerController
			? Cast<AIGHorrorHUD>(PlayerController->GetHUD())
			: nullptr;
		if (!Narrative || !PlayerCharacter || !PlayerController || !HorrorHUD)
		{
			FailProbe(TEXT("sealed-hour UI probe is missing an actor"));
			return;
		}
		if (!Narrative->IsHourSealed())
		{
			FailProbe(TEXT("the hour is not sealed; the gate proves nothing"));
			return;
		}

		// §24 즉시 차단 17도 같은 한 시간에 걸린다. 목표 텍스트는 밤 표시가
		// 켜져 있는 동안 그려지지 않으며, 그 밤 표시는 봉쇄 상태가 그대로
		// 밀어 넣는다. 봉쇄인데 밤 표시가 꺼져 있으면 목표 줄이 다시 나온다.
		const bool bNightPresented = HorrorHUD->IsNightPresentation();

		PlayerController->OpenMissingFloorJournalForTesting();
		const bool bJournalStayedShut = !HorrorHUD->IsMissingFloorJournalVisible();
		// 열렸다면 SetPause(true)가 함께 걸린다. 화면과 시간 두 쪽을 본다.
		const bool bTimeKeptRunning = !UGameplayStatics::IsGamePaused(this);

		// 큐를 본다. 이 거부는 앞선 대사를 밀어내지 않고 뒤에 서므로, 화면에
		// 떠 있는 줄만 읽으면 방금 말한 것이 아니라 아까 말한 것을 읽는다.
		PlayerCharacter->LoadLatestAutosaveForTesting();
		const bool bRestoreRefused = HorrorHUD->HasDialogueLineForTesting(
			TEXT("지금은 되돌릴 때가 아니다."));
		const FString Refusal =
			HorrorHUD->GetActiveDialogueLineForTesting().ToString();

		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_SEALEDUI journal_shut=%d running=%d ")
			TEXT("restore_refused=%d night_presented=%d line=%s"),
			bJournalStayedShut ? 1 : 0,
			bTimeKeptRunning ? 1 : 0,
			bRestoreRefused ? 1 : 0,
			bNightPresented ? 1 : 0,
			*Refusal);
		if (!bJournalStayedShut || !bTimeKeptRunning || !bRestoreRefused
			|| !bNightPresented)
		{
			FailProbe(TEXT("MISSINGFLOOR_SEALEDUI FAIL"));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_SEALEDUI PASS"));
		ProbeStep = EProbeStep::NightFourContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::NightFourContract:
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (!Narrative || !NightFour || !NoiseSubsystem)
		{
			FailProbe(TEXT("stage lost entering night 4"));
			return;
		}

		FIGInteractionContext Context;
		Context.Interactor = Player.Get();
		Context.HoldProgress = 1.0f;
		// The mechanically safe order must produce a continuous 0.40 mask and
		// no pressure-alarm branch. Each control remains a separately persisted
		// first activation rather than one three-stage scripted switch.
		for (AIGMissingFloorEvidence* Control : {
			NightFour->GetCleaningDrain(),
			NightFour->GetFloatBypass(),
			NightFour->GetTransferPump(),
		})
		{
			if (!Control || !Control->IsInteractionEnabled())
			{
				FailProbe(TEXT("a P5 cleaning-circuit control was unavailable"));
				return;
			}
			Context.TargetActor = Control;
			IIGInteractable::Execute_CompleteInteraction(Control, Context);
		}
		if (!Narrative->IsNightFourMaskRunning()
			|| !Narrative->IsPuzzleSolved(FName(TEXT("P5")))
			|| Narrative->GetNightFourControlOrder().Num() != 3)
		{
			FailProbe(TEXT("P5 controls did not settle into the running mask"));
			return;
		}
		if (NightFour->WasHydraulicAlarmTriggered())
		{
			FailProbe(TEXT("the safe P5 order triggered the pressure alarm"));
			return;
		}
		if (!NightFour->IsWaterMaskPlaying()
			|| NoiseSubsystem->GetMaskingAt(
				AIGMissingFloorNightFourDirector::GetWallBreakLocation())
				< 0.39f)
		{
			FailProbe(TEXT("P5 did not create its audible 0.40 wall mask"));
			return;
		}
		AIGMissingFloorEvidence* Wall = NightFour->GetWallBreakTarget();
		if (!Wall || Wall->IsHidden() || !Wall->IsInteractionEnabled())
		{
			FailProbe(TEXT("P5 completion did not arm the cavity wall"));
			return;
		}
		if (!bNightFourFailureRetryVerified)
		{
			// 엔딩 C는 일반 포획 리셋이나 새벽 완료가 아니라 밤 4 한정 재시도다.
			// 성공 공동 경로보다 먼저 한 번 실행하고, 다음 프로브 구간에서
			// 초기화된 조작부로 P5를 다시 구성한다.
			FailureRetryCaptureCountBefore = Narrative->GetCaptureCount();
			Narrative->SetAggressionTier(3);
			Entity->SetAggressionTier(3);
			if (!NightFour->ResolveFailureEnding()
				|| !NightFour->IsFailureEndingActive()
				|| Narrative->GetEndingChoice() != FName(TEXT("Ending.C"))
				|| !NightPhase || !NightPhase->IsHourActive()
				|| !NightPhase->IsFailureEndingSuspended()
				|| NightFour->IsWaterMaskPlaying()
				|| NoiseSubsystem->GetMaskingAt(
					AIGMissingFloorNightFourDirector::GetWallBreakLocation())
					> 0.01f)
			{
				FailProbe(TEXT("ending C did not suspend the hour and remove its mask"));
				return;
			}
			ProbeStep = EProbeStep::NightFourFailureRetryContract;
			StepDeadlineSeconds = 0.0f;
			return;
		}
		Context.TargetActor = Wall;
		IIGInteractable::Execute_CompleteInteraction(Wall, Context);
		IIGInteractable::Execute_CompleteInteraction(Wall, Context);
		if (Narrative->GetNightFourWallStrikeCount() != 2
			|| Narrative->HasTruth(EIGMissingFloorTruth::StillCoveringIt))
		{
			FailProbe(TEXT("T10 crossed before the third hammer strike"));
			return;
		}
		ProbeStep = EProbeStep::NightFourWallContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::NightFourFailureRetryContract:
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		AIGPrologueWorldScene* Scene = WorldScene.Get();
		if (!Narrative || !NightFour || !NightPhase || !Scene
			|| !NightFour->CompleteFailurePresentationForProbe()
			|| !NightFour->IsFailureRetryEnabled()
			|| !NightFour->RequestFailureRetry())
		{
			FailProbe(TEXT("ending C retry affordance did not complete"));
			return;
		}
		const bool bScopedRollbackPassed =
			!NightFour->IsFailureEndingActive()
			&& !NightFour->IsFailureRetryEnabled()
			&& NightPhase->IsHourActive()
			&& !NightPhase->IsFailureEndingSuspended()
			&& Narrative->GetNightIndex() == 4
			&& Narrative->GetAggressionTier() == 1
			&& Narrative->GetCaptureCount() == FailureRetryCaptureCountBefore + 1
			&& Narrative->GetEndingChoice().IsNone()
			&& Narrative->GetNightFourControlOrder().IsEmpty()
			&& Narrative->GetNightFourWallStrikeCount() == 0
			&& !Narrative->IsNightFourWallOpened()
			&& !Narrative->IsPuzzleSolved(FName(TEXT("P5")))
			&& Narrative->WasFirstReportMade()
			&& Narrative->HasTruth(EIGMissingFloorTruth::WaitingForAnAnswer)
			&& !Scene->IsMissingFloorCavityOpen();
		if (!bScopedRollbackPassed)
		{
			FailProbe(TEXT("ending C retry erased durable truth or kept night-four state"));
			return;
		}
		bNightFourFailureRetryVerified = true;
		ProbeStep = EProbeStep::NightFourContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::NightFourWallContract:
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		AIGMissingFloorEvidence* Wall = NightFour
			? NightFour->GetWallBreakTarget()
			: nullptr;
		if (!Narrative || !Wall)
		{
			FailProbe(TEXT("night-4 wall stage lost"));
			return;
		}
		FIGInteractionContext Context;
		Context.Interactor = Player.Get();
		Context.TargetActor = Wall;
		Context.HoldProgress = 1.0f;
		IIGInteractable::Execute_CompleteInteraction(Wall, Context);
		if (!Narrative->HasTruth(EIGMissingFloorTruth::StillCoveringIt)
			|| !Narrative->HasBeatPlayed(FName(TEXT("Night4.PowerCut"))))
		{
			FailProbe(TEXT("third strike did not cross T10 and cut power"));
			return;
		}
		IIGInteractable::Execute_CompleteInteraction(Wall, Context);
		IIGInteractable::Execute_CompleteInteraction(Wall, Context);
		AIGPrologueWorldScene* Scene = WorldScene.Get();
		if (!Narrative->IsNightFourWallOpened()
			|| !Scene || !Scene->IsMissingFloorCavityOpen())
		{
			FailProbe(TEXT("five strikes did not remove the real cavity panel"));
			return;
		}
		// The final choices are intentionally held behind the six-second
		// flashlight read, silence, Mok line and harmless entity pass. Waiting
		// here proves the authored sequence can finish without camera automation;
		// the 28-second ceiling also covers all three gaze fallbacks and the
		// complete silence, dialogue, entity-pass and blackout tail.
		if (!NightFour->IsFinalConfrontationComplete())
		{
			if (StepDeadlineSeconds > 28.0f)
			{
				FailProbe(TEXT("night-4 reveal/confrontation sequence stalled"));
			}
			return;
		}
		if (!NightFour->GetEndingATarget()
			|| NightFour->GetEndingATarget()->IsHidden()
			|| !NightFour->GetEndingATarget()->IsInteractionEnabled()
			|| !NightFour->GetEndingBTarget()
			|| NightFour->GetEndingBTarget()->IsHidden()
			|| !NightFour->GetEndingBTarget()->IsInteractionEnabled())
		{
			FailProbe(TEXT("wall discovery did not expose both mourning choices"));
			return;
		}
		Context.TargetActor = NightFour->GetEndingATarget();
		IIGInteractable::Execute_CompleteInteraction(
			NightFour->GetEndingATarget(), Context);
		ProbeStep = EProbeStep::NightFourEndingContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::NightFourEndingContract:
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (!Narrative || Narrative->GetEndingChoice() != FName(TEXT("Ending.A")))
		{
			FailProbe(TEXT("ending A spatial target did not persist its choice"));
			return;
		}
		if (!Narrative->WasFirstReportMade()
			|| !Narrative->WasSecondReportMade()
			|| !Narrative->HasBeatPlayed(FName(TEXT("Night4.SecondReport"))))
		{
			FailProbe(TEXT("ending divergence changed the common report facts"));
			return;
		}
		if (NightPhase && NightPhase->IsHourActive())
		{
			FailProbe(TEXT("ending resolution did not release the building at dawn"));
			return;
		}
		// Exercise the exact v1 -> v2 normalization boundary in memory. This
		// catches both new night-four fields and the old upper-bound bug that
		// used to discard P4 voicemail/notebook/journal sources on restore.
		FIGMissingFloorNarrativeSnapshot RestoreReceipt = Narrative->GetSnapshot();
		RestoreReceipt.SchemaVersion = 1;
		Narrative->RestoreSnapshot(RestoreReceipt);
		const bool bVoicemailRestored = Narrative->HasSource(
			EIGMissingFloorTruth::WaitingForAnAnswer,
			EIGMissingFloorSource::AnswerRhythmVoicemail);
		const bool bNotebookRestored = Narrative->HasSource(
			EIGMissingFloorTruth::WaitingForAnAnswer,
			EIGMissingFloorSource::AnswerRhythmNotebook);
		const bool bRestoreContractPassed =
			Narrative->GetSnapshot().SchemaVersion
				== UIGMissingFloorNarrativeSubsystem::SnapshotSchemaVersion
			&& Narrative->GetNightFourControlOrder().Num() == 3
			&& Narrative->GetNightFourWallStrikeCount() == 5
			&& Narrative->IsNightFourWallOpened()
			&& Narrative->GetEndingChoice() == FName(TEXT("Ending.A"))
			&& Narrative->WasFirstReportMade()
			&& Narrative->WasSecondReportMade()
			&& bVoicemailRestored
			&& bNotebookRestored;
		if (!bRestoreContractPassed)
		{
			FailProbe(FString::Printf(
				TEXT("v2 restore mismatch: schema=%d controls=%d strikes=%d wall=%d ending=%s reports=%d/%d p4=%d/%d"),
				Narrative->GetSnapshot().SchemaVersion,
				Narrative->GetNightFourControlOrder().Num(),
				Narrative->GetNightFourWallStrikeCount(),
				Narrative->IsNightFourWallOpened() ? 1 : 0,
				*Narrative->GetEndingChoice().ToString(),
				Narrative->WasFirstReportMade() ? 1 : 0,
				Narrative->WasSecondReportMade() ? 1 : 0,
				bVoicemailRestored ? 1 : 0,
				bNotebookRestored ? 1 : 0));
			return;
		}
		// 시작 여부는 IsActive가 아니라 엔딩 이름으로 본다. 앞 단계들이
		// 느리게 흐르면 87초가 이미 지나 스스로 끝나 있을 수도 있고, 그건
		// 결함이 아니라 정상 종료다.
		if (!Epilogue || Epilogue->GetEndingId() != FName(TEXT("Ending.A")))
		{
			FailProbe(TEXT("epilogue did not start with the ending choice"));
			return;
		}
		ProbeStep = EProbeStep::EpilogueContract;
		StepDeadlineSeconds = 0.0f;
		break;
	}

	case EProbeStep::EpilogueContract:
	{
		UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
		if (!Narrative || !Epilogue)
		{
			FailProbe(TEXT("epilogue contract lost its fixtures"));
			return;
		}
		// 87초를 기다리지 않고 남은 큐를 전부 흘린다. 끝나면 액터가 스스로
		// 비활성이 되고 HUD의 마지막 카드도 걷혀 있어야 한다.
		if (Epilogue->IsActive() && !Epilogue->CompleteImmediatelyForProbe())
		{
			FailProbe(TEXT("epilogue did not finish when its cues were flushed"));
			return;
		}
		if (Epilogue->IsActive())
		{
			FailProbe(TEXT("epilogue stayed active after its last cue"));
			return;
		}
		// 몽타주 · 공방 · 가을 · 보도 · 마지막 카드.
		if (Epilogue->GetPlayedSceneCount() != 5)
		{
			FailProbe(FString::Printf(
				TEXT("epilogue played %d scenes, expected 5"),
				Epilogue->GetPlayedSceneCount()));
			return;
		}
		if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
		{
			if (AIGHorrorHUD* Hud = Cast<AIGHorrorHUD>(Controller->GetHUD()))
			{
				if (Hud->IsMissingFloorEpilogueVisible())
				{
					FailProbe(TEXT("epilogue left its last card on screen"));
					return;
				}
			}
		}

		// §22.3: 목격은 서로 독립이고, 어떤 교차에도 들어가지 않는다.
		const int32 TruthsBefore = Narrative->GetConfirmedTruthCount();
		const EIGMissingFloorWitness Witnesses[] = {
			EIGMissingFloorWitness::SeoSleepingPills,
			EIGMissingFloorWitness::HwangWaterBowl,
			EIGMissingFloorWitness::BoothSoundproofing,
			EIGMissingFloorWitness::RooftopCigarettePack,
			EIGMissingFloorWitness::BoothWallCalendar,
			EIGMissingFloorWitness::RecorderEmptyBay,
			EIGMissingFloorWitness::AnnexWorkGlove,
			EIGMissingFloorWitness::StoreNightRoster,
			EIGMissingFloorWitness::Unit401DoorRadio,
			EIGMissingFloorWitness::Unit402Silence,
			EIGMissingFloorWitness::RoofDoorWind,
			EIGMissingFloorWitness::BoothInnerRoomHum,
		};
		for (const EIGMissingFloorWitness Witness : Witnesses)
		{
			Narrative->RecordWitness(Witness);
			if (!Narrative->HasWitness(Witness))
			{
				FailProbe(TEXT("an optional sighting did not persist"));
				return;
			}
		}
		if (Narrative->GetWitnessCount() != UE_ARRAY_COUNT(Witnesses))
		{
			FailProbe(FString::Printf(
				TEXT("witness count %d, expected %d"),
				Narrative->GetWitnessCount(),
				static_cast<int32>(UE_ARRAY_COUNT(Witnesses))));
			return;
		}
		// 같은 것을 두 번 본다고 두 번 적히지 않는다.
		if (Narrative->RecordWitness(EIGMissingFloorWitness::HwangWaterBowl))
		{
			FailProbe(TEXT("a sighting was recorded twice"));
			return;
		}
		if (Narrative->GetConfirmedTruthCount() != TruthsBefore)
		{
			FailProbe(TEXT("optional sightings changed the confirmed truths"));
			return;
		}
		// 저장을 한 바퀴 돌려도 살아남고, 모르는 이름은 복원에서 버려진다.
		FIGMissingFloorNarrativeSnapshot WitnessReceipt = Narrative->GetSnapshot();
		WitnessReceipt.Night.Witnesses.Add(FName(TEXT("Seen.NotAThingThisBuildKnows")));
		Narrative->RestoreSnapshot(WitnessReceipt);
		if (Narrative->GetWitnessCount() != UE_ARRAY_COUNT(Witnesses)
			|| !Narrative->HasWitness(EIGMissingFloorWitness::BoothSoundproofing))
		{
			FailProbe(TEXT("witness restore dropped or kept the wrong names"));
			return;
		}
		PassProbe();
		break;
	}

	default:
		break;
	}
}

void AIGListenerGreyboxDirector::EmitProbeNoise()
{
	// 0.5 so the bait still carries (0.2 -> ~5 m) even when it lands inside
	// the entity's own knock-masking window; the probe must not depend on
	// the bang cycle's phase.
	NoiseSubsystem->ReportNoise(ProbeNoiseLocation, 0.5f, Player.Get());
}

void AIGListenerGreyboxDirector::FailProbe(const FString& Reason)
{
	GetWorldTimerManager().ClearTimer(ProbeTimer);
	ProbeStep = EProbeStep::Done;
	UE_LOG(LogTemp, Error, TEXT("MISSINGFLOOR_GREYBOX FAIL: %s"), *Reason);
	RequestExit(true);
}

void AIGListenerGreyboxDirector::PassProbe()
{
	GetWorldTimerManager().ClearTimer(ProbeTimer);
	ProbeStep = EProbeStep::Done;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("MISSINGFLOOR_M6_AUDIO PASS: six buses, score states, title window"));
	UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_GREYBOX PASS"));
	RequestExit(false);
}

void AIGListenerGreyboxDirector::RequestExit(const bool bFailed)
{
	// Same contract as the REBIRTH harnesses: explicit status, normal main
	// loop shutdown so the log flushes.
	FPlatformMisc::RequestExitWithStatus(false, bFailed ? 1 : 0);
}

void AIGListenerGreyboxDirector::RunArrivalProbe()
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	AIGPrologueWorldScene* Scene = WorldScene.Get();
	if (!Scene || !PuzzleTwo || !Scene->AuditPlayerClearance(Player.Get(), PuzzleTwo->GetBoothDoor()))
	{
		UE_LOG(LogTemp, Error, TEXT("MISSINGFLOOR_ARRIVAL FAIL spatial_clearance"));
		RequestExit(true);
		return;
	}
	const UStaticMeshComponent* BoxComponent = ArrivalParcelBox
		? ArrivalParcelBox->GetPresentationMesh()
		: nullptr;
	const UMaterialInterface* BoxMaterial = ArrivalParcelBox
		&& BoxComponent
		? BoxComponent->GetMaterial(0)
		: nullptr;
	const bool bCardboardPbr = BoxMaterial
		&& BoxMaterial->GetPathName().Contains(TEXT("M_MovingBoxCardboardUV"));
	const bool bInitialInteractionGate = ArrivalContract
		&& ArrivalContract->IsInteractionEnabled()
		&& ArrivalParcelBox && ArrivalParcelBox->IsInteractionEnabled()
		&& ArrivalNotebookBox && ArrivalNotebookBox->IsInteractionEnabled()
		&& ArrivalVoicemailBox && ArrivalVoicemailBox->IsInteractionEnabled()
		&& ArrivalStoreBell && ArrivalStoreBell->IsInteractionEnabled()
		&& Unit401Door && Unit401Door->IsInteractionEnabled()
		&& ArrivalUnit402Note && ArrivalUnit402Note->IsInteractionEnabled()
		&& ArrivalRoofLock && ArrivalRoofLock->IsInteractionEnabled()
		&& SleepTarget && !SleepTarget->IsInteractionEnabled();
	const bool bSafeEvening = bProductionMode
		&& Narrative
		&& Narrative->GetNightIndex() == 0
		&& Narrative->HasBeatPlayed(FName(TEXT("Arrival.Started")))
		&& !Narrative->IsHourSealed()
		&& NightPhase && !NightPhase->IsHourActive()
		&& Scene && !Scene->IsTheHourSealed()
		&& Entity && Entity->IsDormant();
	const bool bBoxPlacement = BoxComponent
		&& BoxComponent->Bounds.Origin.Equals(FVector(20.0f, 70.0f, 924.0f), 2.0f)
		&& BoxComponent->Bounds.BoxExtent.Equals(FVector(24.0f, 19.0f, 24.0f), 2.0f);
	if (!bSafeEvening || !bInitialInteractionGate || !bCardboardPbr || !bBoxPlacement)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MISSINGFLOOR_ARRIVAL FAIL: safe=%d gate=%d cardboard_pbr=%d placement=%d"),
			bSafeEvening ? 1 : 0,
			bInitialInteractionGate ? 1 : 0,
			bCardboardPbr ? 1 : 0,
			bBoxPlacement ? 1 : 0);
		RequestExit(true);
		return;
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("MISSINGFLOOR_ARRIVAL box mesh=%s actor=%s bounds_origin=%s bounds_extent=%s scale=%s visible=%d registered=%d nanite_disabled=%d"),
		BoxComponent && BoxComponent->GetStaticMesh()
			? *BoxComponent->GetStaticMesh()->GetPathName()
			: TEXT("none"),
		ArrivalParcelBox ? *ArrivalParcelBox->GetActorLocation().ToCompactString() : TEXT("none"),
		BoxComponent ? *BoxComponent->Bounds.Origin.ToCompactString() : TEXT("none"),
		BoxComponent ? *BoxComponent->Bounds.BoxExtent.ToCompactString() : TEXT("none"),
		BoxComponent ? *BoxComponent->GetComponentScale().ToCompactString() : TEXT("none"),
		BoxComponent && BoxComponent->IsVisible() ? 1 : 0,
		BoxComponent && BoxComponent->IsRegistered() ? 1 : 0,
		BoxComponent && BoxComponent->bDisallowNanite ? 1 : 0);

	HandleArrivalEvidence(ArrivalRoofLock);
	const bool bRoofFirstSafe = !Narrative->HasBeatPlayed(FName(TEXT("Arrival.Complete"))) && !SleepTarget->IsInteractionEnabled();
	HandleArrivalEvidence(ArrivalStoreBell);
	HandleUnit401Knocked(Unit401Door);
	HandleArrivalEvidence(ArrivalUnit402Note);
	HandleArrivalEvidence(ArrivalVoicemailBox);
	HandleArrivalEvidence(ArrivalNotebookBox);
	HandleArrivalEvidence(ArrivalParcelBox);
	const bool bLastClueRequired = !SleepTarget->IsInteractionEnabled();
	HandleArrivalEvidence(ArrivalContract);
	HandleNeighborhoodDeliveryRead(NeighborhoodDeliveryNote, true);
	const bool bDeliverySaved = Narrative->HasBeatPlayed(FName(TEXT("Neighborhood.Delivery")))
		&& GetNarinCounterLine().EqualTo(NSLOCTEXT("IGMissingFloor", "NarinDeliveryAnswer",
			"작년 여름에 몇 번 맡겼어요. 마지막 건 건물주 아저씨가 찾아가셨고요. 오빠분이 부탁했다고 하던데요."));
	if (!bRoofFirstSafe || !bLastClueRequired || !SleepTarget->IsInteractionEnabled() || !bDeliverySaved)
	{
		UE_LOG(LogTemp, Error, TEXT("MISSINGFLOOR_ARRIVAL FAIL free_order=%d last_clue=%d sleep=%d delivery=%d"),
			bRoofFirstSafe, bLastClueRequired, SleepTarget->IsInteractionEnabled(), bDeliverySaved);
		RequestExit(true);
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("MISSINGFLOOR_ARRIVAL PASS props=7 night=0 hour_sealed=0 cardboard_pbr=1 free_order=1 delivery_branch=1"));
	RequestExit(false);
}

void AIGListenerGreyboxDirector::StartArrivalCapture()
{
	if (!bProductionMode || !Player.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("MISSINGFLOOR_ARRIVAL_CAPTURE FAIL: production stage missing"));
		RequestExit(true);
		return;
	}
	for (TActorIterator<AIGWakeUpDirector> It(GetWorld()); It; ++It)
	{
		It->RestoreStandingCheckpoint();
		break;
	}
	Player->SetCameraMotionEnabled(false);
	if (GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("DisableAllScreenMessages"));
	}
	ArrivalCaptureStep = 0;
	AdvanceArrivalCapture();
	GetWorldTimerManager().SetTimer(
		ArrivalCaptureTimer,
		this,
		&AIGListenerGreyboxDirector::AdvanceArrivalCapture,
		1.4f,
		true);
}

void AIGListenerGreyboxDirector::AdvanceArrivalCapture()
{
	// 승강기 대기 공간은 정면뿐 아니라 출입구와 옆 벽에서도 확인한다.
	if (FParse::Param(FCommandLine::Get(), TEXT("IGLandingAudit")))
	{
		switch (ArrivalCaptureStep++)
		{
		case 0: CaptureTeleportPlayer(FVector(490, -305, 98), 0, 0); break;
		case 5: CaptureShot(TEXT("landing-lobby-lift")); break;
		case 4:
			if (GEngine && FParse::Param(FCommandLine::Get(), TEXT("IGLandingMetrics")))
				GEngine->Exec(GetWorld(), TEXT("csvprofile start"));
			break;
		case 6: CaptureTeleportPlayer(FVector(620, -330, 98), 135, -8); break;
		case 9: CaptureShot(TEXT("landing-lobby-mail")); break;
		case 10: CaptureTeleportPlayer(FVector(643, -490, 98), 90, -4); break;
		case 13: CaptureShot(TEXT("landing-entrance")); break;
		case 14: CaptureTeleportPlayer(FVector(520, -305, 998), 0, -8); break;
		case 17: CaptureShot(TEXT("landing-fourth-lift")); break;
		case 18: CaptureTeleportPlayer(FVector(640, -335, 998), 140, -12); break;
		case 21: CaptureShot(TEXT("landing-fourth-return")); break;
		case 22: CaptureTeleportPlayer(FVector(330, -305, 998), 0, -15); break;
		case 25: CaptureShot(TEXT("landing-corridor-approach")); break;
		case 26: CaptureTeleportPlayer(FVector(30, -310, 998), 180, -8); break;
		case 29: CaptureShot(TEXT("landing-neighbor-doors")); break;
		case 30: CaptureTeleportPlayer(FVector(500, -205, 98), -22, -14); break;
		case 33: CaptureShot(TEXT("landing-lobby-wide")); break;
		case 34: CaptureTeleportPlayer(FVector(632, -257, 98), 90, -8); break;
		case 37: CaptureShot(TEXT("landing-noticeboard")); break;
		case 38: CaptureTeleportPlayer(FVector(401, -287, 98), -90, -8); break;
		case 41: CaptureShot(TEXT("landing-meter-record")); break;
		case 42: CaptureTeleportPlayer(FVector(637, -275, 998), -72, -23); break;
		case 45: CaptureShot(TEXT("landing-fourth-waiting")); break;
		case 46:
			CaptureTeleportPlayer(FVector(600, -305, 998), 0, -8);
			for (TActorIterator<AIGElevator> It(GetWorld()); It; ++It)
			{
				FIGInteractionContext Context;
				Context.Interactor = Player.Get();
				Context.TargetActor = *It;
				IIGInteractable::Execute_CompleteInteraction(*It, Context);
				break;
			}
			break;
		case 50: CaptureShot(TEXT("landing-lift-open")); break;
		case 51:
			if (GEngine && FParse::Param(FCommandLine::Get(), TEXT("IGLandingMetrics")))
				GEngine->Exec(GetWorld(), TEXT("csvprofile stop"));
			break;
		case 53:
			GetWorldTimerManager().ClearTimer(ArrivalCaptureTimer);
			UE_LOG(LogTemp, Display, TEXT("LANDING_CAPTURE PASS shots=12 production=1"));
			RequestExit(false);
			break;
		default: break;
		}
		return;
	}
	// 새 설비의 앞·옆면을 실제 플레이 화면에서 확인한다.
	if (FParse::Param(FCommandLine::Get(), TEXT("IGFixtureAudit")))
	{
		switch (ArrivalCaptureStep++)
		{
		case 0:
			PuzzleOne->SetHourActive(true);
			PuzzleTwo->SetHourActive(true);
			PuzzleTwo->GetBoothDoor()->ForceOpenState(true);
			CaptureTeleportPlayer(FVector(505, -285, 98), -90, -8);
			break;
		case 5: CaptureTeleportPlayer(FVector(75, -223, 98), 90, -70); break;
		case 7: CaptureShot(TEXT("utility-booth-valve")); break;
		case 8: CaptureTeleportPlayer(FVector(505, -285, 98), -90, -8); break;
		case 9: CaptureShot(TEXT("utility-meter-wide")); break;
		case 10: CaptureTeleportPlayer(FVector(542, -300, 98), -90, -8); break;
		case 13: CaptureShot(TEXT("utility-meter-close")); break;
		case 14: CaptureTeleportPlayer(FVector(150, -189, 98), 90, -34); break;
		case 17: CaptureShot(TEXT("utility-booth-front")); break;
		case 18: CaptureTeleportPlayer(FVector(190, -182, 98), 118, -35); break;
		case 21: CaptureShot(TEXT("utility-booth-side")); break;
		case 22: CaptureTeleportPlayer(FVector(5, 305, 1298), -90, -12); break;
		case 25: CaptureShot(TEXT("utility-tank-steel")); break;
		case 26: CaptureTeleportPlayer(FVector(52, 57, 997), 7, -34); break;
		case 29: CaptureShot(TEXT("utility-countertop")); break;
		case 30:
			CaptureTeleportPlayer(FVector(220, -222, 98), 160, -32);
			break;
		case 33: CaptureShot(TEXT("utility-booth-pump")); break;
		case 34:
			if (FParse::Param(FCommandLine::Get(), TEXT("IGBakeCctv"))) break;
			GetWorldTimerManager().ClearTimer(ArrivalCaptureTimer);
			UE_LOG(LogTemp, Display, TEXT("FIXTURE_CAPTURE PASS shots=8 production=1"));
			RequestExit(false);
			break;
		case 35:
			// 저작용 카메라 위치다. 중력으로 바닥에 떨어지지 않게 이 검사에서만 고정한다.
			Player->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			Player->GetCharacterMovement()->StopMovementImmediately();
			Player->SetActorEnableCollision(false);
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{ if (AHUD* HUD = PC->GetHUD()) HUD->bShowHUD = false; }
			CaptureTeleportPlayer(FVector(490, -255, 165), -30, -25);
			break;
		case 38: CaptureShot(TEXT("cctv-source-entrance"), false); break;
		case 39: CaptureTeleportPlayer(FVector(2000, -460, 166), 180, -12); break;
		case 42: CaptureShot(TEXT("cctv-source-parking"), false); break;
		case 43: CaptureTeleportPlayer(FVector(30, -325, 1036), 180, -18); break;
		case 46: CaptureShot(TEXT("cctv-source-stair"), false); break;
		case 47: CaptureTeleportPlayer(FVector(680, -325, 1040), 180, -13); break;
		case 50: CaptureShot(TEXT("cctv-source-corridor"), false); break;
		case 51:
			GetWorldTimerManager().ClearTimer(ArrivalCaptureTimer);
			UE_LOG(LogTemp, Display, TEXT("FIXTURE_CAPTURE PASS shots=12 production=1 atlas_sources=4"));
			RequestExit(false);
			break;
		default: break;
		}
		return;
	}
	// 실제 시작 단계의 조명과 인물이 유지된 상태에서 매장과 뒷길을 검토한다.
	if (FParse::Param(FCommandLine::Get(), TEXT("IGRetailCapture")))
	{
		switch (ArrivalCaptureStep++)
		{
		case 0: CaptureTeleportPlayer(FVector(2000, -460, 98), 0, 10); break;
		case 10: CaptureShot(TEXT("retail-exterior")); break;
		case 11:
			CaptureTeleportPlayer(FVector(2590, -290, 104), 90, -3);
			if (GEngine && FParse::Param(FCommandLine::Get(), TEXT("IGRetailMetrics")))
				GEngine->Exec(GetWorld(), TEXT("csvprofile start"));
			break;
		case 15: CaptureShot(TEXT("retail-counter")); break;
		case 16: CaptureTeleportPlayer(FVector(2710, -275, 104), -90, -18); break;
		case 20: CaptureShot(TEXT("retail-stock")); break;
		case 21: CaptureTeleportPlayer(FVector(2930, -460, 104), -10, -12); break;
		case 25: CaptureShot(TEXT("retail-cooler")); break;
		case 26: CaptureTeleportPlayer(FVector(2050, -1380, 98), -90, -9); break;
		case 30: CaptureShot(TEXT("retail-delivery-lane")); break;
		case 31:
			if (GEngine && FParse::Param(FCommandLine::Get(), TEXT("IGRetailMetrics")))
				GEngine->Exec(GetWorld(), TEXT("csvprofile stop"));
			break;
		case 33:
			CaptureTeleportPlayer(FVector(2590, -270, 104), 90, -9);
			break;
		case 36: CaptureShot(TEXT("retail-clerk")); break;
		case 37:
			CaptureTeleportPlayer(FVector(52, 57, 997), 7, -34);
			break;
		case 40: CaptureShot(TEXT("kitchen-microwave")); break;
		case 41:
			if (FParse::Param(FCommandLine::Get(), TEXT("IGRetailAudit"))) break;
			GetWorldTimerManager().ClearTimer(ArrivalCaptureTimer);
			UE_LOG(LogTemp, Display, TEXT("RETAIL_CAPTURE PASS shots=%d production=1 d3d12=1"), bCaptureMetricsOnly ? 0 : 7);
			RequestExit(false);
			break;
		case 42: CaptureTeleportPlayer(FVector(2520, -277, 104), 65, -6); break;
		case 44: CaptureShot(TEXT("retail-clerk-left")); break;
		case 45: CaptureTeleportPlayer(FVector(2680, -280, 104), 120, -6); break;
		case 48: CaptureShot(TEXT("retail-clerk-right")); break;
		case 49: CaptureTeleportPlayer(FVector(2580, -345, 104), 87, -5); break;
		case 52: CaptureShot(TEXT("retail-clerk-distance")); break;
		case 53: CaptureTeleportPlayer(FVector(2758, -727, 104), 90, -48); break;
		case 56: CaptureShot(TEXT("retail-packaging-back")); break;
		case 57: CaptureTeleportPlayer(FVector(2550, -726, 104), 180, -22); break;
		case 60: CaptureShot(TEXT("retail-seating")); break;
		case 61: CaptureTeleportPlayer(FVector(-85, 40, 997), -145, -26); break;
		case 64: CaptureShot(TEXT("bedroom-lamp")); break;
		case 65: CaptureTeleportPlayer(FVector(1950, -1300, 98), 180, -4); break;
		case 68: CaptureShot(TEXT("retail-backlane-wide")); break;
		case 69:
			if (FParse::Param(FCommandLine::Get(), TEXT("IGSpatialAudit"))) break;
			GetWorldTimerManager().ClearTimer(ArrivalCaptureTimer);
			UE_LOG(LogTemp, Display, TEXT("RETAIL_CAPTURE PASS shots=14 production=1 d3d12=1"));
			RequestExit(false);
			break;
		case 70: CaptureTeleportPlayer(FVector(70, -100, 998), -64, -14); break;
		case 73: CaptureShot(TEXT("spatial-home-entry")); break;
		case 74: CaptureTeleportPlayer(FVector(-130, -305, 998), 0, -24); break;
		case 77: CaptureShot(TEXT("spatial-corridor-floor")); break;
		case 78: CaptureTeleportPlayer(FVector(165, -325, 98), 90, -12); break;
		case 81: CaptureShot(TEXT("spatial-booth-entry")); break;
		case 82:
			// 낮에는 잠긴 방이라, 내부 소품 검수 동안만 문을 열어 둔다.
			PuzzleTwo->GetBoothDoor()->ForceOpenState(true);
			CaptureTeleportPlayer(FVector(165, -245, 98), 35, -25);
			break;
		case 85: CaptureShot(TEXT("spatial-booth-storage")); break;
		case 86:
			PuzzleTwo->GetBoothDoor()->ForceOpenState(false);
			CaptureTeleportPlayer(FVector(2710, -500, 104), 0, -60);
			break;
		case 89: CaptureShot(TEXT("spatial-store-floor")); break;
		case 90: CaptureTeleportPlayer(FVector(5, 305, 1298), -90, -12); break;
		case 93: CaptureShot(TEXT("spatial-roof-valves")); break;
		case 94: CaptureTeleportPlayer(FVector(5, 12, 998), 135, -58); break;
		case 97: CaptureShot(TEXT("spatial-slippers")); break;
		case 98:
			GetWorldTimerManager().ClearTimer(ArrivalCaptureTimer);
			UE_LOG(LogTemp, Display, TEXT("RETAIL_CAPTURE PASS shots=21 production=1 d3d12=1"));
			RequestExit(false);
			break;
		default: break;
		}
		return;
	}
	switch (ArrivalCaptureStep++)
	{
	case 0:
		CaptureTeleportPlayer(FVector(-120.0f, -5.0f, 997.0f), 34.0f, -24.0f);
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		// PSO와 셰이더 준비를 마치고 4.6초짜리 챕터 카드가 완전히 사라질 때까지 기다린다.
		break;
	case 8:
		CaptureShot(TEXT("readme/arrival-moving-boxes"));
		break;
	case 9:
		CaptureTeleportPlayer(FVector(-82.0f, -92.0f, 997.0f), -90.0f, -32.0f);
		break;
	case 10:
	case 11:
		break;
	case 12:
		CaptureShot(TEXT("readme/arrival-contract"));
		break;
	case 13:
	case 14:
		break;
	case 15:
	{
		GetWorldTimerManager().ClearTimer(ArrivalCaptureTimer);
		const FString BoxPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(), TEXT("Docs/Media/readme/arrival-moving-boxes.png")));
		const FString ContractPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(), TEXT("Docs/Media/readme/arrival-contract.png")));
		if (!IFileManager::Get().FileExists(*BoxPath)
			|| !IFileManager::Get().FileExists(*ContractPath))
		{
			UE_LOG(LogTemp, Error, TEXT("MISSINGFLOOR_ARRIVAL_CAPTURE FAIL: screenshot write incomplete"));
			RequestExit(true);
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_ARRIVAL_CAPTURE PASS shots=2 d3d12=1"));
		RequestExit(false);
		break;
	}
	default:
		break;
	}
}

// -- README/night capture tour ---------------------------------------------
//
// README에서 설명하는 시스템을 실제로 찍는 18개 연출 구간이다.
// 기존 데모 캡처와 똑같이 정지 화면은 Docs/Media/<name>.png에,
// 연속 프레임은 Saved/NightCapture/<dir>/frame_%05d.png에 남겨
// ffmpeg GIF 조립 경로를 하나로 유지한다.

void AIGListenerGreyboxDirector::StartNightCapture()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Release the camera from the wake intro: the capture starts standing.
	for (TActorIterator<AIGWakeUpDirector> It(World); It; ++It)
	{
		It->RestoreStandingCheckpoint();
		if (!It->IsFreeRoam())
		{
			It->RequestStopAlarmFallback();
			It->CompleteGettingUp();
		}
		break;
	}
	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("DisableAllScreenMessages"), true);
	}
	// 셰이더가 아직 컴파일 중인 재질은 엔진이 격자로 그린다. 프롤로그 캡처는
	// 이것을 이미 막고 있었지만 밤 캡처는 막지 않아서, 같은 커밋에서도 실행에
	// 따라 격자가 찍힌 프레임과 안 찍힌 프레임이 나왔다.
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	// The dying west fixture must not strobe the stair still.
	if (AIGPrologueWorldScene* SceneNow =
		const_cast<AIGPrologueWorldScene*>(WorldScene.Get()))
	{
		SceneNow->SuspendCorridorFlicker(true);
	}

	int32 StartStep = 0;
	FParse::Value(
		FCommandLine::Get(),
		TEXT("IGNightCaptureStartStep="),
		StartStep);
	EnterCaptureStep(FMath::Clamp(StartStep, 0, 18));
	if (StartStep > 0)
	{
		// A direct art-review stop still begins while the normal night card is
		// fading. Keep timed actions behind that card instead of photographing it.
		CaptureStepSeconds = StartStep == 18 ? -8.0f : -4.0f;
		if (APlayerController* PlayerController =
			World->GetFirstPlayerController())
		{
			if (AHUD* Hud = PlayerController->GetHUD())
			{
				Hud->bShowHUD = false;
			}
		}
	}
	GetWorldTimerManager().SetTimer(
		CaptureTimer,
		this,
		&AIGListenerGreyboxDirector::AdvanceNightCapture,
		0.04f,
		true);
}

namespace IGNightHistogram
{
	/** How the point stages itself before the frame is measured. */
	enum class ESetup : uint8
	{
		/** Torch off, entity parked far away. The corridor as authored. */
		DarkCorridor,
		/** Torch on, looking down the beam. V1's dust motes live here. */
		BeamDust,
		/** Torch off, entity close and lit only by its rim. */
		EntityRim,
		/** Torch on, against the fifth-floor cavity wall. */
		CavityWall,
		/** Torch on, over the settled dust and drag residue on 5F. */
		ResidueFifthFloor,
		/** Torch on, over the corridor runner and the meter box rust. */
		ResidueCorridor,
		/** Chase post-process at full pressure. */
		ChasePost,
		/** A loud noise report, so the ripple ring is on screen. */
		RippleRing,
		/**
		 * Torch on and nothing else. The residue setups also push a dust
		 * disturbance into the air, which is right when the point exists to
		 * show dust and wrong when it exists to read a floor material: the
		 * motes sit between the lens and the surface being judged.
		 */
		SurfaceReading
	};

	struct FPoint
	{
		const TCHAR* Name = TEXT("");
		ESetup Setup = ESetup::DarkCorridor;
		FVector PlayerLocation = FVector::ZeroVector;
		float PlayerYaw = 0.0f;
		float PlayerPitch = 0.0f;
		/** Fraction of pixels below 5% luminance. */
		float ShadowMinimum = 0.0f;
		float ShadowMaximum = 1.0f;
		/** Fraction of pixels above 98% luminance. */
		float HighlightMaximum = 1.0f;
		/**
		 * The HUD is off for every point but one. §11 V4 draws the noise ripple
		 * as a screen-edge arc, so it is a HUD element by design and measuring it
		 * with the HUD hidden would measure an empty corridor instead. Everywhere
		 * else a caption sitting in frame would count its own pixels into both
		 * tails, so it stays off.
		 */
		bool bShowHud = false;
	};

	/**
	 * §11 V5's eight night viewpoints, with bands measured on this build at
	 * 1280x720 and then opened by ±0.10 — never authored first and loosened
	 * until they passed, which would only have proved the bands were loose.
	 * Run-to-run spread is about ±0.001, so the margin exists for a different
	 * GPU's temporal convergence, not for drift in the lighting.
	 *
	 * Both bounds carry weight. The floor catches somebody raising the exposure
	 * or adding a light; the ceiling catches the torch failing or the scene
	 * going black, which is the failure that nearly passed this sweep silently.
	 *
	 * One measured number does not match the design's language: 복도 암부 sits
	 * at 20% of pixels below 5% luminance, where V1's "주광 0, 암부가 진짜 검게
	 * 떨어지도록" reads like it should be most of the frame. The band locks what
	 * actually ships rather than what the sentence implies, and the gap is noted
	 * in IMPLEMENTATION_STATUS for a human to settle by looking.
	 */
	const FPoint Points[] =
	{
		{
			TEXT("corridor_dark"), ESetup::DarkCorridor,
			FVector(60.0f, -305.0f, 997.0f), 0.0f, -3.0f,
			0.14f, 0.34f, 0.010f
		},
		{
			TEXT("beam_dust"), ESetup::BeamDust,
			FVector(-60.0f, -305.0f, 997.0f), 0.0f, -6.0f,
			// 0.18~0.38은 측정값 0.2801에서 저작했는데 그 값이 재현되지 않는다.
			// 커밋된 자산으로 두 번, 두 번 구운 뒤 한 번, DDC 지우고 구운 직후
			// 한 번 — 네 조건 전부 0.115대다. 0.28대가 나온 실행은 딱 한 번
			// 있었고 그 뒤로 다시 나오지 않았다. 원인은 못 찾았지만 여섯 번 중
			// 다섯 번이 일치하는 쪽을 정본으로 둔다.
			//
			// 처음에는 구워진 uasset이 소스보다 오래된 탓이라고 봤는데 아니다.
			// 같은 소스로 두 번 구우면 소스 PNG는 바이트까지 같고, 그렇게 구운
			// 자산으로 재면 커밋된 자산과 같은 값이 나온다.
			0.02f, 0.22f, 0.010f
		},
		{
			TEXT("entity_rim"), ESetup::EntityRim,
			// 존재는 바닥을 기므로 -4도 시점에서는 실루엣이 화면 아래로 잘리고 빈
			// 문만 측정됐다. X=330에서 존재를 실제 복도등 바로 아래인 X=70에 두면,
			// 검수 전용 보조광 없이 장면에 배치된 실등으로 윤곽을 확인할 수 있다.
			// 커밋된 자산 기준 실측 0.2519 / 0.2524. 복도 아트가 08-18에 올라오면서
			// 벽지와 테라조 바닥이 빛을 더 돌려주고, 그만큼 암부가 줄었다.
			FVector(330.0f, -305.0f, 997.0f), 180.0f, -18.0f,
			0.15f, 0.35f, 0.010f
		},
		{
			TEXT("cavity_wall"), ESetup::CavityWall,
			FVector(120.0f, 700.0f, 1297.0f), 0.0f, -2.0f,
			0.14f, 0.34f, 0.010f
		},
		{
			// The first stance faced +X/−Y from (60, 760) and put both floor
			// residues *behind* the camera: the frame was the bay wall and the
			// slab's own grain, so raising the drag-trail material moved
			// 0.04/255 of it and the band was satisfied by pixels that had
			// nothing to do with the residue. A point named for a thing it does
			// not contain is the same defect as a brightness floor that black
			// satisfies. From here the camera is 161 cm over the slab
			// (pawn centre + 64) with a 78° lens, and this framing was checked by
			// projecting both residue masks through this exact transform before
			// it was committed: 100% of the drag trail and 99.9% of the dust
			// joint land inside the frame, the trail right of centre as the
			// subject and the joint running the north wall line behind it. X is
			// 208 rather than 215 so the 34 cm capsule clears the bay stud face
			// at 247: the teleport is bNoCheck, and a pawn that has to resolve
			// penetration moves before the shot, which would make this frame
			// unrepeatable for reasons that have nothing to do with lighting.
			// 밴드는 별관 바닥이 제대로 매핑된 뒤에 다시 쟀다. 이전 0.30..0.52는
			// M_ConcreteDark_X가 수평 슬래브에서 텍스처를 한 줄로 늘려 밝은
			// 띠를 만들던 시절의 값이고, 바닥이 실제 콘크리트 분포를 되찾자
			// 0.6387로 올라갔다. 프레임이 바닥으로 가득 찬 시점이라 암부 비율이
			// 높은 것이 정상이다.
			TEXT("residue_fifth_floor"), ESetup::ResidueFifthFloor,
			FVector(208.0f, 600.0f, 1297.0f), 125.0f, -38.0f,
			0.53f, 0.75f, 0.010f
		},
		{
			TEXT("residue_corridor"), ESetup::ResidueCorridor,
			FVector(80.0f, -300.0f, 997.0f), 190.0f, -34.0f,
			0.14f, 0.34f, 0.010f
		},
		{
			TEXT("chase_post"), ESetup::ChasePost,
			FVector(200.0f, -305.0f, 997.0f), 180.0f, -3.0f,
			0.24f, 0.44f, 0.010f
		},
		{
			// Down the corridor, not across it: at yaw 90 the player is 70 cm
			// from the north wall and the frame is a close-up of plaster, which
			// measured 91% black and told us nothing about the ripple.
			TEXT("ripple_ring"), ESetup::RippleRing,
			FVector(-40.0f, -305.0f, 997.0f), 0.0f, -3.0f,
			0.21f, 0.41f, 0.010f, /*bShowHud=*/true
		},
		{
			// §11 규칙 2는 「어느 바닥을 고르느냐」를 선택으로 만든다. 그
			// 선택은 표면이 눈으로 구분될 때에만 존재하므로, 소리가 갈리는
			// 두 바닥에도 프레임이 있어야 한다. 계단은 X=-277.5 수직통로를
			// +Y로 오르고, 카메라는 상단 착지에서 -Y로 내려다본다 — 디딤판
			// 윗면이 프레임을 채우는 유일한 각도다.
			// -42°는 계단통 전체를 담았지만 디딤판 하나가 화면에서 8px이라
			// 트레드 무늬를 판정할 수 없었다. -65°는 바로 아래 서너 단을
			// 크게 잡는다 — 밟기 전에 실제로 내려다보는 각도이기도 하다.
			// 실측 0.1130 / 0.1136. 계단통은 좁고 벽이 빛을 되돌려 주어서 밝은
			// 편이다. 0.00~1.00은 저작 전 자리표시였고 그 상태로는 아무것도
			// 걸러내지 못한다.
			TEXT("steel_stair"), ESetup::SurfaceReading,
			FVector(-277.5f, 190.0f, 1297.0f), 270.0f, -65.0f,
			0.02f, 0.21f, 0.010f
		},
		{
			// 옥상 route 슬래브의 상단면은 Z=1200이고 서쪽 구간이 가장 넓다.
			// 계단통과 달리 여기에는 빛을 되돌려 줄 벽이 없다. -38°에서는
			// 프레임의 87%가 5% 미만이었고 빔은 우하단 모서리에만 걸렸다 —
			// 밝기 문제가 아니라 시선과 빔이 만나지 않는 구도의 문제다.
			// 발 앞으로 내리면 빔 안쪽에서 도막을 읽을 수 있다.
			TEXT("rooftop_deck"), ESetup::SurfaceReading,
			// (0,-40)은 360cm 물탱크 받침대 내부라 기존 프레임에는 검은 안쪽 면과
			// 잘린 손전등 모서리만 보였다. 폭 160cm 서쪽 보행 슬래브에서 정비 통로를
			// 따라 바라보도록 옮긴다.
			// 실측 0.5935 / 0.5936. 옥상은 빛을 되돌려 줄 벽이 없어서 손전등이
			// 닿는 자리 말고는 전부 떨어진다. 암부 비율이 높은 것이 정상이다.
			FVector(-410.0f, -40.0f, 1297.0f), 90.0f, -34.0f,
			0.49f, 0.69f, 0.010f
		}
	};

	constexpr int32 PointCount = UE_ARRAY_COUNT(Points);

	/** §11 V5 luminance thresholds. */
	constexpr float ShadowThreshold = 0.05f;
	constexpr float HighlightThreshold = 0.98f;

	/**
	 * Lumen and TSR both need frames to converge, and a temporal history that
	 * has not settled reads darker than the authored frame. Measuring early
	 * would quietly pass every shadow floor for the wrong reason.
	 */
	constexpr float SettleSeconds = 1.60f;
	constexpr float TickSeconds = 0.04f;

	/**
	 * How long a requested frame may take to arrive before the sweep gives up.
	 * Without this a run that cannot render at all (a -nullrhi invocation, say)
	 * hangs instead of saying why, and a hang is a worse answer than a failure.
	 */
	constexpr float ShotTimeoutSeconds = 20.0f;
}

void AIGListenerGreyboxDirector::StartHistogramSweep()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Same release as the capture tour: measure a standing player, not the wake
	// intro's pinned camera.
	for (TActorIterator<AIGWakeUpDirector> It(World); It; ++It)
	{
		It->RestoreStandingCheckpoint();
		if (!It->IsFreeRoam())
		{
			It->RequestStopAlarmFallback();
			It->CompleteGettingUp();
		}
		break;
	}
	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("DisableAllScreenMessages"), true);
		if (AHUD* Hud = PlayerController->GetHUD())
		{
			// The HUD is not part of the lighting claim, and a caption sitting in
			// frame would count its own pixels into both tails.
			Hud->bShowHUD = false;
		}
	}
	// A strobing fixture would make every measurement a coin toss on which
	// frame it landed.
	if (AIGPrologueWorldScene* SceneNow =
		const_cast<AIGPrologueWorldScene*>(WorldScene.Get()))
	{
		SceneNow->SuspendCorridorFlicker(true);
	}

	HistogramFailures = 0;
	HistogramMeasured = 0;
	bHistogramShotPending = false;
	// The screenshot pipeline hands over the frame it actually captured, which
	// is the only frame that exists when no swap chain does.
	HistogramScreenshotHandle =
		UGameViewportClient::OnScreenshotCaptured().AddUObject(
			this,
			&AIGListenerGreyboxDirector::HandleHistogramScreenshot);
	EnterHistogramPoint(0);
	GetWorldTimerManager().SetTimer(
		HistogramTimer,
		this,
		&AIGListenerGreyboxDirector::AdvanceHistogramSweep,
		IGNightHistogram::TickSeconds,
		true);
}

void AIGListenerGreyboxDirector::EnterHistogramPoint(const int32 PointIndex)
{
	HistogramPointIndex = PointIndex;
	HistogramPointSeconds = 0.0f;
	if (!IGNightHistogram::Points || PointIndex < 0
		|| PointIndex >= IGNightHistogram::PointCount)
	{
		return;
	}

	const IGNightHistogram::FPoint& Point = IGNightHistogram::Points[PointIndex];
	AIGPlayerCharacter* PlayerCharacter = Player.Get();
	AIGListenerEntity* EntityActor = Entity.Get();
	UIGFlashlightComponent* Torch = PlayerCharacter
		? PlayerCharacter->GetFlashlight()
		: nullptr;
	UWorld* World = GetWorld();

	CaptureTeleportPlayer(Point.PlayerLocation, Point.PlayerYaw, Point.PlayerPitch);

	if (APlayerController* PlayerController = World
		? World->GetFirstPlayerController()
		: nullptr)
	{
		if (AHUD* Hud = PlayerController->GetHUD())
		{
			Hud->bShowHUD = Point.bShowHud;
		}
	}

	// Park the entity out of frame by default; only two points want it visible.
	if (EntityActor)
	{
		EntityActor->SetDormant(false);
		CaptureParkEntity(FVector(640.0f, -305.0f, 960.0f), 180.0f);
	}
	if (Torch)
	{
		Torch->SetAvailable(true);
		Torch->SetOn(false);
	}
	if (UIGMissingFloorAudioSubsystem* AudioDirector = World
		? World->GetSubsystem<UIGMissingFloorAudioSubsystem>()
		: nullptr)
	{
		AudioDirector->SetThreatState(EIGAudioThreatState::Calm);
	}
	if (UIGStressComponent* Stress = PlayerCharacter
		? PlayerCharacter->GetStress()
		: nullptr)
	{
		Stress->SetThreatPressure(0.0f);
	}

	switch (Point.Setup)
	{
	case IGNightHistogram::ESetup::DarkCorridor:
		break;

	case IGNightHistogram::ESetup::SurfaceReading:
		if (Torch)
		{
			Torch->SetOn(true);
		}
		break;

	case IGNightHistogram::ESetup::BeamDust:
	case IGNightHistogram::ESetup::CavityWall:
	case IGNightHistogram::ESetup::ResidueFifthFloor:
	case IGNightHistogram::ESetup::ResidueCorridor:
		if (Torch)
		{
			Torch->SetOn(true);
		}
		// The residue and beam points want his lane in the air, otherwise the
		// two dust systems are measured without the thing they exist to show.
		if (UIGDustSubsystem* Dust = World
			? World->GetSubsystem<UIGDustSubsystem>()
			: nullptr)
		{
			const FVector Ahead = Point.PlayerLocation
				+ FRotator(0.0f, Point.PlayerYaw, 0.0f).Vector() * 220.0f;
			Dust->ReportDisturbance(Ahead, 1.0f);
			Dust->ReportSettledPrint(
				Ahead,
				Point.PlayerYaw,
				EIGDustPrintKind::Drag);
			Dust->ReportSettledPrint(
				Point.PlayerLocation
					+ FRotator(0.0f, Point.PlayerYaw, 0.0f).Vector() * 120.0f,
				Point.PlayerYaw,
				EIGDustPrintKind::Footfall);
		}
		break;

	case IGNightHistogram::ESetup::EntityRim:
		// Close enough to fill frame, far enough not to trip the capture radius.
		CaptureParkEntity(
			Point.PlayerLocation
				+ FRotator(0.0f, Point.PlayerYaw, 0.0f).Vector() * 260.0f
				- FVector(0.0f, 0.0f, 39.0f),
			Point.PlayerYaw + 180.0f);
		break;

	case IGNightHistogram::ESetup::ChasePost:
		if (UIGStressComponent* Stress = PlayerCharacter
			? PlayerCharacter->GetStress()
			: nullptr)
		{
			Stress->SetThreatPressure(0.95f);
		}
		if (UIGMissingFloorAudioSubsystem* AudioDirector = World
			? World->GetSubsystem<UIGMissingFloorAudioSubsystem>()
			: nullptr)
		{
			AudioDirector->SetThreatState(EIGAudioThreatState::Chasing);
		}
		break;

	case IGNightHistogram::ESetup::RippleRing:
		if (UIGNoiseSubsystem* Noise = NoiseSubsystem)
		{
			// A hammer-loud report right beside the player: the §5.1 ripple ring
			// is an edge arc, so it has to be freshly triggered to be in frame.
			Noise->SetGlobalMasking(0.0f);
			Noise->ReportNoise(
				Point.PlayerLocation
					+ FRotator(0.0f, Point.PlayerYaw, 0.0f).Vector() * 90.0f,
				1.0f,
				PlayerCharacter);
		}
		break;

	default:
		break;
	}
}

void AIGListenerGreyboxDirector::AdvanceHistogramSweep()
{
	// While a shot is in flight the delegate owns the sweep. Requesting another
	// would measure one point against another point's frame.
	if (bHistogramShotPending)
	{
		HistogramShotWaitSeconds += IGNightHistogram::TickSeconds;
		if (HistogramShotWaitSeconds < IGNightHistogram::ShotTimeoutSeconds)
		{
			return;
		}
		// Nothing is rendering, so nothing can be measured. Say so instead of
		// waiting forever: a sweep that hangs looks like a slow machine, and a
		// sweep that fails looks like the missing RHI it actually is.
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MISSINGFLOOR_V5 FAIL: no frame arrived for point %s within "
				"%.0f s. The sweep needs a real RHI — run it with "
				"-RenderOffScreen -d3d12 and never with -nullrhi."),
			IGNightHistogram::Points[
				FMath::Clamp(HistogramPointIndex, 0, IGNightHistogram::PointCount - 1)].Name,
			IGNightHistogram::ShotTimeoutSeconds);
		GetWorldTimerManager().ClearTimer(HistogramTimer);
		UGameViewportClient::OnScreenshotCaptured().Remove(
			HistogramScreenshotHandle);
		FPlatformMisc::RequestExit(false);
		return;
	}
	HistogramPointSeconds += IGNightHistogram::TickSeconds;
	if (HistogramPointSeconds < IGNightHistogram::SettleSeconds)
	{
		return;
	}
	if (HistogramPointIndex < 0
		|| HistogramPointIndex >= IGNightHistogram::PointCount)
	{
		GetWorldTimerManager().ClearTimer(HistogramTimer);
		UGameViewportClient::OnScreenshotCaptured().Remove(
			HistogramScreenshotHandle);
		return;
	}

	// A frame is kept for every point so the art review can happen later without
	// anyone having to run the engine again to look — and the same captured
	// frame is what gets measured, so the number and the picture always agree.
	//
	// The request carries no filename on purpose. UGameViewportClient writes the
	// PNG **only when nothing is bound** to OnScreenshotCaptured — the engine's
	// own comment is «If delegate subscribed, fire it instead of writing out a
	// file to disk». This sweep must be bound to measure the pixels, so the file
	// is ours to write, and WriteHistogramFrame does it from the very bitmap the
	// numbers came from. Passing a path here instead would silently do nothing.
	bHistogramShotPending = true;
	HistogramShotWaitSeconds = 0.0f;
	// bShowUI stays true: the authored bands were measured from UI-composited
	// frames, and ripple_ring exists precisely to capture a HUD element.
	FScreenshotRequest::RequestScreenshot(/*bInShowUI=*/true);
}

void AIGListenerGreyboxDirector::WriteHistogramFrame(
	const TCHAR* PointName,
	const int32 Width,
	const int32 Height,
	const TArray<FColor>& Colors) const
{
	const FString Path = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(
			FPaths::ProjectDir(),
			FString::Printf(TEXT("Docs/Media/v5-%s.png"), PointName)));
	// The bitmap arrives as BGRA8 in sRGB, alpha already forced to 255 by the
	// viewport client before it broadcasts.
	const FImageView Frame(Colors.GetData(), Width, Height);
	if (!FImageUtils::SaveImageByExtension(*Path, Frame))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MISSINGFLOOR_V5 FAIL: point %s measured but its frame could "
				"not be written: %s"),
			PointName,
			*Path);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_V5 frame: %s"), *Path);
}

void AIGListenerGreyboxDirector::HandleHistogramScreenshot(
	const int32 Width,
	const int32 Height,
	const TArray<FColor>& Colors)
{
	if (!bHistogramShotPending
		|| HistogramPointIndex < 0
		|| HistogramPointIndex >= IGNightHistogram::PointCount)
	{
		return;
	}
	bHistogramShotPending = false;
	HistogramShotWaitSeconds = 0.0f;

	const IGNightHistogram::FPoint& Point =
		IGNightHistogram::Points[HistogramPointIndex];
	const int32 PixelCount = Colors.Num();
	if (PixelCount <= 0 || Width <= 0 || Height <= 0)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MISSINGFLOOR_V5 FAIL: point %s captured no pixels; the sweep "
				"needs a real RHI (-RenderOffScreen -d3d12, never -nullrhi)"),
			Point.Name);
		GetWorldTimerManager().ClearTimer(HistogramTimer);
		UGameViewportClient::OnScreenshotCaptured().Remove(
			HistogramScreenshotHandle);
		HistogramFailures = IGNightHistogram::PointCount;
		return;
	}

	// Written before the verdict: a point that failed its band is exactly the
	// one somebody will want to look at.
	WriteHistogramFrame(Point.Name, Width, Height, Colors);

	int32 ShadowPixels = 0;
	int32 HighlightPixels = 0;
	for (const FColor& Pixel : Colors)
	{
		// Rec. 709 luma on the tonemapped sRGB values: the contract is about
		// perceived brightness on the player's monitor, not scene radiance
		// before the film curve, and not any single channel.
		const float Luma =
			(0.2126f * Pixel.R + 0.7152f * Pixel.G + 0.0722f * Pixel.B) / 255.0f;
		if (Luma < IGNightHistogram::ShadowThreshold)
		{
			++ShadowPixels;
		}
		else if (Luma > IGNightHistogram::HighlightThreshold)
		{
			++HighlightPixels;
		}
	}
	const float Shadow = static_cast<float>(ShadowPixels) / PixelCount;
	const float Highlight = static_cast<float>(HighlightPixels) / PixelCount;

	// A frame that is *entirely* black is not a dark frame, it is a broken read,
	// and it would satisfy every shadow floor in the table for the wrong reason.
	// Refusing it is what stopped this sweep from reporting a false pass.
	if (Shadow >= 0.9995f)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MISSINGFLOOR_V5 FAIL point=%s frame is entirely black "
				"(%d pixels); nothing rendered, so nothing was measured"),
			Point.Name,
			PixelCount);
		++HistogramFailures;
	}
	else
	{
		const bool bShadowInBand =
			Shadow >= Point.ShadowMinimum && Shadow <= Point.ShadowMaximum;
		const bool bHighlightInBand = Highlight <= Point.HighlightMaximum;
		++HistogramMeasured;
		if (bShadowInBand && bHighlightInBand)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("MISSINGFLOOR_V5 point=%s shadow=%.4f (%.2f..%.2f) "
					"highlight=%.4f (max %.3f) pixels=%d OK"),
				Point.Name,
				Shadow,
				Point.ShadowMinimum,
				Point.ShadowMaximum,
				Highlight,
				Point.HighlightMaximum,
				PixelCount);
		}
		else if (bHistogramReportOnly)
		{
			// Authoring pass: say what it is, do not judge it.
			UE_LOG(
				LogTemp,
				Display,
				TEXT("MISSINGFLOOR_V5 point=%s shadow=%.4f (%.2f..%.2f) "
					"highlight=%.4f (max %.3f) pixels=%d OUT_OF_BAND"),
				Point.Name,
				Shadow,
				Point.ShadowMinimum,
				Point.ShadowMaximum,
				Highlight,
				Point.HighlightMaximum,
				PixelCount);
		}
		else
		{
			++HistogramFailures;
			UE_LOG(
				LogTemp,
				Error,
				TEXT("MISSINGFLOOR_V5 FAIL point=%s shadow=%.4f (%.2f..%.2f) "
					"highlight=%.4f (max %.3f) pixels=%d"),
				Point.Name,
				Shadow,
				Point.ShadowMinimum,
				Point.ShadowMaximum,
				Highlight,
				Point.HighlightMaximum,
				PixelCount);
		}
	}

	const int32 NextIndex = HistogramPointIndex + 1;
	if (NextIndex < IGNightHistogram::PointCount)
	{
		EnterHistogramPoint(NextIndex);
		return;
	}

	GetWorldTimerManager().ClearTimer(HistogramTimer);
	UGameViewportClient::OnScreenshotCaptured().Remove(HistogramScreenshotHandle);
	if (bHistogramReportOnly)
	{
		// An authoring pass judges nothing, so it must not claim a pass either.
		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_V5 REPORT points=%d — bands are authored from "
				"these numbers, never the other way round"),
			HistogramMeasured);
	}
	else if (HistogramFailures == 0)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_V5 PASS points=%d shadow_threshold=%.2f "
				"highlight_threshold=%.2f"),
			HistogramMeasured,
			IGNightHistogram::ShadowThreshold,
			IGNightHistogram::HighlightThreshold);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MISSINGFLOOR_V5 FAIL points=%d failures=%d"),
			HistogramMeasured,
			HistogramFailures);
	}
	FPlatformMisc::RequestExit(false);
}

void AIGListenerGreyboxDirector::CaptureTeleportPlayer(
	const FVector& Location,
	const float Yaw,
	const float Pitch)
{
	AIGPlayerCharacter* PlayerCharacter = Player.Get();
	if (!PlayerCharacter)
	{
		return;
	}
	PlayerCharacter->TeleportTo(
		Location, FRotator(0.0f, Yaw, 0.0f), false, true);
	if (APlayerController* Controller =
		Cast<APlayerController>(PlayerCharacter->GetController()))
	{
		Controller->SetControlRotation(FRotator(Pitch, Yaw, 0.0f));
	}
}

void AIGListenerGreyboxDirector::CaptureParkEntity(
	const FVector& Location,
	const float Yaw)
{
	if (!Entity)
	{
		return;
	}
	Entity->SetPatrolPoints({Location});
	Entity->ParkForBeat(Location, Yaw);
}

void AIGListenerGreyboxDirector::CaptureShot(const TCHAR* BaseName, bool bShowUI) const
{
	if (bCaptureMetricsOnly)
	{
		UE_LOG(LogIndieGame, Display, TEXT("NIGHT_PERF_POINT %s"), BaseName);
		return;
	}
	// 첫 프레임들이 렌더된 뒤에 재질·PSO 작업이 다시 쌓인다. 스틸마다 그
	// 두 번째 물결을 비우고 찍는다.
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	const FString ScreenshotPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(
			FPaths::ProjectDir(),
			FString::Printf(TEXT("Docs/Media/%s.png"), BaseName)));
	if (FParse::Param(FCommandLine::Get(), TEXT("IGFixtureAudit")))
	{
		FVector Eye; FRotator View;
		GetWorld()->GetFirstPlayerController()->GetPlayerViewPoint(Eye, View);
		UE_LOG(LogTemp, Display, TEXT("FIXTURE_VIEW %s eye=%s rotation=%s"), BaseName, *Eye.ToString(), *View.ToString());
	}
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, bShowUI, false);
	UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_CAPTURE shot: %s"), *ScreenshotPath);
}

void AIGListenerGreyboxDirector::CaptureBeginBurst(
	const TCHAR* DirectoryName,
	const float Seconds)
{
	// 성능 검사에서는 같은 동선을 돌되 PNG 읽기·압축·저장 비용을 제외한다.
	if (bCaptureMetricsOnly) { return; }
	CaptureBurstDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Saved/NightCapture"), DirectoryName));
	UE_LOG(LogIndieGame, Display, TEXT("NIGHT_CAPTURE_BURST %s"), *CaptureBurstDirectory);
	IFileManager::Get().MakeDirectory(*CaptureBurstDirectory, true);
	CaptureBurstFrame = 0;
	CaptureBurstAccumulator = 0.0f;
	CaptureBurstEndsAt = CaptureStepSeconds + Seconds;
	bCaptureBurstActive = true;
}

void AIGListenerGreyboxDirector::EnterCaptureStep(const int32 StepIndex)
{
	CaptureStepIndex = StepIndex;
	CaptureStepSeconds = 0.0f;
	bCaptureActionADone = false;
	bCaptureActionBDone = false;
	bCaptureActionCDone = false;
	bCaptureBurstActive = false;

	switch (StepIndex)
	{
	case 0:
		// The night card over the 403 bedroom, seconds into the hour.
		CaptureParkEntity(FVector(540.0f, -305.0f, 960.0f), 180.0f);
		CaptureTeleportPlayer(AIGPrologueWorldScene::GetPlayerStartLocation(), -128.0f, -6.0f);
		break;
	case 1:
		// The one upstairs mid-knock, dead ahead down the corridor. The
		// player stands east of the fire-cabinet beat zone (X > 292): parking
		// inside it once fired the whole tutorial mid-photograph, and the
		// capture reset shot the next two stills from the bedroom.
		// 낮게 기는 몸과 얼굴이 평소 카메라 높이에서 함께 보이는 거리.
		CaptureParkEntity(FVector(150.0f, -305.0f, 960.0f), 0.0f);
		CaptureTeleportPlayer(FVector(420.0f, -305.0f, 997.0f), 180.0f, -24.0f);
		break;
	case 2:
		// Looking down the stair throat at the half-landing cameo.
		if (Entity)
		{
			Entity->TeleportTo(
				AIGNightOneBeatDirector::GetSightingStagePoint(),
				FRotator(0.0f, 180.0f, 0.0f),
				false,
				true);
			Entity->SetPatrolPoints({
				AIGNightOneBeatDirector::GetSightingStagePoint(),
				AIGNightOneBeatDirector::GetSightingShufflePoint(),
			});
		}
		// The landing sits 1.8 m below the throat eye line at 1.5 m out, so
		// the look-down is steep: shallower pitches photograph the far wall.
		CaptureTeleportPlayer(FVector(-300.0f, -305.0f, 1005.0f), 180.0f, -52.0f);
		break;
	case 3:
		// The noise ripple, moments after a deliberate sound.
		CaptureTeleportPlayer(FVector(60.0f, -305.0f, 997.0f), 0.0f, -4.0f);
		break;
	case 4:
		// The sealed common entrance and its refusal prompt.
		CaptureParkEntity(FVector(540.0f, -305.0f, 960.0f), 180.0f);
		CaptureTeleportPlayer(FVector(630.0f, -295.0f, 92.0f), -90.0f, -6.0f);
		break;
	case 5:
		// P1: the meter cabinet with the fifth, nameless dial.
		CaptureTeleportPlayer(FVector(505.0f, -300.0f, 92.0f), -62.0f, -10.0f);
		break;
	case 6:
		// P2: the booth desk — ledger, carbon pad, monitor.
		//
		// 이 컷의 주어는 「두 기록이 다르다」이므로 서류 두 장이 주인공이어야
		// 한다. 예전 자리는 책상에서 1.1m 떨어져 -25도라 모니터가 화면을
		// 차지하고 정서본과 먹지는 아래로 잘렸다. 책상 앞턱(Y=-137.5)에
		// 닿지 않는 선까지 다가가 두 장의 가운데(X=140)를 내려다본다.
		// 모니터는 위쪽에 남아 관리실이라는 것을 계속 말해 준다.
		CaptureTeleportPlayer(FVector(140.0f, -180.0f, 92.0f), 90.0f, -45.0f);
		break;
	case 7:
		// Burst: the extinguisher fall, with the entity resting so the
		// physics beat stays unphotobombed.
		if (Entity)
		{
			Entity->SetDormant(true);
		}
		CaptureTeleportPlayer(FVector(20.0f, -300.0f, 997.0f), -17.0f, -30.0f);
		break;
	case 8:
		// Burst: hear-investigate-chase-capture-reset, first person.
		if (Entity)
		{
			Entity->SetDormant(false);
			Entity->TeleportTo(
				// Start west of the extinguisher already shown in the previous
				// burst. The capture must exercise pursuit, not photograph the
				// capsule wedged against that settled physics prop.
				FVector(150.0f, -305.0f, 960.0f),
				FRotator(0.0f, 180.0f, 0.0f),
				false,
				true);
			Entity->SetPatrolPoints({FVector(150.0f, -305.0f, 960.0f)});
		}
		CaptureTeleportPlayer(FVector(-250.0f, -305.0f, 997.0f), 0.0f, -4.0f);
		break;
	case 9:
		// Dawn, then Hwang Sun-geum answering through her door.
		//
		// 401호 문 앞 47 cm에서 찍고 있었다. 화각이 78도이므로 그 거리에서
		// 보이는 폭은 76 cm인데 문짝만 84 cm다 — 문틀도, 상인방도, 호수판도
		// 프레임 밖이라 화면에는 무늬 없는 회색 판과 문구멍 하나만 남았다.
		// 「401호 문 너머로 대화하는 장면」이라고 걸어 둔 컷이 문으로 읽히지
		// 않았다. 91 cm까지 물러나면 폭 147 cm·높이 83 cm가 들어와 문틀 양쪽
		// (X -200..-192, -104..-96)과 상인방(Z 1100~1108)이 잡히고, 위로
		// 11도 들면 호수판(Z 1110~1118)까지 프레임에 들어온다. 복도가 130 cm
		// 깊이라 문 전체(208 cm)를 정면으로 담을 방법은 없으므로, 문이라는
		// 것과 401호라는 것을 말해 주는 위쪽을 택한다.
		if (NightPhase)
		{
			NightPhase->CompleteNightGoal();
		}
		CaptureTeleportPlayer(FVector(-150.0f, -328.0f, 997.0f), 90.0f, 11.0f);
		break;
	case 10:
		// 밤 3의 조명과 HUD로 옥상과 설비실을 검수한다.
		if (NightPhase) { NightPhase->BeginTheHour(3); NightPhase->SetHourPaused(true); }
		if (Entity)
		{
			Entity->SetDormant(true);
		}
		if (NightThree)
		{
			if (AIGSwingDoor* RoofGate = NightThree->GetStairGate())
			{
				RoofGate->ForceOpenState(true);
			}
			if (AIGSwingDoor* AnnexGate = NightThree->GetAnnexGate())
			{
				AnnexGate->ForceOpenState(true);
			}
		}
		CaptureTeleportPlayer(FVector(-277.5f, -175.0f, 1068.0f), 90.0f, 8.0f);
		break;
	case 11:
		// First 4.075 m leg, squeezed between tank base and guard rail. Start
		// beyond the stair cheek wall so the shot proves the walkable lane
		// instead of filling half the frame with the wall behind the door.
		CaptureTeleportPlayer(FVector(-150.0f, 220.0f, 1297.0f), 0.0f, -7.0f);
		break;
	case 12:
		// The 90-degree turn and second physical fire door into the annex.
		CaptureTeleportPlayer(FVector(130.0f, 350.0f, 1297.0f), 90.0f, -6.0f);
		break;
	case 13:
		// 밤 4의 전원 상태에서 물탱크 밸브와 최종 장면을 확인한다.
		if (NightPhase) { NightPhase->RestartTheHour(4); NightPhase->SetHourPaused(true); }
		CaptureTeleportPlayer(FVector(5.0f, 350.0f, 1297.0f), -90.0f, -4.0f);
		break;
	case 14:
		// Ground-floor motor, volute, pipes and selector inside the booth.
		CaptureTeleportPlayer(FVector(225.0f, -220.0f, 96.0f), 158.0f, -35.0f);
		break;
	case 15:
		// The real middle gypsum face before five strikes remove its collision.
		if (NightFour)
		{
			NightFour->SetFinaleCapturePreview(false, false);
		}
		CaptureTeleportPlayer(FVector(50.0f, 700.0f, 1297.0f), 0.0f, -22.0f);
		break;
	case 16:
		// Close enough to read the board grip and tired workwear as real 3D,
		// while keeping the player camera under normal first-person control.
		if (NightFour)
		{
			NightFour->SetFinaleCapturePreview(true, true);
		}
		CaptureTeleportPlayer(FVector(130.0f, 710.0f, 1297.0f), -90.0f, -4.0f);
		break;
	case 17:
		// M6.5 좌절 안전망은 5회 포획 메모를 실물로 보여 준다.
		// 평소 1인칭 시점에서 바닥을 보면 읽히지만 HUD는 절대 열지 않는다.
		CaptureParkEntity(FVector(540.0f, -305.0f, 960.0f), 180.0f);
		CaptureTeleportPlayer(FVector(-150.0f, -322.0f, 997.0f), 90.0f, -68.0f);
		if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
		{
			// 실제 접근성 설정 범위의 낮은 FOV를 써서 게임플레이 카메라를
			// 바꾸지 않고도 검수 스틸에서 글자가 충분히 읽히게 한다.
			if (UCameraComponent* Camera = PlayerCharacter->GetFirstPersonCamera())
			{
				Camera->SetFieldOfView(68.0f);
			}
		}
		break;
	case 18:
		if (NightPhase) { NightPhase->RestartTheHour(1); NightPhase->SetHourPaused(true); }
		if (Entity) { Entity->SetDormant(true); }
		CaptureTeleportPlayer(FVector(190.f, -305.f, 997.f), 180.f, -12.f);
		CaptureParkEntity(FVector(20.f, -305.f, 960.f), 0.f);
		break;
	default:
		break;
	}
}

void AIGListenerGreyboxDirector::AdvanceNightCapture()
{
	constexpr float TickSeconds = 0.04f;
	CaptureStepSeconds += TickSeconds;

	// Burst frames ride the same timer. Each 1080p PNG write stalls the next
	// request, so a fixed cadence would drop frames and punch holes in the
	// numbering — and ffmpeg's image sequence reader stops at the first gap.
	// Requesting only when the previous shot has been consumed keeps the
	// sequence continuous at whatever rate the disk actually sustains.
	if (bCaptureBurstActive)
	{
		if (!FScreenshotRequest::IsScreenshotRequested())
		{
			const FString FramePath = FPaths::Combine(
				CaptureBurstDirectory,
				FString::Printf(TEXT("frame_%05d.png"), CaptureBurstFrame++));
			FScreenshotRequest::RequestScreenshot(FramePath, true, false);
		}
		if (CaptureStepSeconds >= CaptureBurstEndsAt)
		{
			bCaptureBurstActive = false;
		}
	}

	const auto ActionA = [this](const float AtSeconds) -> bool
	{
		if (!bCaptureActionADone && CaptureStepSeconds >= AtSeconds)
		{
			bCaptureActionADone = true;
			return true;
		}
		return false;
	};
	const auto ActionB = [this](const float AtSeconds) -> bool
	{
		if (!bCaptureActionBDone && CaptureStepSeconds >= AtSeconds)
		{
			bCaptureActionBDone = true;
			return true;
		}
		return false;
	};
	const auto ActionC = [this](const float AtSeconds) -> bool
	{
		if (!bCaptureActionCDone && CaptureStepSeconds >= AtSeconds)
		{
			bCaptureActionCDone = true;
			return true;
		}
		return false;
	};
	const auto StepDone = [this](const float AfterSeconds)
	{
		return CaptureStepSeconds >= AfterSeconds;
	};

	AIGPrologueWorldScene* SceneNow =
		const_cast<AIGPrologueWorldScene*>(WorldScene.Get());
	switch (CaptureStepIndex)
	{
	case 0:
		if (ActionA(1.0f))
		{
			CaptureShot(TEXT("night1-card"));
		}
		// Hold here until the card scrim (4.2 s) and the wake-restore inner
		// voice have both drained, so every later still gets a clean HUD.
		if (StepDone(8.5f))
		{
			EnterCaptureStep(1);
		}
		break;
	case 1:
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("night1-listener-corridor"));
		}
		if (StepDone(1.4f))
		{
			EnterCaptureStep(2);
		}
		break;
	case 2:
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("night1-stair-sighting"));
		}
		if (StepDone(1.4f))
		{
			EnterCaptureStep(3);
		}
		break;
	case 3:
		if (ActionA(0.5f) && NoiseSubsystem)
		{
			if (AIGPlayerCharacter* PlayerCharacter = Player.Get())
			{
				NoiseSubsystem->ReportNoise(
					PlayerCharacter->GetActorLocation()
						+ FVector(30.0f, 0.0f, 0.0f),
					0.5f,
					PlayerCharacter);
			}
		}
		if (ActionB(0.75f))
		{
			CaptureShot(TEXT("hud-noise-ripple"));
		}
		if (StepDone(1.3f))
		{
			EnterCaptureStep(4);
		}
		break;
	case 4:
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("night-sealed-entrance"));
		}
		if (StepDone(1.4f))
		{
			EnterCaptureStep(5);
		}
		break;
	case 5:
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("p1-meter-cabinet"));
		}
		if (StepDone(1.4f))
		{
			EnterCaptureStep(6);
		}
		break;
	case 6:
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("p2-booth-desk"));
		}
		if (StepDone(1.4f))
		{
			EnterCaptureStep(7);
		}
		break;
	case 7:
		if (ActionA(0.2f))
		{
			CaptureBeginBurst(TEXT("extinguisher"), 3.2f);
		}
		if (ActionB(0.4f) && SceneNow)
		{
			SceneNow->DropCorridorExtinguisher();
		}
		if (StepDone(3.8f))
		{
			EnterCaptureStep(8);
		}
		break;
	case 8:
		if (ActionA(0.3f))
		{
			CaptureBeginBurst(TEXT("chase"), 7.4f);
		}
		// Two sounds a second apart: the first turns its head, the second
		// starts the chase the GIF exists for. The capture and the wake in
		// bed both land inside the frame window on purpose.
		if (ActionB(0.5f) && NoiseSubsystem)
		{
			NoiseSubsystem->ReportNoise(
				FVector(100.0f, -305.0f, 960.0f), 0.45f, Player.Get());
		}
		if (ActionC(1.6f) && NoiseSubsystem)
		{
			NoiseSubsystem->ReportNoise(
				FVector(-220.0f, -305.0f, 960.0f), 0.45f, Player.Get());
		}
		if (StepDone(8.2f))
		{
			EnterCaptureStep(9);
		}
		break;
	case 9:
		if (ActionA(0.8f) && Unit401Door)
		{
			FIGInteractionContext KnockContext;
			KnockContext.Interactor = Player.Get();
			KnockContext.TargetActor = Unit401Door;
			KnockContext.HoldProgress = 1.0f;
			IIGInteractable::Execute_CompleteInteraction(
				Unit401Door, KnockContext);
		}
		if (ActionB(4.8f))
		{
			CaptureShot(TEXT("day-corridor-hwang"));
		}
		if (StepDone(5.6f))
		{
			EnterCaptureStep(10);
		}
		break;
	case 10:
		if (ActionA(4.8f))
		{
			CaptureShot(TEXT("night3-roof-stair"));
		}
		if (StepDone(5.4f))
		{
			EnterCaptureStep(11);
		}
		break;
	case 11:
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("night3-roof-passage"));
		}
		if (StepDone(1.5f))
		{
			EnterCaptureStep(12);
		}
		break;
	case 12:
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("night3-annex-doorway"));
		}
		if (StepDone(1.5f))
		{
			EnterCaptureStep(13);
		}
		break;
	case 13:
		if (ActionA(4.8f))
		{
			CaptureShot(TEXT("night4-p5-roof-controls"));
		}
		if (StepDone(5.4f))
		{
			EnterCaptureStep(14);
		}
		break;
	case 14:
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("night4-p5-transfer-pump"));
		}
		if (StepDone(1.5f))
		{
			EnterCaptureStep(15);
		}
		break;
	case 15:
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("night4-cavity-wall"));
		}
		if (ActionB(1.25f) && SceneNow)
		{
			SceneNow->OpenMissingFloorCavity();
			if (NightFour)
			{
				NightFour->SetFinaleCapturePreview(true, false);
			}
		}
		if (ActionC(1.75f))
		{
			CaptureShot(TEXT("night4-cavity-open"));
		}
		if (StepDone(2.35f))
		{
			EnterCaptureStep(16);
		}
		break;
	case 16:
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("night4-mok-confrontation"));
		}
		if (StepDone(1.5f))
		{
			EnterCaptureStep(18);
		}
		break;
	case 17:
		if (ActionA(0.2f))
		{
			CaptureBeginBurst(TEXT("mercy-note"), 2.35f);
			if (NightLoop)
			{
				NightLoop->PlayMercyNoteCapturePreview();
			}
		}
		// 고정 검수 카메라에서 종이가 어두운 타일을 지나면 TSR 히스토리가
		// 실제보다 길게 남는다. 연속 캡처 후의 문서용 스틸만 FXAA로
		// 바꾸어 멈춘 메모를 잔상 없이 남긴다. 게임 렌더러와 GIF는 기본 설정을 유지한다.
		if (ActionC(2.65f))
		{
			if (APlayerController* PlayerController =
				GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
			{
				PlayerController->ConsoleCommand(
					TEXT("r.AntiAliasingMethod 1"), true);
			}
		}
		if (ActionB(3.1f))
		{
			CaptureShot(TEXT("m65-capture-mercy-note"));
		}
		if (StepDone(3.8f))
		{
			GetWorldTimerManager().ClearTimer(CaptureTimer);
			UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_CAPTURE DONE"));
			RequestExit(false);
		}
		break;
	case 18:
		if (ActionA(.8f) && Entity && Player.IsValid())
		{
			Entity->SetDifficultyForTesting(EIGNightDifficulty::Standard);
			Entity->SetDormant(false);
			CaptureTeleportPlayer(FVector(190.f, -305.f, 997.f), 180.f, -12.f);
			CaptureParkEntity(FVector(20.f, -305.f, 960.f), 0.f);
			CaptureBeginBurst(TEXT("physical-capture"), 4.2f);
		}
		if (ActionB(1.4f) && Entity)
		{
			CaptureParkEntity(FVector(100.f, -305.f, 960.f), 0.f);
			Entity->SetActorTickEnabled(true);
		}
		if (ActionC(2.2f) && Entity && Player.IsValid())
		{
			bool bTexturesReady = true;
			for (const TCHAR* TextureRole : {TEXT("D"), TEXT("N"), TEXT("ORM")})
			{
				if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr,
					*FString::Printf(TEXT("/Game/Prototype/Textures/T_ListenerCrawler_%s.T_ListenerCrawler_%s"), TextureRole, TextureRole)))
				{
					const auto& Streaming = Texture->GetStreamableResourceState();
					bTexturesReady &= Texture->IsFullyStreamedIn();
					UE_LOG(LogTemp, Display, TEXT("PHYSICAL_CAPTURE_TEXTURE %s resident=%d requested=%d max=%d forced=%d"),
						TextureRole, Streaming.NumResidentLODs, Streaming.NumRequestedLODs, Streaming.MaxNumLODs,
						Texture->ShouldMipLevelsBeForcedResident());
				}
			}
			UE_LOG(LogTemp, Display, TEXT("PHYSICAL_CAPTURE_CONTACT state=%d dormant=%d physical=%d player=%s face=%s eye=%s"),
				static_cast<int32>(Entity->GetListenerState()), Entity->IsDormant(), Player->HasPhysicalCaptureView(),
				*Player->GetActorLocation().ToString(), *Entity->GetCaptureFaceLocation().ToString(),
				*Player->GetPawnViewLocation().ToString());
			if (Entity->GetListenerState() != EIGListenerState::CaptureHold || !Player->HasPhysicalCaptureView() || !bTexturesReady)
			{
				UE_LOG(LogTemp, Error, TEXT("PHYSICAL_CAPTURE FAIL contact or texture streaming"));
				RequestExit(true);
			}
		}
		if (StepDone(5.f))
		{
			GetWorldTimerManager().ClearTimer(CaptureTimer);
			const bool bCaptured = NightLoop && NightLoop->GetCaptureCount() > 0;
			UE_LOG(LogTemp, Display, TEXT("PHYSICAL_CAPTURE %s"), bCaptured ? TEXT("PASS") : TEXT("FAIL"));
			UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_CAPTURE DONE"));
			RequestExit(!bCaptured);
		}
		break;
	default:
		break;
	}
}
