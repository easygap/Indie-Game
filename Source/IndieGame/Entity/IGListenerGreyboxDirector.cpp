#include "Entity/IGListenerGreyboxDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGSwingDoor.h"
#include "Misc/Paths.h"
#include "Player/IGHorrorHUD.h"
#include "Sequence/IGWakeUpDirector.h"
#include "UnrealClient.h"
#include "Entity/IGListenerEntity.h"
#include "Entity/IGNightLoopDirector.h"
#include "Entity/IGMissingFloorEvidence.h"
#include "Entity/IGMissingFloorNightThreeDirector.h"
#include "Entity/IGMissingFloorPuzzleOneDirector.h"
#include "Entity/IGMissingFloorPuzzleTwoDirector.h"
#include "Entity/IGNightOneBeatDirector.h"
#include "Entity/IGNightPhaseDirector.h"
#include "Entity/IGNoiseSubsystem.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
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
		// The capture tour and the probe are mutually exclusive drivers of
		// the same stage; the tour wins because it needs the screen.
		if (bNightCaptureRequested)
		{
			StartNightCapture();
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
		|| !NightThree->Configure(const_cast<AIGPrologueWorldScene*>(Scene)))
	{
		return false;
	}

	// Night goals: each puzzle announces itself once; the hour decides
	// whether that ends the night.
	PuzzleOne->OnSolved.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleNightOneSolved);
	PuzzleTwo->OnSolved.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleNightTwoSolved);
	NightThree->OnSolved.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleNightThreeSolved);

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
	// One boundary, every consequence, in one place: the entity sleeps by
	// day, the booth locks by day, and the day verbs vanish by night.
	if (Entity)
	{
		Entity->SetDormant(!bActive);
	}
	if (PuzzleTwo)
	{
		PuzzleTwo->SetHourActive(bActive);
	}
	if (NightThree)
	{
		NightThree->SetHourActive(bActive);
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
	if (NightPhase && Narrative && Narrative->GetNightIndex() == 2)
	{
		NightPhase->CompleteNightGoal();
	}
}

void AIGListenerGreyboxDirector::HandleNightThreeSolved()
{
	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (NightPhase && Narrative && Narrative->GetNightIndex() == 3)
	{
		NightPhase->CompleteNightGoal();
	}
}

void AIGListenerGreyboxDirector::HandleSleepRequested(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!NightPhase || !Narrative || NightPhase->IsHourActive())
	{
		return;
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

void AIGListenerGreyboxDirector::StartProbe()
{
	ProbeStep = EProbeStep::PuzzleOneContract;
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
		if (bPlayerBackAtBed && bTierRaised)
		{
			// On to the night-1 beats: walk into the stair throat and expect
			// the cameo on the half-landing.
			if (PlayerCharacter)
			{
				PlayerCharacter->TeleportTo(
					FVector(-300.0f, -305.0f, 1010.0f),
					PlayerCharacter->GetActorRotation(),
					false,
					true);
			}
			ProbeStep = EProbeStep::Night1SightingStage;
			StepDeadlineSeconds = 0.0f;
			break;
		}
		if (StepDeadlineSeconds > 6.0f)
		{
			FailProbe(FString::Printf(
				TEXT("reset incomplete (atBed=%d tier=%d)"),
				bPlayerBackAtBed ? 1 : 0,
				Entity->GetAggressionTier()));
		}
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
		if (NightPhase->IsHourActive())
		{
			FailProbe(TEXT("confirming T7 did not end night 2"));
			return;
		}
		if (!Entity->IsDormant())
		{
			FailProbe(TEXT("dawn after night 2 left the entity awake"));
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

		ProbeStep = EProbeStep::DayTwoContract;
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
		ProbeStep = EProbeStep::NightThreeContract;
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
		if (!Key || !Gate)
		{
			FailProbe(TEXT("keyring or gate unresolved"));
			return;
		}
		if (!Gate->IsLocked())
		{
			FailProbe(TEXT("stair gate stood open before the keyring"));
			return;
		}
		Context.TargetActor = Key;
		IIGInteractable::Execute_CompleteInteraction(Key, Context);
		if (Gate->IsLocked())
		{
			FailProbe(TEXT("keyring did not release the stair gate"));
			return;
		}

		// Up to the annex: the notebook names him and arms the answer. The
		// drop point stays clear of the return portal volume.
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
		Context.TargetActor = CavityListen;
		IIGInteractable::Execute_CompleteInteraction(CavityListen, Context);
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
		Context.TargetActor = CavityListen;
		IIGInteractable::Execute_CompleteInteraction(CavityListen, Context);
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
		Context.TargetActor = Answer;
		IIGInteractable::Execute_CompleteInteraction(Answer, Context);
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
			if (NightPhase->IsHourActive())
			{
				FailProbe(TEXT("the answer did not end night 3"));
				return;
			}
			if (!Narrative->IsFinalChoiceUnlocked())
			{
				FailProbe(TEXT("T6+T7+T9 did not unlock the final choice"));
				return;
			}
			PassProbe();
			break;
		}
		if (StepDeadlineSeconds > 12.0f)
		{
			FailProbe(TEXT("the wall never answered"));
		}
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
// Ten staged stops that photograph the systems the README talks about, with
// the same direct-into-Docs/Media discipline the legacy demo captures use.
// Stills land as Docs/Media/<name>.png; the two bursts land under
// Saved/NightCapture/<dir>/frame_%05d.png for the ffmpeg GIF pass.

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

	EnterCaptureStep(0);
	GetWorldTimerManager().SetTimer(
		CaptureTimer,
		this,
		&AIGListenerGreyboxDirector::AdvanceNightCapture,
		0.04f,
		true);
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
		CaptureTeleportPlayer(FVector(162.0f, -164.0f, 92.0f), 90.0f, -12.0f);
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
				FVector(300.0f, -305.0f, 960.0f),
				FRotator(0.0f, 180.0f, 0.0f),
				false,
				true);
			Entity->SetPatrolPoints({FVector(300.0f, -305.0f, 960.0f)});
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
				FVector(-220.0f, -305.0f, 960.0f), 0.45f, Player.Get());
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
			GetWorldTimerManager().ClearTimer(CaptureTimer);
			UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_CAPTURE DONE"));
			RequestExit(false);
		}
		break;
	default:
		break;
	}
}
