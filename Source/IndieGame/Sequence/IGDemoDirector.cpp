#include "Sequence/IGDemoDirector.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "IndieGame.h"
#include "Interaction/IGAlarmClock.h"
#include "Interaction/IGCheckoutCounter.h"
#include "Interaction/IGElevator.h"
#include "Interaction/IGFridge.h"
#include "Interaction/IGInteractable.h"
#include "Interaction/IGPickupItem.h"
#include "Interaction/IGSwingDoor.h"
#include "Misc/Paths.h"
#include "Sequence/IGWakeUpDirector.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

AIGDemoDirector::AIGDemoDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AIGDemoDirector::ConfigureDemo(
	AIGWakeUpDirector* InWakeDirector,
	AIGAlarmClock* InAlarmClock,
	AIGFridge* InFridge,
	AIGPickupItem* InWallet,
	AIGSwingDoor* InHomeDoor,
	AIGElevator* InElevator,
	AIGSwingDoor* InBuildingDoor,
	AIGPickupItem* InWaterBottle,
	AIGCheckoutCounter* InCheckout,
	const bool bInCaptureStills,
	const bool bInDumpFrames,
	const bool bInExitWhenDone)
{
	WakeDirector = InWakeDirector;
	AlarmClock = InAlarmClock;
	Fridge = InFridge;
	Wallet = InWallet;
	HomeDoor = InHomeDoor;
	Elevator = InElevator;
	BuildingDoor = InBuildingDoor;
	WaterBottle = InWaterBottle;
	Checkout = InCheckout;
	bCaptureStills = bInCaptureStills;
	bDumpFrames = bInDumpFrames;
	bExitWhenDone = bInExitWhenDone;

	BuildScript();
	StepIndex = INDEX_NONE;
	AdvanceStep();
	SetActorTickEnabled(true);

	if (bDumpFrames)
	{
		IFileManager::Get().MakeDirectory(
			*FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DemoFrames")), true);
	}

	UE_LOG(
		LogIndieGame,
		Display,
		TEXT("Demo walkthrough started (%d steps, stills=%d frames=%d)"),
		Steps.Num(),
		bCaptureStills ? 1 : 0,
		bDumpFrames ? 1 : 0);
}

void AIGDemoDirector::BeginPlay()
{
	Super::BeginPlay();
}

AIGDemoDirector::FIGDemoStep AIGDemoDirector::MakeWait(
	const float Duration, const FVector& LookAt, const bool bLook)
{
	FIGDemoStep Step;
	Step.Type = EIGDemoStepType::Wait;
	Step.Duration = Duration;
	Step.LookAt = LookAt;
	Step.bHasLookAt = bLook;
	return Step;
}

AIGDemoDirector::FIGDemoStep AIGDemoDirector::MakeRideWait(
	const float Duration,
	const FVector& LookAt,
	const float ReferencePawnZ)
{
	FIGDemoStep Step = MakeWait(Duration, LookAt);
	Step.bTrackPawnHeight = true;
	Step.LookAtPawnZOffset = LookAt.Z - ReferencePawnZ;
	return Step;
}

AIGDemoDirector::FIGDemoStep AIGDemoDirector::MakeWalk(const FVector& Target)
{
	FIGDemoStep Step;
	Step.Type = EIGDemoStepType::WalkTo;
	Step.Target = Target;
	return Step;
}

AIGDemoDirector::FIGDemoStep AIGDemoDirector::MakeWalkLook(
	const FVector& Target, const FVector& LookAt)
{
	FIGDemoStep Step = MakeWalk(Target);
	Step.LookAt = LookAt;
	Step.bHasLookAt = true;
	return Step;
}

AIGDemoDirector::FIGDemoStep AIGDemoDirector::MakeInteract(
	AActor* Actor, const float PostDelay, const FVector& LookAt)
{
	FIGDemoStep Step;
	Step.Type = EIGDemoStepType::Interact;
	Step.Actor = Actor;
	Step.Duration = PostDelay;
	Step.LookAt = LookAt;
	Step.bHasLookAt = true;
	return Step;
}

AIGDemoDirector::FIGDemoStep AIGDemoDirector::MakeStill(const TCHAR* Name)
{
	FIGDemoStep Step;
	Step.Type = EIGDemoStepType::Still;
	Step.Duration = 0.35f;
	Step.StillName = Name;
	return Step;
}

void AIGDemoDirector::BuildScript()
{
	Steps.Reset();

	constexpr float FloorZ = 900.0f; // unit 404 is on the 4th floor
	constexpr float StandingPawnZ = FloorZ + 98.0f;
	const FVector AlarmSpot(-160, -35, FloorZ + 75);
	const FVector FridgeFace(122, -20, FloorZ + 105);
	const FVector FridgeInside(150, -20, FloorZ + 100);
	const FVector WalletSpot(-150, -185, FloorZ + 80);
	const FVector DoorSpot(140, -218, FloorZ + 120);
	const FVector ElevatorSpot(700, -305, FloorZ + 110);
	const FVector KitchenSpot(168, 130, FloorZ + 110);
	const FVector NeighbourDoorSpot(-40, -242, FloorZ + 118);
	// Look through the doorway at the whole car. The previous target sat on
	// the near-right COP and made the jamb fill a quarter of the still.
	const FVector CabPanelSpot(820, -305, 140);
	const FVector LobbyDoorSpot(643, -380, 115);
	const FVector VillaFacadeSpot(240, -390, 620);
	const FVector StoreFar(2400, -457, 240);
	const FVector LampSpot(1000, -565, 340);
	const FVector CoolerSpot(2925, -552, 125);
	// Close on the cup ramyeon standing on the gondola top: the cup, its
	// printed sleeve and the foil lid are three separate props, and this is
	// the angle that shows whether they line up.
	const FVector RamyeonSpot(2620, -368, 150);
	const FVector RegisterSpot(2620, -262, 102);
	const FVector StoreDoorSpot(2405, -457, 150);

	// -- waking up ---------------------------------------------------------
	Steps.Add(MakeWait(2.7f, AlarmSpot, false)); // eyes opening under the fade
	Steps.Add(MakeStill(TEXT("prologue-bedroom")));
	Steps.Add(MakeWait(1.2f, AlarmSpot));        // turn toward the noise
	{
		FIGDemoStep Stop;
		Stop.Type = EIGDemoStepType::StopAlarm;
		Stop.Duration = 1.2f;
		Stop.LookAt = AlarmSpot;
		Stop.bHasLookAt = true;
		Steps.Add(Stop);
	}
	{
		FIGDemoStep GetUp;
		GetUp.Type = EIGDemoStepType::RequestGetUp;
		GetUp.Duration = 3.2f;
		Steps.Add(GetUp);
	}

	// -- the empty fridge --------------------------------------------------
	Steps.Add(MakeWalkLook(FVector(20, 10, 0), KitchenSpot));
	Steps.Add(MakeWait(0.8f, KitchenSpot));      // the fitted kitchen line
	Steps.Add(MakeStill(TEXT("prologue-kitchen")));
	Steps.Add(MakeWalkLook(FVector(88, -20, 0), FridgeFace));
	Steps.Add(MakeInteract(Fridge.Get(), 3.8f, FridgeInside));

	// -- wallet, then out the door ----------------------------------------
	Steps.Add(MakeWalkLook(FVector(-70, -140, 0), WalletSpot));
	Steps.Add(MakeInteract(Wallet.Get(), 1.5f, WalletSpot));
	// Aim beyond the doorway centre: the walk step completes 55 cm early,
	// leaving the pawn aligned with the narrow opening before the door moves.
	Steps.Add(MakeWalkLook(FVector(198, -160, 0), DoorSpot));
	Steps.Add(MakeInteract(HomeDoor.Get(), 1.7f, DoorSpot));
	// The matching overshoot settles the pawn near y=-305, squarely across
	// the threshold and clear of both the opened leaf and corridor walls.
	Steps.Add(MakeWalk(FVector(143, -360, 0)));

	// -- 4F corridor and the elevator ride down ---------------------------
	// The lift is at the far end, so this is a real walk down the hallway.
	// Frame the landing from far enough east that our own swung-open leaf is
	// seen edge-on instead of filling the lens.
	Steps.Add(MakeWalkLook(FVector(330, -300, 0), NeighbourDoorSpot));
	Steps.Add(MakeWait(1.0f, NeighbourDoorSpot));
	Steps.Add(MakeStill(TEXT("prologue-corridor")));
	Steps.Add(MakeWalkLook(FVector(430, -305, 0), FVector(700, -300, FloorZ + 150)));
	Steps.Add(MakeWalk(FVector(520, -305, 0)));
	Steps.Add(MakeWalkLook(FVector(600, -305, 0), ElevatorSpot));
	Steps.Add(MakeInteract(Elevator.Get(), 1.6f, ElevatorSpot)); // call it
	// Walk target sits beyond the desired stopping point because the tour's
	// 55 cm arrival radius would otherwise advance while the capsule was
	// still straddling the lift threshold.
	Steps.Add(MakeWalk(FVector(840, -305, 0)));                  // step fully inside
	{
		// Doors close, four floors chime past, doors open on the lobby. Look
		// around the car on the way down rather than at the doors from 10 cm.
		// Every target here has to be in the frame the rider is actually in:
		// aiming at a lobby-height point while still on the fourth floor
		// pitches the camera straight at the floor.
		// Track the rider's vertical position. Fixed 4F look targets survived
		// the closed-door transfer in the old capture and yanked the camera up
		// toward the sky while the pawn was already standing in the lobby cab.
		Steps.Add(MakeRideWait(
			2.4f, FVector(747, -232, FloorZ + 122), StandingPawnZ)); // the COP
		Steps.Add(MakeRideWait(
			3.1f, FVector(846, -305, FloorZ + 165), StandingPawnZ)); // rear panel
		Steps.Add(MakeRideWait(
			5.0f, FVector(714, -305, FloorZ + 128), StandingPawnZ)); // doors
	}
	// Step out and turn back: the open car from the landing shows the panel,
	// the handrails, the lit ceiling and the stone floor in one frame.
	// Likewise, aim 55 cm beyond the documentation camera position so the
	// captured view actually settles at X=625 rather than X=680.
	Steps.Add(MakeWalkLook(FVector(570, -305, 0), CabPanelSpot));
	Steps.Add(MakeWait(1.1f, CabPanelSpot));
	Steps.Add(MakeStill(TEXT("prologue-elevator")));
	Steps.Add(MakeWalkLook(FVector(640, -345, 0), FVector(530, -243, 152)));
	Steps.Add(MakeWait(0.9f, FVector(530, -243, 152)));           // mailbox wall
	Steps.Add(MakeStill(TEXT("prologue-lobby")));
	Steps.Add(MakeWalkLook(FVector(643, -340, 0), LobbyDoorSpot));
	Steps.Add(MakeInteract(BuildingDoor.Get(), 1.8f, LobbyDoorSpot));
	Steps.Add(MakeWalkLook(FVector(643, -455, 0), FVector(900, -520, 150)));

	// Look back at the villa from the alley: granite cladding, the railings
	// and the pilotis car park under it.
	Steps.Add(MakeWalkLook(FVector(760, -600, 0), VillaFacadeSpot));
	Steps.Add(MakeWait(1.1f, VillaFacadeSpot));
	Steps.Add(MakeStill(TEXT("prologue-villa")));

	// -- the alley ---------------------------------------------------------
	Steps.Add(MakeWalkLook(FVector(700, -520, 0), StoreFar));
	// Let the turn finish before the shutter: stills fired mid-rotation come
	// out as a smear of motion blur.
	Steps.Add(MakeWait(1.0f, StoreFar));
	Steps.Add(MakeStill(TEXT("prologue-alley")));
	Steps.Add(MakeWalk(FVector(1000, -537, 0)));
	Steps.Add(MakeWait(1.5f, LampSpot));         // the streetlight dies overhead
	Steps.Add(MakeWalkLook(FVector(1520, -520, 0), StoreFar));
	Steps.Add(MakeWalk(FVector(2080, -490, 0)));
	Steps.Add(MakeWalk(FVector(2330, -460, 0))); // sliding door senses us
	Steps.Add(MakeWait(1.2f, StoreDoorSpot));    // let both glass leaves clear

	// -- the store ---------------------------------------------------------
	// Continue well past the kick rail. The former shallow target completed
	// at the door plane, where the capsule could remain caught on the sill.
	Steps.Add(MakeWalk(FVector(2515, -457, 0)));
	Steps.Add(MakeWalkLook(FVector(2620, -450, 0), RamyeonSpot));
	Steps.Add(MakeWait(1.1f, RamyeonSpot));
	Steps.Add(MakeStill(TEXT("prologue-ramyeon")));
	Steps.Add(MakeWalkLook(FVector(2640, -460, 0), FVector(2900, -430, 140)));
	Steps.Add(MakeWalkLook(FVector(2800, -470, 0), CoolerSpot));
	Steps.Add(MakeWait(0.9f, CoolerSpot));       // settle, then frame the cooler
	Steps.Add(MakeStill(TEXT("prologue-store")));
	Steps.Add(MakeWalkLook(FVector(2848, -532, 0), CoolerSpot));
	Steps.Add(MakeInteract(WaterBottle.Get(), 2.2f, CoolerSpot));
	// Reach the register around the open east ends of the gondola and
	// counter. The 62 cm slot between them is narrower than the capsule.
	Steps.Add(MakeWalk(FVector(2900, -430, 0)));
	Steps.Add(MakeWalk(FVector(2840, -195, 0)));
	Steps.Add(MakeWalkLook(FVector(2665, -250, 0), RegisterSpot));
	Steps.Add(MakeInteract(Checkout.Get(), 3.4f, RegisterSpot));
	Steps.Add(MakeWait(2.6f, StoreDoorSpot));    // ...the chime rings by itself
	Steps.Add(MakeWait(3.0f, StoreDoorSpot));

	if (bExitWhenDone)
	{
		FIGDemoStep ExitStep;
		ExitStep.Type = EIGDemoStepType::Exit;
		Steps.Add(ExitStep);
	}
}

void AIGDemoDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bScriptFinished || !Steps.IsValidIndex(StepIndex))
	{
		return;
	}

	DumpFrameIfDue(DeltaSeconds);
	UpdateLook(DeltaSeconds);

	FIGDemoStep& Step = Steps[StepIndex];
	StepElapsed += DeltaSeconds;

	switch (Step.Type)
	{
	case EIGDemoStepType::WalkTo:
		UpdateWalk(DeltaSeconds);
		break;

	case EIGDemoStepType::Wait:
	case EIGDemoStepType::Interact:
	case EIGDemoStepType::StopAlarm:
	case EIGDemoStepType::RequestGetUp:
	case EIGDemoStepType::Still:
		if (StepElapsed >= Step.Duration)
		{
			AdvanceStep();
		}
		break;

	case EIGDemoStepType::Exit:
		break;
	}
}

