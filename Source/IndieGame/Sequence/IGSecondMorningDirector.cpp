#include "Sequence/IGSecondMorningDirector.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "IndieGame.h"
#include "Interaction/IGElevator.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGSlidingDoor.h"
#include "Interaction/IGSwingDoor.h"
#include "Interaction/IGTimeEntryPuzzle.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Player/IGStressComponent.h"
#include "Save/IGSaveSubsystem.h"
#include "Sequence/IGChapterTwoHumanGateDirector.h"
#include "TimerManager.h"

namespace IGSecondMorningTimeEntry
{
	FName PuzzleName(const EIGTimeEntryPuzzleId PuzzleId)
	{
		return PuzzleId == EIGTimeEntryPuzzleId::P1Alarm
			? FName(TEXT("P1"))
			: FName(TEXT("P2"));
	}

	FGameplayTag StateTag(
		const EIGTimeEntryPuzzleId PuzzleId,
		const TCHAR* Leaf)
	{
		const TCHAR* PuzzleNameText =
			PuzzleId == EIGTimeEntryPuzzleId::P1Alarm
				? TEXT("P1")
				: TEXT("P2");
		return FGameplayTag::RequestGameplayTag(
			FName(*FString::Printf(
				TEXT("State.CH02.%s.%s"),
				PuzzleNameText,
				Leaf)),
			false);
	}
}

AIGSecondMorningDirector::AIGSecondMorningDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	P2Shutter = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("P2Shutter"));
	P2Shutter->SetupAttachment(SceneRoot);
	P2Shutter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P2Shutter->SetGenerateOverlapEvents(false);
	P2Shutter->SetCanEverAffectNavigation(false);
	P2Shutter->SetVisibility(false, true);
}

void AIGSecondMorningDirector::Configure(
	AIGPrologueWorldScene* InScene,
	AIGChapterTwoHumanGateDirector* InHumanGateDirector,
	AIGSwingDoor* InMirrorRoomDoor,
	UPointLightComponent* InMirrorRoomLamp,
	AIGElevator* InElevator,
	AIGSlidingDoor* InStoreDoor,
	UAudioComponent* InJingleComponent,
	AIGReadableNote* InExistingReceipt,
	AIGReadableNote* InDuplicateReceipt,
	AIGReadableNote* InHomePlanner,
	AIGReadableNote* InMirrorAlarmMemo,
	AIGReadableNote* InMailboxBills,
	AIGReadableNote* InOfferingNote,
	AIGReadableNote* InNightRoster,
	AIGReadableNote* InManagementNotice,
	UStaticMesh* InCubeMesh,
	UStaticMesh* InAlarmHousingMesh,
	UStaticMesh* InPosHousingMesh,
	UMaterialInterface* InBodyMaterial,
	UMaterialInterface* InDisplayOffMaterial,
	UMaterialInterface* InDisplayGlassMaterial,
	UMaterialInterface* InAlarmDisplayOnMaterial,
	UMaterialInterface* InPosDisplayOnMaterial,
	UMaterialInterface* InButtonMaterial)
{
	Scene = InScene;
	HumanGateDirector = InHumanGateDirector;
	MirrorRoomDoor = InMirrorRoomDoor;
	MirrorRoomLamp = InMirrorRoomLamp;
	Elevator = InElevator;
	StoreDoor = InStoreDoor;
	JingleComponent = InJingleComponent;
	ExistingReceipt = InExistingReceipt;
	DuplicateReceipt = InDuplicateReceipt;
	HomePlanner = InHomePlanner;
	MirrorAlarmMemo = InMirrorAlarmMemo;
	MailboxBills = InMailboxBills;
	OfferingNote = InOfferingNote;
	NightRoster = InNightRoster;
	ManagementNotice = InManagementNotice;
	if (P2Shutter)
	{
		P2Shutter->SetStaticMesh(InCubeMesh);
		P2Shutter->SetMaterial(0, InBodyMaterial);
		P2Shutter->SetRelativeLocation(FVector(2405.0f, -457.0f, 230.0f));
		P2Shutter->SetRelativeScale3D(FVector(1.2f, 0.06f, 0.2f));
		P2Shutter->SetCollisionProfileName(
			UCollisionProfile::BlockAll_ProfileName);
	}
	SpawnTimeEntryPuzzles(
		InCubeMesh,
		InAlarmHousingMesh,
		InPosHousingMesh,
		InBodyMaterial,
		InDisplayOffMaterial,
		InDisplayGlassMaterial,
		InAlarmDisplayOnMaterial,
		InPosDisplayOnMaterial,
		InButtonMaterial);
}

void AIGSecondMorningDirector::SpawnTimeEntryPuzzles(
	UStaticMesh* CubeMesh,
	UStaticMesh* AlarmHousingMesh,
	UStaticMesh* PosHousingMesh,
	UMaterialInterface* BodyMaterial,
	UMaterialInterface* DisplayOffMaterial,
	UMaterialInterface* DisplayGlassMaterial,
	UMaterialInterface* AlarmDisplayOnMaterial,
	UMaterialInterface* PosDisplayOnMaterial,
	UMaterialInterface* ButtonMaterial)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh)
	{
		return;
	}

	const auto SpawnPuzzle = [this,
		World,
		CubeMesh,
		AlarmHousingMesh,
		PosHousingMesh,
		BodyMaterial,
		DisplayOffMaterial,
		DisplayGlassMaterial,
		AlarmDisplayOnMaterial,
		PosDisplayOnMaterial,
		ButtonMaterial](
		const EIGTimeEntryPuzzleId PuzzleId,
		const FTransform& Transform)
	{
		AIGTimeEntryPuzzle* Puzzle =
			World->SpawnActorDeferred<AIGTimeEntryPuzzle>(
				AIGTimeEntryPuzzle::StaticClass(),
				Transform,
				this,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Puzzle)
		{
			UStaticMesh* AuthoredHousingMesh =
				PuzzleId == EIGTimeEntryPuzzleId::P1Alarm
					? AlarmHousingMesh
					: PosHousingMesh;
			Puzzle->Configure(
				this,
				PuzzleId,
				CubeMesh,
				AuthoredHousingMesh,
				BodyMaterial,
				DisplayOffMaterial,
				DisplayGlassMaterial,
				AlarmDisplayOnMaterial,
				PosDisplayOnMaterial,
				ButtonMaterial);
			Puzzle->FinishSpawning(Transform);
		}
		return Puzzle;
	};

	P1TimeEntry = SpawnPuzzle(
		EIGTimeEntryPuzzleId::P1Alarm,
		FTransform(
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(376.0f, 32.0f, 982.0f)));
	P2TimeEntry = SpawnPuzzle(
		EIGTimeEntryPuzzleId::P2Transaction,
		FTransform(
			FRotator(0.0f, -90.0f, 0.0f),
			FVector(2600.0f, -282.0f, 122.0f)));
}

