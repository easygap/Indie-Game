#include "Sequence/IGSecondMorningDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/IGElevator.h"
#include "Interaction/IGReadableNote.h"
#include "Interaction/IGSlidingDoor.h"
#include "Interaction/IGSwingDoor.h"
#include "Narrative/IGRebirthNarrativeSubsystem.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Player/IGStressComponent.h"
#include "Save/IGSaveSubsystem.h"
#include "TimerManager.h"

AIGSecondMorningDirector::AIGSecondMorningDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AIGSecondMorningDirector::Configure(
	AIGPrologueWorldScene* InScene,
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
	AIGReadableNote* InManagementNotice)
{
	Scene = InScene;
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
			if (bRestoredEmployeeCall)
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
		FName(TEXT("P2.ReceiptComparisonProxy")));
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
		if (Scene)
		{
			// Functional P2 entry proxy: this restores an overwritten record.
			// It does not pick up a product or charge the player a second time.
			Scene->RevealSecondReceipt();
		}
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
			RegisterSecondMorningTruth(
				TEXT("Truth.Alarm0510"),
				FName(TEXT("CH02.MirrorAlarmMemo")),
				FName(TEXT("P1.AlarmArithmeticProxy")));
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGCH02",
					"AlarmMemoThought",
					"5시 30분보다 스무 분 전이면… 내가 맞춘 알람은 5시 10분이다."),
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