void AIGDemoDirector::AdvanceStep()
{
	++StepIndex;
	StepElapsed = 0.0f;
	WalkTimeout = 0.0f;

	if (!Steps.IsValidIndex(StepIndex))
	{
		bScriptFinished = true;
		return;
	}

	FIGDemoStep& Step = Steps[StepIndex];
	if (Step.bHasLookAt)
	{
		CurrentLookAt = Step.LookAt;
		bHasCurrentLookAt = true;
	}

	switch (Step.Type)
	{
	case EIGDemoStepType::WalkTo:
		if (!Step.bHasLookAt)
		{
			// Look where we are going.
			CurrentLookAt = Step.Target + FVector(0, 0, 140);
			bHasCurrentLookAt = true;
		}
		break;

	case EIGDemoStepType::StopAlarm:
		if (AIGAlarmClock* Alarm = AlarmClock.Get())
		{
			Alarm->StopAlarm();
		}
		break;

	case EIGDemoStepType::RequestGetUp:
		if (AIGWakeUpDirector* Wake = WakeDirector.Get())
		{
			Wake->RequestGetUp();
		}
		break;

	case EIGDemoStepType::Interact:
		ExecuteInteraction(Step.Actor.Get());
		break;

	case EIGDemoStepType::Still:
		RequestStill(Step.StillName);
		break;

	case EIGDemoStepType::Exit:
		UE_LOG(LogIndieGame, Display, TEXT("Demo walkthrough complete; exiting."));
		FPlatformMisc::RequestExitWithStatus(
			false,
			0,
			TEXT("CH01 spatial-continuity capture completed"));
		break;

	default:
		break;
	}
}