void AIGSecondMorningDirector::BeginPlay()
{
	Super::BeginPlay();
	ResolveTags();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->OnStoryStateTagChanged.AddUniqueDynamic(
				this, &ThisClass::HandleStoryStateChanged);
		}
	}

	if (Elevator)
	{
		Elevator->OnIntermediateStopOpened.AddUniqueDynamic(
			this, &ThisClass::HandleIntermediateStopOpened);
		// 403 is a high-value optional investigation, not a movement lock.
		// Players who distrust the open room may go straight downstairs.
		Elevator->SetInteractionEnabled(true);
	}

	BindNote(ExistingReceipt);
	BindNote(DuplicateReceipt);
	BindNote(HomePlanner);
	BindNote(MirrorAlarmMemo);
	BindNote(MailboxBills);
	BindNote(OfferingNote);
	BindNote(NightRoster);
	BindNote(ManagementNotice);

	GetWorldTimerManager().SetTimer(
		DirectorStepHandle,
		this,
		&ThisClass::HandleDirectorStep,
		0.1f,
		true);
	GetWorldTimerManager().SetTimer(
		PuzzlePressureHandle,
		this,
		&ThisClass::PollPuzzlePressure,
		1.0f,
		true);

	if (JingleComponent)
	{
		JingleComponent->Stop();
		JingleComponent->SetSound(
			UIGToneSequenceSoundWave::CreateStoreJingle(this, -1.0f, 1.12f));
		JingleComponent->Play();
	}

	if (Scene)
	{
		Scene->SetStoreNorthLightsLive(false);
	}

	// A future save/load coordinator may restore tags before this director is
	// spawned. Reconcile that snapshot here instead of relying exclusively on
	// change notifications that have already happened.
	const bool bRestoredLeftHome = HasState(LeftHomeTag);
	if (HasState(LegacyWaterPurchasedTag) && !HasState(CalledEmployeeTag))
	{
		// Pre-REBIRTH saves called this POS interaction a second purchase.
		// Preserve their progress without restoring the contradicted motive.
		AddState(CalledEmployeeTag);
	}
	const bool bRestoredEmployeeCall = HasState(CalledEmployeeTag);
	const bool bRestoredDuplicateRead = HasState(ReadDuplicateReceiptTag);
	const bool bRestoredReturn = HasState(ReturnedTag);
	RestoreTimeEntryPuzzles();
	bP1PressureArmed = HasState(EnteredMirrorRoomTag);
	if (bRestoredDuplicateRead)
	{
		RegisterDeathOverlayTruth();
	}
	// Old CH02 saves exposed only LeftHome. Migrate that fact once, then let
	// the canonical outfit chapter array alone drive the restored player prop.
	RestoreOutfitFromCanonical(bRestoredLeftHome);
	if (bRestoredReturn && !CanConvergeSecondMorning())
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UIGStoryStateSubsystem* StoryState =
				GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
			{
				StoryState->RemoveState(ReturnedTag);
			}
		}
	}
	else
	{
		if (Scene)
		{
			if (bRestoredEmployeeCall
				&& IsPuzzleResolved(FName(TEXT("P2"))))
			{
				Scene->RevealSecondReceipt();
			}
		}
		RefreshReturnGate();
		if (bRestoredReturn)
		{
			StartEndingBeat();
		}
	}

	RefreshObjective();
}

void AIGSecondMorningDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	ClearThreat();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->OnStoryStateTagChanged.RemoveDynamic(
				this, &ThisClass::HandleStoryStateChanged);
		}
	}

	if (Elevator)
	{
		Elevator->OnIntermediateStopOpened.RemoveDynamic(
			this, &ThisClass::HandleIntermediateStopOpened);
	}

	for (AIGReadableNote* Note :
		{ExistingReceipt.Get(), DuplicateReceipt.Get(), HomePlanner.Get(),
			MirrorAlarmMemo.Get(), MailboxBills.Get(), OfferingNote.Get(),
			NightRoster.Get(), ManagementNotice.Get()})
	{
		if (Note)
		{
			Note->OnReadStateChanged.RemoveDynamic(this, &ThisClass::HandleNoteRead);
		}
	}
	for (AIGTimeEntryPuzzle* Puzzle :
		{P1TimeEntry.Get(), P2TimeEntry.Get()})
	{
		if (Puzzle)
		{
			Puzzle->Destroy();
		}
	}
	P1TimeEntry = nullptr;
	P2TimeEntry = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AIGSecondMorningDirector::ResolveTags()
{
	auto Tag = [](const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	};

	StartedTag = Tag(TEXT("State.CH02.Loop.Started"));
	WakeAlarmStoppedTag = Tag(TEXT("State.CH02.Wake.AlarmStopped"));
	WakeStandingTag = Tag(TEXT("State.CH02.Wake.Standing"));
	FridgeCheckedTag = Tag(TEXT("State.CH02.Loop.FridgeChecked"));
	HasWalletTag = Tag(TEXT("State.CH02.Loop.HasWallet"));
	LeftHomeTag = Tag(TEXT("State.CH02.Loop.LeftHome"));
	CorridorDarkTag = Tag(TEXT("State.CH02.Loop.CorridorDark"));
	SawMirrorRoomTag = Tag(TEXT("State.CH02.Loop.SawMirrorRoom"));
	EnteredMirrorRoomTag = Tag(TEXT("State.CH02.Loop.EnteredMirrorRoom"));
	LiftStoppedTag = Tag(TEXT("State.CH02.Loop.LiftStopped"));
	ReadNoticeTag = Tag(TEXT("State.CH02.Loop.ReadNotice"));
	EnteredAlleyTag = Tag(TEXT("State.CH02.Loop.EnteredAlley"));
	EnteredStoreTag = Tag(TEXT("State.CH02.Loop.EnteredStore"));
	SawReceiptTag = Tag(TEXT("State.CH02.Loop.SawReceipt"));
	CalledEmployeeTag = Tag(TEXT("State.CH02.Loop.CalledEmployee"));
	ReadDuplicateReceiptTag =
		Tag(TEXT("State.CH02.Loop.ReadDuplicateReceipt"));
	HumanChecksUnlockedTag =
		Tag(TEXT("State.CH02.HumanGate.Unlocked"));
	HumanHelpAttemptedTag =
		Tag(TEXT("State.CH02.HumanGate.HelpAttempted"));
	HumanLobbyWitnessedTag =
		Tag(TEXT("State.CH02.HumanGate.LobbyWitnessed"));
	StoreHumanMotiveTag =
		Tag(TEXT("State.CH02.HumanGate.StoreMotive"));
	LegacyWaterPurchasedTag =
		Tag(TEXT("State.CH02.Loop.WaterPurchased"));
	ReturnedTag = Tag(TEXT("State.CH02.Loop.Returned"));
	ChapterIdTag = Tag(TEXT("Chapter.CH02"));
	WokeCheckpointTag = Tag(TEXT("Checkpoint.CH02.Woke"));
	CorridorCheckpointTag = Tag(TEXT("Checkpoint.CH02.Corridor"));
	StoreCheckpointTag = Tag(TEXT("Checkpoint.CH02.Store"));
}

void AIGSecondMorningDirector::BindNote(AIGReadableNote* Note)
{
	if (Note)
	{
		Note->OnReadStateChanged.AddUniqueDynamic(this, &ThisClass::HandleNoteRead);
	}
}

bool AIGSecondMorningDirector::HasState(const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return false;
	}
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			return StoryState->HasState(Tag);
		}
	}
	return false;
}

void AIGSecondMorningDirector::AddState(const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->AddState(Tag);
		}
	}
}

void AIGSecondMorningDirector::RemoveState(const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->RemoveState(Tag);
		}
	}
}

bool AIGSecondMorningDirector::IsActive() const
{
	return HasState(StartedTag) && !HasState(ReturnedTag);
}

FText AIGSecondMorningDirector::GetObjectiveText() const
{
	if (!IsActive())
	{
		return FText::GetEmpty();
	}

	if (!HasState(WakeAlarmStoppedTag))
	{
		return NSLOCTEXT("IGCH02", "ObjectiveStopAlarm", "알람을 끄자");
	}
	if (!HasState(LeftHomeTag))
	{
		return NSLOCTEXT(
			"IGCH02",
			"ObjectiveLeaveHome",
			"달라진 아침을 확인하거나 복도로 나가자");
	}
	if (CanConvergeSecondMorning())
	{
		return NSLOCTEXT(
			"IGCH02",
			"ObjectiveConvergedReturn",
			"방으로 돌아가 달라진 소리를 확인하자");
	}
	if (!HasState(EnteredStoreTag)
		&& HasState(StoreHumanMotiveTag))
	{
		return NSLOCTEXT(
			"IGCH02",
			"ObjectiveFindHumanAtStore",
			"불 켜진 편의점에서 사람을 찾자");
	}
	if (!HasState(EnteredStoreTag)
		&& HasState(HumanChecksUnlockedTag)
		&& !HasState(HumanHelpAttemptedTag)
		&& !HasState(HumanLobbyWitnessedTag))
	{
		return NSLOCTEXT(
			"IGCH02",
			"ObjectiveFindReachableHuman",
			"연락할 사람을 찾아보자");
	}
	if (HasState(SawMirrorRoomTag)
		&& !HasState(EnteredMirrorRoomTag)
		&& !HasState(EnteredAlleyTag)
		&& !HasState(EnteredStoreTag))
	{
		return NSLOCTEXT(
			"IGCH02",
			"ObjectiveMirrorRoom",
			"열린 403호를 보거나 아래층으로 내려가자");
	}
	if (!HasState(EnteredStoreTag))
	{
		return NSLOCTEXT(
			"IGCH02",
			"ObjectiveFindStore",
			"불 켜진 편의점에서 사람을 찾거나 다른 흔적을 확인하자");
	}
	if (!HasState(CalledEmployeeTag))
	{
		return NSLOCTEXT(
			"IGCH02",
			"ObjectiveEmployeeCall",
			"카운터의 직원 호출 버튼을 눌러 보자");
	}
	if (!HasState(SawReceiptTag))
	{
		return NSLOCTEXT(
			"IGCH02",
			"ObjectiveReceipt",
			"POS 옆의 04:44 거래 기록을 확인하자");
	}
	if (!IsPuzzleResolved(FName(TEXT("P2"))))
	{
		return NSLOCTEXT(
			"IGCH02",
			"ObjectiveRestoreTransaction",
			"거래 복원 단말에 처음 결제 시각을 입력하자");
	}
	if (!HasState(ReadDuplicateReceiptTag))
	{
		return NSLOCTEXT(
			"IGCH02",
			"ObjectiveDuplicateReceipt",
			"처음 결제와 지금 기록을 대조하자");
	}
	return NSLOCTEXT(
		"IGCH02",
		"ObjectiveFindSecondTruth",
		"실제 알람 시각이나 나를 찾은 흔적을 확인하자");
}

