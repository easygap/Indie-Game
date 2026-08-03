#include "Sequence/IGChapterTwoHumanGateDirector.h"

#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "IndieGame.h"
#include "Interaction/IGInspectable.h"
#include "Interaction/IGZoneTrigger.h"
#include "Narrative/IGStoryStateSubsystem.h"
#include "Player/IGHorrorHUD.h"
#include "Save/IGSaveSubsystem.h"
#include "TimerManager.h"

namespace IGChapterTwoHumanGate
{
	const FVector Radio401LocalLocation(-150.0f, -174.0f, 1028.0f);
	const FVector Doorbell401LocalLocation(-206.0f, -237.0f, 1022.0f);
	const FVector Doorbell402LocalLocation(58.0f, -237.0f, 1022.0f);
	const FVector LobbyLocalLocation(580.0f, -305.0f, 110.0f);
}

AIGChapterTwoHumanGateAction::AIGChapterTwoHumanGateAction()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionSurface =
		CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionSurface"));
	SetRootComponent(InteractionSurface);
	InteractionSurface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSurface->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSurface->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionSurface->SetGenerateOverlapEvents(false);
	InteractionSurface->SetCanEverAffectNavigation(false);

	PresentationMesh =
		CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Presentation"));
	PresentationMesh->SetupAttachment(InteractionSurface);
	PresentationMesh->SetCollisionProfileName(
		UCollisionProfile::NoCollision_ProfileName);
	PresentationMesh->SetGenerateOverlapEvents(false);
	PresentationMesh->SetCanEverAffectNavigation(false);
	PresentationMesh->SetCastShadow(false);
}

void AIGChapterTwoHumanGateAction::Configure(
	AIGChapterTwoHumanGateDirector* InDirector,
	const EIGChapterTwoHumanCheckAction InAction,
	UStaticMesh* CubeMesh,
	UMaterialInterface* Material,
	const FVector& SizeCentimeters,
	const FText& Prompt,
	const bool bShowVisual)
{
	Director = InDirector;
	Action = InAction;
	const FVector SafeSize(
		FMath::Max(1.0f, SizeCentimeters.X),
		FMath::Max(1.0f, SizeCentimeters.Y),
		FMath::Max(1.0f, SizeCentimeters.Z));
	InteractionSurface->SetBoxExtent(SafeSize * 0.5f);
	PresentationMesh->SetStaticMesh(CubeMesh);
	PresentationMesh->SetMaterial(0, Material);
	PresentationMesh->SetRelativeScale3D(SafeSize / 100.0f);
	PresentationMesh->SetVisibility(bShowVisual, true);
	InteractionPrompt = Prompt;
	InteractionTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Interaction.Inspect")),
		false);
	SetAvailable(false);
}

void AIGChapterTwoHumanGateAction::SetAvailable(const bool bAvailable)
{
	SetInteractionEnabled(bAvailable);
	InteractionSurface->SetCollisionEnabled(
		bAvailable
			? ECollisionEnabled::QueryOnly
			: ECollisionEnabled::NoCollision);
}

void AIGChapterTwoHumanGateAction::CompleteInteraction_Implementation(
	const FIGInteractionContext& Context)
{
	Super::CompleteInteraction_Implementation(Context);
	if (Director)
	{
		Director->HandleAction(Action, this);
	}
}

AIGChapterTwoHumanGateDirector::AIGChapterTwoHumanGateDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	NightShiftStickerProxy =
		CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NightShiftStickerProxy"));
	NightShiftStickerProxy->SetupAttachment(SceneRoot);
	NightShiftStickerProxy->SetCollisionProfileName(
		UCollisionProfile::NoCollision_ProfileName);
	NightShiftStickerProxy->SetGenerateOverlapEvents(false);
	NightShiftStickerProxy->SetCanEverAffectNavigation(false);
	NightShiftStickerProxy->SetCastShadow(false);

	Door402IndicatorLens =
		CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door402IndicatorLens"));
	Door402IndicatorLens->SetupAttachment(SceneRoot);
	Door402IndicatorLens->SetCollisionProfileName(
		UCollisionProfile::NoCollision_ProfileName);
	Door402IndicatorLens->SetGenerateOverlapEvents(false);
	Door402IndicatorLens->SetCanEverAffectNavigation(false);
	Door402IndicatorLens->SetCastShadow(false);

	Door402IndicatorLight =
		CreateDefaultSubobject<UPointLightComponent>(TEXT("Door402IndicatorLight"));
	Door402IndicatorLight->SetupAttachment(SceneRoot);
	Door402IndicatorLight->SetIntensity(0.0f);
	Door402IndicatorLight->SetAttenuationRadius(95.0f);
	Door402IndicatorLight->SetLightColor(FLinearColor(0.95f, 0.16f, 0.07f));
	Door402IndicatorLight->SetCastShadows(false);
	Door402IndicatorLight->SetVolumetricScatteringIntensity(0.0f);
}