void AIGDemoDirector::UpdateLook(const float DeltaSeconds)
{
	if (!bHasCurrentLookAt)
	{
		return;
	}

	APawn* Pawn = GetDemoPawn();
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Pawn || !Controller)
	{
		return;
	}

	const FVector EyeLocation =
		Pawn->GetActorLocation() + FVector(0, 0, 64);
	FVector ResolvedLookAt = CurrentLookAt;
	if (Steps.IsValidIndex(StepIndex) && Steps[StepIndex].bTrackPawnHeight)
	{
		ResolvedLookAt.Z =
			Pawn->GetActorLocation().Z + Steps[StepIndex].LookAtPawnZOffset;
	}
	const FRotator DesiredRotation =
		(ResolvedLookAt - EyeLocation).Rotation();
	const FRotator NewRotation = FMath::RInterpTo(
		Controller->GetControlRotation(),
		DesiredRotation,
		DeltaSeconds,
		2.4f);
	Controller->SetControlRotation(NewRotation);
}

void AIGDemoDirector::UpdateWalk(const float DeltaSeconds)
{
	APawn* Pawn = GetDemoPawn();
	if (!Pawn || !Steps.IsValidIndex(StepIndex))
	{
		return;
	}

	const FIGDemoStep& Step = Steps[StepIndex];
	FVector ToTarget = Step.Target - Pawn->GetActorLocation();
	ToTarget.Z = 0.0f;

	WalkTimeout += DeltaSeconds;
	if (ToTarget.Size2D() <= ArriveDistance)
	{
		AdvanceStep();
		return;
	}

	if (WalkTimeout >= MaxWalkSeconds)
	{
		// A verification tour must expose a broken threshold instead of
		// teleporting through it and recording an impossible transition.
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT("Demo walk step %d timed out at %s (target %s, remaining %.1f cm); aborting spatial-continuity capture."),
			StepIndex,
			*Pawn->GetActorLocation().ToCompactString(),
			*Step.Target.ToCompactString(),
			ToTarget.Size2D());
		bScriptFinished = true;
		SetActorTickEnabled(false);
		FPlatformMisc::RequestExitWithStatus(
			false,
			1,
			TEXT("CH01 spatial-continuity capture timed out"));
		return;
	}

	Pawn->AddMovementInput(ToTarget.GetSafeNormal2D(), 1.0f);
}