FString AIGSecondMorningDirector::GetObjectiveTextAscii() const
{
	if (!IsActive())
	{
		return FString();
	}
	if (!HasState(WakeAlarmStoppedTag))
	{
		return TEXT("Turn off the alarm");
	}
	if (!HasState(LeftHomeTag))
	{
		return TEXT("Inspect the changed morning or step into the corridor");
	}
	if (CanConvergeSecondMorning())
	{
		return TEXT("Return home and check the changed sound");
	}
	if (!HasState(EnteredStoreTag)
		&& HasState(StoreHumanMotiveTag))
	{
		return TEXT("Find someone in the lit convenience store");
	}
	if (!HasState(EnteredStoreTag)
		&& HasState(HumanChecksUnlockedTag)
		&& !HasState(HumanHelpAttemptedTag)
		&& !HasState(HumanLobbyWitnessedTag))
	{
		return TEXT("Try to reach another person");
	}
	if (HasState(SawMirrorRoomTag)
		&& !HasState(EnteredMirrorRoomTag)
		&& !HasState(EnteredAlleyTag)
		&& !HasState(EnteredStoreTag))
	{
		return TEXT("Inspect the open 403 or continue downstairs");
	}
	if (!HasState(EnteredStoreTag))
	{
		return TEXT("Look for someone in the lit store or inspect another trace");
	}
	if (!HasState(CalledEmployeeTag))
	{
		return TEXT("Use the employee-call button at the counter");
	}
	if (!HasState(SawReceiptTag))
	{
		return TEXT("Inspect the 04:44 transaction beside the POS");
	}
	if (!IsPuzzleResolved(FName(TEXT("P2"))))
	{
		return TEXT("Enter the original purchase time into the recovery terminal");
	}
	if (!HasState(ReadDuplicateReceiptTag))
	{
		return TEXT("Compare the first purchase with the current record");
	}
	return TEXT("Find the real alarm time or signs that someone searched");
}

float AIGSecondMorningDirector::GetObjectiveProgress() const
{
	if (!HasState(StartedTag))
	{
		return 0.0f;
	}
	if (HasState(ReturnedTag))
	{
		return 1.0f;
	}

	float Progress = 0.0f;
	Progress += HasState(WakeAlarmStoppedTag) ? 0.20f : 0.0f;
	Progress += HasState(LeftHomeTag) ? 0.30f : 0.0f;
	Progress += FMath::Min(GetSecondMorningTruthCount(), 2) * 0.25f;
	return FMath::Clamp(Progress, 0.0f, 0.99f);
}

void AIGSecondMorningDirector::RegisterSecondMorningTruth(
	const TCHAR* TruthTagName,
	const FName SourceId,
	const FName PuzzleId) const
{
	UGameInstance* GameInstance = GetGameInstance();
	if (UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr)
	{
		bool bStateChanged = RebirthState->RegisterTruthSource(
			FGameplayTag::RequestGameplayTag(
				FName(TruthTagName),
				false),
			SourceId);
		if (!PuzzleId.IsNone())
		{
			bStateChanged |= RebirthState->MarkPuzzleResolved(PuzzleId);
		}
		if (bStateChanged)
		{
			RequestCheckpointAutosave(
				HasState(EnteredStoreTag)
					? StoreCheckpointTag
					: HasState(LeftHomeTag)
						? CorridorCheckpointTag
						: WokeCheckpointTag);
		}
	}
	RefreshReturnGate();
}

void AIGSecondMorningDirector::RegisterDeathOverlayTruth() const
{
	RegisterSecondMorningTruth(
		TEXT("Truth.DeathOverlay"),
		FName(TEXT("CH02.DuplicateReceipt")),
		FName(TEXT("P2")));
}

int32 AIGSecondMorningDirector::GetSecondMorningTruthCount() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!RebirthState)
	{
		return 0;
	}

	int32 Confirmed = 0;
	for (const TCHAR* TruthName :
		{TEXT("Truth.Alarm0510"),
			TEXT("Truth.DeathOverlay"),
			TEXT("Truth.WasSearched")})
	{
		const FGameplayTag TruthTag = FGameplayTag::RequestGameplayTag(
			FName(TruthName),
			false);
		Confirmed += RebirthState->HasTruth(TruthTag) ? 1 : 0;
	}
	return Confirmed;
}

bool AIGSecondMorningDirector::CanConvergeSecondMorning() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	return RebirthState
		&& RebirthState->CanConverge(
			EIGRebirthConvergencePoint::C3SecondMorning);
}

bool AIGSecondMorningDirector::RunRebirthEndToEndRoute(
	const bool bP2First,
	const bool bSkipP1,
	const bool bSkipP2)
{
	// Even a freedom route that skips both deductions must prove that the
	// production scene still contains two fully authored, interactive devices.
	const int32 PhysicalContractCount = GetTimeEntryPhysicalContractCount();
	const int32 AuthoredHousingCount = GetAuthoredTimeEntryHousingCount();
	const int32 LayeredDisplayCount = GetLayeredTimeEntryDisplayCount();
	if (PhysicalContractCount != 2
		|| AuthoredHousingCount != 2
		|| LayeredDisplayCount != 2)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_E2E FAIL ch02_route stage=physical "
				"contracts=%d housings=%d displays=%d "
				"p1_contract=%d p1_housing=%d p1_display=%d "
				"p2_contract=%d p2_housing=%d p2_display=%d"),
			PhysicalContractCount,
			AuthoredHousingCount,
			LayeredDisplayCount,
			P1TimeEntry && P1TimeEntry->HasPhysicalContract() ? 1 : 0,
			P1TimeEntry && P1TimeEntry->UsesAuthoredHousing() ? 1 : 0,
			P1TimeEntry && P1TimeEntry->UsesLayeredDisplay() ? 1 : 0,
			P2TimeEntry && P2TimeEntry->HasPhysicalContract() ? 1 : 0,
			P2TimeEntry && P2TimeEntry->UsesAuthoredHousing() ? 1 : 0,
			P2TimeEntry && P2TimeEntry->UsesLayeredDisplay() ? 1 : 0);
		return false;
	}

	UIGRebirthNarrativeSubsystem* RebirthState =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UIGRebirthNarrativeSubsystem>()
			: nullptr;
	if (!RebirthState
		|| !MirrorAlarmMemo
		|| !MailboxBills
		|| !DuplicateReceipt
		|| (bSkipP1 && bSkipP2 && !HomePlanner))
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_E2E FAIL ch02_route stage=prerequisites "
				"state=%d memo=%d bills=%d duplicate=%d planner=%d"),
			RebirthState ? 1 : 0,
			MirrorAlarmMemo ? 1 : 0,
			MailboxBills ? 1 : 0,
			DuplicateReceipt ? 1 : 0,
			HomePlanner ? 1 : 0);
		return false;
	}

	AddState(WakeAlarmStoppedTag);
	AddState(WakeStandingTag);
	AddState(LeftHomeTag);
	// A repeated threshold must not replay or duplicate the CH02 outfit.
	CommitOutfitAtFirstExit();
	// Entering the impossible mirror room unlocks, but never requires, S6's
	// ordinary human checks. Its E2E helper reaches the lobby first to prove
	// that skipping every help interaction still converges on the store.
	AddState(SawMirrorRoomTag);
	AddState(EnteredMirrorRoomTag);
	if (!HumanGateDirector
		|| !HumanGateDirector->RunRebirthEndToEndValidation())
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT("REBIRTH_E2E FAIL ch02_route stage=human_gate director=%d"),
			HumanGateDirector ? 1 : 0);
		return false;
	}
	bool bPuzzleRoutePassed = false;
	if (bSkipP1 && bSkipP2)
	{
		// Both world puzzles remain available. The familiar 404 planner is the
		// authored low-pressure Alarm0510 source for a player who refuses them.
		HandleNoteRead(HomePlanner, false);
		bPuzzleRoutePassed = true;
	}
	else if (bSkipP1)
	{
		bPuzzleRoutePassed = RunP2EndToEndStep();
	}
	else if (bSkipP2)
	{
		bPuzzleRoutePassed = RunP1EndToEndStep();
	}
	else
	{
		bPuzzleRoutePassed = bP2First
			? RunP2EndToEndStep() && RunP1EndToEndStep()
			: RunP1EndToEndStep() && RunP2EndToEndStep();
	}
	if (!bPuzzleRoutePassed)
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_E2E FAIL ch02_route stage=puzzles "
				"p2_first=%d skip_p1=%d skip_p2=%d"),
			bP2First ? 1 : 0,
			bSkipP1 ? 1 : 0,
			bSkipP2 ? 1 : 0);
		return false;
	}
	HandleNoteRead(MailboxBills, false);

	const FIGRebirthNarrativeSnapshot Snapshot =
		RebirthState->BuildSnapshot();
	int32 ChapterTwoOutfitRecords = 0;
	for (const FName ChapterId : Snapshot.EquippedOutfitChapters)
	{
		ChapterTwoOutfitRecords +=
			ChapterId == FName(TEXT("CH02")) ? 1 : 0;
	}
	const bool bP1Resolved = IsPuzzleResolved(FName(TEXT("P1")));
	const bool bP2Resolved = IsPuzzleResolved(FName(TEXT("P2")));
	const int32 ExpectedTruthCount = bSkipP1 || bSkipP2 ? 2 : 3;
	const bool bRouteReady =
		ChapterTwoOutfitRecords == 1
		&& bP1Resolved == !bSkipP1
		&& bP2Resolved == !bSkipP2
		&& GetSecondMorningTruthCount() == ExpectedTruthCount
		&& RebirthState->HasTruth(FGameplayTag::RequestGameplayTag(
			FName(TEXT("Truth.WasSearched")),
			false))
		&& CanConvergeSecondMorning();
	if (bRouteReady)
	{
		AddState(ReturnedTag);
	}
	else
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_E2E FAIL ch02_route stage=convergence "
				"outfits=%d p1=%d p2=%d truths=%d expected_truths=%d "
				"searched=%d converge=%d"),
			ChapterTwoOutfitRecords,
			bP1Resolved ? 1 : 0,
			bP2Resolved ? 1 : 0,
			GetSecondMorningTruthCount(),
			ExpectedTruthCount,
			RebirthState->HasTruth(FGameplayTag::RequestGameplayTag(
				FName(TEXT("Truth.WasSearched")),
				false)) ? 1 : 0,
			CanConvergeSecondMorning() ? 1 : 0);
	}
	return bRouteReady && HasState(ReturnedTag);
}

