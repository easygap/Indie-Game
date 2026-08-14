#include "Entity/IGListenerGreyboxDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/CameraComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGSwingDoor.h"
#include "Misc/Paths.h"
#include "Player/IGHorrorHUD.h"
#include "Sequence/IGWakeUpDirector.h"
#include "Engine/GameViewportClient.h"
#include "Player/IGStressComponent.h"
#include "UnrealClient.h"
#include "Entity/IGListenerEntity.h"
#include "Entity/IGNightLoopDirector.h"
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
#include "TimerManager.h"

namespace IGListenerGreybox
{
	// 4F floor slab height in the prologue villa (IGPrologueWorldScene's
	// FourthFloorZ). The stage traces nothing: greybox placement rides the
	// same constant the corridor was built with.
	constexpr float FourthFloorZ = 900.0f;
	constexpr float EntityHalfHeight = 58.0f;
	// The 404 fridge hum, first of the §5.1 masking pockets. Placeholder
	// until night-one dressing registers real machines (M2).
	const FVector FridgeHumLocation(155.0f, -20.0f, FourthFloorZ + 60.0f);
	constexpr float FridgeHumRadius = 200.0f;
	constexpr float FridgeHumMasking = 0.2f;

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

	bProbeRequested =
		FParse::Param(FCommandLine::Get(), TEXT("IGListenerGreyboxProbe"));
	bNightCaptureRequested =
		FParse::Param(FCommandLine::Get(), TEXT("IGNightCapture"));
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
		else if (bProbeRequested)
		{
			StartProbe();
		}
		return;
	}
	if (SetupRetrySeconds >= IGListenerGreybox::SetupGiveUpSeconds)
	{
		GetWorldTimerManager().ClearTimer(SetupTimer);
		if (bProbeRequested)
		{
			FailProbe(TEXT("stage setup timed out (world scene or player missing)"));
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

	NoiseSubsystem->RegisterHumSource(
		IGListenerGreybox::FridgeHumLocation,
		IGListenerGreybox::FridgeHumRadius,
		IGListenerGreybox::FridgeHumMasking);

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
		NightPhase->BeginTheHour(/*NightIndex=*/1);
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
				0.05f);
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
				0.15f);
			Unit401Door->OnExamined.AddUObject(
				this, &AIGListenerGreyboxDirector::HandleUnit401Knocked);
		}
	}

	// The initial hour state fired before these actors existed; apply it to
	// them now that they do. Without a night phase (capture tours) everything
	// keeps its natural default and nothing is put to sleep.
	if (NightPhase)
	{
		HandleHourActiveChanged(NightPhase->IsHourActive());
	}

	UE_LOG(LogTemp, Display,
		TEXT("MISSINGFLOOR_GREYBOX stage ready: %d patrol stops, entity at %s, "
			"hour_sealed=%d"),
		PatrolPoints.Num(),
		*PatrolPoints[0].ToCompactString(),
		Scene->IsTheHourSealed() ? 1 : 0);
	return true;
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
	NightPhase->CompleteNightGoal();
}

void AIGListenerGreyboxDirector::HandleFifthDawnCompleted()
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!NightPhase || !Narrative || NightPhase->IsHourActive()
		|| Narrative->GetNightIndex() != 3)
	{
		return;
	}
	NightPhase->BeginTheHour(4);
}

