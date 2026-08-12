#include "Entity/IGListenerGreyboxDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Camera/CameraComponent.h"
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
#include "Entity/IGMissingFloorFifthDawnDirector.h"
#include "Entity/IGMissingFloorNightFourDirector.h"
#include "Entity/IGMissingFloorNightThreeDirector.h"
#include "Entity/IGMissingFloorPuzzleOneDirector.h"
#include "Entity/IGMissingFloorPuzzleTwoDirector.h"
#include "Entity/IGNightOneBeatDirector.h"
#include "Entity/IGNightPhaseDirector.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Environment/IGDustSubsystem.h"
#include "Player/IGBeamDustComponent.h"
#include "Player/IGFlashlightComponent.h"
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
	bMercyNoteProbeRequested =
		FParse::Param(FCommandLine::Get(), TEXT("IGM65MercyNoteProbe"));

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
	NightThree->OnSolved.AddUObject(
		this, &AIGListenerGreyboxDirector::HandleNightThreeSolved);
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
	if (NightFour)
	{
		NightFour->SetHourActive(bActive);
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
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (NightPhase && Narrative && Narrative->GetNightIndex() == 3)
	{
		// The call happens as soon as signal returns at 05:30. Night 4 may
		// disobey a scene-preservation warning, but it never exists because the
		// protagonist simply forgot to report a voice behind a wall.
		Narrative->SetFirstReportMade(true);
		Narrative->MarkBeatPlayed(FName(TEXT("Night3.FirstReport")));
		NightPhase->CompleteNightGoal();
	}
}

void AIGListenerGreyboxDirector::HandleNightFourResolved()
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!NightPhase || !Narrative || Narrative->GetNightIndex() != 4)
	{
		return;
	}
	// A/B/C all return to the same discoverable world fact. The emotional
	// choice is already stored by the night-four director; dawn owns the call.
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
		Torch->SetAvailable(false);
		Dust->ClearDisturbances();

		UE_LOG(
			LogTemp,
			Display,
			TEXT("MISSINGFLOOR_PERCEPTION PASS: corridor/stairwell reverb, "
				"dust x%.2f over his lane, %d/%d motes lit, dry when dark"),
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
			if (!Narrative->IsPuzzleSolved(FName(TEXT("P3"))))
			{
				FailProbe(TEXT("P3 was not booked after the wall was identified"));
				return;
			}
			if (!Narrative->WasFirstReportMade())
			{
				FailProbe(TEXT("night 3 ended without the 05:30 first report"));
				return;
			}
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
		if (StepDeadlineSeconds > 12.0f)
		{
			FailProbe(TEXT("the wall never answered"));
		}
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