int32 AIGSecondMorningDirector::GetTimeEntryPhysicalContractCount() const
{
	int32 Count = 0;
	for (const AIGTimeEntryPuzzle* Puzzle :
		{P1TimeEntry.Get(), P2TimeEntry.Get()})
	{
		Count += Puzzle && Puzzle->HasPhysicalContract() ? 1 : 0;
	}
	return Count;
}

int32 AIGSecondMorningDirector::GetTimeEntryMeshComponentCount() const
{
	int32 Count = 0;
	for (const AIGTimeEntryPuzzle* Puzzle :
		{P1TimeEntry.Get(), P2TimeEntry.Get()})
	{
		Count += Puzzle ? Puzzle->GetPhysicalMeshComponentCount() : 0;
	}
	return Count;
}

int32 AIGSecondMorningDirector::GetAuthoredTimeEntryHousingCount() const
{
	int32 Count = 0;
	for (const AIGTimeEntryPuzzle* Puzzle :
		{P1TimeEntry.Get(), P2TimeEntry.Get()})
	{
		Count += Puzzle && Puzzle->UsesAuthoredHousing() ? 1 : 0;
	}
	return Count;
}

int32 AIGSecondMorningDirector::GetLayeredTimeEntryDisplayCount() const
{
	int32 Count = 0;
	for (const AIGTimeEntryPuzzle* Puzzle :
		{P1TimeEntry.Get(), P2TimeEntry.Get()})
	{
		Count += Puzzle && Puzzle->UsesLayeredDisplay() ? 1 : 0;
	}
	return Count;
}

bool AIGSecondMorningDirector::RunP1EndToEndStep()
{
	HandleNoteRead(MirrorAlarmMemo, false);
	if (!P1TimeEntry
		|| !P1TimeEntry->HasPhysicalContract())
	{
		return false;
	}
	for (int32 AttemptIndex = 0; AttemptIndex < 3; ++AttemptIndex)
	{
		P1TimeEntry->HandleButton(EIGTimeEntryButtonAction::Confirm);
	}
	const FGameplayTag P1PressureCap = IGSecondMorningTimeEntry::StateTag(
		EIGTimeEntryPuzzleId::P1Alarm,
		TEXT("Pressure3"));
	if (P1TimeEntry->GetWrongAttempts() != 3
		|| !HasState(P1PressureCap)
		|| !P1TimeEntry->RunAutomatedSolution()
		|| HasState(P1PressureCap))
	{
		return false;
	}
	return true;
}

bool AIGSecondMorningDirector::RunP2EndToEndStep()
{
	AddState(EnteredStoreTag);
	AddState(CalledEmployeeTag);
	HandleNoteRead(ExistingReceipt, true);
	HandleNoteRead(ExistingReceipt, false);
	if (!HasState(IGSecondMorningTimeEntry::StateTag(
			EIGTimeEntryPuzzleId::P2Transaction,
			TEXT("SourceApprovalRead")))
		|| !P2TimeEntry
		|| !P2TimeEntry->HasPhysicalContract())
	{
		return false;
	}
	for (int32 AttemptIndex = 0; AttemptIndex < 3; ++AttemptIndex)
	{
		P2TimeEntry->HandleButton(EIGTimeEntryButtonAction::Confirm);
	}
	const FGameplayTag P2PressureCap = IGSecondMorningTimeEntry::StateTag(
		EIGTimeEntryPuzzleId::P2Transaction,
		TEXT("Pressure3"));
	const float ShutterBottom = P2Shutter
		? P2Shutter->GetRelativeLocation().Z
			- P2Shutter->GetRelativeScale3D().Z * 50.0f
		: 0.0f;
	if (P2TimeEntry->GetWrongAttempts() != 3
		|| !HasState(P2PressureCap)
		|| !P2Shutter
		|| P2Shutter->GetCollisionEnabled()
			!= ECollisionEnabled::QueryAndPhysics
		|| !FMath::IsNearlyEqual(ShutterBottom, 205.0f, 0.5f)
		|| !P2TimeEntry->RunAutomatedSolution()
		|| HasState(P2PressureCap))
	{
		return false;
	}
	HandleNoteRead(DuplicateReceipt, false);
	return HasState(ReadDuplicateReceiptTag);
}

void AIGSecondMorningDirector::RefreshReturnGate() const
{
	if (Scene)
	{
		Scene->SetChapterTwoReturnZoneArmed(
			CanConvergeSecondMorning()
			&& !HasState(ReturnedTag));
	}
}

void AIGSecondMorningDirector::RestoreOutfitFromCanonical(
	const bool bAllowLegacyMigration)
{
	UGameInstance* GameInstance = GetGameInstance();
	UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!RebirthState)
	{
		return;
	}

	const FName ChapterOutfitId(TEXT("CH02"));
	bool bCanonicalEquipped =
		RebirthState->BuildSnapshot().EquippedOutfitChapters.Contains(
			ChapterOutfitId);
	if (!bCanonicalEquipped && bAllowLegacyMigration)
	{
		RebirthState->MarkOutfitEquipped(ChapterOutfitId);
		bCanonicalEquipped =
			RebirthState->BuildSnapshot().EquippedOutfitChapters.Contains(
				ChapterOutfitId);
	}

	const APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr)
	{
		Player->SetRebirthOutfitEquipped(bCanonicalEquipped);
	}
}