void AIGChapterTwoHumanGateDirector::Configure(
	AIGInspectable* InPhoneInspectable,
	UStaticMesh* CubeMesh,
	UMaterialInterface* DarkMaterial,
	UMaterialInterface* PanelMaterial,
	UMaterialInterface* LitMaterial)
{
	if (bConfigured || !GetWorld() || !CubeMesh)
	{
		return;
	}
	bConfigured = true;
	PhoneInspectable = InPhoneInspectable;

	NightShiftStickerProxy->SetStaticMesh(CubeMesh);
	NightShiftStickerProxy->SetMaterial(0, PanelMaterial);
	NightShiftStickerProxy->SetRelativeLocation(FVector(58.0f, -235.8f, 1053.0f));
	NightShiftStickerProxy->SetRelativeScale3D(FVector(0.28f, 0.012f, 0.16f));

	Door402IndicatorLens->SetStaticMesh(CubeMesh);
	Door402IndicatorLens->SetMaterial(0, LitMaterial);
	Door402IndicatorLens->SetRelativeLocation(FVector(58.0f, -237.2f, 1036.0f));
	Door402IndicatorLens->SetRelativeScale3D(FVector(0.035f, 0.014f, 0.035f));
	Door402IndicatorLight->SetRelativeLocation(
		FVector(58.0f, -245.0f, 1036.0f));
	Set402IndicatorLit(false);

	if (PhoneInspectable)
	{
		const FTransform PhoneTransform = PhoneInspectable->GetActorTransform();
		struct FPhoneChoice
		{
			EIGChapterTwoHumanCheckAction Action;
			float LocalY;
			FText Prompt;
		};
		const FPhoneChoice Choices[] = {
			{EIGChapterTwoHumanCheckAction::PhoneMother,
				-4.5f,
				NSLOCTEXT("IGCH02", "PhoneMotherPrompt", "엄마에게 전화하기")},
			{EIGChapterTwoHumanCheckAction::PhoneEmergency112,
				0.0f,
				NSLOCTEXT("IGCH02", "Phone112Prompt", "112에 전화하기")},
			{EIGChapterTwoHumanCheckAction::PhonePatrolManager,
				4.5f,
				NSLOCTEXT(
					"IGCH02",
					"PhoneManagerPrompt",
					"순회 관리인에게 전화하기")},
		};
		for (const FPhoneChoice& Choice : Choices)
		{
			const FTransform RowTransform(
				FRotator::ZeroRotator,
				FVector(0.0f, Choice.LocalY, 1.45f));
			if (AIGChapterTwoHumanGateAction* PhoneAction = SpawnAction(
				Choice.Action,
				RowTransform * PhoneTransform,
				FVector(7.0f, 4.0f, 2.2f),
				CubeMesh,
				DarkMaterial,
				Choice.Prompt,
				false))
			{
				PhoneActions.Add(PhoneAction);
			}
		}
	}

	Doorbell401Action = SpawnAction(
		EIGChapterTwoHumanCheckAction::Doorbell401,
		FTransform(
			FRotator::ZeroRotator,
			ToWorld(IGChapterTwoHumanGate::Doorbell401LocalLocation)),
		FVector(12.0f, 5.0f, 14.0f),
		CubeMesh,
		DarkMaterial,
		NSLOCTEXT("IGCH02", "Doorbell401Prompt", "401호 벨 누르기"),
		false);
	Doorbell402Action = SpawnAction(
		EIGChapterTwoHumanCheckAction::Doorbell402,
		FTransform(
			FRotator::ZeroRotator,
			ToWorld(IGChapterTwoHumanGate::Doorbell402LocalLocation)),
		FVector(10.0f, 4.0f, 12.0f),
		CubeMesh,
		DarkMaterial,
		NSLOCTEXT(
			"IGCH02",
			"Doorbell402Prompt",
			"402호 벨 누르기 · 야간 근무 중"),
		true);
	LobbyZone = SpawnLobbyZone();
}

