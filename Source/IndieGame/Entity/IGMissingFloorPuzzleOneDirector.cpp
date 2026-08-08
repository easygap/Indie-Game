#include "Entity/IGMissingFloorPuzzleOneDirector.h"

#include "Audio/IGAmbienceSoundWave.h"
#include "Audio/IGAudioHelpers.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/IGPrologueWorldScene.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Interaction/IGReadableNote.h"
#include "Materials/MaterialInterface.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Entity/IGMissingFloorEvidence.h"
#include "Player/IGHorrorHUD.h"

namespace IGPuzzleOne
{
	/**
	 * Lobby coordinates, matching the fixtures BuildLobby placed on the street
	 * wall. Duplicated here with intent: the scene's coordinate namespace is
	 * .cpp-local, so a director either duplicates the numbers or the scene
	 * grows an accessor per prop.
	 */
	const FVector FifthMeterFace(542.0f, -364.6f, 152.0f);
	const FVector BreakerFace(576.0f, -365.6f, 134.0f);
	const FVector ReadingSheetLocation(672.0f, -241.2f, 150.0f);

	/**
	 * Eight centimeters above the 4F ceiling slab (world Z 1160), directly over
	 * the third corridor fixture, on the corridor centreline. A tight radius
	 * keeps it a corridor cue — "somewhere above the ceiling" — rather than a
	 * building-wide bed.
	 */
	const FVector BallastHumLocation(300.0f, -305.0f, 1168.0f);
	constexpr float BallastHumVolume = 0.28f;
	constexpr float BallastHumInnerRadius = 90.0f;
	constexpr float BallastHumFalloff = 620.0f;

	/** Throwing a breaker is a deliberate, loud act. See §5.1. */
	constexpr float BreakerHoldSeconds = 0.6f;
	constexpr float BreakerNoiseLoudness = 0.55f;
	/** Leaning in to read a dial barely sounds at all. */
	constexpr float DialNoiseLoudness = 0.05f;

	const FName PuzzleId(TEXT("P1"));
}

AIGMissingFloorPuzzleOneDirector::AIGMissingFloorPuzzleOneDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AIGMissingFloorPuzzleOneDirector::Configure(AIGPrologueWorldScene* InScene)
{
	UWorld* World = GetWorld();
	if (!World || !InScene)
	{
		return false;
	}
	Scene = InScene;

	UStaticMesh* CubeMesh =
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh)
	{
		return false;
	}
	// Reuse the material the lobby already put on these fixtures so the
	// interaction surfaces disappear into the props they belong to.
	UMaterialInterface* DialMaterial = nullptr;
	if (const UStaticMeshComponent* Disc = InScene->GetFifthMeterDisc())
	{
		DialMaterial = Disc->GetMaterial(0);
	}
	UMaterialInterface* ToggleMaterial = nullptr;
	if (const UStaticMeshComponent* Toggle = InScene->GetUnnamedBreakerToggle())
	{
		ToggleMaterial = Toggle->GetMaterial(0);
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// The fifth dial. The thought does the work of the missing nameplate: the
	// player is told what they are looking at, never what it means.
	SpawnParameters.Name = TEXT("MissingFloorMeterDial");
	MeterDialEvidence = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPuzzleOne::FifthMeterFace),
		SpawnParameters);
	if (!MeterDialEvidence)
	{
		return false;
	}
	MeterDialEvidence->Configure(
		CubeMesh,
		DialMaterial,
		FVector(15.0f, 3.0f, 15.0f),
		NSLOCTEXT("IGMissingFloor", "P1MeterPrompt", "계량기"),
		NSLOCTEXT(
			"IGMissingFloor",
			"P1MeterThought",
			"…다섯 개다. 이 집은 네 세대인데."),
		EIGMissingFloorTruth::LivedUpstairs,
		EIGMissingFloorSource::MeterFifthDial,
		0.0f,
		IGPuzzleOne::DialNoiseLoudness);
	MeterDialEvidence->OnExamined.AddUObject(
		this, &AIGMissingFloorPuzzleOneDirector::HandleMeterExamined);

	// The unnamed breaker. A hold, so throwing it is a decision rather than a
	// stray click — and the noise it makes is the price of checking.
	SpawnParameters.Name = TEXT("MissingFloorBreaker");
	BreakerAction = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPuzzleOne::BreakerFace),
		SpawnParameters);
	if (!BreakerAction)
	{
		return false;
	}
	BreakerAction->Configure(
		CubeMesh,
		ToggleMaterial,
		FVector(7.0f, 3.0f, 8.0f),
		NSLOCTEXT("IGMissingFloor", "P1BreakerPrompt", "이름 없는 회로"),
		FText::GetEmpty(),
		// Files nothing on its own: what it produces is a sound, and the sound
		// is what the player reasons from.
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		IGPuzzleOne::BreakerHoldSeconds,
		IGPuzzleOne::BreakerNoiseLoudness);
	BreakerAction->OnExamined.AddUObject(
		this, &AIGMissingFloorPuzzleOneDirector::HandleBreakerThrown);

	// The meter-reading sheet. Five columns; the fifth stops in July 2024.
	// Authored as padded rows for now — a real column presentation is a HUD
	// change and is tracked separately.
	SpawnParameters.Name = TEXT("MissingFloorReadingSheet");
	ReadingSheet = World->SpawnActor<AIGReadableNote>(
		AIGReadableNote::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPuzzleOne::ReadingSheetLocation),
		SpawnParameters);
	if (!ReadingSheet)
	{
		return false;
	}
	ReadingSheet->ConfigurePrototypeVisuals(
		CubeMesh,
		DialMaterial,
		FVector(21.0f, 1.2f, 29.7f));
	ReadingSheet->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "P1SheetPrompt", "검침 기록지"));
	ReadingSheet->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "P1SheetTitle", "달빛빌라 검침 기록"),
		{
			NSLOCTEXT("IGMissingFloor", "P1SheetHeader", "호수   4월   5월   6월   7월"),
			NSLOCTEXT("IGMissingFloor", "P1Sheet401", "401    182   174   169   201"),
			NSLOCTEXT("IGMissingFloor", "P1Sheet402", "402    240   233   251   266"),
			NSLOCTEXT("IGMissingFloor", "P1Sheet403", "403    118   121   115   130"),
			NSLOCTEXT("IGMissingFloor", "P1Sheet404", "404    97    102   99    104"),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "P1SheetFifth", "(공란)  63    58    61    0"),
			FText::GetEmpty(),
			NSLOCTEXT(
				"IGMissingFloor",
				"P1SheetNote",
				"* 다섯 번째 칸은 2024년 7월 이후 계속 0. 검침 불필요."),
		});
	ReadingSheet->OnReadStateChanged.AddDynamic(
		this, &AIGMissingFloorPuzzleOneDirector::HandleSheetRead);

	CreateBallastHum();
	return true;
}

void AIGMissingFloorPuzzleOneDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BallastHum)
	{
		BallastHum->Stop();
	}
	Super::EndPlay(EndPlayReason);
}

void AIGMissingFloorPuzzleOneDirector::CreateBallastHum()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UIGAmbienceSoundWave* Wave =
		NewObject<UIGAmbienceSoundWave>(this, TEXT("FifthFloorBallastWave"));
	// The store's ballast buzz is literally the right sound: a fluorescent
	// fitting with a thin high whine. Reused rather than re-synthesized.
	Wave->Configure(EIGAmbienceMode::StoreBuzz, 0x5A17C0DEu);

	BallastHum = NewObject<UAudioComponent>(this, TEXT("FifthFloorBallastHum"));
	BallastHum->RegisterComponent();
	BallastHum->AttachToComponent(
		GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	BallastHum->SetWorldLocation(IGPuzzleOne::BallastHumLocation);
	BallastHum->SetSound(Wave);
	BallastHum->AttenuationSettings = IGAudio::MakeAttenuation(
		this,
		IGPuzzleOne::BallastHumInnerRadius,
		IGPuzzleOne::BallastHumFalloff);
	BallastHum->bAllowSpatialization = true;
	BallastHum->bAutoActivate = false;
	BallastHum->SetVolumeMultiplier(IGPuzzleOne::BallastHumVolume);
	// Deliberately not played: there is nothing above the ceiling until the
	// player gives that circuit power.
}

void AIGMissingFloorPuzzleOneDirector::HandleMeterExamined(
	AIGMissingFloorEvidence* Evidence)
{
	// The dial that does not turn. Said once, on the first look, and only if
	// the fixture the lobby built is actually there to look at. The line
	// claims nothing the greybox does not show — the dials are static meshes,
	// so it reads the nameplate and the dead needle, not motion.
	if (const AIGPrologueWorldScene* WorldScene = Scene.Get())
	{
		if (WorldScene->GetFifthMeterDisc())
		{
			AIGHorrorHUD::PushThought(
				this,
				NSLOCTEXT(
					"IGMissingFloor",
					"P1DialStill",
					"…다섯 번째만 명판이 없어. 바늘도 죽어 있고."),
				3.8f);
		}
	}
}

void AIGMissingFloorPuzzleOneDirector::HandleBreakerThrown(
	AIGMissingFloorEvidence* Evidence)
{
	if (bBreakerThrown)
	{
		return;
	}
	bBreakerThrown = true;

	// Raise the toggle so the world shows what was done, then let the sound
	// answer. This is the whole confirmation: no card, no tick, no popup.
	if (AIGPrologueWorldScene* WorldScene = Scene.Get())
	{
		if (UStaticMeshComponent* Toggle = WorldScene->GetUnnamedBreakerToggle())
		{
			Toggle->SetRelativeLocation(
				Toggle->GetRelativeLocation() + FVector(0.0f, 0.0f, 3.0f));
		}
	}

	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateRelayClick(this),
		IGPuzzleOne::BreakerFace,
		0.8f);

	if (BallastHum)
	{
		BallastHum->Play();
		bBallastHumAudible = true;
	}

	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->MarkPuzzleSolved(IGPuzzleOne::PuzzleId);
	}

	AIGHorrorHUD::PushThought(
		this,
		NSLOCTEXT("IGMissingFloor", "P1BallastThought", "…위에서 불이 들어왔어."),
		4.0f);

	OnSolved.Broadcast();
}

void AIGMissingFloorPuzzleOneDirector::HandleSheetRead(
	AIGReadableNote* Note,
	const bool bOpened)
{
	// File on open, not on close: the player has seen the fifth column.
	if (!bOpened)
	{
		return;
	}
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->RegisterTruthSource(
			EIGMissingFloorTruth::LivedUpstairs,
			EIGMissingFloorSource::MeterReadingSheet);
	}
}

bool AIGMissingFloorPuzzleOneDirector::ValidateFixtures() const
{
	const AIGPrologueWorldScene* WorldScene = Scene.Get();
	return WorldScene != nullptr
		&& WorldScene->GetFifthMeterDisc() != nullptr
		&& WorldScene->GetUnnamedBreakerToggle() != nullptr
		&& MeterDialEvidence != nullptr
		&& BreakerAction != nullptr
		&& ReadingSheet != nullptr
		&& BallastHum != nullptr;
}

UIGMissingFloorNarrativeSubsystem*
AIGMissingFloorPuzzleOneDirector::GetNarrative() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
}