void AIGSecondMorningDirector::CommitOutfitAtFirstExit()
{
	UGameInstance* GameInstance = GetGameInstance();
	UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!RebirthState)
	{
		return;
	}

	const FName ChapterOutfitId(TEXT("CH02"));
	const bool bFirstPresentation =
		RebirthState->MarkOutfitEquipped(ChapterOutfitId);
	const bool bCanonicalEquipped =
		RebirthState->BuildSnapshot().EquippedOutfitChapters.Contains(
			ChapterOutfitId);

	const APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr)
	{
		Player->SetRebirthOutfitEquipped(
			bCanonicalEquipped,
			bFirstPresentation);
	}
}

void AIGSecondMorningDirector::RequestCheckpointAutosave(
	const FGameplayTag& CheckpointTag) const
{
	if (!ChapterIdTag.IsValid() || !CheckpointTag.IsValid())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UIGSaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UIGSaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem)
	{
		return;
	}

	const FName MapPackageName =
		GetWorld() ? GetWorld()->GetOutermost()->GetFName() : NAME_None;
	SaveSubsystem->RequestAutosave(
		ChapterIdTag,
		MapPackageName,
		CheckpointTag);
}

bool AIGSecondMorningDirector::IsPuzzleResolved(const FName PuzzleId) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UIGRebirthNarrativeSubsystem* RebirthState = GameInstance
		? GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>()
		: nullptr;
	if (!RebirthState)
	{
		return false;
	}
	const FIGRebirthNarrativeSnapshot Snapshot = RebirthState->BuildSnapshot();
	const TArray<FName>& Resolved = Snapshot.ResolvedPuzzles;
	if (Resolved.Contains(PuzzleId))
	{
		return true;
	}
	// v3 세이브 스키마는 유지하되 기능 프록시 시절 식별자를 읽는다.
	const FName LegacyPuzzleId = PuzzleId == FName(TEXT("P1"))
		? FName(TEXT("P1.AlarmArithmeticProxy"))
		: PuzzleId == FName(TEXT("P2"))
			? FName(TEXT("P2.ReceiptComparisonProxy"))
			: NAME_None;
	return !LegacyPuzzleId.IsNone() && Resolved.Contains(LegacyPuzzleId);
}

void AIGSecondMorningDirector::RestoreTimeEntryPuzzles()
{
	const auto RestorePuzzle = [this](AIGTimeEntryPuzzle* Puzzle)
	{
		if (!Puzzle)
		{
			return;
		}
		const EIGTimeEntryPuzzleId PuzzleId = Puzzle->GetPuzzleId();
		const int32 Hour = HasState(
			IGSecondMorningTimeEntry::StateTag(PuzzleId, TEXT("Hour05")))
			? 5
			: 4;
		const int32 Minute = HasState(
			IGSecondMorningTimeEntry::StateTag(PuzzleId, TEXT("Minute10")))
			? 10
			: HasState(IGSecondMorningTimeEntry::StateTag(
				PuzzleId,
				TEXT("Minute31")))
				? 31
				: 44;
		int32 PressureStage = 0;
		for (int32 Stage = 3; Stage >= 1; --Stage)
		{
			if (HasState(IGSecondMorningTimeEntry::StateTag(
				PuzzleId,
				*FString::Printf(TEXT("Pressure%d"), Stage))))
			{
				PressureStage = Stage;
				break;
			}
		}
		Puzzle->RestoreState(
			Hour,
			Minute,
			PressureStage,
			IsPuzzleResolved(
				IGSecondMorningTimeEntry::PuzzleName(PuzzleId)));
	};

	RestorePuzzle(P1TimeEntry);
	RestorePuzzle(P2TimeEntry);
	RefreshTimeEntryAvailability();
	ApplyP1PressureStage(P1TimeEntry ? P1TimeEntry->GetWrongAttempts() : 0);
	if (HasState(CalledEmployeeTag)
		&& P2TimeEntry
		&& !P2TimeEntry->IsSolved())
	{
		SetP2ShutterStage(P2TimeEntry->GetWrongAttempts());
	}
	else if (P2Shutter)
	{
		P2Shutter->SetVisibility(false, true);
		P2Shutter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AIGSecondMorningDirector::RefreshTimeEntryAvailability()
{
	if (P1TimeEntry)
	{
		P1TimeEntry->SetAvailable(!P1TimeEntry->IsSolved());
	}
	if (P2TimeEntry)
	{
		P2TimeEntry->SetAvailable(
			HasState(CalledEmployeeTag)
			&& !P2TimeEntry->IsSolved());
	}
}

void AIGSecondMorningDirector::ClearTimeEntryStateTags(
	const AIGTimeEntryPuzzle& Puzzle) const
{
	const EIGTimeEntryPuzzleId PuzzleId = Puzzle.GetPuzzleId();
	for (const TCHAR* Leaf : {
		TEXT("Hour05"),
		TEXT("Minute10"),
		TEXT("Minute31"),
		TEXT("Pressure1"),
		TEXT("Pressure2"),
		TEXT("Pressure3")})
	{
		RemoveState(IGSecondMorningTimeEntry::StateTag(PuzzleId, Leaf));
	}
}

void AIGSecondMorningDirector::PersistTimeEntryState(
	const AIGTimeEntryPuzzle& Puzzle) const
{
	ClearTimeEntryStateTags(Puzzle);
	const EIGTimeEntryPuzzleId PuzzleId = Puzzle.GetPuzzleId();
	if (Puzzle.GetHour() == 5)
	{
		AddState(IGSecondMorningTimeEntry::StateTag(
			PuzzleId,
			TEXT("Hour05")));
	}
	if (Puzzle.GetMinute() == 10)
	{
		AddState(IGSecondMorningTimeEntry::StateTag(
			PuzzleId,
			TEXT("Minute10")));
	}
	else if (Puzzle.GetMinute() == 31)
	{
		AddState(IGSecondMorningTimeEntry::StateTag(
			PuzzleId,
			TEXT("Minute31")));
	}
	if (Puzzle.GetWrongAttempts() > 0)
	{
		AddState(IGSecondMorningTimeEntry::StateTag(
			PuzzleId,
			*FString::Printf(
				TEXT("Pressure%d"),
				FMath::Clamp(Puzzle.GetWrongAttempts(), 1, 3))));
	}
}

void AIGSecondMorningDirector::HandleTimeEntrySelectionChanged(
	AIGTimeEntryPuzzle* Puzzle)
{
	if (!Puzzle || Puzzle->IsSolved())
	{
		return;
	}
	PersistTimeEntryState(*Puzzle);
	RequestCheckpointAutosave(
		Puzzle->GetPuzzleId() == EIGTimeEntryPuzzleId::P2Transaction
			? StoreCheckpointTag
			: CorridorCheckpointTag);
}

void AIGSecondMorningDirector::HandleTimeEntryConfirmed(
	AIGTimeEntryPuzzle* Puzzle,
	const bool bCorrect)
{
	if (!Puzzle)
	{
		return;
	}

	const bool bP1 =
		Puzzle->GetPuzzleId() == EIGTimeEntryPuzzleId::P1Alarm;
	if (!bCorrect)
	{
		PersistTimeEntryState(*Puzzle);
		if (bP1)
		{
			ApplyP1PressureStage(Puzzle->GetWrongAttempts());
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateClothSettle(this),
				FVector(525.0f, 85.0f, 965.0f),
				0.86f,
				1.0f,
				90.0f,
				650.0f);
		}
		else
		{
			SetP2ShutterStage(Puzzle->GetWrongAttempts());
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateShutterMotorStep(this),
				FVector(2405.0f, -457.0f, 220.0f));
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateThermalPrinterFeed(this),
				Puzzle->GetActorLocation(),
				0.82f);
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateDoorChime(this),
				FVector(2460.0f, -520.0f, 125.0f),
				0.72f);
		}
		SetThreat(0.16f + Puzzle->GetWrongAttempts() * 0.09f, 4.8f);
		RequestCheckpointAutosave(bP1 ? CorridorCheckpointTag : StoreCheckpointTag);
		return;
	}

	ClearTimeEntryStateTags(*Puzzle);
	if (bP1)
	{
		P1PressureElapsedSeconds = 0.0f;
	}
	else
	{
		P2PressureElapsedSeconds = 0.0f;
	}
	if (bP1)
	{
		RegisterSecondMorningTruth(
			TEXT("Truth.Alarm0510"),
			FName(TEXT("CH02.P1Clock")),
			FName(TEXT("P1")));
		ApplyP1SolvedLight();
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateAlarmFirstNote(this),
			Puzzle->GetActorLocation(),
			0.70f);
	}
	else
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UIGRebirthNarrativeSubsystem* RebirthState =
				GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
			{
				RebirthState->MarkPuzzleResolved(FName(TEXT("P2")));
			}
		}
		if (Scene)
		{
			Scene->RevealSecondReceipt();
		}
		BeginP2ShutterRise();
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateJingleOpeningNotes(this),
			Puzzle->GetActorLocation(),
			0.72f);
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateThermalPrinterFeed(this),
			Puzzle->GetActorLocation(),
			0.95f);
		RequestCheckpointAutosave(StoreCheckpointTag);
	}
	ClearThreat();
	RefreshTimeEntryAvailability();
	RefreshObjective();
}