void AIGChapterTwoHumanGateDirector::BeginPlay()
{
	Super::BeginPlay();
	ResolveTags();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->OnStoryStateTagChanged.AddUniqueDynamic(
				this,
				&ThisClass::HandleStoryStateChanged);
		}
	}

	if (PhoneInspectable)
	{
		// The persistent CH01 notification target sits beneath three CH02 call
		// rows. Leave the phone art visible but stop it swallowing their trace.
		bPhoneInteractionWasEnabled =
			PhoneInspectable->IsInteractionEnabled();
		bPhoneCollisionWasEnabled =
			PhoneInspectable->GetActorEnableCollision();
		bPhoneStateCaptured = true;
		PhoneInspectable->SetInteractionEnabled(false);
		PhoneInspectable->SetActorEnableCollision(false);
	}

	if (LobbyZone)
	{
		LobbyZone->OnZoneTriggered.AddUniqueDynamic(
			this,
			&ThisClass::HandleLobbyEntered);
	}

	ReconcileState(false);
}

void AIGChapterTwoHumanGateDirector::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Stop401Radio();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UIGStoryStateSubsystem* StoryState =
			GameInstance->GetSubsystem<UIGStoryStateSubsystem>())
		{
			StoryState->OnStoryStateTagChanged.RemoveDynamic(
				this,
				&ThisClass::HandleStoryStateChanged);
		}
	}

	if (LobbyZone)
	{
		LobbyZone->OnZoneTriggered.RemoveDynamic(
			this,
			&ThisClass::HandleLobbyEntered);
		LobbyZone->Destroy();
		LobbyZone = nullptr;
	}
	for (AIGChapterTwoHumanGateAction* PhoneAction : PhoneActions)
	{
		if (PhoneAction)
		{
			PhoneAction->Destroy();
		}
	}
	PhoneActions.Reset();
	for (AIGChapterTwoHumanGateAction* Doorbell :
		{Doorbell401Action.Get(), Doorbell402Action.Get()})
	{
		if (Doorbell)
		{
			Doorbell->Destroy();
		}
	}
	Doorbell401Action = nullptr;
	Doorbell402Action = nullptr;

	if (PhoneInspectable && bPhoneStateCaptured)
	{
		PhoneInspectable->SetActorEnableCollision(
			bPhoneCollisionWasEnabled);
		PhoneInspectable->SetInteractionEnabled(
			bPhoneInteractionWasEnabled);
	}

	Super::EndPlay(EndPlayReason);
}

AIGChapterTwoHumanGateAction*
AIGChapterTwoHumanGateDirector::SpawnAction(
	const EIGChapterTwoHumanCheckAction Action,
	const FTransform& WorldTransform,
	const FVector& SizeCentimeters,
	UStaticMesh* CubeMesh,
	UMaterialInterface* Material,
	const FText& Prompt,
	const bool bShowVisual)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIGChapterTwoHumanGateAction* Result =
		World->SpawnActor<AIGChapterTwoHumanGateAction>(
			AIGChapterTwoHumanGateAction::StaticClass(),
			WorldTransform,
			Parameters);
	if (Result)
	{
		Result->Configure(
			this,
			Action,
			CubeMesh,
			Material,
			SizeCentimeters,
			Prompt,
			bShowVisual);
	}
	return Result;
}

AIGZoneTrigger* AIGChapterTwoHumanGateDirector::SpawnLobbyZone()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FTransform ZoneTransform(
		FRotator::ZeroRotator,
		ToWorld(IGChapterTwoHumanGate::LobbyLocalLocation));
	AIGZoneTrigger* Zone = World->SpawnActorDeferred<AIGZoneTrigger>(
		AIGZoneTrigger::StaticClass(),
		ZoneTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Zone)
	{
		Zone->SetZoneExtent(FVector(118.0f, 58.0f, 105.0f));
		Zone->StateTagOnEnter = FGameplayTag::RequestGameplayTag(
			FName(TEXT("State.CH02.HumanGate.LobbyWitnessed")),
			false);
		Zone->ThoughtOnEnter = NSLOCTEXT(
			"IGCH02",
			"StoreHumanMotiveThought",
			"편의점엔… 사람 있겠지.");
		Zone->FinishSpawning(ZoneTransform);
	}
	return Zone;
}

