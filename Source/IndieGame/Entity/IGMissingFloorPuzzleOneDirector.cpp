#include "Entity/IGMissingFloorPuzzleOneDirector.h"

#include "Audio/IGAmbienceSoundWave.h"
#include "Audio/IGAudioHelpers.h"
#include "Audio/IGMissingFloorAudioSubsystem.h"
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
	// 미명칭 차단기가 내려와 있는 자리. 스위치판 왼쪽 열 맨 아래 슬롯이다.
	const FVector BreakerFace(569.4f, -365.6f, 140.0f);
	const FVector CommonBreakerFace(582.7f, -365.6f, 151.0f);
	const FName UnpoweredObservation(TEXT("P1.Isolation.Unpowered"));
	const FName PoweredObservation(TEXT("P1.Isolation.Powered"));
	const FVector ReadingSheetLocation(401.0f, -361.2f, 150.0f);

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

void AIGMissingFloorPuzzleOneDirector::UpdateMeterMotion()
{
	GetWorldTimerManager().ClearTimer(MeterRotationTimer);
	if (Scene.IsValid())
	{
		GetWorldTimerManager().SetTimer(MeterRotationTimer, this,
			&AIGMissingFloorPuzzleOneDirector::AdvanceMeterDisc, .05f, true);
	}
}

void AIGMissingFloorPuzzleOneDirector::AdvanceMeterDisc()
{
	if (Scene.IsValid())
	{
		Scene->AdvanceUtilityMeters(2.1f, bHourActive && bBreakerThrown, bCommonLightsEnabled);
	}
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
	// 검침 기록지는 금속 클립보드에 끼운 종이다. 사무실에서 계속 쓰는
	// 장부이므로 젖은 공지가 아니라 멀쩡한 종이를 쓴다. 예전에는 다섯째
	// 계량기 원판의 재질을 그대로 넘겨서, A4 한 장이 검은 플라스틱 판으로
	// 서 있었다.
	UMaterialInterface* SheetMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Prototype/Materials/M_LobbyMeterSheet.M_LobbyMeterSheet"));
	// Reuse the material the lobby already put on this fixture so the
	// interaction surface disappears into the prop it belongs to.
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
	//
	// 이 액터는 그림을 갖지 않는다. 씬이 다섯 자리 모두에 문자판·유리·회전
	// 원판을 세워 두는데, 여기에 15cm 판을 하나 더 띄우면 그 셋을 통째로
	// 덮는다. 덮이는 것이 하필 P1이 보라고 하는 바로 그 계량기다 — 문자판도
	// 안 보이고, 「다섯째만 안 돈다」를 읽을 원판도 안 보인다. 상호작용만
	// 맡기고 그림은 씬에 맡긴다.
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
		nullptr,
		FVector(15.0f, 3.0f, 15.0f),
		NSLOCTEXT("IGMissingFloor", "P1MeterPrompt", "계량기 — 원판 확인"),
		FText::GetEmpty(),
		EIGMissingFloorTruth::None,
		EIGMissingFloorSource::None,
		1.2f,
		IGPuzzleOne::DialNoiseLoudness,
		/*bPresentationVisible=*/false);
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
		IGPuzzleOne::BreakerNoiseLoudness,
		// 씬이 세워 둔 미명칭 토글 위에 겹치는 대리물이다. 그림을 켜 두면
		// 진짜 토글을 가려, 내려간 것이 올라가는 것도 보이지 않는다.
		/*bPresentationVisible=*/false);
	BreakerAction->OnExamined.AddUObject(
		this, &AIGMissingFloorPuzzleOneDirector::HandleBreakerThrown);
	SpawnParameters.Name = TEXT("MissingFloorCommonLighting");
	CommonLightAction = World->SpawnActor<AIGMissingFloorEvidence>(
		AIGMissingFloorEvidence::StaticClass(),
		FTransform(FRotator::ZeroRotator, IGPuzzleOne::CommonBreakerFace), SpawnParameters);
	if (!CommonLightAction) { return false; }
	CommonLightAction->Configure(CubeMesh, nullptr, FVector(7, 3, 7),
		NSLOCTEXT("IGMissingFloor", "P1CommonPrompt", "공용 조명 — 내리기"),
		FText::GetEmpty(), EIGMissingFloorTruth::None, EIGMissingFloorSource::None,
		IGPuzzleOne::BreakerHoldSeconds, IGPuzzleOne::BreakerNoiseLoudness, false);
	CommonLightAction->OnExamined.AddUObject(this, &AIGMissingFloorPuzzleOneDirector::HandleCommonLighting);

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
		SheetMaterial,
		FVector(21.0f, 0.08f, 29.7f));
	ReadingSheet->SetInteractionPrompt(
		NSLOCTEXT("IGMissingFloor", "P1SheetPrompt", "검침 기록지"));
	ReadingSheet->SetNoteText(
		NSLOCTEXT("IGMissingFloor", "P1SheetTitle", "달빛빌라 검침 기록"),
		{
			NSLOCTEXT("IGMissingFloor", "P1SheetHeader", "월 사용량(kWh) / 4월  5월  6월  7월"),
			NSLOCTEXT("IGMissingFloor", "P1Sheet401", "401    182   174   169   201"),
			NSLOCTEXT("IGMissingFloor", "P1Sheet402", "402    240   233   251   266"),
			NSLOCTEXT("IGMissingFloor", "P1Sheet403", "403    118   121   115   130"),
			NSLOCTEXT("IGMissingFloor", "P1Sheet404", "공용   97    102   99    104"),
			FText::GetEmpty(),
			NSLOCTEXT("IGMissingFloor", "P1SheetFifth", "(공란)  63    58    61    0"),
			FText::GetEmpty(),
			NSLOCTEXT(
				"IGMissingFloor",
				"P1SheetNote",
				"(공란)  24.07~ 0  검침 생략\n공용은 복도등 계량. 이름 없는 것은 확인 후 기입."),
		});
	ReadingSheet->OnReadStateChanged.AddDynamic(
		this, &AIGMissingFloorPuzzleOneDirector::HandleSheetRead);

	// 밤1의 목표는 회로가 아니라 T1이다. 회로는 확인이고, 확인은 두 기록이
	// 맞물린 뒤에야 뜻이 생긴다.
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		TruthHandle = Narrative->OnTruthConfirmed.AddUObject(
			this, &AIGMissingFloorPuzzleOneDirector::HandleTruthConfirmed);
	}

	CreateBallastHum();
	return true;
}

void AIGMissingFloorPuzzleOneDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(MeterRotationTimer);
	if (Scene.IsValid())
	{
		Scene->SetCommonInspectionLightsEnabled(true);
	}
	if (UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative())
	{
		Narrative->OnTruthConfirmed.Remove(TruthHandle);
	}
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
		IGPuzzleOne::BallastHumFalloff,
		EIGAudioBus::World);
	BallastHum->bAllowSpatialization = true;
	BallastHum->bAutoActivate = false;
	BallastHum->SetVolumeMultiplier(IGPuzzleOne::BallastHumVolume);
	if (UIGMissingFloorAudioSubsystem* AudioDirector =
		World->GetSubsystem<UIGMissingFloorAudioSubsystem>())
	{
		AudioDirector->RegisterComponent(BallastHum, EIGAudioBus::World);
	}
	// Deliberately not played: there is nothing above the ceiling until the
	// player gives that circuit power.
}

void AIGMissingFloorPuzzleOneDirector::HandleMeterExamined(
	AIGMissingFloorEvidence* Evidence)
{
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative) { return; }
	if (!bHourActive || bCommonLightsEnabled)
	{
		AIGHorrorHUD::PushThought(this,
			NSLOCTEXT("IGMissingFloor", "P1DialStill", "공용 계량기는 따로 있다. 이름 없는 건 어느 회로지."), 3.8f);
		return;
	}
	Narrative->MarkBeatPlayed(bBreakerThrown ? IGPuzzleOne::PoweredObservation : IGPuzzleOne::UnpoweredObservation);
	AIGHorrorHUD::PushThought(this, bBreakerThrown
		? NSLOCTEXT("IGMissingFloor", "P1IsolatedRunning", "복도는 꺼졌는데 이 원판은 돈다.")
		: NSLOCTEXT("IGMissingFloor", "P1IsolatedStopped", "원판이 움직이지 않는다. 위에서도 소리가 안 난다."), 3.8f);
	if (Narrative->HasBeatPlayed(IGPuzzleOne::PoweredObservation)
		&& Narrative->HasBeatPlayed(IGPuzzleOne::UnpoweredObservation))
	{
		Narrative->RegisterTruthSource(EIGMissingFloorTruth::LivedUpstairs, EIGMissingFloorSource::MeterFifthDial);
	}
	AnnounceSolvedIfReady();
}

void AIGMissingFloorPuzzleOneDirector::HandleCommonLighting(AIGMissingFloorEvidence* Evidence)
{
	if (!bHourActive) { return; }
	bCommonLightsEnabled = !bCommonLightsEnabled;
	if (AIGPrologueWorldScene* WorldScene = Scene.Get())
	{
		WorldScene->SetCommonInspectionLightsEnabled(bCommonLightsEnabled);
		if (UStaticMeshComponent* Toggle = WorldScene->GetCommonBreakerToggle())
		{
			FVector At = Toggle->GetRelativeLocation();
			At.Z = 151.0f - (bCommonLightsEnabled ? 0.0f : 3.0f);
			Toggle->SetRelativeLocation(At);
		}
	}
	CommonLightAction->SetInteractionPrompt(bCommonLightsEnabled
		? NSLOCTEXT("IGMissingFloor", "P1CommonPrompt", "공용 조명 — 내리기")
		: NSLOCTEXT("IGMissingFloor", "P1CommonRestore", "공용 조명 — 올리기"));
	IGAudio::SpawnOneShotAt(this, UIGToneSequenceSoundWave::CreateRelayClick(this),
		IGPuzzleOne::CommonBreakerFace, .8f, 1.f, 90.f, 900.f, EIGAudioBus::Puzzle);
}