void AIGSecondMorningDirector::PollPuzzlePressure()
{
	if (AIGReadableNote::GetOpenNote() || CanConvergeSecondMorning())
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const float RiseInterval = Accessibility
		? Accessibility->GetPressureRiseIntervalSeconds()
		: 55.0f;

	const auto TickPressure = [this, RiseInterval](
		AIGTimeEntryPuzzle* Puzzle,
		const bool bActive,
		const bool bP1,
		float& ElapsedSeconds)
	{
		if (!bActive || !Puzzle || Puzzle->IsSolved())
		{
			ElapsedSeconds = 0.0f;
			return;
		}
		ElapsedSeconds += 1.0f;
		if (ElapsedSeconds < RiseInterval)
		{
			return;
		}
		ElapsedSeconds = FMath::Max(0.0f, ElapsedSeconds - RiseInterval);
		RaiseTimedPuzzlePressure(Puzzle, bP1);
	};

	TickPressure(
		P1TimeEntry,
		bP1PressureArmed,
		true,
		P1PressureElapsedSeconds);
	TickPressure(
		P2TimeEntry,
		HasState(CalledEmployeeTag),
		false,
		P2PressureElapsedSeconds);
}

void AIGSecondMorningDirector::RaiseTimedPuzzlePressure(
	AIGTimeEntryPuzzle* Puzzle,
	const bool bP1)
{
	if (!Puzzle || !Puzzle->AdvancePressureStage())
	{
		return;
	}

	PersistTimeEntryState(*Puzzle);
	const int32 PressureStage = Puzzle->GetPressureStage();
	if (bP1)
	{
		ApplyP1PressureStage(PressureStage);
		if (PressureStage >= 2)
		{
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateClothSettle(this),
				FVector(525.0f, 85.0f, 965.0f),
				PressureStage == 2 ? 0.52f : 0.68f,
				PressureStage == 2 ? 0.92f : 0.82f,
				90.0f,
				650.0f);
		}
		if (PressureStage >= 3)
		{
			PlayCorridorSnap(
				MirrorRoomDoor
					? MirrorRoomDoor->GetActorLocation() - FVector(55.0f, 0.0f, 0.0f)
					: GetActorLocation(),
				0.18f);
		}
	}
	else
	{
		SetP2ShutterStage(PressureStage);
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateShutterMotorStep(this),
			FVector(2405.0f, -457.0f, 220.0f),
			PressureStage == 1 ? 0.58f : 0.72f);
		if (PressureStage >= 2)
		{
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateDoorChime(this),
				FVector(2460.0f, -520.0f, 125.0f),
				0.44f);
		}
		if (PressureStage >= 3)
		{
			IGAudio::SpawnOneShotAt(
				this,
				UIGToneSequenceSoundWave::CreateThermalPrinterFeed(this),
				Puzzle->GetActorLocation(),
				0.64f);
		}
	}

	SetThreat(0.16f + PressureStage * 0.09f, 4.8f);
	RequestCheckpointAutosave(bP1 ? CorridorCheckpointTag : StoreCheckpointTag);
}

bool AIGSecondMorningDirector::RequestManualHint()
{
	if (AIGReadableNote::GetOpenNote() || CanConvergeSecondMorning())
	{
		return false;
	}
	const APlayerController* Controller =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return false;
	}

	AIGTimeEntryPuzzle* Candidate = nullptr;
	bool bCandidateIsP1 = false;
	float BestDistanceSquared = FMath::Square(420.0f);
	const auto Consider = [&Candidate, &bCandidateIsP1, &BestDistanceSquared, Pawn](
		AIGTimeEntryPuzzle* Puzzle,
		const bool bActive,
		const bool bP1)
	{
		if (!bActive || !Puzzle || Puzzle->IsSolved())
		{
			return;
		}
		const float DistanceSquared = FVector::DistSquared(
			Pawn->GetActorLocation(),
			Puzzle->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared)
		{
			Candidate = Puzzle;
			bCandidateIsP1 = bP1;
			BestDistanceSquared = DistanceSquared;
		}
	};
	Consider(P1TimeEntry, bP1PressureArmed, true);
	Consider(P2TimeEntry, HasState(CalledEmployeeTag), false);
	if (!Candidate)
	{
		return false;
	}

	if (bCandidateIsP1)
	{
		PresentP1ManualHint();
	}
	else
	{
		PresentP2ManualHint();
	}
	return true;
}

void AIGSecondMorningDirector::PresentP1ManualHint()
{
	static const FText Hints[] =
	{
		NSLOCTEXT("IGCH02", "P1ManualHintContext", "집결은 5시 30분. 알람은 그보다 스무 분 전이었어."),
		NSLOCTEXT("IGCH02", "P1ManualHintRelation", "05:30과 ‘20분 전’을 한 시각으로 맞춰 보자."),
		NSLOCTEXT("IGCH02", "P1ManualHintAnswer", "집결 시각보다 20분 앞선 05:10을 입력하자.")
	};
	AIGHorrorHUD::PushThought(this, Hints[P1ManualHintStage], 4.8f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateScannerBeep(this),
		P1TimeEntry ? P1TimeEntry->GetActorLocation() : GetActorLocation(),
		0.28f,
		1.0f + P1ManualHintStage * 0.08f);
	P1ManualHintStage = FMath::Min(P1ManualHintStage + 1, 2);
}

void AIGSecondMorningDirector::PresentP2ManualHint()
{
	static const FText Hints[] =
	{
		NSLOCTEXT("IGCH02", "P2ManualHintContext", "처음 받은 영수증과 지금 남은 거래 기록을 다시 보자."),
		NSLOCTEXT("IGCH02", "P2ManualHintRelation", "두 기록에서 서로 다른 결제 시각을 대조하면 돼."),
		NSLOCTEXT("IGCH02", "P2ManualHintAnswer", "처음 결제한 시각 04:31을 복원 단말에 입력하자.")
	};
	AIGHorrorHUD::PushThought(this, Hints[P2ManualHintStage], 4.8f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateThermalPrinterFeed(this),
		P2TimeEntry ? P2TimeEntry->GetActorLocation() : GetActorLocation(),
		0.34f);
	P2ManualHintStage = FMath::Min(P2ManualHintStage + 1, 2);
}

void AIGSecondMorningDirector::ApplyP1PressureStage(
	const int32 PressureStage)
{
	if (MirrorRoomLamp && !IsPuzzleResolved(FName(TEXT("P1"))))
	{
		MirrorRoomLamp->SetIntensity(
			300.0f * (1.0f - FMath::Clamp(PressureStage, 0, 3) * 0.10f));
	}
}

void AIGSecondMorningDirector::ApplyP1SolvedLight()
{
	if (!MirrorRoomLamp)
	{
		return;
	}
	MirrorRoomLamp->SetVisibility(true);
	MirrorRoomLamp->SetIntensity(620.0f);
	GetWorldTimerManager().ClearTimer(P1SolvedLightHandle);
	GetWorldTimerManager().SetTimer(
		P1SolvedLightHandle,
		this,
		&ThisClass::RestoreP1LightAfterSolve,
		2.0f,
		false);
}

void AIGSecondMorningDirector::RestoreP1LightAfterSolve()
{
	if (MirrorRoomLamp)
	{
		MirrorRoomLamp->SetIntensity(300.0f);
	}
}

void AIGSecondMorningDirector::SetP2ShutterStage(
	const int32 PressureStage)
{
	if (!P2Shutter)
	{
		return;
	}
	const float Height = 20.0f + FMath::Clamp(PressureStage, 0, 3) * 5.0f;
	P2Shutter->SetRelativeScale3D(FVector(1.2f, 0.06f, Height / 100.0f));
	P2Shutter->SetRelativeLocation(FVector(
		2405.0f,
		-457.0f,
		240.0f - Height * 0.5f));
	P2Shutter->SetVisibility(true, true);
	P2Shutter->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AIGSecondMorningDirector::BeginP2ShutterRise()
{
	if (!P2Shutter || !GetWorld())
	{
		return;
	}
	P2ShutterAnimationStartTime = GetWorld()->GetTimeSeconds();
	P2ShutterAnimationStartZ = P2Shutter->GetRelativeLocation().Z;
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateShutterMotorRise(this),
		P2Shutter->GetComponentLocation());
	GetWorldTimerManager().ClearTimer(P2ShutterAnimationHandle);
	GetWorldTimerManager().SetTimer(
		P2ShutterAnimationHandle,
		this,
		&ThisClass::AnimateP2ShutterRise,
		0.05f,
		true);
}