void AIGChapterTwoHumanGateDirector::ResolveTags()
{
	auto Tag = [](const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	};

	MirrorSightImpossibleTag =
		Tag(TEXT("State.CH02.Loop.SawMirrorRoom"));
	MirrorEntryImpossibleTag =
		Tag(TEXT("State.CH02.Loop.EnteredMirrorRoom"));
	LiftImpossibleTag = Tag(TEXT("State.CH02.Loop.LiftStopped"));
	EnteredStoreTag = Tag(TEXT("State.CH02.Loop.EnteredStore"));
	HumanChecksUnlockedTag =
		Tag(TEXT("State.CH02.HumanGate.Unlocked"));
	PhoneAttemptedTag =
		Tag(TEXT("State.CH02.HumanGate.PhoneAttempted"));
	PhoneMotherTag =
		Tag(TEXT("State.CH02.HumanGate.Phone.Mother"));
	PhoneEmergency112Tag =
		Tag(TEXT("State.CH02.HumanGate.Phone.Emergency112"));
	PhonePatrolManagerTag =
		Tag(TEXT("State.CH02.HumanGate.Phone.PatrolManager"));
	AlarmFirstTonePlayedTag =
		Tag(TEXT("State.CH02.HumanGate.AlarmFirstTonePlayed"));
	Doorbell401AttemptedTag =
		Tag(TEXT("State.CH02.HumanGate.Doorbell401Attempted"));
	Doorbell402AttemptedTag =
		Tag(TEXT("State.CH02.HumanGate.Doorbell402Attempted"));
	HelpAttemptedTag =
		Tag(TEXT("State.CH02.HumanGate.HelpAttempted"));
	LobbyWitnessedTag =
		Tag(TEXT("State.CH02.HumanGate.LobbyWitnessed"));
	StoreMotiveTag =
		Tag(TEXT("State.CH02.HumanGate.StoreMotive"));
	ChapterIdTag = Tag(TEXT("Chapter.CH02"));
	WokeCheckpointTag = Tag(TEXT("Checkpoint.CH02.Woke"));
	CorridorCheckpointTag = Tag(TEXT("Checkpoint.CH02.Corridor"));
	StoreCheckpointTag = Tag(TEXT("Checkpoint.CH02.Store"));
}

bool AIGChapterTwoHumanGateDirector::HasState(
	const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return false;
	}
	const UGameInstance* GameInstance = GetGameInstance();
	const UIGStoryStateSubsystem* StoryState = GameInstance
		? GameInstance->GetSubsystem<UIGStoryStateSubsystem>()
		: nullptr;
	return StoryState && StoryState->HasState(Tag);
}

bool AIGChapterTwoHumanGateDirector::AddState(
	const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return false;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UIGStoryStateSubsystem* StoryState = GameInstance
		? GameInstance->GetSubsystem<UIGStoryStateSubsystem>()
		: nullptr;
	return StoryState && StoryState->AddState(Tag);
}

void AIGChapterTwoHumanGateDirector::ReconcileState(
	const bool bPresentUnlockThought)
{
	if ((HasState(MirrorSightImpossibleTag)
			|| HasState(MirrorEntryImpossibleTag)
			|| HasState(LiftImpossibleTag))
		&& !HasState(HumanChecksUnlockedTag))
	{
		EnsureHumanChecksUnlocked(bPresentUnlockThought);
	}
	if ((HasState(HelpAttemptedTag) || HasState(LobbyWitnessedTag))
		&& !HasState(StoreMotiveTag))
	{
		CommitStoreMotive(false);
	}

	UpdateActionAvailability();
	Set402IndicatorLit(HasState(Doorbell402AttemptedTag));
	if (HasState(Doorbell401AttemptedTag))
	{
		Stop401Radio();
	}
	else
	{
		Start401Radio();
	}
}

void AIGChapterTwoHumanGateDirector::UpdateActionAvailability()
{
	const bool bUnlocked = HasState(HumanChecksUnlockedTag);
	const bool bPhoneAvailable = bUnlocked && !HasState(PhoneAttemptedTag);
	for (AIGChapterTwoHumanGateAction* PhoneAction : PhoneActions)
	{
		if (PhoneAction)
		{
			PhoneAction->SetAvailable(bPhoneAvailable);
		}
	}
	if (Doorbell401Action)
	{
		Doorbell401Action->SetAvailable(
			bUnlocked && !HasState(Doorbell401AttemptedTag));
	}
	if (Doorbell402Action)
	{
		Doorbell402Action->SetAvailable(
			bUnlocked && !HasState(Doorbell402AttemptedTag));
	}
}