void AIGMissingFloorPuzzleOneDirector::HandleBreakerThrown(
	AIGMissingFloorEvidence* Evidence)
{
	if (!bHourActive)
	{
		// 낮의 투입. 계전기가 바로 되돌려서 딸깍 소리 하나로 끝난다. 위는
		// 그대로 조용하고, 회로는 밤에 다시 올릴 수 있다.
		IGAudio::SpawnOneShotAt(
			this,
			UIGToneSequenceSoundWave::CreateRelayClick(this),
			IGPuzzleOne::BreakerFace,
			0.8f,
			1.0f,
			90.0f,
			900.0f,
			EIGAudioBus::Puzzle);
		AIGHorrorHUD::PushThought(
			this,
			NSLOCTEXT(
				"IGMissingFloor",
				"P1BreakerDaytime",
				"올리자마자 떨어진다. 계전기가 안 물린다."),
			3.4f);
		return;
	}
	bBreakerThrown = !bBreakerThrown;
	UpdateMeterMotion();
	BreakerAction->SetInteractionPrompt(bBreakerThrown
		? NSLOCTEXT("IGMissingFloor", "P1BreakerLower", "이름 없는 회로 — 내리기")
		: NSLOCTEXT("IGMissingFloor", "P1BreakerRaise", "이름 없는 회로 — 올리기"));

	// Raise the toggle so the world shows what was done, then let the sound
	// answer. This is the whole confirmation: no card, no tick, no popup.
	if (AIGPrologueWorldScene* WorldScene = Scene.Get())
	{
		if (UStaticMeshComponent* Toggle = WorldScene->GetUnnamedBreakerToggle())
		{
			FVector At = Toggle->GetRelativeLocation();
			At.Z = 140.0f + (bBreakerThrown ? 3.0f : 0.0f);
			Toggle->SetRelativeLocation(At);
		}
	}

	IGAudio::SpawnOneShotAt(
		this,
		UIGToneSequenceSoundWave::CreateRelayClick(this),
		IGPuzzleOne::BreakerFace,
		0.8f,
		1.0f,
		90.0f,
		900.0f,
		EIGAudioBus::Puzzle);

	if (BallastHum)
	{
		if (bBreakerThrown) { BallastHum->Play(); }
		else { BallastHum->Stop(); }
		bBallastHumAudible = bBreakerThrown;
	}
	if (!bBreakerThrown)
	{
		return;
	}

	const UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	const bool bKnowsWhoseCircuit = Narrative
		&& Narrative->HasTruth(EIGMissingFloorTruth::LivedUpstairs);
	AIGHorrorHUD::PushThought(
		this,
		bKnowsWhoseCircuit
			? NSLOCTEXT("IGMissingFloor", "P1BallastThought", "위에서 불이 들어왔다.")
			: NSLOCTEXT(
				"IGMissingFloor",
				"P1BallastUnread",
				"천장 쪽에서 전기 들어가는 소리가 났다."),
		4.0f);

	AnnounceSolvedIfReady();
}

void AIGMissingFloorPuzzleOneDirector::SetHourActive(const bool bActive)
{
	bHourActive = bActive;
	UpdateMeterMotion();
	if (BallastHum)
	{
		bBallastHumAudible = bActive && bBreakerThrown;
		if (bBallastHumAudible) { BallastHum->Play(); }
		else { BallastHum->Stop(); }
	}
	if (!bActive && Scene.IsValid())
	{
		Scene->SetCommonInspectionLightsEnabled(true);
		bCommonLightsEnabled = true;
		if (UStaticMeshComponent* Toggle = Scene->GetCommonBreakerToggle())
		{
			FVector At = Toggle->GetRelativeLocation(); At.Z = 151.0f; Toggle->SetRelativeLocation(At);
		}
		CommonLightAction->SetInteractionPrompt(NSLOCTEXT("IGMissingFloor", "P1CommonPrompt", "공용 조명 — 내리기"));
	}
}

void AIGMissingFloorPuzzleOneDirector::HandleTruthConfirmed(
	const EIGMissingFloorTruth Truth)
{
	if (Truth == EIGMissingFloorTruth::LivedUpstairs)
	{
		AnnounceSolvedIfReady();
	}
}

void AIGMissingFloorPuzzleOneDirector::AnnounceSolvedIfReady()
{
	// 불을 켰어도 그 위에 뭐가 있는지 모르면 밤은 안 끝난다. 계량기 다섯과
	// 검침표 다섯 칸이 맞물려야 「위층」이 된다.
	if (bSolvedAnnounced || !bBreakerThrown)
	{
		return;
	}
	UIGMissingFloorNarrativeSubsystem* Narrative = GetNarrative();
	if (!Narrative || !Narrative->HasTruth(EIGMissingFloorTruth::LivedUpstairs))
	{
		return;
	}
	bSolvedAnnounced = true;
	Narrative->MarkPuzzleSolved(IGPuzzleOne::PuzzleId);
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
		&& WorldScene->GetCommonBreakerToggle() != nullptr
		&& CommonLightAction != nullptr
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