void AIGSecondMorningDirector::AnimateP2ShutterRise()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!P2Shutter)
	{
		World->GetTimerManager().ClearTimer(P2ShutterAnimationHandle);
		return;
	}
	const float Alpha = FMath::Clamp(
		static_cast<float>(
			(World->GetTimeSeconds() - P2ShutterAnimationStartTime) / 1.2),
		0.0f,
		1.0f);
	FVector Location = P2Shutter->GetRelativeLocation();
	Location.Z = FMath::Lerp(P2ShutterAnimationStartZ, 265.0f, Alpha);
	P2Shutter->SetRelativeLocation(Location);
	if (Alpha >= 1.0f)
	{
		GetWorldTimerManager().ClearTimer(P2ShutterAnimationHandle);
		P2Shutter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P2Shutter->SetVisibility(false, true);
	}
}

void AIGSecondMorningDirector::HandleStoryStateChanged(
	const FGameplayTag StateTag,
	const bool bAdded)
{
	if (!bAdded)
	{
		return;
	}

	if (StateTag.MatchesTagExact(LeftHomeTag))
	{
		CommitOutfitAtFirstExit();
		StartCorridorBlackout();
		RequestCheckpointAutosave(CorridorCheckpointTag);
	}
	else if (StateTag.MatchesTagExact(EnteredMirrorRoomTag))
	{
		StartMirrorRoomBeat();
	}
	else if (StateTag.MatchesTagExact(EnteredStoreTag))
	{
		StartStoreBeat();
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UIGRebirthNarrativeSubsystem* RebirthState =
				GameInstance->GetSubsystem<UIGRebirthNarrativeSubsystem>())
			{
				RebirthState->MarkLocationVisited(FName(TEXT("CH02.Store")));
			}
		}
		RefreshReturnGate();
		RequestCheckpointAutosave(StoreCheckpointTag);
	}
	else if (StateTag.MatchesTagExact(CalledEmployeeTag))
	{
		// 직원 호출은 P2 입력 단말과 셔터만 깨운다. 덮인 거래 기록은
		// 플레이어가 04:31을 직접 복원한 뒤에만 출력된다.
		RefreshTimeEntryAvailability();
		SetP2ShutterStage(P2TimeEntry ? P2TimeEntry->GetWrongAttempts() : 0);
		RequestCheckpointAutosave(StoreCheckpointTag);
	}
	else if (StateTag.MatchesTagExact(ReadDuplicateReceiptTag))
	{
		// This is the first actual CH02 observation of the 04:44 overwrite.
		// Keep the original 04:31 purchase profile untouched.
		RegisterDeathOverlayTruth();
	}
	else if (StateTag.MatchesTagExact(ReturnedTag))
	{
		// Scene-side arming is the primary gate; keep this prerequisite here
		// as a defensive check for scripted/direct state injection.
		if (CanConvergeSecondMorning())
		{
			// Returned is emitted by the physical fourth-floor homecoming
			// volume. Saving the store checkpoint here used to reload the
			// player beside the POS while the 404 ending beat played upstairs.
			RequestCheckpointAutosave(CorridorCheckpointTag);
			StartEndingBeat();
		}
		else if (UGameInstance* GameInstance = GetGameInstance())
		{
			// Do not leave an impossible Returned tag latched: IsActive()
			// treats it as chapter completion and would otherwise hide the
			// objective permanently before two independent truths exist.
			if (UIGStoryStateSubsystem* StoryState =
				GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
			{
				StoryState->RemoveState(ReturnedTag);
			}
		}
	}

	RefreshObjective();
}

void AIGSecondMorningDirector::RefreshObjective()
{
	if (APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (AIGHorrorHUD* HUD = Cast<AIGHorrorHUD>(PlayerController->GetHUD()))
		{
			HUD->SetObjectiveProvider(this);
		}
	}
}

void AIGSecondMorningDirector::StartCorridorBlackout()
{
	if (NextCorridorFixture != INDEX_NONE || !Scene)
	{
		return;
	}

	// The west fixture is already dead when the door opens. The remaining
	// three only fail after the player has passed them and can no longer see
	// them, with at least three seconds between snaps.
	Scene->SuspendCorridorFlicker(true);
	Scene->SetFixtureLive(0, false, true);
	NextCorridorFixture = 1;
	NextBlackoutAllowedTime =
		(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + 3.0;
}

void AIGSecondMorningDirector::HandleDirectorStep()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (ActiveThreatPressure > 0.0f)
	{
		if (Now < ThreatEndTime)
		{
			// Stress deliberately forgets threat pressure each frame. A story
			// beat that lasts seconds must therefore reassert its pressure.
			ApplyThreatPressure(ActiveThreatPressure);
		}
		else
		{
			ClearThreat();
		}
	}

	if (NextCorridorFixture == INDEX_NONE || !Scene)
	{
		return;
	}

	if (Now < NextBlackoutAllowedTime)
	{
		return;
	}

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	const FVector FixtureLocation = Scene->GetCorridorFixtureLocation(NextCorridorFixture);
	const FVector PlayerLocation = Pawn->GetActorLocation();
	const FVector ToFixture = (FixtureLocation - PlayerLocation).GetSafeNormal();
	const FVector ViewForward = PlayerController->GetControlRotation().Vector();
	const bool bPassedFixture = PlayerLocation.X > FixtureLocation.X + 55.0f;
	const bool bOutsideView = FVector::DotProduct(ViewForward, ToFixture) < 0.15f;
	if (!bPassedFixture || !bOutsideView)
	{
		return;
	}

	const float ScareAmounts[] = {0.10f, 0.14f, 0.18f};
	const int32 ScareIndex = FMath::Clamp(NextCorridorFixture - 1, 0, 2);
	const int32 FixtureToKill = NextCorridorFixture;
	PlayCorridorSnap(FixtureLocation, ScareAmounts[ScareIndex]);
	// Let the relay crack lead the picture by a few frames. The player hears
	// an electrical cause, then turns to discover its visual consequence.
	FTimerHandle FixtureKillHandle;
	const TWeakObjectPtr<AIGSecondMorningDirector> WeakThis(this);
	GetWorldTimerManager().SetTimer(
		FixtureKillHandle,
		[WeakThis, FixtureToKill]()
		{
			if (AIGSecondMorningDirector* Director = WeakThis.Get();
				Director && Director->Scene)
			{
				Director->Scene->SetFixtureLive(FixtureToKill, false, true);
			}
		},
		0.055f,
		false);

	if (NextCorridorFixture == 1)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH02", "BehindLightsThought", "…뒤에서 나갔나."),
			3.6f);
	}

	++NextCorridorFixture;
	NextBlackoutAllowedTime = Now + 3.0;
	if (NextCorridorFixture >= Scene->GetCorridorFixtureCount())
	{
		NextCorridorFixture = INDEX_NONE;
		AddState(CorridorDarkTag);
	}
}

void AIGSecondMorningDirector::PlayCorridorSnap(
	const FVector& Location,
	const float ScareAmount)
{
	AIGHorrorHUD::PushFearDirection(this, Location, 1.0f);
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateFluorescentBallastSnap(this),
		Location,
		0.68f,
		1.0f,
		80.0f,
		850.0f);

	const APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;
	if (UIGStressComponent* Stress = Player ? Player->GetStress() : nullptr)
	{
		Stress->ApplyScare(ScareAmount);
	}
}

void AIGSecondMorningDirector::StartMirrorRoomBeat()
{
	if (bMirrorBeatConsumed)
	{
		return;
	}
	bMirrorBeatConsumed = true;

	SetThreat(0.30f, 18.0f);
	PlayCorridorSnap(MirrorRoomDoor ? MirrorRoomDoor->GetActorLocation() : GetActorLocation(), 0.30f);

	GetWorldTimerManager().SetTimer(
		MirrorDoorHandle,
		this,
		&ThisClass::CloseMirrorDoor,
		0.7f,
		false);
	GetWorldTimerManager().SetTimer(
		MirrorLampHandle,
		this,
		&ThisClass::KillMirrorLampAndPlayKeypad,
		1.75f,
		false);
}