void AIGChapterTwoHumanGateDirector::EnsureHumanChecksUnlocked(
	const bool bPresentThought)
{
	if (!AddState(HumanChecksUnlockedTag))
	{
		UpdateActionAvailability();
		return;
	}

	UpdateActionAvailability();
	if (bPresentThought)
	{
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH02",
				"FindHumanThought",
				"연락할 사람을 찾아보자."),
			3.2f);
	}
	RequestCheckpointAutosave();
}

void AIGChapterTwoHumanGateDirector::CommitStoreMotive(
	const bool bQueueThought)
{
	if (HasState(StoreMotiveTag))
	{
		return;
	}
	if (bQueueThought)
	{
		if (!GetWorldTimerManager().IsTimerActive(
				StoreMotiveThoughtHandle))
		{
			GetWorldTimerManager().SetTimer(
				StoreMotiveThoughtHandle,
				this,
				&ThisClass::PushStoreMotiveThought,
				2.55f,
				false);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(StoreMotiveThoughtHandle);
	AddState(StoreMotiveTag);
}

void AIGChapterTwoHumanGateDirector::PushStoreMotiveThought()
{
	if (HasState(StoreMotiveTag))
	{
		return;
	}
	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT(
			"IGCH02",
			"StoreHumanMotiveAfterHelp",
			"편의점엔… 사람 있겠지."),
		3.2f);
	AddState(StoreMotiveTag);
	RequestCheckpointAutosave();
}

void AIGChapterTwoHumanGateDirector::HandleAction(
	const EIGChapterTwoHumanCheckAction Action,
	AIGChapterTwoHumanGateAction* Source)
{
	if (!HasState(HumanChecksUnlockedTag))
	{
		return;
	}

	bool bCommitted = false;
	switch (Action)
	{
	case EIGChapterTwoHumanCheckAction::PhoneMother:
	case EIGChapterTwoHumanCheckAction::PhoneEmergency112:
	case EIGChapterTwoHumanCheckAction::PhonePatrolManager:
	{
		if (HasState(PhoneAttemptedTag))
		{
			break;
		}
		const FGameplayTag ChoiceTag =
			Action == EIGChapterTwoHumanCheckAction::PhoneMother
				? PhoneMotherTag
				: Action == EIGChapterTwoHumanCheckAction::PhoneEmergency112
					? PhoneEmergency112Tag
					: PhonePatrolManagerTag;
		AddState(ChoiceTag);
		AddState(PhoneAttemptedTag);
		// All three numbers share the same impossible response: precisely the
		// first 04:44 alarm tone once, then a local failure card. No caller,
		// dispatcher or manager voice object is ever created.
		PlayAlarmFirstToneOnce();
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGCH02",
				"PhoneConnectionFailed",
				"통화 연결 불가."),
			2.2f);
		bCommitted = true;
		break;
	}

	case EIGChapterTwoHumanCheckAction::Doorbell401:
		if (!HasState(Doorbell401AttemptedTag))
		{
			Stop401Radio();
			AddState(Doorbell401AttemptedTag);
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGCH02",
					"Doorbell401Result",
					"라디오가 멎었다. 문은 열리지 않는다."),
				2.4f);
			bCommitted = true;
		}
		break;

	case EIGChapterTwoHumanCheckAction::Doorbell402:
		if (!HasState(Doorbell402AttemptedTag))
		{
			AddState(Doorbell402AttemptedTag);
			Set402IndicatorLit(true);
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGCH02",
					"Doorbell402Result",
					"야간 근무 중. 표시등만 켜졌다."),
				2.4f);
			bCommitted = true;
		}
		break;

	default:
		break;
	}

	if (!bCommitted)
	{
		return;
	}
	// The failed response gets room to land first. The motive thought then
	// commits StoreMotive in the same callback, so the objective changes only
	// after "편의점엔… 사람 있겠지." is on screen.
	CommitStoreMotive(true);
	AddState(HelpAttemptedTag);
	UpdateActionAvailability();
	if (Source)
	{
		Source->SetAvailable(false);
	}
	RequestCheckpointAutosave();
}