void AIGDemoDirector::ExecuteInteraction(AActor* Target)
{
	APawn* Pawn = GetDemoPawn();
	if (!Target || !Pawn || !Target->GetClass()->ImplementsInterface(UIGInteractable::StaticClass()))
	{
		return;
	}

	FIGInteractionContext Context;
	Context.Interactor = Pawn;
	Context.TargetActor = Target;
	Context.HoldProgress = 1.0f;
	IIGInteractable::Execute_CompleteInteraction(Target, Context);
}

void AIGDemoDirector::RequestStill(const FString& BaseName) const
{
	if (!bCaptureStills || BaseName.IsEmpty())
	{
		return;
	}
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	if (APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PlayerController->ConsoleCommand(TEXT("DisableAllScreenMessages"), true);
	}

	if (BaseName == TEXT("prologue-corridor"))
	{
		// The 404 leaf otherwise occupies most of the frame from the authored
		// landing position and hides the corridor the still is meant to show.
		if (AIGSwingDoor* Door = HomeDoor.Get())
		{
			Door->ForceOpenState(false);
		}
	}

	const FString ScreenshotPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectDir(),
		FString::Printf(TEXT("Docs/Media/%s.png"), *BaseName)));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogIndieGame, Display, TEXT("Capture requested: %s"), *ScreenshotPath);
}

void AIGDemoDirector::DumpFrameIfDue(const float DeltaSeconds)
{
	if (!bDumpFrames)
	{
		return;
	}

	FrameAccumulator += DeltaSeconds;
	if (FrameAccumulator < FrameInterval)
	{
		return;
	}
	FrameAccumulator = 0.0f;

	const FString FramePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("DemoFrames"),
		FString::Printf(TEXT("frame_%05d.png"), FrameCounter++)));
	FScreenshotRequest::RequestScreenshot(FramePath, true, false);
}

APawn* AIGDemoDirector::GetDemoPawn() const
{
	const APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PlayerController ? PlayerController->GetPawn() : nullptr;
}