void AIGListenerGreyboxDirector::HandleSleepRequested(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!NightPhase || !Narrative || NightPhase->IsHourActive())
	{
		return;
	}
	if (Narrative->GetNightIndex() == 3
		&& !Narrative->WasFifthDawnInterludeCompleted())
	{
		if (bProbeRequested)
		{
			// Exercise the real fade/audio/input/save plumbing, then finish in the
			// same frame so CI does not idle for 160 seconds before night 4.
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
			return;
		}
		if (FifthDawn && FifthDawn->IsActive())
		{
			return;
		}
		if (FifthDawn && FifthDawn->StartInterlude(Player.Get()))
		{
			return;
		}
		// A missing presentation must not strand a save between nights. The
		// validation gate still fails the build through ValidateTimeline.
		Narrative->SetFifthDawnInterludeCompleted(true);
	}
	const int32 NextNight =
		FMath::Clamp(Narrative->GetNightIndex() + 1, 1, 4);
	NightPhase->BeginTheHour(NextNight);
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

	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	const FText Speaker =
		NSLOCTEXT("IGMissingFloor", "HwangSpeaker", "황순금");
	if (Narrative && Narrative->HasTruth(EIGMissingFloorTruth::WasStillAlive))
	{
		AIGHorrorHUD::PushDialogue(
			this,
			Speaker,
			NSLOCTEXT(
				"IGMissingFloor",
				"Hwang401AfterT7",
				"들었냐. 종이는 눌린 대로 남는다. …벽도 그렇다."),
			EIGDialogueChannel::Conversation,
			0.0f,
			EIGDialoguePriority::Story);
	}
	else if (Narrative && Narrative->GetNightIndex() >= 2)
	{
		AIGHorrorHUD::PushDialogue(
			this,
			Speaker,
			NSLOCTEXT(
				"IGMissingFloor",
				"Hwang401HintP2",
				"관리실 대장은 두 벌이다. 위엣장은 볼펜이 쓰고, 아랫장은 힘이 쓴다."),
			EIGDialogueChannel::Conversation,
			0.0f,
			EIGDialoguePriority::Story);
	}
	else
	{
		AIGHorrorHUD::PushDialogue(
			this,
			Speaker,
			NSLOCTEXT(
				"IGMissingFloor",
				"Hwang401Greeting",
				"새로 온 사람이구나. …네가 뭘 듣는지부터 말해라."),
			EIGDialogueChannel::Conversation,
			0.0f,
			EIGDialoguePriority::Story);
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
		const float ExpectedChase[] = {264.0f, 330.0f, 330.0f, 374.0f};
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
			&& FMath::IsNearlyEqual(Quiet.ChaseSpeed, 374.0f * 0.8f, 0.01f)
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
		// Every 없는 층 truth needs two independent records. One must not do.
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
		if (NoiseSubsystem->GetMaskingAt(
			IGListenerGreybox::FridgeHumLocation) <= 0.0f)
		{
			FailProbe(TEXT("fridge hum pocket reports no masking"));
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
					FVector(-150.0f, -269.5f, 900.12f)) <= 1.0f);
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
				FVector(-150.0f, -269.5f, 900.12f));
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
					FVector(-300.0f, -305.0f, 1010.0f),
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
				FVector(-445.0f, -305.0f, 888.0f)) <= 250.0f;
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

		// Crossing with the agent's message confirms T7 and, since this is
		// night 2, ends the night through the goal exit.
		Context.TargetActor = AgentNote;
		IIGInteractable::Execute_CompleteInteraction(AgentNote, Context);
		IIGInteractable::Execute_CompleteInteraction(AgentNote, Context);
		if (!Narrative->HasTruth(EIGMissingFloorTruth::WasStillAlive))
		{
			FailProbe(TEXT("T7 did not confirm after crossing both records"));
			return;
		}
		// §8 밤2 ends at 403's door, not here. T7 arms 비트 2-5 and the hour has
		// to still be running, or the return chase never happens.
		if (!NightPhase->IsHourActive())
		{
			FailProbe(TEXT("confirming T7 released her to dawn from the booth"));
			return;
		}
		if (!NightTwoBeats
			|| NightTwoBeats->GetReturnStage()
				!= EIGNightTwoReturnStage::AwaitingExit)
		{
			FailProbe(FString::Printf(
				TEXT("§8 비트 2-5 did not arm on T7: stage=%d"),
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
		bCctvShapeSeen = false;
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

		// 화면 가장자리를 지나가는 낮은 형체 — latched, because the crossing is
		// shorter than the whole live window and the poll must not have to land
		// inside it.
		bCctvShapeSeen = bCctvShapeSeen || Channel->IsShapeCrossing();
		if (Channel->GetState() == EIGCctvChannelState::Live)
		{
			CctvCapturesAtLive = Channel->GetCaptureCount();
			// Read the target while there is still a picture in it. Once is
			// enough, and the beat is not repeatable so there is no second chance.
			// 2.80 s past the press is 0.44 through the live window, which is
			// inside the low shape's crossing. Measuring there means the reading
			// and the exported frame both contain the thing the beat is about.
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
		if (Channel->HasFeed() || Channel->IsOnScreen()
			|| Channel->IsShapeCrossing())
		{
			FailProbe(FString::Printf(
				TEXT("§14 the dead channel is still allocated: "
					"feed=%d onscreen=%d shape=%d"),
				Channel->HasFeed() ? 1 : 0,
				Channel->IsOnScreen() ? 1 : 0,
				Channel->IsShapeCrossing() ? 1 : 0));
			return;
		}
		if (!bCctvShapeSeen)
		{
			FailProbe(TEXT("the low shape never crossed the frame"));
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
				"%d captures, low shape crossed, released on death, "
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
		if (Journal->IsHidden())
		{
			FailProbe(TEXT("journal stayed hidden after T7 by day"));
			return;
		}

		Context.TargetActor = Labels;
		IIGInteractable::Execute_CompleteInteraction(Labels, Context);
		IIGInteractable::Execute_CompleteInteraction(Labels, Context);
		Context.TargetActor = Forum;
		IIGInteractable::Execute_CompleteInteraction(Forum, Context);
		IIGInteractable::Execute_CompleteInteraction(Forum, Context);
		Context.TargetActor = Journal;
		IIGInteractable::Execute_CompleteInteraction(Journal, Context);
		IIGInteractable::Execute_CompleteInteraction(Journal, Context);

		if (Narrative->HasTruth(EIGMissingFloorTruth::TenantIdentity))
		{
			FailProbe(TEXT("T2 confirmed from the labels alone"));
			return;
		}
		if (!Narrative->HasSource(
			EIGMissingFloorTruth::FiveNightsOfThirst,
			EIGMissingFloorSource::KnockTallyJournal))
		{
			FailProbe(TEXT("journal read did not file the tally record"));
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
		Context.TargetActor = Key;
		IIGInteractable::Execute_CompleteInteraction(Key, Context);
		if (Gate->IsLocked() || AnnexGate->IsLocked())
		{
			FailProbe(TEXT("the two labelled keys did not release both gates"));
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
			|| NoiseSubsystem->GetMaskingAt(FVector(246.0f, 700.0f, 1300.0f)) < 0.39f)
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
					FVector(246.0f, 700.0f, 1300.0f)) > 0.01f)
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
	EnterCaptureStep(FMath::Clamp(StartStep, 0, 17));
	if (StartStep > 0)
	{
		// A direct art-review stop still begins while the normal night card is
		// fading. Keep timed actions behind that card instead of photographing it.
		CaptureStepSeconds = -4.0f;
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
		RippleRing
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
			0.02f, 0.16f, 0.010f
		},
		{
			TEXT("entity_rim"), ESetup::EntityRim,
			FVector(120.0f, -305.0f, 997.0f), 180.0f, -4.0f,
			0.28f, 0.48f, 0.010f
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
	Entity->TeleportTo(Location, FRotator(0.0f, Yaw, 0.0f), false, true);
	Entity->SetPatrolPoints({Location});
}

void AIGListenerGreyboxDirector::CaptureShot(const TCHAR* BaseName) const
{
	const FString ScreenshotPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(
			FPaths::ProjectDir(),
			FString::Printf(TEXT("Docs/Media/%s.png"), BaseName)));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_CAPTURE shot: %s"), *ScreenshotPath);
}

void AIGListenerGreyboxDirector::CaptureBeginBurst(
	const TCHAR* DirectoryName,
	const float Seconds)
{
	CaptureBurstDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("NightCapture"), DirectoryName));
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
		CaptureTeleportPlayer(FVector(-48.0f, 60.0f, 997.0f), -128.0f, -6.0f);
		break;
	case 1:
		// The one upstairs mid-knock, dead ahead down the corridor. The
		// player stands east of the fire-cabinet beat zone (X > 292): parking
		// inside it once fired the whole tutorial mid-photograph, and the
		// capture reset shot the next two stills from the bedroom.
		CaptureParkEntity(FVector(150.0f, -305.0f, 960.0f), 180.0f);
		CaptureTeleportPlayer(FVector(330.0f, -305.0f, 997.0f), 180.0f, -6.0f);
		break;
	case 2:
		// Looking down the stair throat at the half-landing cameo.
		if (Entity)
		{
			Entity->TeleportTo(
				FVector(-445.0f, -305.0f, 888.0f),
				FRotator(0.0f, 180.0f, 0.0f),
				false,
				true);
			Entity->SetPatrolPoints({
				FVector(-445.0f, -305.0f, 888.0f),
				FVector(-445.0f, -255.0f, 888.0f),
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
		CaptureTeleportPlayer(FVector(160.0f, -214.0f, 92.0f), 90.0f, -25.0f);
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
		if (NightPhase)
		{
			NightPhase->CompleteNightGoal();
		}
		CaptureTeleportPlayer(FVector(-150.0f, -284.0f, 997.0f), 90.0f, -6.0f);
		break;
	case 10:
		// Portal-free climb: the camera remains inside the real upper stair.
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
		// Night-four roof hardware: both valves must sit on the tank, not hover
		// over the lane or masquerade as four unrelated puzzle switches.
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
		CaptureTeleportPlayer(FVector(100.0f, 700.0f, 1297.0f), 0.0f, -5.0f);
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
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("night3-roof-stair"));
		}
		if (StepDone(1.5f))
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
		if (ActionA(0.9f))
		{
			CaptureShot(TEXT("night4-p5-roof-controls"));
		}
		if (StepDone(1.5f))
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
			GetWorldTimerManager().ClearTimer(CaptureTimer);
			UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_CAPTURE DONE"));
			RequestExit(false);
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
	default:
		break;
	}
}