void AIGChapterTwoHumanGateDirector::PlayAlarmFirstToneOnce()
{
	if (!AddState(AlarmFirstTonePlayedTag))
	{
		return;
	}

	UIGToneSequenceSoundWave* AlarmFirstTone =
		NewObject<UIGToneSequenceSoundWave>(this);
	if (!AlarmFirstTone)
	{
		return;
	}
	FIGToneNote FirstNote;
	FirstNote.StartSeconds = 0.0f;
	FirstNote.DurationSeconds = 0.38f;
	FirstNote.FrequencyHz = 880.0f;
	FirstNote.Amplitude = 0.18f;
	FirstNote.AttackFraction = 0.015f;
	FirstNote.ReleasePower = 1.15f;
	FirstNote.Waveform = EIGToneWaveform::SoftSquare;
	TArray<FIGToneNote> Notes;
	Notes.Add(FirstNote);
	AlarmFirstTone->ConfigureNotes(MoveTemp(Notes), false);

	IGAudio::SpawnOneShotAt(
		this,
		AlarmFirstTone,
		PhoneInspectable
			? PhoneInspectable->GetActorLocation()
			: GetActorLocation(),
		0.82f,
		1.0f,
		45.0f,
		520.0f);
	++AlarmFirstTonePlayCount;
}

void AIGChapterTwoHumanGateDirector::Start401Radio()
{
	if (Radio401Component || HasState(Doorbell401AttemptedTag))
	{
		return;
	}

	Radio401Component =
		NewObject<UAudioComponent>(this, TEXT("ChapterTwo401Radio"));
	if (!Radio401Component)
	{
		return;
	}
	Radio401Component->bAutoActivate = false;
	Radio401Component->bAutoDestroy = false;
	Radio401Component->bOverrideAttenuation = true;
	Radio401Component->AttenuationOverrides.bAttenuate = true;
	Radio401Component->AttenuationOverrides.bSpatialize = true;
	Radio401Component->AttenuationOverrides.DistanceAlgorithm =
		EAttenuationDistanceModel::NaturalSound;
	Radio401Component->AttenuationOverrides.AttenuationShapeExtents =
		FVector(65.0f, 0.0f, 0.0f);
	Radio401Component->AttenuationOverrides.FalloffDistance = 380.0f;
	Radio401Component->AttenuationOverrides.dBAttenuationAtMax = -58.0f;
	Radio401Component->SetSound(
		UIGToneSequenceSoundWave::CreateMuffledPrayerRadio(this));
	Radio401Component->SetVolumeMultiplier(0.28f);
	Radio401Component->RegisterComponent();
	Radio401Component->SetWorldLocation(
		ToWorld(IGChapterTwoHumanGate::Radio401LocalLocation));
	Radio401Component->Play();
}

void AIGChapterTwoHumanGateDirector::Stop401Radio()
{
	if (!Radio401Component)
	{
		return;
	}
	Radio401Component->Stop();
	Radio401Component->DestroyComponent();
	Radio401Component = nullptr;
}

void AIGChapterTwoHumanGateDirector::Set402IndicatorLit(const bool bLit)
{
	Door402IndicatorLens->SetVisibility(bLit, true);
	Door402IndicatorLight->SetIntensity(bLit ? 240.0f : 0.0f);
	Door402IndicatorLight->SetVisibility(bLit);
}

void AIGChapterTwoHumanGateDirector::HandleStoryStateChanged(
	const FGameplayTag StateTag,
	const bool bAdded)
{
	if (bAdded
		&& (StateTag.MatchesTagExact(MirrorSightImpossibleTag)
			|| StateTag.MatchesTagExact(MirrorEntryImpossibleTag)
			|| StateTag.MatchesTagExact(LiftImpossibleTag)))
	{
		EnsureHumanChecksUnlocked(true);
	}
	if (StateTag.MatchesTagExact(HumanChecksUnlockedTag)
		|| StateTag.MatchesTagExact(PhoneAttemptedTag)
		|| StateTag.MatchesTagExact(Doorbell401AttemptedTag)
		|| StateTag.MatchesTagExact(Doorbell402AttemptedTag))
	{
		ReconcileState(false);
	}
}

void AIGChapterTwoHumanGateDirector::HandleLobbyEntered(
	AIGZoneTrigger* Zone)
{
	if (Zone != LobbyZone)
	{
		return;
	}
	AddState(LobbyWitnessedTag);
	// The zone itself presents "편의점엔… 사람 있겠지." after committing
	// LobbyWitnessed. Only the objective state is added here, so the line is
	// neither doubled nor played before the player can see the lit storefront.
	CommitStoreMotive(false);
	RequestCheckpointAutosave();
}