void AIGSecondMorningDirector::CloseMirrorDoor()
{
	bP1PressureArmed = true;
	P1PressureElapsedSeconds = 0.0f;
	if (MirrorRoomDoor)
	{
		// Creak only. The absence of a slam is part of the scare.
		MirrorRoomDoor->BeginScriptedSwing(false, true, true);
	}
}

void AIGSecondMorningDirector::KillMirrorLampAndPlayKeypad()
{
	if (MirrorRoomLamp)
	{
		MirrorRoomLamp->SetIntensity(0.0f);
		MirrorRoomLamp->SetVisibility(false);
	}

	// Four quiet descending keypad notes outside the closed room.
	for (int32 ToneIndex = 0; ToneIndex < 4; ++ToneIndex)
	{
		FTimerHandle ToneHandle;
		GetWorldTimerManager().SetTimer(
			ToneHandle,
			[this, ToneIndex]()
			{
				if (!IsValid(this))
				{
					return;
				}
				IGAudio::SpawnOneShotAt(
					this,
					UIGToneSequenceSoundWave::CreateScannerBeep(this),
					MirrorRoomDoor
						? MirrorRoomDoor->GetActorLocation() + FVector(0, -20, 120)
						: GetActorLocation(),
					0.23f,
					1.0f - ToneIndex * 0.09f,
					50.0f,
					480.0f);
			},
			ToneIndex * 0.13f,
			false);
	}

	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGCH02",
			"KeypadOutsideThought",
			"도어락 소리가… 바깥에서 났다. 문은 아직 열린다."),
		4.4f);
}

void AIGSecondMorningDirector::SetThreat(
	const float Pressure,
	const float DurationSeconds)
{
	ActiveThreatPressure = FMath::Clamp(Pressure, 0.0f, 1.0f);
	ThreatEndTime =
		(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + DurationSeconds;
	ApplyThreatPressure(ActiveThreatPressure);

	GetWorldTimerManager().ClearTimer(ThreatHandle);
	if (DurationSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			ThreatHandle,
			this,
			&ThisClass::ClearThreat,
			DurationSeconds,
			false);
	}
}

void AIGSecondMorningDirector::ClearThreat()
{
	ActiveThreatPressure = 0.0f;
	ThreatEndTime = 0.0;
	ApplyThreatPressure(0.0f);
}

void AIGSecondMorningDirector::ApplyThreatPressure(const float Pressure) const
{
	const APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	AIGPlayerCharacter* Player = PlayerController
		? Cast<AIGPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;
	if (UIGStressComponent* Stress = Player ? Player->GetStress() : nullptr)
	{
		Stress->SetThreatPressure(Pressure);
	}
}

void AIGSecondMorningDirector::HandleIntermediateStopOpened(AIGElevator* StoppedElevator)
{
	if (StoppedElevator != Elevator)
	{
		return;
	}

	if (HasState(LiftStoppedTag))
	{
		// A restored story tag means the scare was already consumed, not that
		// the lift should remain held forever. Always schedule its release.
		if (Scene)
		{
			Scene->RevealElevatorFootprints();
		}
		GetWorldTimerManager().SetTimer(
			LiftResumeHandle,
			this,
			&ThisClass::ResumeLift,
			0.25f,
			false);
		return;
	}

	AddState(LiftStoppedTag);
	SetThreat(0.45f, 5.2f);
	const FVector IntermediateCab = Elevator->GetIntermediateCabWorldLocation();
	PlayCorridorSnap(IntermediateCab + FVector(0, 0, 120), 0.22f);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGCH02", "UnpressedFloorThought", "눌리지도 않은 층이잖아."),
		3.8f);

	// The first water tick comes from the black slit.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateWaterDripMetalRing(this),
		IntermediateCab + FVector(0, 0, 80),
		0.48f,
		1.0f,
		40.0f,
		520.0f);

	GetWorldTimerManager().SetTimer(
		LiftRevealHandle,
		this,
		&ThisClass::RevealLiftFootprints,
		4.4f,
		false);
	GetWorldTimerManager().SetTimer(
		LiftResumeHandle,
		this,
		&ThisClass::ResumeLift,
		5.0f,
		false);
}

void AIGSecondMorningDirector::RevealLiftFootprints()
{
	if (Scene)
	{
		Scene->RevealElevatorFootprints();
	}
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGCH02", "FootprintsThought", "아까는… 없었어."),
		3.8f);
}

void AIGSecondMorningDirector::ResumeLift()
{
	if (Elevator)
	{
		Elevator->ReleaseIntermediateStop();
	}
}

void AIGSecondMorningDirector::StartStoreBeat()
{
	if (bStoreBeatConsumed)
	{
		return;
	}
	bStoreBeatConsumed = true;

	GetWorldTimerManager().SetTimer(
		StoreChimeHandle,
		this,
		&ThisClass::PlaySecondStoreChime,
		0.8f,
		false);
}

void AIGSecondMorningDirector::PlaySecondStoreChime()
{
	if (StoreDoor)
	{
		StoreDoor->PlayChime(0.78f);
	}
}

void AIGSecondMorningDirector::StartEndingBeat()
{
	if (bEndingConsumed)
	{
		return;
	}
	bEndingConsumed = true;
	ClearThreat();
	if (Scene)
	{
		Scene->FinishChapterTwo();
	}
}

void AIGSecondMorningDirector::HandleNoteRead(
	AIGReadableNote* Note,
	const bool bOpened)
{
	if (!Note)
	{
		return;
	}

	if (Note == ExistingReceipt)
	{
		if (bOpened)
		{
			AddState(SawReceiptTag);
		}
		else
		{
			// The reading panel intentionally suppresses the rest of the HUD.
			// React after the paper is lowered so this clue is never hidden
			// behind the receipt the player is trying to read.
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGCH02", "ExistingReceiptThought",
					"난 아무것도 고르지 않았는데. …내 카드 번호다."),
				4.8f);
		}
		return;
	}

	if (Note == DuplicateReceipt)
	{
		if (!bOpened
			&& HasState(CalledEmployeeTag)
			&& IsPuzzleResolved(FName(TEXT("P2")))
			&& !HasState(ReadDuplicateReceiptTag))
		{
			// The second paper is a restored transaction record, never a second
			// purchase. Lowering it commits the optional greybox comparison.
			AddState(ReadDuplicateReceiptTag);
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGCH02",
					"DuplicateReceiptThought",
					"처음 결제는 4시 31분인데. 4시 44분이… 거래까지 덮었어."),
				5.0f);
		}
		return;
	}

	if (Note == HomePlanner)
	{
		if (!bOpened)
		{
			// This planner has occupied the same 404 desk since CH01. Reading
			// it in CH02 is the low-pressure alternate to P1, so a player who
			// refuses the impossible 403 room still has a physical 05:10 source.
			RegisterSecondMorningTruth(
				TEXT("Truth.Alarm0510"),
				FName(TEXT("CH02.HomePlanner0510")));
			RefreshObjective();
			// If this is the second independent truth and the player is
			// physically back inside 404, C3 has already been reached. Requiring
			// them to leave home and cross the corridor return volume backwards
			// made the homecoming beat fire during another departure.
			if (HasState(LeftHomeTag) && CanConvergeSecondMorning())
			{
				AddState(ReturnedTag);
			}
		}
		return;
	}

	if (Note == MirrorAlarmMemo)
	{
		if (!bOpened)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGCH02",
					"AlarmMemoThought",
					"5시 30분보다 스무 분 전. 단말에 맞는 시각을 넣어 보자."),
				4.8f);
			RefreshObjective();
		}
		return;
	}

	if (Note == MailboxBills)
	{
		if (!bOpened)
		{
			RegisterSecondMorningTruth(
				TEXT("Truth.WasSearched"),
				FName(TEXT("CH02.ManagementComplaint")));
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGCH02",
					"ManagementComplaintThought",
					"401호도 관리실도… 나를 찾고 있었어."),
				4.8f);
			RefreshObjective();
		}
		return;
	}

	if (Note == OfferingNote)
	{
		if (!bOpened)
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGCH02", "OfferingThought",
					"누굴… 기다리는 거지."),
				4.4f);
		}
		return;
	}

	if (bOpened && Note == ManagementNotice)
	{
		AddState(ReadNoticeTag);
	}
}
