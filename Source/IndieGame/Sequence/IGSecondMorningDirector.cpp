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
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Player/IGPlayerCharacter.h"
#include "Player/IGStressComponent.h"
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
	AIGReadableNote* InMailboxBills,
	AIGReadableNote* InSaltMemo,
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
	MailboxBills = InMailboxBills;
	SaltMemo = InSaltMemo;
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
	}

	BindNote(ExistingReceipt);
	BindNote(DuplicateReceipt);
	BindNote(MailboxBills);
	BindNote(SaltMemo);
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
		{ExistingReceipt.Get(), DuplicateReceipt.Get(), MailboxBills.Get(),
			SaltMemo.Get(), NightRoster.Get(), ManagementNotice.Get()})
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
	EnteredStoreTag = Tag(TEXT("State.CH02.Loop.EnteredStore"));
	SawReceiptTag = Tag(TEXT("State.CH02.Loop.SawReceipt"));
	HasWaterTag = Tag(TEXT("State.CH02.Loop.HasWater"));
	WaterPurchasedTag = Tag(TEXT("State.CH02.Loop.WaterPurchased"));
	ReturnedTag = Tag(TEXT("State.CH02.Loop.Returned"));
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
	if (!HasState(FridgeCheckedTag))
	{
		return FText::GetEmpty();
	}
	if (!HasState(HasWaterTag))
	{
		return NSLOCTEXT(
			"IGCH02", "ObjectiveErrand", "…또 물이 없다. 편의점에 다녀오자");
	}
	if (!HasState(WaterPurchasedTag))
	{
		return NSLOCTEXT("IGCH02", "ObjectiveCheckout", "계산하고 나가자");
	}
	return NSLOCTEXT("IGCH02", "ObjectiveReturn", "집으로 돌아가자");
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
	if (!HasState(FridgeCheckedTag))
	{
		return FString();
	}
	if (!HasState(HasWaterTag))
	{
		return TEXT("No water again. Go to the convenience store");
	}
	if (!HasState(WaterPurchasedTag))
	{
		return TEXT("Pay and leave");
	}
	return TEXT("Go home");
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

	int32 Completed = 0;
	for (const FGameplayTag& Tag :
		{FridgeCheckedTag, HasWalletTag, LeftHomeTag, SawMirrorRoomTag,
			LiftStoppedTag, EnteredStoreTag, SawReceiptTag, HasWaterTag,
			WaterPurchasedTag})
	{
		Completed += HasState(Tag) ? 1 : 0;
	}
	return Completed / 9.0f;
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
		StartCorridorBlackout();
	}
	else if (StateTag.MatchesTagExact(EnteredMirrorRoomTag))
	{
		StartMirrorRoomBeat();
	}
	else if (StateTag.MatchesTagExact(EnteredStoreTag))
	{
		StartStoreBeat();
	}
	else if (StateTag.MatchesTagExact(WaterPurchasedTag))
	{
		if (Scene)
		{
			Scene->RevealSecondReceipt();
		}
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT("IGCH02", "DuplicateReceiptThought", "같은 시간. 같은 물."),
			4.4f);
	}
	else if (StateTag.MatchesTagExact(ReturnedTag))
	{
		StartEndingBeat();
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
	Scene->SetFixtureLive(NextCorridorFixture, false, true);
	PlayCorridorSnap(FixtureLocation, ScareAmounts[ScareIndex]);

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
		UIGToneSequenceSoundWave::CreateFootstep(this, 2.8f, 0.32f),
		Location,
		0.75f,
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
		&ThisClass::KillMirrorLampAndLock,
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

void AIGSecondMorningDirector::KillMirrorLampAndLock()
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
		NSLOCTEXT("IGCH02", "LockedInsideThought", "지금… 안에서 잠갔는데."),
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
	if (StoppedElevator != Elevator || HasState(LiftStoppedTag))
	{
		return;
	}

	AddState(LiftStoppedTag);
	SetThreat(0.45f, 5.2f);
	PlayCorridorSnap(Elevator->GetActorLocation() - FVector(0, 0, 780), 0.22f);
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGCH02", "UnpressedFloorThought", "눌리지도 않은 층이잖아."),
		3.8f);

	// The first water tick comes from the black slit.
	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateFootstep(this, 2.25f, 0.18f),
		Elevator->GetActorLocation() - FVector(0, 0, 820),
		0.55f,
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
	if (!bOpened || !Note)
	{
		return;
	}

	if (Note == ExistingReceipt)
	{
		AddState(SawReceiptTag);
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH02", "ExistingReceiptThought",
				"계산은 아직 안 했는데. …내 카드 번호다."),
			4.8f);
	}
	else if (Note == MailboxBills)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH02", "MailboxBillsThought",
				"고지서가 꽉 찼는데… 수취인이 다 404호야."),
			4.8f);
	}
	else if (Note == SaltMemo)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH02", "OfferingThought",
				"밥에 숟가락은… 저렇게 꽂는 게 아닌데."),
			4.4f);
	}
	else if (Note == ManagementNotice)
	{
		AddState(ReadNoticeTag);
	}
}