void AIGChapterTwoHumanGateDirector::RequestCheckpointAutosave() const
{
	UGameInstance* GameInstance = GetGameInstance();
	UIGSaveSubsystem* SaveSubsystem = GameInstance
		? GameInstance->GetSubsystem<UIGSaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem || !ChapterIdTag.IsValid())
	{
		return;
	}

	const FGameplayTag CheckpointTag =
		HasState(EnteredStoreTag)
			? StoreCheckpointTag
			: (HasState(MirrorSightImpossibleTag)
					|| HasState(MirrorEntryImpossibleTag)
					|| HasState(LiftImpossibleTag))
				? CorridorCheckpointTag
				: WokeCheckpointTag;
	SaveSubsystem->RequestAutosave(
		ChapterIdTag,
		GetWorld() ? GetWorld()->GetOutermost()->GetFName() : NAME_None,
		CheckpointTag);
}

FVector AIGChapterTwoHumanGateDirector::ToWorld(
	const FVector& LocalLocation) const
{
	return GetActorTransform().TransformPosition(LocalLocation);
}

bool AIGChapterTwoHumanGateDirector::RunRebirthEndToEndValidation()
{
	if (!HasState(HumanChecksUnlockedTag)
		|| PhoneActions.Num() != 3
		|| !Doorbell401Action
		|| !Doorbell402Action)
	{
		return false;
	}

	const bool bNoHelpBeforeLobby = !HasState(HelpAttemptedTag);
	HandleLobbyEntered(LobbyZone);
	const bool bDirectLobbyConverged =
		bNoHelpBeforeLobby
		&& HasState(LobbyWitnessedTag)
		&& HasState(StoreMotiveTag)
		&& !HasState(HelpAttemptedTag);

	HandleAction(
		EIGChapterTwoHumanCheckAction::PhoneMother,
		PhoneActions[0]);
	// The other two numbers must be rejected by the shared one-choice latch.
	HandleAction(
		EIGChapterTwoHumanCheckAction::PhoneEmergency112,
		PhoneActions[1]);
	HandleAction(
		EIGChapterTwoHumanCheckAction::PhonePatrolManager,
		PhoneActions[2]);
	HandleAction(
		EIGChapterTwoHumanCheckAction::Doorbell401,
		Doorbell401Action);
	HandleAction(
		EIGChapterTwoHumanCheckAction::Doorbell402,
		Doorbell402Action);

	const int32 PhoneChoiceCount =
		(HasState(PhoneMotherTag) ? 1 : 0)
		+ (HasState(PhoneEmergency112Tag) ? 1 : 0)
		+ (HasState(PhonePatrolManagerTag) ? 1 : 0);
	const bool bPassed =
		bDirectLobbyConverged
		&& HasState(PhoneAttemptedTag)
		&& PhoneChoiceCount == 1
		&& HasState(AlarmFirstTonePlayedTag)
		&& AlarmFirstTonePlayCount == 1
		&& HasState(Doorbell401AttemptedTag)
		&& !Radio401Component
		&& HasState(Doorbell402AttemptedTag)
		&& Door402IndicatorLens->IsVisible()
		&& HasState(HelpAttemptedTag)
		&& HasState(StoreMotiveTag);

	if (bPassed)
	{
		UE_LOG(
			LogIndieGame,
			Display,
			TEXT(
				"REBIRTH_E2E PASS s6_human_gate direct_lobby=1 "
				"phone_choice=1 alarm_once=1 voice=0 "
				"bell401_radio_stop=1 door_open=0 "
				"bell402_light=1 optional=1"));
	}
	else
	{
		UE_LOG(
			LogIndieGame,
			Error,
			TEXT(
				"REBIRTH_E2E FAIL s6_human_gate direct_lobby=%d "
				"phone_choices=%d alarm_plays=%d bell401=%d bell402=%d"),
			bDirectLobbyConverged ? 1 : 0,
			PhoneChoiceCount,
			AlarmFirstTonePlayCount,
			HasState(Doorbell401AttemptedTag) ? 1 : 0,
			HasState(Doorbell402AttemptedTag) ? 1 : 0);
	}
	return bPassed;
}
