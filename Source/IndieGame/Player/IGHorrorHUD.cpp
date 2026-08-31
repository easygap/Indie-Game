#include "Player/IGHorrorHUD.h"
#include "Player/IGInputBindingSubsystem.h"

#include "Accessibility/IGAccessibilitySubsystem.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/IGNoiseSubsystem.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "IndieGame.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Interaction/IGReadableNote.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGInteractionComponent.h"
#include "Player/IGFrontendMenuLayout.h"
#include "Player/IGPlayerController.h"
#include "Sequence/IGMorningRoutineDirector.h"
#include "Sequence/IGObjectiveProvider.h"
#include "Sequence/IGWakeUpDirector.h"
#include "UObject/UObjectGlobals.h"

namespace IGHorrorHUD
{
	constexpr double DirectorSearchInterval = 2.0;
	constexpr int32 LensDropletTextureSize = 128;
	constexpr int32 HudRoundedMaskTextureSize = 64;
	constexpr int32 MaximumDialogueQueueDepth = 6;
	constexpr int32 MaximumAudioCaptionQueueDepth = 4;
	constexpr double StoryDialogueMaximumQueueAge = 14.0;
	constexpr double AmbientDialogueMaximumQueueAge = 6.0;
	constexpr float DialogueGlyphsPerSecond = 11.5f;
	constexpr float DialogueMinimumSeconds = 2.2f;
	constexpr float DialogueMaximumSeconds = 9.0f;
	constexpr float FocusAcquireDelaySeconds = 0.09f;
	constexpr float FocusAcquireRevealSeconds = 0.09f;
	constexpr double FirstPersonKnockDurationSeconds = 0.22;
	constexpr int32 FirstPersonKnockFrameCount = 4;
	constexpr double CaptureEmbraceDurationSeconds = 1.2;
	constexpr int32 CaptureEmbraceFrameCount = 4;

	/**
	 * The noise ripple (§5.1). One slot, deliberately short, with a minimum
	 * gap so a walking player gets a pulse per few steps rather than a strobe.
	 */
	constexpr double NoiseRippleDurationSeconds = 0.85;
	constexpr double NoiseRippleRetriggerSeconds = 0.34;
	/** Arc geometry: segment count, sweep at full carry, and stroke weight. */
	constexpr int32 NoiseRippleSegmentCount = 14;
	constexpr float NoiseRippleMaximumSweepDegrees = 78.0f;
	constexpr float NoiseRippleMinimumSweepDegrees = 16.0f;
	constexpr float NoiseRippleMaximumThickness = 2.6f;

	// Native font sizes per text role. Presentation scale is applied once for the
	// current resolution and accessibility setting, then reused for measurement
	// and drawing so Korean wrapping stays pixel-consistent.
	constexpr int32 LargeFontSize = 24;
	constexpr int32 FrontendTitleFontSize = 64;
	constexpr int32 MediumFontSize = 20;
	constexpr int32 SmallFontSize = 18;
	constexpr int32 PhoneMetaFontSize = 15;

	const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.9f);
	const FLinearColor PaleGray(0.82f, 0.84f, 0.82f, 0.95f);
	const FLinearColor MutedGray(0.62f, 0.64f, 0.62f, 0.9f);
	const FLinearColor RedAccent(0.72f, 0.08f, 0.06f, 1.0f);
	const FLinearColor ThoughtBlue(0.74f, 0.78f, 0.86f, 1.0f);
	const FLinearColor DialogueIvory(0.88f, 0.87f, 0.81f, 1.0f);
	const FLinearColor DialogueTeal(0.42f, 0.64f, 0.59f, 1.0f);

	// Title-screen palette: wet concrete, old ivory and one oxidized focus mark.
	// Keeping these semantic tokens here avoids per-row colour drift.
	const FLinearColor FrontendInk(0.018f, 0.024f, 0.025f, 1.0f);
	const FLinearColor FrontendIvory(0.89f, 0.88f, 0.83f, 1.0f);
	const FLinearColor FrontendMuted(0.55f, 0.57f, 0.54f, 1.0f);
	const FLinearColor FrontendOxide(0.62f, 0.25f, 0.21f, 1.0f);
	const FLinearColor FrontendFocus(0.028f, 0.033f, 0.032f, 0.88f);
	const FLinearColor FrontendGuide(0.46f, 0.48f, 0.45f, 0.34f);

	// 설정 전용 색은 의미 단위로 묶는다. 행마다 임의의 RGB를 넣지 않아야
	// 선택·경고·완료 상태의 대비를 한 곳에서 조정할 수 있다.
	const FLinearColor SettingsPanel(0.035f, 0.043f, 0.043f, 0.985f);
	const FLinearColor SettingsRail(0.024f, 0.030f, 0.030f, 0.97f);
	const FLinearColor SettingsRaised(0.072f, 0.083f, 0.081f, 0.96f);
	const FLinearColor SettingsSelected(0.115f, 0.132f, 0.128f, 0.98f);
	const FLinearColor SettingsDivider(0.22f, 0.24f, 0.23f, 0.52f);
	const FLinearColor SettingsPrimary(0.90f, 0.91f, 0.87f, 1.0f);
	const FLinearColor SettingsSecondary(0.61f, 0.64f, 0.61f, 0.96f);
	const FLinearColor SettingsAccent(0.76f, 0.26f, 0.20f, 1.0f);
	const FLinearColor SettingsSuccess(0.46f, 0.71f, 0.58f, 1.0f);

	enum class EJournalLane : int32
	{
		Administration = 0,
		Life = 1,
		Personal = 2,
	};

	enum class EJournalThumbnail : int32
	{
		Document = 0,
		Meter = 1,
		Plaster = 2,
		Tank = 3,
		Metal = 4,
	};

	struct FJournalEntryDefinition
	{
		FName SourceId;
		EJournalLane Lane = EJournalLane::Life;
		const TCHAR* Title = nullptr;
		const TCHAR* Excerpt = nullptr;
		const TCHAR* WhereWhen = nullptr;
		EJournalThumbnail Thumbnail = EJournalThumbnail::Document;
	};

	/**
	 * This table presents observation, never interpretation. Confirmation lines
	 * are derived later from the narrative snapshot, so UI copy cannot award a
	 * truth or silently become a second puzzle router.
	 */
	static const TArray<FJournalEntryDefinition>& JournalEntries()
	{
		static const TArray<FJournalEntryDefinition> Entries = {
			{TEXT("Lobby.MeterFifthDial"), EJournalLane::Administration,
				TEXT("다섯 번째 계량기"), TEXT("봉인 테이프 아래, 지워진 5층 표기"),
				TEXT("공동현관 계량기함 · 밤 1"), EJournalThumbnail::Meter},
			{TEXT("Office.MeterReadingSheet"), EJournalLane::Administration,
				TEXT("검침 기록지"), TEXT("5층 / 03471 kWh"),
				TEXT("관리실 사본 · 밤 1"), EJournalThumbnail::Document},
			{TEXT("Office.BoardDeliveryReceipt"), EJournalLane::Administration,
				TEXT("석고보드 납품서"), TEXT("9.5T 석고보드 / 7월 26일"),
				TEXT("관리실 원본 · 밤 3"), EJournalThumbnail::Document},
			{TEXT("Office.CarbonLedgerOriginal"), EJournalLane::Administration,
				TEXT("민원 원장 먹지"), TEXT("401호: 벽에서 쿵쿵. 사람 소리 같다."),
				TEXT("관리실 책상 · 7/27~7/31"), EJournalThumbnail::Document},
			{TEXT("Office.AgentMoveOutMessage"), EJournalLane::Administration,
				TEXT("중개인 문자"), TEXT("5층 짐 뺐습니다. 7/26."),
				TEXT("관리실 휴대폰 · 밤 2"), EJournalThumbnail::Metal},
			{TEXT("Office.EvictionWarning"), EJournalLane::Administration,
				TEXT("퇴거 통지"), TEXT("403호. 계약 위반. 이번 주 내 퇴거 바랍니다."),
				TEXT("관리실 유리 · 밤 3 아침"), EJournalThumbnail::Document},

			{TEXT("Forum.NoisePosts"), EJournalLane::Life,
				TEXT("층간소음 게시글"), TEXT("04:20~04:40, 공구 카트 끄는 소리"),
				TEXT("403호 우편물 · 낮 2"), EJournalThumbnail::Document},
			{TEXT("Forum.FinalPost"), EJournalLane::Life,
				TEXT("마지막 게시글"), TEXT("오늘은 올라가서 직접 말하겠습니다."),
				TEXT("게시글 출력물 · 2024-07-26"), EJournalThumbnail::Document},
			{TEXT("Fifth.LandingImpactMark"), EJournalLane::Life,
				TEXT("계단참 충격 자국"), TEXT("철골 모서리와 바닥에 같은 검은 이염"),
				TEXT("5층 계단참 · 밤 3"), EJournalThumbnail::Metal},
			{TEXT("Fifth.FreshPlasterDating"), EJournalLane::Life,
				TEXT("두 겹의 마감"), TEXT("안쪽 보드와 바깥 실란트의 굳은 정도가 다르다."),
				TEXT("5층 공동벽 · 밤 3"), EJournalThumbnail::Plaster},
			{TEXT("Fifth.PipeWaterComparison"), EJournalLane::Life,
				TEXT("배관 청음"), TEXT("한쪽은 물, 한쪽은 먹먹한 빈 잔향"),
				TEXT("5층 서비스 벽 · 밤 3"), EJournalThumbnail::Metal},
			{TEXT("Fifth.WallEchoByHand"), EJournalLane::Life,
				TEXT("직접 두드린 벽"), TEXT("둘째 벽만 소리가 짧게 끊겼다."),
				TEXT("5층 · 밤 3"), EJournalThumbnail::Plaster},
			{TEXT("Unit401.KnockTallyJournal"), EJournalLane::Life,
				TEXT("황순금 소리 일지"), TEXT("7/27부터 닷새. 마지막 날은 세 번뿐."),
				TEXT("401호 · 낮 3"), EJournalThumbnail::Document},
			{TEXT("Roof.TankWaterAudition"), EJournalLane::Life,
				TEXT("저수조 표찰"), TEXT("용량 2,000 L / 만수"),
				TEXT("옥상 · 밤 3"), EJournalThumbnail::Tank},
			{TEXT("Fifth.AnswerReturned"), EJournalLane::Life,
				TEXT("벽의 대답"), TEXT("둘, 쉬고, 하나."),
				TEXT("5층 공동벽 · 05:12"), EJournalThumbnail::Plaster},
			{TEXT("Fifth.BreakerCutIntervention"), EJournalLane::Life,
				TEXT("차단기 손자국"), TEXT("방금 내려간 주차단기. 관리실 쪽 흙먼지."),
				TEXT("1층 배전반 · 밤 4"), EJournalThumbnail::Metal},

			{TEXT("Estate.ShippingLabels"), EJournalLane::Personal,
				TEXT("반송 소포"), TEXT("백도하 / 무영로 달빛빌라 5"),
				TEXT("403호 이삿짐 · 입주일"), EJournalThumbnail::Document},
			{TEXT("Fifth.TunerNotebookName"), EJournalLane::Personal,
				TEXT("조율 수첩"), TEXT("백도하 / 야간 조율 일정"),
				TEXT("5층 벽 틈 · 밤 3"), EJournalThumbnail::Document},
			{TEXT("Fifth.TunerWorkSchedule"), EJournalLane::Personal,
				TEXT("작업 시간표"), TEXT("마지막 작업 23:10 / 귀가 04:18"),
				TEXT("조율 수첩 · 밤 3"), EJournalThumbnail::Document},
			{TEXT("Fifth.PipeAuditionCriterion"), EJournalLane::Personal,
				TEXT("청음 메모"), TEXT("빈 벽은 낮은 음이 길게 남는다."),
				TEXT("조율 수첩 여백 · 밤 3"), EJournalThumbnail::Document},
			{TEXT("Phone.AnswerRhythmVoicemail"), EJournalLane::Personal,
				TEXT("마지막 음성사서함"), TEXT("문 두드리면 알지? 둘, 하나."),
				TEXT("휴대폰 · 입주 전"), EJournalThumbnail::Metal},
			{TEXT("Fifth.AnswerRhythmNotebook"), EJournalLane::Personal,
				TEXT("수첩의 리듬"), TEXT("● ●  —  ●"),
				TEXT("조율 수첩 여백 · 밤 3"), EJournalThumbnail::Document},
			{TEXT("Unit401.AnswerRhythmJournal"), EJournalLane::Personal,
				TEXT("일지의 답"), TEXT("나도 두드려 줬다. 그랬더니 조용하데."),
				TEXT("401호 · 2024-07-29"), EJournalThumbnail::Document},
		};
		return Entries;
	}

	static float SmoothStep01(const float Value)
	{
		const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
		return Clamped * Clamped * (3.0f - 2.0f * Clamped);
	}
}

void AIGHorrorHUD::BeginPlay()
{
	Super::BeginPlay();
	bLayoutValidationEnabled = FParse::Param(
		FCommandLine::Get(),
		TEXT("IGFrontendShippingProbe"))
		|| FParse::Param(
			FCommandLine::Get(),
			TEXT("IGMissingFloorJournalPreview"))
		|| FParse::Param(
			FCommandLine::Get(),
			TEXT("IGAudioCalibrationPreview"))
		|| FParse::Param(
			FCommandLine::Get(),
			TEXT("IGMissingFloorEndingPreview"))
		|| FParse::Param(
			FCommandLine::Get(),
			TEXT("IGDisplaySettingsPreview"));
	InitializeKoreanFont();
	InitializeFrontendMenuTextures();

	// Optional: absent until Scripts/Prepare-AIArt.ps1 has produced it, in
	// which case the reading panel falls back to a flat fill.
	NotePaperTexture = LoadObject<UTexture2D>(
		nullptr, TEXT("/Game/Prototype/Textures/T_PaperOld_V2_D.T_PaperOld_V2_D"));
	ReceiptPaperTexture = LoadObject<UTexture2D>(
		nullptr, TEXT("/Game/Prototype/Textures/T_PaperClean_V2_D.T_PaperClean_V2_D"));
	InitializeLensDropletTexture();
	InitializeDialogueSurfaceTextures();
	InitializeAudioCalibrationTexture();
	InitializeMissingFloorJournalTextures();
	InitializeFirstPersonActionTextures();
#if !UE_BUILD_SHIPPING
	bFirstPersonKnockPreview = FParse::Param(
		FCommandLine::Get(),
		TEXT("IGM0KnockPreview"));
	bCaptureEmbracePreview = FParse::Param(
		FCommandLine::Get(),
		TEXT("IGM1CapturePreview"));
	bCaptureWakeEchoPreview = FParse::Param(
		FCommandLine::Get(),
		TEXT("IGM1WakeEchoPreview"));
#endif

	ResolveInteractionComponent();
	ResolveDirectors();

	if (UWorld* World = GetWorld())
	{
		NextDirectorSearchTime = World->GetTimeSeconds() + IGHorrorHUD::DirectorSearchInterval;
		// The noise bus is a world subsystem, not a game-instance one: every
		// other subsystem lookup in this file goes through the game instance
		// and would silently return null here.
		if (UIGNoiseSubsystem* Noise = World->GetSubsystem<UIGNoiseSubsystem>())
		{
			NoiseSubsystem = Noise;
			NoiseReportedHandle = Noise->OnNoiseReported.AddUObject(
				this, &AIGHorrorHUD::HandleNoiseReported);
		}
	}
	if (const AIGPlayerController* IndieController =
		Cast<AIGPlayerController>(GetOwningPlayerController()))
	{
		IndieController->RefreshMenuHud();
	}
}

void AIGHorrorHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UIGNoiseSubsystem* Noise = NoiseSubsystem.Get())
	{
		Noise->OnNoiseReported.Remove(NoiseReportedHandle);
	}
	NoiseReportedHandle.Reset();
	NoiseSubsystem = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AIGHorrorHUD::HandleNoiseReported(const FIGNoiseEvent& Event)
{
	// Only the player's own sounds get a ring. The bus also carries the
	// entity's knocks and any scripted bait, and telling the player "you made
	// that sound" when they did not would teach the wrong rule.
	const APawn* OwningPawn = GetOwningPawn();
	if (!OwningPawn || Event.Instigator.Get() != OwningPawn)
	{
		return;
	}
	if (Event.Loudness <= 0.0f)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Game time, not the event's real time: every other HUD timer is game
	// time, and mixing the two produces a garbage age after any pause.
	const double Now = World->GetTimeSeconds();
	const bool bRippleLive = Now < RippleEndTime;
	if (bRippleLive)
	{
		const bool bRetriggerBlocked =
			Now - RippleStartTime < IGHorrorHUD::NoiseRippleRetriggerSeconds;
		// A quieter sound never interrupts a louder ring, and nothing
		// interrupts a ring that only just started.
		if (bRetriggerBlocked || Event.Loudness <= RippleLoudness)
		{
			return;
		}
	}

	RippleWorldLocation = Event.Location;
	RippleRadiusCentimeters = Event.Radius;
	RippleLoudness = Event.Loudness;
	RippleStartTime = Now;
	RippleEndTime = Now + IGHorrorHUD::NoiseRippleDurationSeconds;
}

void AIGHorrorHUD::InitializeDialogueSurfaceTextures()
{
	DialogueFilmTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Prototype/Textures/T_HudDialogueFilm_D.T_HudDialogueFilm_D"));

	constexpr int32 TextureSize = IGHorrorHUD::HudRoundedMaskTextureSize;
	constexpr float SourceRadius = TextureSize * 0.25f;
	TArray64<uint8> PixelBytes;
	PixelBytes.SetNumZeroed(TextureSize * TextureSize * sizeof(FColor));
	FColor* Pixels = reinterpret_cast<FColor*>(PixelBytes.GetData());
	for (int32 Row = 0; Row < TextureSize; ++Row)
	{
		for (int32 Column = 0; Column < TextureSize; ++Column)
		{
			const FVector2D PixelCenter(
				static_cast<float>(Column) + 0.5f,
				static_cast<float>(Row) + 0.5f);
			const FVector2D NearestCornerCenter(
				FMath::Clamp(PixelCenter.X, SourceRadius, TextureSize - SourceRadius),
				FMath::Clamp(PixelCenter.Y, SourceRadius, TextureSize - SourceRadius));
			const float SignedDistance =
				(PixelCenter - NearestCornerCenter).Size() - SourceRadius;
			const float Coverage = 1.0f - IGHorrorHUD::SmoothStep01(
				(SignedDistance + 1.0f) * 0.5f);
			Pixels[Column + Row * TextureSize] = FLinearColor(
				1.0f,
				1.0f,
				1.0f,
				Coverage).ToFColorSRGB();
		}
	}

	const FName TextureName = MakeUniqueObjectName(
		GetTransientPackage(),
		UTexture2D::StaticClass(),
		TEXT("HudRoundedMask"));
	HudRoundedMaskTexture = UTexture2D::CreateTransient(
		TextureSize,
		TextureSize,
		PF_B8G8R8A8,
		TextureName,
		PixelBytes);
	if (HudRoundedMaskTexture)
	{
		HudRoundedMaskTexture->Filter = TF_Bilinear;
		HudRoundedMaskTexture->AddressX = TA_Clamp;
		HudRoundedMaskTexture->AddressY = TA_Clamp;
		HudRoundedMaskTexture->NeverStream = true;
		HudRoundedMaskTexture->UpdateResource();
	}
}

void AIGHorrorHUD::InitializeFrontendMenuTextures()
{
	FrontendTitleBackgroundTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/UI/Textures/T_TitleBackground_D.T_TitleBackground_D"));
	if (!FrontendTitleBackgroundTexture)
	{
		UE_LOG(
			LogIndieGame,
			Warning,
			TEXT("Title key art is unavailable; front end will use its safe fallback."));
	}

	// §9 에필로그의 세 정지 화면. 없으면 그 장면은 글자만 남는다.
	EpilogueWorkshopTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/UI/Textures/T_EpilogueWorkshop_D.T_EpilogueWorkshop_D"));
	EpilogueAutumnTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/UI/Textures/T_EpilogueAutumn_D.T_EpilogueAutumn_D"));
	EpilogueServiceBayTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/UI/Textures/T_EpilogueServiceBay_D.T_EpilogueServiceBay_D"));
	if (!EpilogueWorkshopTexture || !EpilogueAutumnTexture
		|| !EpilogueServiceBayTexture)
	{
		UE_LOG(
			LogIndieGame,
			Warning,
			TEXT("Epilogue stills are unavailable; those scenes draw text only."));
	}

	constexpr int32 ShadeWidth = 256;
	TArray64<uint8> PixelBytes;
	PixelBytes.SetNumZeroed(ShadeWidth * sizeof(FColor));
	FColor* Pixels = reinterpret_cast<FColor*>(PixelBytes.GetData());
	for (int32 Column = 0; Column < ShadeWidth; ++Column)
	{
		const float U = (Column + 0.5f) / ShadeWidth;
		const float Coverage = FMath::Pow(1.0f - U, 2.15f);
		Pixels[Column] = FColor(
			0,
			0,
			0,
			FMath::RoundToInt(255.0f * Coverage));
	}
	const FName ShadeName = MakeUniqueObjectName(
		GetTransientPackage(),
		UTexture2D::StaticClass(),
		TEXT("FrontendShade"));
	FrontendShadeTexture = UTexture2D::CreateTransient(
		ShadeWidth,
		1,
		PF_B8G8R8A8,
		ShadeName,
		PixelBytes);
	if (FrontendShadeTexture)
	{
		FrontendShadeTexture->Filter = TF_Bilinear;
		FrontendShadeTexture->AddressX = TA_Clamp;
		FrontendShadeTexture->AddressY = TA_Clamp;
		FrontendShadeTexture->NeverStream = true;
		FrontendShadeTexture->UpdateResource();
	}
}

void AIGHorrorHUD::InitializeAudioCalibrationTexture()
{
	AudioCalibrationWallTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Prototype/Textures/T_AudioCalibrationWall_D."
			"T_AudioCalibrationWall_D"));
}

void AIGHorrorHUD::InitializeMissingFloorJournalTextures()
{
	MissingFloorJournalTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Prototype/Textures/T_MissingFloorJournalPaper_D."
			"T_MissingFloorJournalPaper_D"));
	JournalMeterTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Prototype/Textures/T_MeterBox_D.T_MeterBox_D"));
	JournalPlasterTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Prototype/Textures/T_MissingFloorDryPlaster_D."
			"T_MissingFloorDryPlaster_D"));
	JournalTankTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Prototype/Textures/T_WaterTankGalvanized_D."
			"T_WaterTankGalvanized_D"));
	// The capture, not T_MetalBrushed_D. A journal thumbnail stands in for the
	// place the evidence came from -- the fifth-floor landing, the ground-floor
	// distribution board -- so it has to be the metal the player saw. Every
	// metal surface in the game samples T_Photo_MetalBrushed_D, because
	// create_textured_materials._load_texture takes the CC0 capture over the
	// procedural fallback; the other three thumbnails already match their
	// surface, and this one was the odd one out, showing a texture that
	// appears nowhere in the world. Scripts/check_cook_references.py fails if
	// the two sides drift apart again.
	JournalMetalTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Prototype/Textures/T_Photo_MetalBrushed_D."
			"T_Photo_MetalBrushed_D"));
}

void AIGHorrorHUD::InitializeFirstPersonActionTextures()
{
	static const TCHAR* KnockTexturePaths[] = {
		TEXT("/Game/Prototype/Textures/T_FPHandKnock0_D.T_FPHandKnock0_D"),
		TEXT("/Game/Prototype/Textures/T_FPHandKnock1_D.T_FPHandKnock1_D"),
		TEXT("/Game/Prototype/Textures/T_FPHandKnock2_D.T_FPHandKnock2_D"),
		TEXT("/Game/Prototype/Textures/T_FPHandKnock3_D.T_FPHandKnock3_D"),
	};
	FirstPersonKnockFrames.SetNum(IGHorrorHUD::FirstPersonKnockFrameCount);
	for (int32 FrameIndex = 0;
		FrameIndex < IGHorrorHUD::FirstPersonKnockFrameCount;
		++FrameIndex)
	{
		FirstPersonKnockFrames[FrameIndex] = LoadObject<UTexture2D>(
			nullptr,
			KnockTexturePaths[FrameIndex]);
	}

	static const TCHAR* CaptureTexturePaths[] = {
		TEXT("/Game/Prototype/Textures/T_FPCaptureEmbrace0_D.T_FPCaptureEmbrace0_D"),
		TEXT("/Game/Prototype/Textures/T_FPCaptureEmbrace1_D.T_FPCaptureEmbrace1_D"),
		TEXT("/Game/Prototype/Textures/T_FPCaptureEmbrace2_D.T_FPCaptureEmbrace2_D"),
		TEXT("/Game/Prototype/Textures/T_FPCaptureEmbrace3_D.T_FPCaptureEmbrace3_D"),
	};
	CaptureEmbraceFrames.SetNum(IGHorrorHUD::CaptureEmbraceFrameCount);
	for (int32 FrameIndex = 0;
		FrameIndex < IGHorrorHUD::CaptureEmbraceFrameCount;
		++FrameIndex)
	{
		CaptureEmbraceFrames[FrameIndex] = LoadObject<UTexture2D>(
			nullptr,
			CaptureTexturePaths[FrameIndex]);
	}
}

void AIGHorrorHUD::PlayFirstPersonKnock()
{
	if (const UWorld* World = GetWorld())
	{
		FirstPersonKnockStartTime = World->GetTimeSeconds();
	}
}

void AIGHorrorHUD::PlayCaptureEmbrace(const float DurationSeconds)
{
	if (const UWorld* World = GetWorld())
	{
		CaptureEmbraceStartTime = World->GetTimeSeconds();
		CaptureEmbraceEndTime = CaptureEmbraceStartTime + FMath::Max(
			0.05f,
			DurationSeconds);
	}
}

void AIGHorrorHUD::PlayCaptureWakeEcho(
	const int32 CaptureCount,
	const float VisualDurationSeconds,
	const float OwnershipDurationSeconds)
{
	if (const UWorld* World = GetWorld())
	{
		const float SafeVisualDuration = FMath::Max(0.05f, VisualDurationSeconds);
		const float SafeOwnershipDuration = FMath::Max(
			SafeVisualDuration,
			OwnershipDurationSeconds);
		CaptureWakeEchoStartTime = World->GetTimeSeconds();
		CaptureWakeEchoVisualEndTime =
			CaptureWakeEchoStartTime + SafeVisualDuration;
		CaptureWakeEchoEndTime =
			CaptureWakeEchoStartTime + SafeOwnershipDuration;
		CaptureWakeEchoCount = FMath::Max(1, CaptureCount);
	}
}

void AIGHorrorHUD::InitializeLensDropletTexture()
{
	constexpr int32 TextureSize = IGHorrorHUD::LensDropletTextureSize;
	TArray64<uint8> PixelBytes;
	PixelBytes.SetNumZeroed(TextureSize * TextureSize * sizeof(FColor));
	FColor* Pixels = reinterpret_cast<FColor*>(PixelBytes.GetData());

	for (int32 Row = 0; Row < TextureSize; ++Row)
	{
		for (int32 Column = 0; Column < TextureSize; ++Column)
		{
			const float X =
				((static_cast<float>(Column) + 0.5f) / TextureSize) * 2.0f - 1.0f;
			const float Y =
				((static_cast<float>(Row) + 0.5f) / TextureSize) * 2.0f - 1.0f;
			const float Vertical01 = FMath::Clamp((Y + 1.0f) * 0.5f, 0.0f, 1.0f);
			// A lens bead is never a clean icon. Slightly shear its centre and vary
			// the edge at two frequencies so the proxy reads as a thin water film
			// even though it deliberately avoids an expensive refraction pass.
			const float WarpedX = X
				+ Y * 0.035f
				+ FMath::Sin(Y * 4.7f) * 0.018f;
			const float HalfWidth = FMath::Lerp(0.50f, 0.62f, Vertical01);
			const float EdgeVariation =
				FMath::Sin(X * 3.1f + Y * 4.3f) * 0.020f
				+ FMath::Sin(X * 8.4f - Y * 5.2f) * 0.010f;
			const float EllipseRadius = FMath::Sqrt(
				FMath::Square(WarpedX / HalfWidth)
				+ FMath::Square((Y - 0.03f) / 0.90f))
				+ EdgeVariation;

			const float Coverage = 1.0f - IGHorrorHUD::SmoothStep01(
				(EllipseRadius - 0.88f) / 0.12f);
			const float Rim = Coverage * IGHorrorHUD::SmoothStep01(
				(EllipseRadius - 0.70f) / 0.23f);
			const float HighlightDistance = FVector2D(
				WarpedX + 0.24f,
				(Y + 0.29f) * 1.35f).Size();
			const float Highlight = Coverage * (
				1.0f - IGHorrorHUD::SmoothStep01((HighlightDistance - 0.04f) / 0.16f));
			const float LowerShadow = Coverage * IGHorrorHUD::SmoothStep01(
				((X * 0.42f + Y * 0.58f) + 0.10f) / 0.90f);

			const float Alpha = Coverage * FMath::Clamp(
				0.022f + Rim * 0.23f + Highlight * 0.24f + LowerShadow * 0.025f,
				0.0f,
				0.54f);
			const float Luminance = FMath::Clamp(
				0.52f + Rim * 0.16f + Highlight * 0.22f - LowerShadow * 0.12f,
				0.28f,
				0.90f);
			Pixels[Column + Row * TextureSize] = FLinearColor(
				Luminance * 0.90f,
				Luminance * 0.96f,
				Luminance,
				Alpha).ToFColorSRGB();
		}
	}

	const FName TextureName = MakeUniqueObjectName(
		GetTransientPackage(),
		UTexture2D::StaticClass(),
		TEXT("CH03LensDroplet"));
	LensDropletTexture = UTexture2D::CreateTransient(
		TextureSize,
		TextureSize,
		PF_B8G8R8A8,
		TextureName,
		PixelBytes);
	if (LensDropletTexture)
	{
		LensDropletTexture->Filter = TF_Bilinear;
		LensDropletTexture->AddressX = TA_Clamp;
		LensDropletTexture->AddressY = TA_Clamp;
		LensDropletTexture->NeverStream = true;
		LensDropletTexture->UpdateResource();
	}
}

UFontFace* AIGHorrorHUD::LoadBundledFontFace(
	const TCHAR* RelativePath,
	const TCHAR* FontFaceName)
{
	FString FontPath = FPaths::Combine(
		FPlatformProcess::BaseDir(),
		TEXT("UI/Fonts"),
		RelativePath);
	if (!FPaths::FileExists(FontPath))
	{
		// Editor targets keep authored binaries beside the module instead of
		// copying them into the project root. Shipping stages the same files
		// next to the executable through RuntimeDependencies.
		FontPath = FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Source/IndieGame/UI/Fonts"),
			RelativePath);
	}
	TArray<uint8> FontBytes;
	if (!FPaths::FileExists(FontPath)
		|| !FFileHelper::LoadFileToArray(FontBytes, *FontPath))
	{
		return nullptr;
	}

	UFontFace* FontFace = NewObject<UFontFace>(this, FontFaceName);
	FontFace->LoadingPolicy = EFontLoadingPolicy::Inline;
	FontFace->Hinting = EFontHinting::Default;
	FontFace->SourceFilename = FontPath;
	FontFace->FontFaceData = FFontFaceData::MakeFontFaceData(MoveTemp(FontBytes));
	return FontFace;
}

UFont* AIGHorrorHUD::MakeRuntimeFont(
	UFontFace* FontFace,
	const int32 PixelSize,
	const TCHAR* FontName)
{
	if (!FontFace)
	{
		return nullptr;
	}
	UFont* RuntimeFont = NewObject<UFont>(this, FontName);
	RuntimeFont->FontCacheType = EFontCacheType::Runtime;
	RuntimeFont->LegacyFontSize = PixelSize;
	FTypefaceEntry& TypefaceEntry = RuntimeFont->GetMutableInternalCompositeFont()
		.DefaultTypeface.Fonts.AddDefaulted_GetRef();
	TypefaceEntry.Name = TEXT("Regular");
	TypefaceEntry.Font = FFontData(FontFace);
	return RuntimeFont;
}

void AIGHorrorHUD::InitializeKoreanFont()
{
	KoreanBodyFontFace = LoadBundledFontFace(
		TEXT("Pretendard-Regular.otf"),
		TEXT("KoreanBodyFontFace"));
	KoreanEmphasisFontFace = LoadBundledFontFace(
		TEXT("Pretendard-SemiBold.otf"),
		TEXT("KoreanEmphasisFontFace"));
	KoreanDisplayFontFace = LoadBundledFontFace(
		TEXT("GowunBatang-Bold.ttf"),
		TEXT("KoreanDisplayFontFace"));
	if (KoreanBodyFontFace && KoreanEmphasisFontFace && KoreanDisplayFontFace)
	{
		KoreanFontLarge = MakeRuntimeFont(
			KoreanDisplayFontFace.Get(),
			IGHorrorHUD::LargeFontSize,
			TEXT("KoreanFontLarge"));
		KoreanFrontendTitleFont = MakeRuntimeFont(
			KoreanDisplayFontFace.Get(),
			IGHorrorHUD::FrontendTitleFontSize,
			TEXT("KoreanFrontendTitleFont"));
		KoreanFontMedium = MakeRuntimeFont(
			KoreanEmphasisFontFace.Get(),
			IGHorrorHUD::MediumFontSize,
			TEXT("KoreanFontMedium"));
		KoreanFontSmall = MakeRuntimeFont(
			KoreanBodyFontFace.Get(),
			IGHorrorHUD::SmallFontSize,
			TEXT("KoreanFontSmall"));
		KoreanPhoneMetaFont = MakeRuntimeFont(
			KoreanBodyFontFace.Get(),
			IGHorrorHUD::PhoneMetaFontSize,
			TEXT("KoreanPhoneMetaFont"));

		// Receipts deliberately retain a denser system face when available;
		// all navigational and story UI uses the deterministic bundled pair.
		KoreanReceiptFontFace = KoreanBodyFontFace;
		KoreanReceiptHeaderFont = MakeRuntimeFont(
			KoreanEmphasisFontFace.Get(), 21, TEXT("KoreanReceiptHeaderFont"));
		KoreanReceiptFont = MakeRuntimeFont(
			KoreanBodyFontFace.Get(), 12, TEXT("KoreanReceiptFont"));
		UE_LOG(LogIndieGame, Display, TEXT("Bundled Korean HUD type system loaded."));
		return;
	}

	// Development fallback only. A packaged build stages the bundled faces via
	// RuntimeDependencies, while this path keeps the HUD usable in a partial
	// source checkout and reports the missing asset in the log.
	FString FontsDirectory = FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR"));
	FontsDirectory = FontsDirectory.IsEmpty()
		? TEXT("C:/Windows/Fonts")
		: FPaths::Combine(FontsDirectory, TEXT("Fonts"));

	const TCHAR* CandidateFonts[] = {
		TEXT("malgun.ttf"),      // Malgun Gothic: ships with every Windows 10/11
		TEXT("malgunbd.ttf"),
		TEXT("NanumGothic.ttf"),
		TEXT("gulim.ttc"),
		TEXT("batang.ttc"),
	};

	for (const TCHAR* FontFileName : CandidateFonts)
	{
		const FString FontPath = FPaths::Combine(FontsDirectory, FontFileName);
		TArray<uint8> FontBytes;
		if (!FPaths::FileExists(FontPath) || !FFileHelper::LoadFileToArray(FontBytes, *FontPath))
		{
			continue;
		}

		UFontFace* FontFace = NewObject<UFontFace>(this, TEXT("KoreanFontFace"));
		FontFace->LoadingPolicy = EFontLoadingPolicy::Inline;
		FontFace->Hinting = EFontHinting::Default;
		FontFace->SourceFilename = FontPath;
		FontFace->FontFaceData = FFontFaceData::MakeFontFaceData(MoveTemp(FontBytes));

		KoreanFontLarge = MakeRuntimeFont(
			FontFace, IGHorrorHUD::LargeFontSize, TEXT("KoreanFontLarge"));
		KoreanFrontendTitleFont = MakeRuntimeFont(
			FontFace,
			IGHorrorHUD::FrontendTitleFontSize,
			TEXT("KoreanFrontendTitleFont"));
		KoreanFontMedium = MakeRuntimeFont(
			FontFace, IGHorrorHUD::MediumFontSize, TEXT("KoreanFontMedium"));
		KoreanFontSmall = MakeRuntimeFont(
			FontFace, IGHorrorHUD::SmallFontSize, TEXT("KoreanFontSmall"));
		KoreanPhoneMetaFont = MakeRuntimeFont(
			FontFace,
			IGHorrorHUD::PhoneMetaFontSize,
			TEXT("KoreanPhoneMetaFont"));

		// Receipt printers use a compact, almost fixed-width bitmap face. Keep
		// the fallback UI on Malgun Gothic, but prefer the narrower Gulim/Dotum
		// family for the 80 mm thermal roll. Column positions are still
		// measured explicitly, so Korean and ASCII remain aligned at 720p.
		KoreanReceiptFontFace = FontFace;
		const TCHAR* ReceiptCandidateFonts[] = {
			TEXT("gulim.ttc"),
			TEXT("Dotum.ttc"),
		};
		for (const TCHAR* ReceiptFontFileName : ReceiptCandidateFonts)
		{
			const FString ReceiptFontPath =
				FPaths::Combine(FontsDirectory, ReceiptFontFileName);
			TArray<uint8> ReceiptFontBytes;
			if (!FPaths::FileExists(ReceiptFontPath)
				|| !FFileHelper::LoadFileToArray(ReceiptFontBytes, *ReceiptFontPath))
			{
				continue;
			}

			KoreanReceiptFontFace =
				NewObject<UFontFace>(this, TEXT("ReceiptFontFace"));
			KoreanReceiptFontFace->LoadingPolicy = EFontLoadingPolicy::Inline;
			KoreanReceiptFontFace->Hinting = EFontHinting::Monochrome;
			KoreanReceiptFontFace->SourceFilename = ReceiptFontPath;
			KoreanReceiptFontFace->FontFaceData =
				FFontFaceData::MakeFontFaceData(MoveTemp(ReceiptFontBytes));
			break;
		}

		KoreanReceiptHeaderFont = MakeRuntimeFont(
			KoreanReceiptFontFace.Get(), 21, TEXT("KoreanReceiptHeaderFont"));
		KoreanReceiptFont = MakeRuntimeFont(
			KoreanReceiptFontFace.Get(), 12, TEXT("KoreanReceiptFont"));

		UE_LOG(LogIndieGame, Display, TEXT("HUD Korean font loaded: %s"), *FontPath);
		return;
	}

	UE_LOG(
		LogIndieGame,
		Warning,
		TEXT("No Korean-capable system font found; HUD falls back to ASCII strings."));
}

UFont* AIGHorrorHUD::GetFontForRole(const EIGHudTextRole TextRole) const
{
	if (SupportsKorean())
	{
		switch (TextRole)
		{
		case EIGHudTextRole::Objective:
			return KoreanFontLarge.Get();
		case EIGHudTextRole::Prompt:
			return KoreanFontMedium.Get();
		case EIGHudTextRole::Thought:
		case EIGHudTextRole::Dialogue:
		case EIGHudTextRole::Speaker:
		case EIGHudTextRole::Hint:
		default:
			return KoreanFontSmall.Get();
		}
	}

	if (!GEngine)
	{
		return nullptr;
	}
	return TextRole == EIGHudTextRole::Hint
		|| TextRole == EIGHudTextRole::Speaker
		? GEngine->GetSmallFont()
		: GEngine->GetMediumFont();
}

void AIGHorrorHUD::PushThought(
	const UObject* WorldContext,
	const FText& Thought,
	const float DurationSeconds)
{
	const UWorld* World = GEngine && WorldContext
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (AIGHorrorHUD* HorrorHUD = PlayerController
		? Cast<AIGHorrorHUD>(PlayerController->GetHUD())
		: nullptr)
	{
		HorrorHUD->ShowDialogue(
			FText::GetEmpty(),
			Thought,
			EIGDialogueChannel::InnerVoice,
			DurationSeconds,
			EIGDialoguePriority::Story);
	}
}

void AIGHorrorHUD::ShowThought(const FText& Thought, const float DurationSeconds)
{
	ShowDialogue(
		FText::GetEmpty(),
		Thought,
		EIGDialogueChannel::InnerVoice,
		DurationSeconds,
		EIGDialoguePriority::Story);
}

void AIGHorrorHUD::PushDialogue(
	const UObject* WorldContext,
	const FText& Speaker,
	const FText& Line,
	const EIGDialogueChannel Channel,
	const float MinimumDurationSeconds,
	const EIGDialoguePriority Priority)
{
	const UWorld* World = GEngine && WorldContext
		? GEngine->GetWorldFromContextObject(
			WorldContext,
			EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const APlayerController* PlayerController =
		World ? World->GetFirstPlayerController() : nullptr;
	if (AIGHorrorHUD* HorrorHUD = PlayerController
		? Cast<AIGHorrorHUD>(PlayerController->GetHUD())
		: nullptr)
	{
		HorrorHUD->ShowDialogue(
			Speaker,
			Line,
			Channel,
			MinimumDurationSeconds,
			Priority);
	}
}

void AIGHorrorHUD::ShowDialogue(
	const FText& Speaker,
	const FText& Line,
	const EIGDialogueChannel Channel,
	const float MinimumDurationSeconds,
	const EIGDialoguePriority Priority)
{
	const UWorld* World = GetWorld();
	if (!World || Line.IsEmpty())
	{
		return;
	}

	if (Channel == EIGDialogueChannel::VoiceSubtitle)
	{
		const UGameInstance* GameInstance = World->GetGameInstance();
		const UIGAccessibilitySubsystem* Accessibility = GameInstance
			? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
			: nullptr;
		if (!Accessibility || !Accessibility->AreSubtitlesEnabled())
		{
			return;
		}
	}

	FIGDialogueMessage Message;
	Message.Speaker = Speaker;
	Message.Line = Line;
	Message.Channel = Channel;
	Message.Priority = Priority;
	Message.MinimumDurationSeconds = FMath::Max(0.0f, MinimumDurationSeconds);
	const double CurrentTime = World->GetTimeSeconds();
	Message.QueuedAt = CurrentTime;
	EnqueueDialogue(MoveTemp(Message), CurrentTime);
}

float AIGHorrorHUD::CalculateDialogueDuration(
	const FString& Line,
	const float MinimumDurationSeconds) const
{
	int32 VisibleGlyphs = 0;
	for (const TCHAR Character : Line)
	{
		if (!FChar::IsWhitespace(Character))
		{
			++VisibleGlyphs;
		}
	}
	const float ReadingDuration = FMath::Clamp(
		1.15f + VisibleGlyphs / IGHorrorHUD::DialogueGlyphsPerSecond,
		IGHorrorHUD::DialogueMinimumSeconds,
		IGHorrorHUD::DialogueMaximumSeconds);
	return FMath::Max(ReadingDuration, MinimumDurationSeconds);
}

void AIGHorrorHUD::ActivateDialogue(
	FIGDialogueMessage&& Message,
	const double CurrentTime)
{
	CurrentDialogue = MoveTemp(Message);
	bHasCurrentDialogue = true;
	bCurrentDialogueHasContinuation = false;
	CurrentDialogueLines.Reset();
	DialogueLayoutScale = -1.0f;
	DialogueLayoutWidth = -1.0f;
	DialogueLayoutMaximumLines = 0;
	DialogueStartTime = CurrentTime;
	DialogueEndTime = CurrentTime + CalculateDialogueDuration(
		CurrentDialogue.Line.ToString(),
		CurrentDialogue.MinimumDurationSeconds);
}

void AIGHorrorHUD::EnqueueDialogue(
	FIGDialogueMessage&& Message,
	const double CurrentTime)
{
	const FString IncomingLine = Message.Line.ToString();
	const FString IncomingSpeaker = Message.Speaker.ToString();
	const EIGDialogueChannel IncomingChannel = Message.Channel;
	const auto IsSameMessage = [
		&IncomingLine,
		&IncomingSpeaker,
		IncomingChannel](
		const FIGDialogueMessage& Candidate)
	{
		return Candidate.Channel == IncomingChannel
			&& Candidate.Line.ToString().Equals(IncomingLine)
			&& Candidate.Speaker.ToString().Equals(IncomingSpeaker);
	};

	if (bHasCurrentDialogue
		&& CurrentDialogue.Channel == Message.Channel
		&& IsSameMessage(CurrentDialogue))
	{
		DialogueEndTime = FMath::Max(
			DialogueEndTime,
			CurrentTime + CalculateDialogueDuration(
				IncomingLine,
				Message.MinimumDurationSeconds));
		return;
	}
	for (FIGDialogueMessage& Queued : DialogueQueue)
	{
		if (Queued.Channel == Message.Channel && IsSameMessage(Queued))
		{
			Queued.MinimumDurationSeconds = FMath::Max(
				Queued.MinimumDurationSeconds,
				Message.MinimumDurationSeconds);
			Queued.QueuedAt = CurrentTime;
			return;
		}
	}

	if (!bHasCurrentDialogue)
	{
		ActivateDialogue(MoveTemp(Message), CurrentTime);
		return;
	}

	if (static_cast<uint8>(Message.Priority)
		> static_cast<uint8>(CurrentDialogue.Priority))
	{
		CurrentDialogue.MinimumDurationSeconds = FMath::Max(
			0.8f,
			static_cast<float>(DialogueEndTime - CurrentTime));
		CurrentDialogue.QueuedAt = CurrentTime;
		DialogueQueue.Insert(MoveTemp(CurrentDialogue), 0);
		if (DialogueQueue.Num() > IGHorrorHUD::MaximumDialogueQueueDepth)
		{
			DialogueQueue.RemoveAt(DialogueQueue.Num() - 1);
		}
		ActivateDialogue(MoveTemp(Message), CurrentTime);
		return;
	}

	if (DialogueQueue.Num() >= IGHorrorHUD::MaximumDialogueQueueDepth)
	{
		int32 RemovalIndex = INDEX_NONE;
		for (int32 Index = DialogueQueue.Num() - 1; Index >= 0; --Index)
		{
			if (static_cast<uint8>(DialogueQueue[Index].Priority)
				<= static_cast<uint8>(Message.Priority))
			{
				RemovalIndex = Index;
				break;
			}
		}
		if (RemovalIndex == INDEX_NONE)
		{
			return;
		}
		DialogueQueue.RemoveAt(RemovalIndex);
	}
	DialogueQueue.Add(MoveTemp(Message));
}

void AIGHorrorHUD::AdvanceDialogueQueue(const double CurrentTime)
{
	if (bHasCurrentDialogue && CurrentTime < DialogueEndTime)
	{
		return;
	}
	bHasCurrentDialogue = false;
	CurrentDialogueLines.Reset();

	while (!DialogueQueue.IsEmpty())
	{
		FIGDialogueMessage Next = MoveTemp(DialogueQueue[0]);
		DialogueQueue.RemoveAt(0);
		const double MaximumAge = Next.Priority == EIGDialoguePriority::Ambient
			? IGHorrorHUD::AmbientDialogueMaximumQueueAge
			: IGHorrorHUD::StoryDialogueMaximumQueueAge;
		if (!Next.bContinuation && CurrentTime - Next.QueuedAt > MaximumAge)
		{
			continue;
		}
		ActivateDialogue(MoveTemp(Next), CurrentTime);
		return;
	}
}

void AIGHorrorHUD::SuspendDialoguePresentation(const double CurrentTime)
{
	if (bHasCurrentDialogue && DialogueOccludedAt < 0.0)
	{
		DialogueOccludedAt = CurrentTime;
	}
}

void AIGHorrorHUD::ResumeDialoguePresentation(const double CurrentTime)
{
	if (DialogueOccludedAt < 0.0)
	{
		return;
	}
	const double OccludedDuration = FMath::Max(0.0, CurrentTime - DialogueOccludedAt);
	DialogueStartTime += OccludedDuration;
	DialogueEndTime += OccludedDuration;
	for (FIGDialogueMessage& Queued : DialogueQueue)
	{
		Queued.QueuedAt += OccludedDuration;
	}
	DialogueOccludedAt = -1.0;
}

namespace
{
	// §10.5. 고도가 먼저다. 훅 자체가 「위에서 나는 소리」라서, 위아래가
	// 조금이라도 서면 좌우보다 그쪽을 말해야 한다.
	constexpr float SoundBearingElevationDegrees = 25.0f;
	// 화면 안이라고 볼 각. 이 안쪽은 눈이 이미 알고 있으므로 적지 않는다.
	constexpr float SoundBearingOnScreenDegrees = 50.0f;
	constexpr float SoundBearingBehindDegrees = 130.0f;
	constexpr float SoundBearingMinimumDistance = 40.0f;
}

FText AIGHorrorHUD::MakeSoundBearingTag(
	const UObject* WorldContext,
	const FVector& SourceLocation)
{
	const UWorld* World = GEngine && WorldContext
		? GEngine->GetWorldFromContextObject(
			WorldContext,
			EGetWorldErrorMode::ReturnNull)
		: nullptr;
	APlayerController* PlayerController =
		World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		return FText::GetEmpty();
	}
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector ToSource = SourceLocation - ViewLocation;
	if (ToSource.SizeSquared()
		< SoundBearingMinimumDistance * SoundBearingMinimumDistance)
	{
		// 발밑에서 난 소리에 방위를 붙이면 고개만 돌려도 딱지가 뒤집힌다.
		return FText::GetEmpty();
	}

	const float Elevation = FMath::RadiansToDegrees(
		FMath::Asin(FMath::Clamp(
			ToSource.GetSafeNormal().Z, -1.0f, 1.0f)));
	if (FMath::Abs(Elevation) >= SoundBearingElevationDegrees)
	{
		return Elevation > 0.0f
			? NSLOCTEXT("IGHUD", "SoundBearingAbove", "위")
			: NSLOCTEXT("IGHUD", "SoundBearingBelow", "아래");
	}

	const float RelativeYaw = FMath::FindDeltaAngleDegrees(
		ViewRotation.Yaw,
		ToSource.Rotation().Yaw);
	const float AbsoluteYaw = FMath::Abs(RelativeYaw);
	if (AbsoluteYaw <= SoundBearingOnScreenDegrees)
	{
		return FText::GetEmpty();
	}
	if (AbsoluteYaw >= SoundBearingBehindDegrees)
	{
		return NSLOCTEXT("IGHUD", "SoundBearingBehind", "뒤");
	}
	return RelativeYaw > 0.0f
		? NSLOCTEXT("IGHUD", "SoundBearingRight", "오른쪽")
		: NSLOCTEXT("IGHUD", "SoundBearingLeft", "왼쪽");
}

void AIGHorrorHUD::PushAudioCaptionAt(
	const UObject* WorldContext,
	const FText& Caption,
	const float DurationSeconds,
	const FVector& SourceLocation)
{
	const FText Bearing = MakeSoundBearingTag(WorldContext, SourceLocation);
	if (Bearing.IsEmpty())
	{
		PushAudioCaption(WorldContext, Caption, DurationSeconds);
		return;
	}
	PushAudioCaption(
		WorldContext,
		FText::Format(
			NSLOCTEXT("IGHUD", "SoundCaptionWithBearing", "[{0}] {1}"),
			Bearing,
			Caption),
		DurationSeconds);
}

void AIGHorrorHUD::PushAudioCaption(
	const UObject* WorldContext,
	const FText& Caption,
	const float DurationSeconds)
{
	const UWorld* World = GEngine && WorldContext
		? GEngine->GetWorldFromContextObject(
			WorldContext,
			EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	if (!Accessibility || !Accessibility->AreSoundCaptionsEnabled())
	{
		return;
	}

	const APlayerController* PlayerController =
		World ? World->GetFirstPlayerController() : nullptr;
	if (AIGHorrorHUD* HorrorHUD = PlayerController
		? Cast<AIGHorrorHUD>(PlayerController->GetHUD())
		: nullptr)
	{
		HorrorHUD->ShowAudioCaption(Caption, DurationSeconds);
	}
}

void AIGHorrorHUD::ShowAudioCaption(
	const FText& Caption,
	const float DurationSeconds)
{
	const UWorld* World = GetWorld();
	if (!World || Caption.IsEmpty())
	{
		return;
	}
	const double CurrentTime = World->GetTimeSeconds();
	const float ClampedDuration = FMath::Max(0.8f, DurationSeconds);
	if (!CurrentAudioCaption.IsEmpty()
		&& CurrentTime < AudioCaptionEndTime
		&& CurrentAudioCaption.ToString().Equals(Caption.ToString()))
	{
		AudioCaptionEndTime = FMath::Max(
			AudioCaptionEndTime,
			CurrentTime + ClampedDuration);
		return;
	}

	FIGAudioCaptionMessage Message;
	Message.Caption = Caption;
	Message.DurationSeconds = ClampedDuration;
	Message.QueuedAt = CurrentTime;
	if (CurrentAudioCaption.IsEmpty() || CurrentTime >= AudioCaptionEndTime)
	{
		ActivateAudioCaption(MoveTemp(Message), CurrentTime);
		return;
	}
	for (FIGAudioCaptionMessage& Queued : AudioCaptionQueue)
	{
		if (Queued.Caption.ToString().Equals(Caption.ToString()))
		{
			Queued.DurationSeconds = FMath::Max(
				Queued.DurationSeconds,
				ClampedDuration);
			Queued.QueuedAt = CurrentTime;
			return;
		}
	}
	if (AudioCaptionQueue.Num() >= IGHorrorHUD::MaximumAudioCaptionQueueDepth)
	{
		AudioCaptionQueue.RemoveAt(0);
	}
	AudioCaptionQueue.Add(MoveTemp(Message));
}

void AIGHorrorHUD::ActivateAudioCaption(
	FIGAudioCaptionMessage&& Message,
	const double CurrentTime)
{
	CurrentAudioCaption = MoveTemp(Message.Caption);
	AudioCaptionStartTime = CurrentTime;
	AudioCaptionEndTime = CurrentTime + FMath::Max(0.8f, Message.DurationSeconds);
}

void AIGHorrorHUD::AdvanceAudioCaptionQueue(const double CurrentTime)
{
	if (!CurrentAudioCaption.IsEmpty() && CurrentTime < AudioCaptionEndTime)
	{
		return;
	}
	CurrentAudioCaption = FText::GetEmpty();
	while (!AudioCaptionQueue.IsEmpty())
	{
		FIGAudioCaptionMessage Next = MoveTemp(AudioCaptionQueue[0]);
		AudioCaptionQueue.RemoveAt(0);
		// A caption that would be badly detached from its sound is safer to drop
		// than to present as a false current event.
		if (CurrentTime - Next.QueuedAt > 3.0)
		{
			continue;
		}
		ActivateAudioCaption(MoveTemp(Next), CurrentTime);
		return;
	}
}

void AIGHorrorHUD::PushFearDirection(
	const UObject* WorldContext,
	const FVector& WorldLocation,
	const float DurationSeconds)
{
	const UWorld* World = GEngine && WorldContext
		? GEngine->GetWorldFromContextObject(
			WorldContext,
			EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	if (!Accessibility || !Accessibility->UsesDirectionalFearCues())
	{
		return;
	}

	const APlayerController* PlayerController =
		World ? World->GetFirstPlayerController() : nullptr;
	if (AIGHorrorHUD* HorrorHUD = PlayerController
		? Cast<AIGHorrorHUD>(PlayerController->GetHUD())
		: nullptr)
	{
		HorrorHUD->ShowFearDirection(WorldLocation, DurationSeconds);
	}
}

void AIGHorrorHUD::PushLensDroplet(
	const UObject* WorldContext,
	const float DurationSeconds)
{
	const UWorld* World = GEngine && WorldContext
		? GEngine->GetWorldFromContextObject(
			WorldContext,
			EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const APlayerController* PlayerController =
		World ? World->GetFirstPlayerController() : nullptr;
	if (AIGHorrorHUD* HorrorHUD = PlayerController
		? Cast<AIGHorrorHUD>(PlayerController->GetHUD())
		: nullptr)
	{
		HorrorHUD->ShowLensDroplet(DurationSeconds);
	}
}

void AIGHorrorHUD::ShowLensDroplet(const float DurationSeconds)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!LensDropletTexture)
	{
		InitializeLensDropletTexture();
	}

	LensDropletStartTime = World->GetTimeSeconds();
	LensDropletEndTime = LensDropletStartTime + FMath::Max(1.6f, DurationSeconds);
}

bool AIGHorrorHUD::GetLensDropletRenderSample(
	FVector2D& OutPosition,
	FVector2D& OutSize,
	FVector2D& OutCanvasSize,
	float& OutAlpha,
	bool& bOutReducedMotion,
	double& OutWorldTime) const
{
	if (LensDropletLastRenderTime < 0.0
		|| LensDropletLastSize.X <= 0.0f
		|| LensDropletLastSize.Y <= 0.0f
		|| LensDropletLastCanvasSize.X <= 0.0f
		|| LensDropletLastCanvasSize.Y <= 0.0f)
	{
		return false;
	}
	OutPosition = LensDropletLastPosition;
	OutSize = LensDropletLastSize;
	OutCanvasSize = LensDropletLastCanvasSize;
	OutAlpha = LensDropletLastAlpha;
	bOutReducedMotion = bLensDropletLastReducedMotion;
	OutWorldTime = LensDropletLastRenderTime;
	return true;
}

void AIGHorrorHUD::ShowFearDirection(
	const FVector& WorldLocation,
	const float DurationSeconds)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FearCueWorldLocation = WorldLocation;
	FearCueStartTime = World->GetTimeSeconds();
	FearCueEndTime = FearCueStartTime + FMath::Max(0.35f, DurationSeconds);
}

void AIGHorrorHUD::SetAccessibilityMenuState(
	const bool bVisible,
	const int32 SelectedRow)
{
	bAccessibilityMenuVisible = bVisible;
	AccessibilitySelectedRow = FMath::Clamp(SelectedRow, 0, 16);
}

void AIGHorrorHUD::SetSystemMenuState(
	const FIGSystemMenuPresentation& Presentation)
{
	const bool bPresentationChanged = Presentation.bVisible
		&& (!bSystemMenuVisible
			|| bSystemMenuIsTitle != Presentation.bTitle
			|| bSystemMenuIsCredits != Presentation.bCredits
			|| bSystemMenuIsContentNotice != Presentation.bContentNotice
			|| bSystemMenuIsKeyBindings != Presentation.bKeyBindings
			|| SystemMenuKeyBindingSelection != Presentation.KeyBindingSelection
			|| bSystemMenuKeyBindingCapturing != Presentation.bKeyBindingCapturing
			|| bSystemMenuKeyBindingColumnGamepad
				!= Presentation.bKeyBindingColumnGamepad
			|| bSystemMenuIsAudioCalibration != Presentation.bAudioCalibration
			|| bSystemMenuIsDisplaySettings != Presentation.bDisplaySettings
			|| bSystemMenuUseTitleBackdrop != Presentation.bUseTitleBackdrop);
	if (bPresentationChanged)
	{
		SystemMenuOpenedAt = FPlatformTime::Seconds();
	}
	else if (!Presentation.bVisible)
	{
		SystemMenuOpenedAt = -1.0;
	}
	bSystemMenuVisible = Presentation.bVisible;
	bSystemMenuIsTitle = Presentation.bTitle;
	bSystemMenuUseTitleBackdrop = Presentation.bUseTitleBackdrop;
	bSystemMenuIsCredits = Presentation.bCredits;
	bSystemMenuIsContentNotice = Presentation.bContentNotice;
	bSystemMenuIsKeyBindings = Presentation.bKeyBindings;
	SystemMenuKeyBindingSelection = Presentation.KeyBindingSelection;
	bSystemMenuKeyBindingCapturing = Presentation.bKeyBindingCapturing;
	bSystemMenuKeyBindingColumnGamepad = Presentation.bKeyBindingColumnGamepad;
	SystemMenuKeyBindingStatus = Presentation.KeyBindingStatus;
	bSystemMenuKeyBindingStatusIsError = Presentation.bKeyBindingStatusIsError;
	bSystemMenuIsAudioCalibration = Presentation.bAudioCalibration;
	bSystemMenuIsDisplaySettings = Presentation.bDisplaySettings;
	SystemMenuSelectedRow = FMath::Clamp(
		Presentation.SelectedRow,
		0,
		IGFrontendMenuLayout::ActionCount - 1);
	bSystemMenuCanContinue = Presentation.bCanContinue;
	bSystemMenuNightFiveAvailable = Presentation.bNightFiveAvailable;
	bSystemMenuNightFiveSpent = Presentation.bNightFiveSpent;
	bSystemMenuNightFivePlaying = Presentation.bNightFivePlaying;
	bSystemMenuConfirmNewGame = Presentation.bConfirmNewGame;
	bSystemMenuHeadphoneRecommendation =
		Presentation.bHeadphoneRecommendation;
	DisplaySettingsSelectedRow = FMath::Clamp(
		Presentation.DisplaySelectedRow,
		0,
		8);
	AudioCalibrationSelectedRow = FMath::Clamp(
		Presentation.AudioCalibrationSelectedRow,
		0,
		3);
	AudioCalibrationVolumeStep = FMath::Clamp(
		Presentation.AudioCalibrationVolumeStep,
		0,
		6);
	AudioCalibrationBrightnessStep = FMath::Clamp(
		Presentation.AudioCalibrationBrightnessStep,
		0,
		4);
	bSystemMenuAudioCalibrationFirstRun =
		Presentation.bAudioCalibrationFirstRun;
	bSystemMenuHeadphoneOutput = Presentation.bHeadphoneOutput;
	SystemMenuAudioCalibrationMusicStep = FMath::Clamp(
		Presentation.AudioCalibrationMusicStep, 0, 4);
	SystemMenuAudioCalibrationAmbienceStep = FMath::Clamp(
		Presentation.AudioCalibrationAmbienceStep, 0, 4);
	DisplayWindowModeIndex = FMath::Clamp(Presentation.WindowModeIndex, 0, 2);
	DisplayResolutionIndex = FMath::Clamp(Presentation.ResolutionIndex, 0, 2);
	DisplayQualityIndex = FMath::Clamp(Presentation.QualityIndex, 0, 1);
	DisplayFrameLimitIndex = FMath::Clamp(Presentation.FrameLimitIndex, 0, 2);
	bSystemMenuVSync = Presentation.bVSync;
	bDisplaySettingsApplied = Presentation.bDisplaySettingsApplied;
	bDisplaySettingsAwaitingConfirmation =
		Presentation.bDisplaySettingsAwaitingConfirmation;
	DisplayConfirmationSecondsRemaining =
		FMath::Max(0, Presentation.ConfirmationSecondsRemaining);
	SystemMenuStatusText = Presentation.StatusText;
	bSystemMenuStatusIsError = Presentation.bStatusIsError;
}

void AIGHorrorHUD::SetMissingFloorJournalState(
	const bool bVisible,
	const int32 PageIndex)
{
	bMissingFloorJournalVisible = bVisible;
	MissingFloorJournalPageIndex = FMath::Clamp(
		PageIndex,
		0,
		FMath::Max(0, GetMissingFloorJournalPageCount() - 1));
}

int32 AIGHorrorHUD::GetMissingFloorJournalPageCount() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UIGMissingFloorNarrativeSubsystem* Narrative = GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
	if (!Narrative)
	{
		return 1;
	}

	TSet<FName> ObservedSources;
	for (const FIGMissingFloorTruthRecord& Record :
		Narrative->GetSnapshot().Truths)
	{
		ObservedSources.Append(Record.SourceIds);
	}

	int32 Counts[3] = {0, 0, 0};
	for (const IGHorrorHUD::FJournalEntryDefinition& Entry :
		IGHorrorHUD::JournalEntries())
	{
		if (ObservedSources.Contains(Entry.SourceId))
		{
			++Counts[static_cast<int32>(Entry.Lane)];
		}
	}
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const float UserTextScale = Accessibility
		? Accessibility->GetCaptionSizeScale()
		: 1.0f;
	const int32 CardsPerLanePerPage = UserTextScale > 1.50f
		? 1
		: UserTextScale > 1.15f ? 2 : 3;
	return FMath::Max(
		1,
		FMath::Max3(
			FMath::DivideAndRoundUp(Counts[0], CardsPerLanePerPage),
			FMath::DivideAndRoundUp(Counts[1], CardsPerLanePerPage),
			FMath::DivideAndRoundUp(Counts[2], CardsPerLanePerPage)));
}

void AIGHorrorHUD::ShowChapterCard(
	const UObject* WorldContext,
	const FText& Eyebrow,
	const FText& Title,
	const FText& Subtitle,
	const float DurationSeconds)
{
	const UWorld* World = GEngine && WorldContext
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (AIGHorrorHUD* HorrorHUD = PlayerController
		? Cast<AIGHorrorHUD>(PlayerController->GetHUD())
		: nullptr)
	{
		HorrorHUD->PresentChapterCard(Eyebrow, Title, Subtitle, DurationSeconds);
	}
}

void AIGHorrorHUD::PresentChapterCard(
	const FText& Eyebrow,
	const FText& Title,
	const FText& Subtitle,
	const float DurationSeconds)
{
	const UWorld* World = GetWorld();
	if (!World || Title.IsEmpty())
	{
		return;
	}

	ChapterCardEyebrow = Eyebrow;
	ChapterCardTitle = Title;
	ChapterCardSubtitle = Subtitle;
	ChapterCardStartTime = World->GetTimeSeconds();
	ChapterCardEndTime = ChapterCardStartTime + FMath::Max(1.8f, DurationSeconds);
}

void AIGHorrorHUD::BeginMissingFloorFailureEnding(
	const float InitialElapsedSeconds)
{
	bMissingFloorFailureEndingVisible = true;
	bMissingFloorFailureRetryEnabled = false;
	MissingFloorFailureEndingStartedAt = GetWorld()
		? GetWorld()->GetTimeSeconds()
			- FMath::Max(InitialElapsedSeconds, 0.0f)
		: 0.0;
}

void AIGHorrorHUD::EndMissingFloorFailureEnding()
{
	bMissingFloorFailureEndingVisible = false;
	bMissingFloorFailureRetryEnabled = false;
	MissingFloorFailureEndingStartedAt = 0.0;
}

void AIGHorrorHUD::SetObjectiveProvider(UObject* InObjectiveProvider)
{
	ObjectiveProvider = IsValid(InObjectiveProvider)
		&& InObjectiveProvider->GetClass()->ImplementsInterface(
			UIGObjectiveProvider::StaticClass())
		? InObjectiveProvider
		: nullptr;
}

void AIGHorrorHUD::SetMorningDirector(AIGMorningRoutineDirector* InMorningDirector)
{
	SetObjectiveProvider(InMorningDirector);
}

void AIGHorrorHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !GEngine)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
#if !UE_BUILD_SHIPPING
	if (bFirstPersonKnockPreview
		&& CurrentTime >= FirstPersonKnockPreviewNextTime)
	{
		PlayFirstPersonKnock();
		FirstPersonKnockPreviewNextTime = CurrentTime + 0.62;
	}
	if (bCaptureEmbracePreview
		&& CurrentTime >= CaptureEmbracePreviewNextTime)
	{
		PlayCaptureEmbrace(
			static_cast<float>(IGHorrorHUD::CaptureEmbraceDurationSeconds));
		CaptureEmbracePreviewNextTime = CurrentTime + 1.8;
	}
	if (bCaptureWakeEchoPreview
		&& CurrentTime >= CaptureWakeEchoPreviewNextTime)
	{
		PlayCaptureWakeEcho(1, 0.68f);
		CaptureWakeEchoPreviewNextTime = CurrentTime + 1.45;
	}
#endif
	BeginLayoutValidationSample();
	if (DrawCaptureEmbrace(CurrentTime))
	{
		// 포획은 실패 UI가 아니다. 카메라가 완전히 어두워질 때까지 화면을
		// 점유해 프롬프트가 포옹을 게임 오버처럼 보이게 만들지 않도록 한다.
		SuspendDialoguePresentation(CurrentTime);
		LastHudDrawTime = CurrentTime;
		FinalizeLayoutValidationSample();
		return;
	}
	if (DrawCaptureWakeEcho(CurrentTime))
	{
		// 기상 잔상도 세계 안의 사건이다. 입력이 돌아오기 전에 목표나
		// 상호작용 문구가 먼저 나타나 기억 효과를 설명하지 않도록 한다.
		SuspendDialoguePresentation(CurrentTime);
		LastHudDrawTime = CurrentTime;
		FinalizeLayoutValidationSample();
		return;
	}
	if (bAccessibilityMenuVisible)
	{
		SuspendDialoguePresentation(CurrentTime);
		DrawAccessibilityPanel();
		FinalizeLayoutValidationSample();
		return;
	}
	if (bSystemMenuVisible)
	{
		SuspendDialoguePresentation(CurrentTime);
		DrawSystemMenuPanel();
		FinalizeLayoutValidationSample();
		return;
	}
	if (bMissingFloorJournalVisible)
	{
		SuspendDialoguePresentation(CurrentTime);
		DrawMissingFloorJournalPanel();
		FinalizeLayoutValidationSample();
		return;
	}
	if (DrawMissingFloorEpilogue(CurrentTime))
	{
		// 막간과 같은 규칙이다. 화면은 에필로그가 통째로 갖되, 환경음 자막은
		// 그리는 것을 허락한다 — 몽타주 구간에서는 그것이 유일한 그림이다.
		SuspendDialoguePresentation(CurrentTime);
		DrawAudioCaption(CurrentTime, Canvas->ClipY - 24.0f);
		LastHudDrawTime = CurrentTime;
		FinalizeLayoutValidationSample();
		return;
	}
	if (DrawMissingFloorFailureEnding(CurrentTime))
	{
		SuspendDialoguePresentation(CurrentTime);
		LastHudDrawTime = CurrentTime;
		FinalizeLayoutValidationSample();
		return;
	}

	if (DrawChapterCard(CurrentTime))
	{
		SuspendDialoguePresentation(CurrentTime);
		LastHudDrawTime = CurrentTime;
		FinalizeLayoutValidationSample();
		return;
	}
	ResumeDialoguePresentation(CurrentTime);
	if (bSensoryInterludePresentation)
	{
		// Camera fade supplies the black image. The HUD contributes only the
		// optional directional sound caption; no crosshair or objective can
		// make the scene read as ordinary gameplay with a missing render.
		SuspendDialoguePresentation(CurrentTime);
		DrawSensoryInterludeSkip();
		DrawAudioCaption(CurrentTime, Canvas->ClipY - 24.0f);
		LastHudDrawTime = CurrentTime;
		FinalizeLayoutValidationSample();
		return;
	}
	// The droplet belongs to the camera lens, while prompts and captions remain
	// optically crisp on top of it. Draw it before every native HUD element.
	DrawLensDroplet(CurrentTime);

	if (!InteractionComponent.IsValid())
	{
		ResolveInteractionComponent();
	}

	if ((!WakeDirector.IsValid() || !ObjectiveProvider.IsValid())
		&& CurrentTime >= NextDirectorSearchTime)
	{
		ResolveDirectors();
		NextDirectorSearchTime = CurrentTime + IGHorrorHUD::DirectorSearchInterval;
	}

	const UIGInteractionComponent* Interaction = InteractionComponent.Get();
	const bool bHasFocus = Interaction && IsValid(Interaction->GetFocusedActor());

	const float DeltaSeconds = LastHudDrawTime > 0.0
		? FMath::Clamp(static_cast<float>(CurrentTime - LastHudDrawTime), 0.0f, 0.2f)
		: 0.0f;
	LastHudDrawTime = CurrentTime;

	UpdateFocusBracket(bHasFocus ? Interaction->GetFocusedActor() : nullptr, DeltaSeconds);
	DrawFirstPersonKnock(CurrentTime);
	DrawCrosshair(bHasFocus ? IGHorrorHUD::RedAccent : IGHorrorHUD::PaleGray);
	const float HoldProgress = Interaction ? Interaction->GetHoldProgress() : 0.0f;
	if (FocusBracketAlpha > 0.01f)
	{
		DrawFocusBracket(
			IGHorrorHUD::RedAccent,
			HoldProgress);
	}
	DrawFearDirection(CurrentTime);
	DrawNoiseRipple(CurrentTime);

	// Objective line. The hour shows none: during 없는 층's night the player
	// is told nothing and has to listen instead (§11 V4).
	if (bNightPresentation)
	{
		// Intentionally empty: gated here rather than inside GetObjectiveText,
		// whose exact body the release gate pins.
	}
	else if (SupportsKorean())
	{
		const FText Objective = GetObjectiveText();
		if (!Objective.IsEmpty())
		{
			DrawCenteredText(Objective, 42.0f, IGHorrorHUD::PaleGray, EIGHudTextRole::Objective);
		}
	}
	else
	{
		const FString Objective = GetObjectiveTextAscii();
		if (!Objective.IsEmpty())
		{
			DrawCenteredText(
				FText::FromString(Objective), 42.0f, IGHorrorHUD::PaleGray,
				EIGHudTextRole::Objective);
		}
	}

	// A note being read owns the screen: no crosshair chatter under the paper.
	if (AIGReadableNote::GetOpenNote())
	{
		SuspendDialoguePresentation(CurrentTime);
		DrawNotePanel();
		DrawAudioCaption(CurrentTime, Canvas->ClipY - 24.0f);
		FinalizeLayoutValidationSample();
		return;
	}
	ResumeDialoguePresentation(CurrentTime);

	// Focused interaction prompt and hold progress.
	if (bHasFocus)
	{
		const FText FocusedPrompt = Interaction->GetFocusedPrompt();
		if (!FocusedPrompt.IsEmpty())
		{
			const AActor* FocusedActor = Interaction->GetFocusedActor();
			const bool bKnockVerb = FocusedActor
				&& FocusedActor->ActorHasTag(
					FName(TEXT("MissingFloor.Verb.Knock")));
			const bool bListenVerb = FocusedActor
				&& FocusedActor->ActorHasTag(
					FName(TEXT("MissingFloor.Verb.Listen")));
			const FText PromptFormat = bKnockVerb
				? bUsingGamepad
					? NSLOCTEXT("IGHUD", "KnockPromptFormatGamepad", "[ B ]  {0}")
					: NSLOCTEXT("IGHUD", "KnockPromptFormatKeyboard", "[ Q ]  {0}")
				: bListenVerb
					? bUsingGamepad
						? NSLOCTEXT("IGHUD", "ListenPromptFormatGamepad", "[ RT ]  {0}")
						: NSLOCTEXT("IGHUD", "ListenPromptFormatKeyboard", "[ E ]  {0}")
					: bUsingGamepad
						? NSLOCTEXT("IGHUD", "PromptFormatGamepad", "[ A ]  {0}")
						: NSLOCTEXT("IGHUD", "PromptFormatKeyboard", "[ E ]  {0}");
			const FText Prompt = FText::Format(
				PromptFormat,
				FocusedPrompt);
			DrawCenteredText(
				Prompt,
				(Canvas->ClipY * 0.5f) + 54.0f,
				IGHorrorHUD::RedAccent,
				EIGHudTextRole::Prompt);
		}
	}

	float DialoguePanelTop = Canvas->ClipY;
	const bool bDialogueVisible = DrawDialoguePanel(CurrentTime, DialoguePanelTop);
	const float DialogueLaneGap = 14.0f * FMath::Clamp(
		Canvas->ClipY / 1080.0f,
		0.85f,
		2.0f);
	const bool bAudioCaptionVisible = DrawAudioCaption(
		CurrentTime,
		bDialogueVisible
			? DialoguePanelTop - DialogueLaneGap
			: Canvas->ClipY - 54.0f);

	// Control hints.
	if (!bDialogueVisible && !bAudioCaptionVisible)
	{
		const FText Hints = SupportsKorean()
		? bUsingGamepad
			? NSLOCTEXT(
				"IGHUD",
				"HintsGamepad",
				"LS 이동  ·  L3 달리기  ·  LB 점프  ·  R3 앉기  ·  A 상호작용  ·  B 두드리기  ·  X 손전등")
			: NSLOCTEXT(
				"IGHUD",
				"HintsKeyboard",
				"WASD 이동  ·  Shift 달리기  ·  Space 점프  ·  C 앉기  ·  E 상호작용  ·  Q 두드리기  ·  F 손전등")
		: FText::FromString(
			bUsingGamepad
				? TEXT("LS MOVE  |  L3 SPRINT  |  LB JUMP  |  R3 CROUCH  |  A INTERACT  |  B KNOCK  |  X FLASHLIGHT")
				: TEXT("WASD MOVE  |  SHIFT SPRINT  |  SPACE JUMP  |  C CROUCH  |  E INTERACT  |  Q KNOCK  |  F FLASHLIGHT"));
		DrawCenteredText(
			Hints,
			FMath::Max(0.0f, Canvas->ClipY - 34.0f),
			IGHorrorHUD::MutedGray,
			EIGHudTextRole::Hint);
	}
	FinalizeLayoutValidationSample();
}

bool AIGHorrorHUD::GetLayoutValidationSample(
	FVector2D& OutCanvasSize,
	FVector2D& OutBoundsMin,
	FVector2D& OutBoundsMax,
	int32& OutElementCount,
	bool& bOutAllInsideCanvas,
	bool& bOutAllInsideSettingsContainers,
	uint64& OutFrameSerial) const
{
	if (!bLayoutValidationEnabled || !bLayoutValidationSampleReady)
	{
		return false;
	}
	OutCanvasSize = LayoutValidationCanvasSize;
	OutBoundsMin = LayoutValidationBoundsMin;
	OutBoundsMax = LayoutValidationBoundsMax;
	OutElementCount = LayoutValidationElementCount;
	bOutAllInsideCanvas = bLayoutValidationAllInsideCanvas;
	bOutAllInsideSettingsContainers =
		bLayoutValidationAllInsideSettingsContainers;
	OutFrameSerial = LayoutValidationFrameSerial;
	return true;
}

bool AIGHorrorHUD::GetDialogueRenderSample(
	FVector2D& OutPanelMinimum,
	FVector2D& OutPanelMaximum,
	FVector2D& OutCanvasSize,
	int32& OutLineCount,
	bool& bOutSpeakerVisible,
	bool& bOutHasContinuation,
	bool& bOutInsideSafeArea,
	uint64& OutFrameSerial) const
{
	if (DialogueLastRenderSerial == 0
		|| DialogueLastLineCount <= 0
		|| DialogueLastPanelMaximum.X <= DialogueLastPanelMinimum.X
		|| DialogueLastPanelMaximum.Y <= DialogueLastPanelMinimum.Y)
	{
		return false;
	}
	OutPanelMinimum = DialogueLastPanelMinimum;
	OutPanelMaximum = DialogueLastPanelMaximum;
	OutCanvasSize = DialogueLastCanvasSize;
	OutLineCount = DialogueLastLineCount;
	bOutSpeakerVisible = bDialogueLastSpeakerVisible;
	bOutHasContinuation = bDialogueLastHasContinuation;
	bOutInsideSafeArea = bDialogueLastInsideSafeArea;
	OutFrameSerial = DialogueLastRenderSerial;
	return true;
}

void AIGHorrorHUD::BeginLayoutValidationSample()
{
	if (!bLayoutValidationEnabled)
	{
		return;
	}
	LayoutValidationCanvasSize = FVector2D(Canvas->ClipX, Canvas->ClipY);
	LayoutValidationBoundsMin = FVector2D(
		TNumericLimits<float>::Max(),
		TNumericLimits<float>::Max());
	LayoutValidationBoundsMax = FVector2D(
		TNumericLimits<float>::Lowest(),
		TNumericLimits<float>::Lowest());
	LayoutValidationElementCount = 0;
	bLayoutValidationAllInsideCanvas = true;
	bLayoutValidationAllInsideSettingsContainers = true;
	bLayoutValidationSampleReady = false;
}

void AIGHorrorHUD::RecordLayoutValidationRect(
	const FVector2D& Minimum,
	const FVector2D& Maximum)
{
	if (!bLayoutValidationEnabled || !Canvas)
	{
		return;
	}
	LayoutValidationBoundsMin.X = FMath::Min(
		LayoutValidationBoundsMin.X,
		Minimum.X);
	LayoutValidationBoundsMin.Y = FMath::Min(
		LayoutValidationBoundsMin.Y,
		Minimum.Y);
	LayoutValidationBoundsMax.X = FMath::Max(
		LayoutValidationBoundsMax.X,
		Maximum.X);
	LayoutValidationBoundsMax.Y = FMath::Max(
		LayoutValidationBoundsMax.Y,
		Maximum.Y);
	++LayoutValidationElementCount;
	constexpr float PixelTolerance = 1.5f;
	bLayoutValidationAllInsideCanvas =
		bLayoutValidationAllInsideCanvas
		&& Minimum.X >= -PixelTolerance
		&& Minimum.Y >= -PixelTolerance
		&& Maximum.X <= Canvas->ClipX + PixelTolerance
		&& Maximum.Y <= Canvas->ClipY + PixelTolerance;
}

void AIGHorrorHUD::FinalizeLayoutValidationSample()
{
	if (!bLayoutValidationEnabled)
	{
		return;
	}
	if (LayoutValidationElementCount <= 0)
	{
		LayoutValidationBoundsMin = FVector2D::ZeroVector;
		LayoutValidationBoundsMax = FVector2D::ZeroVector;
		bLayoutValidationAllInsideCanvas = false;
	}
	++LayoutValidationFrameSerial;
	bLayoutValidationSampleReady = true;
}

float AIGHorrorHUD::GetResolutionTextScale(const float UserScale) const
{
	if (!Canvas)
	{
		return FMath::Clamp(UserScale, 0.85f, 2.0f);
	}
	const float ResolutionScale = FMath::Clamp(
		Canvas->ClipY / 1080.0f,
		0.85f,
		2.0f);
	return FMath::Clamp(UserScale, 0.85f, 2.0f) * ResolutionScale;
}

void AIGHorrorHUD::PrepareDialoguePage(
	const float TextScale,
	const float MaximumWidth,
	const int32 MaximumLines,
	const double CurrentTime)
{
	if (!bHasCurrentDialogue
		|| (CurrentDialogueLines.Num() > 0
			&& FMath::IsNearlyEqual(DialogueLayoutScale, TextScale, 0.01f)
			&& FMath::IsNearlyEqual(DialogueLayoutWidth, MaximumWidth, 1.0f)
			&& DialogueLayoutMaximumLines == MaximumLines))
	{
		return;
	}

	TArray<FString> Lines;
	FString Remainder;
	WrapHudText(
		CurrentDialogue.Line.ToString(),
		GetFontForRole(EIGHudTextRole::Dialogue),
		TextScale,
		MaximumWidth,
		MaximumLines,
		Lines,
		Remainder);
	if (Lines.IsEmpty())
	{
		Lines.Add(CurrentDialogue.Line.ToString());
	}

	if (!Remainder.IsEmpty())
	{
		FIGDialogueMessage Continuation = CurrentDialogue;
		Continuation.Line = FText::FromString(Remainder);
		Continuation.MinimumDurationSeconds = 0.0f;
		Continuation.QueuedAt = CurrentTime;
		Continuation.bContinuation = true;
		DialogueQueue.Insert(MoveTemp(Continuation), 0);
		if (DialogueQueue.Num() > IGHorrorHUD::MaximumDialogueQueueDepth)
		{
			DialogueQueue.RemoveAt(DialogueQueue.Num() - 1);
		}

		CurrentDialogue.Line = FText::FromString(FString::Join(Lines, TEXT("\n")));
		bCurrentDialogueHasContinuation = true;
		DialogueStartTime = CurrentTime;
		DialogueEndTime = CurrentTime + CalculateDialogueDuration(
			CurrentDialogue.Line.ToString(),
			CurrentDialogue.MinimumDurationSeconds);
	}
	CurrentDialogueLines = MoveTemp(Lines);
	DialogueLayoutScale = TextScale;
	DialogueLayoutWidth = MaximumWidth;
	DialogueLayoutMaximumLines = MaximumLines;
}

void AIGHorrorHUD::DrawRoundedHudSurface(
	const FVector2D& Position,
	const FVector2D& Size,
	const float CornerRadius,
	const FLinearColor& Color) const
{
	if (!Canvas || Size.X <= 0.0f || Size.Y <= 0.0f || Color.A <= 0.001f)
	{
		return;
	}

	if (!HudRoundedMaskTexture || !HudRoundedMaskTexture->GetResource())
	{
		FCanvasTileItem Fallback(Position, Size, Color);
		Fallback.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Fallback);
		return;
	}

	const float Radius = FMath::Clamp(
		CornerRadius,
		1.0f,
		FMath::Min(Size.X, Size.Y) * 0.5f);
	const float XStops[] = {
		Position.X,
		Position.X + Radius,
		Position.X + Size.X - Radius,
		Position.X + Size.X,
	};
	const float YStops[] = {
		Position.Y,
		Position.Y + Radius,
		Position.Y + Size.Y - Radius,
		Position.Y + Size.Y,
	};
	constexpr float UvStops[] = {0.0f, 0.25f, 0.75f, 1.0f};
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Column = 0; Column < 3; ++Column)
		{
			const FVector2D TileSize(
				XStops[Column + 1] - XStops[Column],
				YStops[Row + 1] - YStops[Row]);
			if (TileSize.X <= 0.01f || TileSize.Y <= 0.01f)
			{
				continue;
			}
			FCanvasTileItem Tile(
				FVector2D(XStops[Column], YStops[Row]),
				HudRoundedMaskTexture->GetResource(),
				TileSize,
				FVector2D(UvStops[Column], UvStops[Row]),
				FVector2D(UvStops[Column + 1], UvStops[Row + 1]),
				Color);
			Tile.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Tile);
		}
	}
}

void AIGHorrorHUD::DrawSensoryInterludeSkip()
{
	if (!Canvas || !bSensoryInterludeSkipAvailable)
	{
		return;
	}

	const float Scale = FMath::Clamp(Canvas->ClipY / 1080.0f, 0.68f, 1.35f);
	const float SafeInset = FMath::Max(24.0f * Scale, Canvas->ClipX * 0.035f);
	const float PanelWidth = FMath::Min(338.0f * Scale, Canvas->ClipX - SafeInset * 2.0f);
	const float PanelHeight = 68.0f * Scale;
	const FVector2D PanelPosition(
		Canvas->ClipX - SafeInset - PanelWidth,
		SafeInset);
	DrawRoundedHudSurface(
		PanelPosition,
		FVector2D(PanelWidth, PanelHeight),
		12.0f * Scale,
		FLinearColor(0.012f, 0.016f, 0.015f, 0.78f));

	const FText Label = bSensoryInterludeSkipToggleMode
		? (bSensoryInterludeSkipInProgress
			? (bUsingGamepad
				? NSLOCTEXT("IGHUD", "FifthDawnSkipToggleCancelPad", "Y  다시 눌러 취소")
				: NSLOCTEXT("IGHUD", "FifthDawnSkipToggleCancel", "TAB  다시 눌러 취소"))
			: (bUsingGamepad
				? NSLOCTEXT("IGHUD", "FifthDawnSkipToggleStartPad", "Y  눌러 건너뛰기")
				: NSLOCTEXT("IGHUD", "FifthDawnSkipToggleStart", "TAB  눌러 건너뛰기")))
		: FText::Format(
			bUsingGamepad
				? NSLOCTEXT("IGHUD", "FifthDawnSkipHoldPad", "Y  {0}초 누르기 · 건너뛰기")
				: NSLOCTEXT("IGHUD", "FifthDawnSkipHold", "TAB  {0}초 누르기 · 건너뛰기"),
			FText::AsNumber(SensoryInterludeSkipHoldSeconds));
	const float TextScale = GetFittedTextScale(
		Label,
		EIGHudTextRole::Hint,
		0.82f * Scale,
		PanelWidth - 34.0f * Scale,
		0.58f * Scale);
	DrawLeftAlignedText(
		Label,
		PanelPosition + FVector2D(17.0f * Scale, 13.0f * Scale),
		FLinearColor(0.78f, 0.79f, 0.75f, 0.94f),
		EIGHudTextRole::Hint,
		TextScale);

	const FVector2D TrackPosition =
		PanelPosition + FVector2D(17.0f * Scale, PanelHeight - 15.0f * Scale);
	const FVector2D TrackSize(PanelWidth - 34.0f * Scale, 3.0f * Scale);
	DrawRoundedHudSurface(
		TrackPosition,
		TrackSize,
		1.5f * Scale,
		FLinearColor(0.25f, 0.27f, 0.25f, 0.72f));
	if (SensoryInterludeSkipProgress > 0.001f)
	{
		DrawRoundedHudSurface(
			TrackPosition,
			FVector2D(TrackSize.X * SensoryInterludeSkipProgress, TrackSize.Y),
			1.5f * Scale,
			FLinearColor(0.63f, 0.21f, 0.18f, 0.96f));
	}
}

void AIGHorrorHUD::BeginMissingFloorEpilogueScene(
	const EIGMissingFloorEpilogueScene Scene,
	const FText& Heading,
	const TArray<FText>& BodyLines,
	const FText& Footnote)
{
	MissingFloorEpilogueScene = Scene;
	MissingFloorEpilogueHeading = Heading;
	MissingFloorEpilogueBodyLines = BodyLines;
	MissingFloorEpilogueFootnote = Footnote;
	MissingFloorEpilogueSceneStartedAt = GetWorld()
		? GetWorld()->GetTimeSeconds()
		: 0.0;
}

void AIGHorrorHUD::EndMissingFloorEpilogue()
{
	MissingFloorEpilogueScene = EIGMissingFloorEpilogueScene::None;
	MissingFloorEpilogueHeading = FText::GetEmpty();
	MissingFloorEpilogueBodyLines.Reset();
	MissingFloorEpilogueFootnote = FText::GetEmpty();
	MissingFloorEpilogueSceneStartedAt = 0.0;
}

bool AIGHorrorHUD::DrawMissingFloorEpilogue(const double CurrentTime)
{
	if (!Canvas
		|| MissingFloorEpilogueScene == EIGMissingFloorEpilogueScene::None)
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const float UserTextScale = Accessibility
		? Accessibility->GetCaptionSizeScale()
		: 1.0f;
	const bool bReducedMotion = Accessibility
		&& Accessibility->IsReducedCameraMotionEnabled();
	const float Scale = FMath::Clamp(
		FMath::Min(Canvas->ClipY / 1080.0f, Canvas->ClipX / 1920.0f),
		0.62f,
		1.35f);
	const float TypeScale = Scale * FMath::Clamp(UserTextScale, 0.85f, 2.0f);
	const float Elapsed = FMath::Max(
		static_cast<float>(CurrentTime - MissingFloorEpilogueSceneStartedAt),
		0.0f);
	// 장면이 바뀌는 속도가 감정의 속도다. 모션 감소에서는 이동만 없애고
	// 페이드는 남긴다 — 컷으로 갈리면 몽타주가 점멸로 읽힌다.
	const float Entrance = IGHorrorHUD::SmoothStep01(Elapsed / 1.15f);

	FCanvasTileItem Blackout(
		FVector2D::ZeroVector,
		FVector2D(Canvas->ClipX, Canvas->ClipY),
		FLinearColor(0.004f, 0.005f, 0.005f, 1.0f));
	Canvas->DrawItem(Blackout);

	if (MissingFloorEpilogueScene == EIGMissingFloorEpilogueScene::Montage)
	{
		// 소리만 지나가는 구간이다. 자막을 켠 플레이어에게만 글자가 있다.
		DrawSensoryInterludeSkip();
		return true;
	}

	if (MissingFloorEpilogueScene == EIGMissingFloorEpilogueScene::Card)
	{
		// 마지막 한 줄. 다른 어떤 것도 같은 화면에 두지 않는다.
		const FText Card = MissingFloorEpilogueBodyLines.Num() > 0
			? MissingFloorEpilogueBodyLines[0]
			: FText::GetEmpty();
		// 마지막 카드에는 우회 안내도 두지 않는다. 여기까지 왔으면 남은
		// 것은 한 문장뿐이고, 그 옆에 버튼을 놓을 이유가 없다.
		if (!Card.IsEmpty())
		{
			DrawCenteredText(
				Card,
				Canvas->ClipY * 0.5f - 22.0f * TypeScale,
				FLinearColor(0.88f, 0.87f, 0.83f, Entrance),
				EIGHudTextRole::Prompt,
				1.06f * TypeScale);
			RecordLayoutValidationRect(
				FVector2D(Canvas->ClipX * 0.10f, Canvas->ClipY * 0.5f - 34.0f * TypeScale),
				FVector2D(Canvas->ClipX * 0.90f, Canvas->ClipY * 0.5f + 34.0f * TypeScale));
		}
		return true;
	}

	const float SafeInset = FMath::Max(30.0f * Scale, Canvas->ClipX * 0.072f);
	const float ContentLeft = SafeInset;
	const float ContentWidth = Canvas->ClipX - SafeInset * 2.0f;

	// 정지 화면은 있으면 얹고 없으면 넘어간다. 에필로그의 뜻은 문장에
	// 있으므로 텍스처가 아직 임포트되지 않은 빌드에서도 장면이 성립한다.
	UTexture2D* SceneTexture = nullptr;
	switch (MissingFloorEpilogueScene)
	{
	case EIGMissingFloorEpilogueScene::Workshop:
		SceneTexture = EpilogueWorkshopTexture;
		break;
	case EIGMissingFloorEpilogueScene::Autumn:
		SceneTexture = EpilogueAutumnTexture;
		break;
	case EIGMissingFloorEpilogueScene::ServiceBay:
		SceneTexture = EpilogueServiceBayTexture;
		break;
	default:
		break;
	}

	float PenY = Canvas->ClipY * 0.16f;
	if (SceneTexture && SceneTexture->GetResource())
	{
		// 판을 원본 비례로 맞춘다. 고정 높이를 쓰면 세로 사진이 가로로
		// 눌려 건물이 납작해지는데, 그건 이 화면에서 가장 눈에 띄는 거짓말이다.
		const float SourceWidth =
			FMath::Max(static_cast<float>(SceneTexture->GetSizeX()), 1.0f);
		const float SourceHeight =
			FMath::Max(static_cast<float>(SceneTexture->GetSizeY()), 1.0f);
		const float SourceAspect = SourceWidth / SourceHeight;
		const float MaximumHeight = FMath::Clamp(
			Canvas->ClipY * 0.46f,
			190.0f * Scale,
			560.0f * Scale);
		float PlateWidth = ContentWidth;
		float PlateHeight = PlateWidth / SourceAspect;
		if (PlateHeight > MaximumHeight)
		{
			PlateHeight = MaximumHeight;
			PlateWidth = PlateHeight * SourceAspect;
		}
		const float PlateLeft = ContentLeft + (ContentWidth - PlateWidth) * 0.5f;
		// 아주 느린 밀기. 사진이 아니라 기억이라는 신호이고, 모션 감소에서는
		// 그 이동만 0이 된다.
		const float Drift = bReducedMotion
			? 0.0f
			: (1.0f - Entrance) * 16.0f * Scale;
		FCanvasTileItem Plate(
			FVector2D(PlateLeft, PenY - Drift),
			SceneTexture->GetResource(),
			FVector2D(PlateWidth, PlateHeight),
			FVector2D(0.0f, 0.0f),
			FVector2D(1.0f, 1.0f),
			FLinearColor(0.82f, 0.83f, 0.80f, Entrance));
		Plate.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Plate);
		RecordLayoutValidationRect(
			FVector2D(PlateLeft, PenY - Drift),
			FVector2D(PlateLeft + PlateWidth, PenY - Drift + PlateHeight));
		PenY += PlateHeight + 40.0f * Scale;
	}
	else
	{
		PenY = Canvas->ClipY * 0.30f;
	}

	if (!MissingFloorEpilogueHeading.IsEmpty())
	{
		DrawLeftAlignedText(
			MissingFloorEpilogueHeading,
			FVector2D(ContentLeft, PenY),
			FLinearColor(0.56f, 0.55f, 0.51f, 0.92f * Entrance),
			EIGHudTextRole::Speaker,
			0.76f * TypeScale);
		PenY += 40.0f * FMath::Max(Scale, TypeScale * 0.72f);
	}

	const float LineStride = 42.0f * FMath::Max(Scale, TypeScale * 0.74f);
	for (const FText& Line : MissingFloorEpilogueBodyLines)
	{
		if (Line.IsEmpty())
		{
			PenY += LineStride * 0.55f;
			continue;
		}
		DrawLeftAlignedText(
			Line,
			FVector2D(ContentLeft, PenY),
			FLinearColor(0.88f, 0.87f, 0.83f, Entrance),
			EIGHudTextRole::Dialogue,
			0.90f * TypeScale);
		PenY += LineStride;
	}

	if (!MissingFloorEpilogueFootnote.IsEmpty())
	{
		PenY += 12.0f * Scale;
		DrawLeftAlignedText(
			MissingFloorEpilogueFootnote,
			FVector2D(ContentLeft, PenY),
			FLinearColor(0.62f, 0.61f, 0.57f, 0.90f * Entrance),
			EIGHudTextRole::Hint,
			0.74f * TypeScale);
		PenY += 34.0f * FMath::Max(Scale, TypeScale * 0.70f);
	}

	RecordLayoutValidationRect(
		FVector2D(ContentLeft, Canvas->ClipY * 0.16f),
		FVector2D(ContentLeft + ContentWidth, FMath::Min(PenY, Canvas->ClipY)));
	DrawSensoryInterludeSkip();
	return true;
}

bool AIGHorrorHUD::DrawMissingFloorFailureEnding(const double CurrentTime)
{
	if (!Canvas || !bMissingFloorFailureEndingVisible)
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const float UserTextScale = Accessibility
		? Accessibility->GetCaptionSizeScale()
		: 1.0f;
	const bool bReducedMotion = Accessibility
		&& Accessibility->IsReducedCameraMotionEnabled();
	const float Scale = FMath::Clamp(
		FMath::Min(Canvas->ClipY / 1080.0f, Canvas->ClipX / 1920.0f),
		0.62f,
		1.35f);
	const float TypeScale = Scale * FMath::Clamp(UserTextScale, 0.85f, 2.0f);
	const float Elapsed = FMath::Max(
		static_cast<float>(CurrentTime - MissingFloorFailureEndingStartedAt),
		0.0f);
	const float EntranceAlpha = bReducedMotion
		? 1.0f
		: IGHorrorHUD::SmoothStep01(Elapsed / 0.42f);
	const float ScrollAlpha = bReducedMotion
		? (Elapsed >= 3.1f ? 1.0f : 0.0f)
		: IGHorrorHUD::SmoothStep01((Elapsed - 2.55f) / 0.72f);

	FCanvasTileItem Blackout(
		FVector2D::ZeroVector,
		FVector2D(Canvas->ClipX, Canvas->ClipY),
		FLinearColor(0.002f, 0.003f, 0.003f, 1.0f));
	Canvas->DrawItem(Blackout);

	const float SafeInset = FMath::Max(18.0f * Scale, Canvas->ClipY * 0.035f);
	const FVector2D PanelSize(
		FMath::Min(760.0f * Scale, Canvas->ClipX - SafeInset * 2.0f),
		FMath::Min(900.0f * Scale, Canvas->ClipY - SafeInset * 2.0f));
	const FVector2D PanelPosition(
		(Canvas->ClipX - PanelSize.X) * 0.5f,
		(Canvas->ClipY - PanelSize.Y) * 0.5f);
	DrawRoundedHudSurface(
		PanelPosition + FVector2D(0.0f, 9.0f * Scale),
		PanelSize,
		24.0f * Scale,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.46f * EntranceAlpha));
	DrawRoundedHudSurface(
		PanelPosition,
		PanelSize,
		22.0f * Scale,
		FLinearColor(0.91f, 0.90f, 0.86f, EntranceAlpha));

	const float Padding = 34.0f * Scale;
	const float TopBarHeight = 68.0f * Scale;
	const float ContentLeft = PanelPosition.X + Padding;
	const float ContentWidth = PanelSize.X - Padding * 2.0f;
	DrawLeftAlignedText(
		NSLOCTEXT("IGHUD", "EndingCAppSection", "주거  ·  빌라"),
		FVector2D(ContentLeft, PanelPosition.Y + 22.0f * Scale),
		FLinearColor(0.20f, 0.22f, 0.20f, 0.88f * EntranceAlpha),
		EIGHudTextRole::Speaker,
		0.82f * TypeScale);
	DrawRoundedHudSurface(
		FVector2D(ContentLeft, PanelPosition.Y + TopBarHeight - 2.0f * Scale),
		FVector2D(ContentWidth, 1.0f * Scale),
		0.5f * Scale,
		FLinearColor(0.36f, 0.37f, 0.34f, 0.24f * EntranceAlpha));

	const float BodyTop = PanelPosition.Y + TopBarHeight + 18.0f * Scale;
	const float HeroHeight = FMath::Clamp(
		PanelSize.Y * 0.27f,
		150.0f * Scale,
		238.0f * Scale);
	const float HeroAlpha = EntranceAlpha * (1.0f - ScrollAlpha);
	if (HeroAlpha > 0.01f && FrontendTitleBackgroundTexture
		&& FrontendTitleBackgroundTexture->GetResource())
	{
		const float HeroTravel = bReducedMotion ? 0.0f : 24.0f * Scale * ScrollAlpha;
		FCanvasTileItem Hero(
			FVector2D(ContentLeft, BodyTop - HeroTravel),
			FrontendTitleBackgroundTexture->GetResource(),
			FVector2D(ContentWidth, HeroHeight),
			FVector2D(0.20f, 0.26f),
			FVector2D(1.00f, 0.74f),
			FLinearColor(0.92f, 0.94f, 0.91f, HeroAlpha));
		Hero.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Hero);
		RecordLayoutValidationRect(
			FVector2D(ContentLeft, BodyTop - HeroTravel),
			FVector2D(ContentLeft + ContentWidth, BodyTop - HeroTravel + HeroHeight));
		DrawRoundedHudSurface(
			FVector2D(ContentLeft + 14.0f * Scale, BodyTop + 14.0f * Scale - HeroTravel),
			FVector2D(72.0f * Scale, 31.0f * Scale),
			15.5f * Scale,
			FLinearColor(0.05f, 0.06f, 0.055f, 0.80f * HeroAlpha));
		DrawLeftAlignedText(
			NSLOCTEXT("IGHUD", "EndingCUnitChip", "403호"),
			FVector2D(
				ContentLeft + 29.0f * Scale,
				BodyTop + 20.0f * Scale - HeroTravel),
			FLinearColor(0.91f, 0.90f, 0.84f, HeroAlpha),
			EIGHudTextRole::Hint,
			0.66f * Scale);
	}

	const float ListingAlpha = EntranceAlpha * (1.0f - ScrollAlpha);
	if (ListingAlpha > 0.01f)
	{
		float PenY = BodyTop + HeroHeight + 28.0f * Scale;
		DrawLeftAlignedText(
			NSLOCTEXT("IGHUD", "EndingCListingTitle", "무영로 달빛빌라 403호"),
			FVector2D(ContentLeft, PenY),
			FLinearColor(0.095f, 0.105f, 0.095f, ListingAlpha),
			EIGHudTextRole::Prompt,
			0.98f * TypeScale);
		PenY += 45.0f * FMath::Max(Scale, TypeScale * 0.72f);
		DrawLeftAlignedText(
			NSLOCTEXT("IGHUD", "EndingCListingCopy", "채광 좋은 남향, 즉시 입주 가능"),
			FVector2D(ContentLeft, PenY),
			FLinearColor(0.25f, 0.27f, 0.24f, 0.88f * ListingAlpha),
			EIGHudTextRole::Dialogue,
			0.83f * TypeScale);
		PenY += 42.0f * FMath::Max(Scale, TypeScale * 0.72f);
		DrawLeftAlignedText(
			NSLOCTEXT("IGHUD", "EndingCListingMeta", "빌라  ·  4층  ·  남향"),
			FVector2D(ContentLeft, PenY),
			FLinearColor(0.38f, 0.40f, 0.37f, 0.78f * ListingAlpha),
			EIGHudTextRole::Hint,
			0.70f * TypeScale);
	}

	const float CommentAlpha = EntranceAlpha * ScrollAlpha;
	if (CommentAlpha > 0.01f)
	{
		const float MotionOffset = bReducedMotion
			? 0.0f
			: (1.0f - ScrollAlpha) * 28.0f * Scale;
		float PenY = BodyTop + MotionOffset;
		DrawLeftAlignedText(
			NSLOCTEXT("IGHUD", "EndingCScrolledTitle", "무영로 달빛빌라 403호"),
			FVector2D(ContentLeft, PenY),
			FLinearColor(0.095f, 0.105f, 0.095f, CommentAlpha),
			EIGHudTextRole::Prompt,
			0.90f * TypeScale);
		PenY += 48.0f * FMath::Max(Scale, TypeScale * 0.72f);
		DrawLeftAlignedText(
			NSLOCTEXT("IGHUD", "EndingCCommentHeading", "입주자 후기"),
			FVector2D(ContentLeft, PenY),
			FLinearColor(0.31f, 0.33f, 0.30f, 0.90f * CommentAlpha),
			EIGHudTextRole::Speaker,
			0.72f * TypeScale);
		PenY += 39.0f * FMath::Max(Scale, TypeScale * 0.72f);

		const float CommentHeight = FMath::Min(
			244.0f * Scale + 34.0f * FMath::Max(UserTextScale - 1.0f, 0.0f),
			PanelPosition.Y + PanelSize.Y - PenY - 118.0f * Scale);
		DrawRoundedHudSurface(
			FVector2D(ContentLeft, PenY),
			FVector2D(ContentWidth, CommentHeight),
			16.0f * Scale,
			FLinearColor(0.84f, 0.83f, 0.78f, 0.78f * CommentAlpha));
		RecordLayoutValidationRect(
			FVector2D(ContentLeft, PenY),
			FVector2D(ContentLeft + ContentWidth, PenY + CommentHeight));
		const float CommentInset = 24.0f * Scale;
		DrawLeftAlignedText(
			NSLOCTEXT("IGHUD", "EndingCCommentLineOne", "이 집 새벽에 노크 소리 나요."),
			FVector2D(ContentLeft + CommentInset, PenY + 27.0f * Scale),
			FLinearColor(0.11f, 0.12f, 0.105f, CommentAlpha),
			EIGHudTextRole::Dialogue,
			0.84f * TypeScale);
		DrawLeftAlignedText(
			NSLOCTEXT("IGHUD", "EndingCCommentLineTwo", "두 명이서 하는 것 같아요."),
			FVector2D(
				ContentLeft + CommentInset,
				PenY + 27.0f * Scale + 43.0f * FMath::Max(Scale, TypeScale * 0.72f)),
			FLinearColor(0.11f, 0.12f, 0.105f, CommentAlpha),
			EIGHudTextRole::Dialogue,
			0.84f * TypeScale);
		DrawLeftAlignedText(
			NSLOCTEXT("IGHUD", "EndingCCommentMeta", "방금 전  ·  조회 17"),
			FVector2D(
				ContentLeft + CommentInset,
				PenY + CommentHeight - 37.0f * Scale),
			FLinearColor(0.39f, 0.40f, 0.37f, 0.80f * CommentAlpha),
			EIGHudTextRole::Hint,
			0.66f * TypeScale);
	}

	if (bMissingFloorFailureRetryEnabled)
	{
		const float FooterHeight = 84.0f * Scale;
		const float FooterY = PanelPosition.Y + PanelSize.Y - FooterHeight;
		DrawRoundedHudSurface(
			FVector2D(PanelPosition.X + 2.0f * Scale, FooterY),
			FVector2D(PanelSize.X - 4.0f * Scale, FooterHeight - 2.0f * Scale),
			20.0f * Scale,
			FLinearColor(0.075f, 0.086f, 0.080f, 0.98f));
		RecordLayoutValidationRect(
			FVector2D(PanelPosition.X + 2.0f * Scale, FooterY),
			FVector2D(
				PanelPosition.X + PanelSize.X - 2.0f * Scale,
				FooterY + FooterHeight - 2.0f * Scale));
		const FText RetryText = bUsingGamepad
			? NSLOCTEXT("IGHUD", "EndingCRetryGamepad", "A  밤 4를 다시 시작할 수 있다")
			: NSLOCTEXT("IGHUD", "EndingCRetryKeyboard", "E  밤 4를 다시 시작할 수 있다");
		const float RetryScale = GetFittedTextScale(
			RetryText,
			EIGHudTextRole::Prompt,
			0.86f * TypeScale,
			ContentWidth,
			0.58f * Scale);
		const float RetryWidth = MeasureTextWidth(
			RetryText.ToString(),
			GetFontForRole(EIGHudTextRole::Prompt),
			RetryScale);
		DrawLeftAlignedText(
			RetryText,
			FVector2D(
				PanelPosition.X + (PanelSize.X - RetryWidth) * 0.5f,
				FooterY + 24.0f * Scale),
			FLinearColor(0.90f, 0.89f, 0.84f, 1.0f),
			EIGHudTextRole::Prompt,
			RetryScale);
	}

	RecordLayoutValidationRect(PanelPosition, PanelPosition + PanelSize);
	return true;
}

void AIGHorrorHUD::DrawDialogueFilm(
	const FVector2D& Position,
	const FVector2D& Size,
	const float CornerRadius,
	const float Alpha) const
{
	if (!Canvas || !DialogueFilmTexture || !DialogueFilmTexture->GetResource()
		|| Size.X <= 0.0f || Size.Y <= 0.0f || Alpha <= 0.001f)
	{
		return;
	}

	const float Radius = FMath::Clamp(
		CornerRadius,
		0.0f,
		FMath::Min(Size.X, Size.Y) * 0.5f);
	const auto DrawFilmRegion = [this, Position, Size, Alpha](
		const FVector2D& RegionPosition,
		const FVector2D& RegionSize)
	{
		if (RegionSize.X <= 0.01f || RegionSize.Y <= 0.01f)
		{
			return;
		}
		const FVector2D Uv0(
			(RegionPosition.X - Position.X) / Size.X,
			(RegionPosition.Y - Position.Y) / Size.Y);
		const FVector2D Uv1(
			(RegionPosition.X + RegionSize.X - Position.X) / Size.X,
			(RegionPosition.Y + RegionSize.Y - Position.Y) / Size.Y);
		FCanvasTileItem Grain(
			RegionPosition,
			DialogueFilmTexture->GetResource(),
			RegionSize,
			Uv0,
			Uv1,
			FLinearColor(0.78f, 0.86f, 0.82f, Alpha));
		Grain.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Grain);
	};

	// Three rectangles are the inexpensive equivalent of a rounded clip: the
	// curved corner squares remain owned by the 9-slice base surface.
	DrawFilmRegion(
		FVector2D(Position.X + Radius, Position.Y),
		FVector2D(FMath::Max(0.0f, Size.X - Radius * 2.0f), Size.Y));
	DrawFilmRegion(
		FVector2D(Position.X, Position.Y + Radius),
		FVector2D(Radius, FMath::Max(0.0f, Size.Y - Radius * 2.0f)));
	DrawFilmRegion(
		FVector2D(Position.X + Size.X - Radius, Position.Y + Radius),
		FVector2D(Radius, FMath::Max(0.0f, Size.Y - Radius * 2.0f)));
}

bool AIGHorrorHUD::DrawDialoguePanel(
	const double CurrentTime,
	float& OutPanelTop)
{
	OutPanelTop = Canvas ? Canvas->ClipY : 0.0f;
	if (!Canvas)
	{
		return false;
	}
	AdvanceDialogueQueue(CurrentTime);
	if (!bHasCurrentDialogue)
	{
		return false;
	}

	const UGameInstance* GameInstance = GetWorld()
		? GetWorld()->GetGameInstance()
		: nullptr;
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	while (bHasCurrentDialogue
		&& CurrentDialogue.Channel == EIGDialogueChannel::VoiceSubtitle
		&& (!Accessibility || !Accessibility->AreSubtitlesEnabled()))
	{
		DialogueEndTime = CurrentTime;
		AdvanceDialogueQueue(CurrentTime);
	}
	if (!bHasCurrentDialogue)
	{
		return false;
	}

	const FIGAccessibilitySettings Settings = Accessibility
		? Accessibility->GetSettings()
		: FIGAccessibilitySettings();
	const float ResolutionScale = FMath::Clamp(
		Canvas->ClipY / 1080.0f,
		0.85f,
		2.0f);
	const float TextScale = GetResolutionTextScale(Settings.CaptionSizeScale);
	const float SafeAreaScale = Settings.CaptionSafeAreaScale;
	const float SafeWidth = Canvas->ClipX * SafeAreaScale;
	const float PanelWidth = FMath::Min(
		FMath::Clamp(
			Canvas->ClipX * 0.56f,
			520.0f * ResolutionScale,
			840.0f * ResolutionScale),
		FMath::Max(280.0f, SafeWidth - 48.0f * ResolutionScale));
	const float HorizontalPadding = 30.0f * ResolutionScale;
	const float MaximumTextWidth = FMath::Max(
		220.0f,
		PanelWidth - HorizontalPadding * 2.0f);
	const int32 MaximumLines = Settings.CaptionSizeScale > 1.25f ? 3 : 2;
	PrepareDialoguePage(
		TextScale,
		MaximumTextWidth,
		MaximumLines,
		CurrentTime);
	if (CurrentDialogueLines.IsEmpty())
	{
		return false;
	}

	UFont* BodyFont = GetFontForRole(EIGHudTextRole::Dialogue);
	UFont* SpeakerFont = GetFontForRole(EIGHudTextRole::Speaker);
	float BodyRawWidth = 0.0f;
	float BodyRawHeight = 19.0f;
	if (BodyFont)
	{
		Canvas->StrLen(BodyFont, TEXT("한Ag"), BodyRawWidth, BodyRawHeight, true);
	}
	float SpeakerRawWidth = 0.0f;
	float SpeakerRawHeight = 14.0f;
	if (SpeakerFont)
	{
		Canvas->StrLen(
			SpeakerFont,
			TEXT("한Ag"),
			SpeakerRawWidth,
			SpeakerRawHeight,
			true);
	}
	const float BodyHeight = FMath::Max(16.0f, BodyRawHeight * TextScale);
	const float LineStep = BodyHeight * (
		CurrentDialogueLines.Num() >= 3 ? 1.36f : 1.32f);
	const bool bHasSpeaker = !CurrentDialogue.Speaker.IsEmpty();
	const float SpeakerScale = TextScale * 0.78f;
	const float SpeakerHeight = bHasSpeaker
		? FMath::Max(11.0f, SpeakerRawHeight * SpeakerScale)
		: 0.0f;
	const float SpeakerChipHeight = bHasSpeaker
		? FMath::Max(24.0f * ResolutionScale, SpeakerHeight + 10.0f * ResolutionScale)
		: 0.0f;
	const float HeaderGap = bHasSpeaker ? 10.0f * ResolutionScale : 0.0f;
	const float TopPadding = 14.0f * ResolutionScale;
	const float BottomPadding = (
		bCurrentDialogueHasContinuation ? 26.0f : 19.0f) * ResolutionScale;
	const float LinesHeight = BodyHeight
		+ LineStep * FMath::Max(0, CurrentDialogueLines.Num() - 1);
	const float PanelHeight = TopPadding + SpeakerChipHeight + HeaderGap
		+ LinesHeight + BottomPadding;
	const float SafeHorizontalInset = Canvas->ClipX * (1.0f - SafeAreaScale) * 0.5f;
	const float SafeVerticalInset = Canvas->ClipY * (1.0f - SafeAreaScale) * 0.5f;

	const double Elapsed = CurrentTime - DialogueStartTime;
	const double Remaining = DialogueEndTime - CurrentTime;
	const float FadeIn = IGHorrorHUD::SmoothStep01(
		static_cast<float>(Elapsed / 0.24));
	const float FadeOut = IGHorrorHUD::SmoothStep01(
		static_cast<float>(Remaining / 0.16));
	const float Alpha = FMath::Min(FadeIn, FadeOut);
	const bool bReducedMotion = Accessibility
		&& Accessibility->IsReducedCameraMotionEnabled();
	const float TravelY = bReducedMotion
		? 0.0f
		: (1.0f - FadeIn) * 10.0f * ResolutionScale;
	const float PanelX = (Canvas->ClipX - PanelWidth) * 0.5f;
	const float PanelY = FMath::Clamp(
		Canvas->ClipY - SafeVerticalInset - PanelHeight
			- 42.0f * ResolutionScale - TravelY,
		SafeVerticalInset + 24.0f * ResolutionScale,
		Canvas->ClipY - SafeVerticalInset - PanelHeight
			- 24.0f * ResolutionScale);
	OutPanelTop = PanelY;

	FLinearColor Accent = IGHorrorHUD::DialogueTeal;
	if (CurrentDialogue.Channel == EIGDialogueChannel::InnerVoice)
	{
		Accent = IGHorrorHUD::ThoughtBlue;
	}
	else if (CurrentDialogue.Channel == EIGDialogueChannel::Device)
	{
		Accent = FLinearColor(0.50f, 0.70f, 0.62f, 1.0f);
	}
	else if (CurrentDialogue.Priority == EIGDialoguePriority::Critical)
	{
		Accent = IGHorrorHUD::RedAccent;
	}
	Accent.A = Alpha;
	const float SurfaceAlpha = Settings.CaptionBackgroundOpacity * Alpha;
	const float CornerRadius = 10.0f * ResolutionScale;
	if (SurfaceAlpha > 0.001f)
	{
		DrawRoundedHudSurface(
			FVector2D(PanelX, PanelY + 8.0f * ResolutionScale),
			FVector2D(PanelWidth, PanelHeight),
			CornerRadius + 2.0f * ResolutionScale,
			FLinearColor(0.0f, 0.0f, 0.0f, SurfaceAlpha * 0.23f));
		DrawRoundedHudSurface(
			FVector2D(PanelX, PanelY + 3.0f * ResolutionScale),
			FVector2D(PanelWidth, PanelHeight),
			CornerRadius,
			FLinearColor(0.0f, 0.0f, 0.0f, SurfaceAlpha * 0.38f));
		FLinearColor BorderColor = Accent;
		BorderColor.A = SurfaceAlpha * 0.28f;
		DrawRoundedHudSurface(
			FVector2D(PanelX, PanelY),
			FVector2D(PanelWidth, PanelHeight),
			CornerRadius,
			BorderColor);
		const float BorderInset = FMath::Max(1.0f, ResolutionScale);
		DrawRoundedHudSurface(
			FVector2D(PanelX + BorderInset, PanelY + BorderInset),
			FVector2D(
				PanelWidth - BorderInset * 2.0f,
				PanelHeight - BorderInset * 2.0f),
			FMath::Max(2.0f, CornerRadius - BorderInset),
			FLinearColor(0.012f, 0.017f, 0.016f, SurfaceAlpha));
		DrawDialogueFilm(
			FVector2D(PanelX + BorderInset, PanelY + BorderInset),
			FVector2D(
				PanelWidth - BorderInset * 2.0f,
				PanelHeight - BorderInset * 2.0f),
			FMath::Max(2.0f, CornerRadius - BorderInset),
			SurfaceAlpha * 0.14f);
	}
	FLinearColor KeylineColor = Accent;
	KeylineColor.A = Alpha * 0.52f;
	FCanvasTileItem Keyline(
		FVector2D(PanelX + HorizontalPadding, PanelY),
		FVector2D(56.0f * ResolutionScale, FMath::Max(1.0f, ResolutionScale)),
		KeylineColor);
	Keyline.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Keyline);
	RecordLayoutValidationRect(
		FVector2D(PanelX, PanelY),
		FVector2D(PanelX + PanelWidth, PanelY + PanelHeight));

	float PenY = PanelY + TopPadding;
	const bool bUseTextOutline = Settings.CaptionBackgroundOpacity < 0.42f;
	if (bHasSpeaker)
	{
		const float SpeakerTextWidth = MeasureTextWidth(
			CurrentDialogue.Speaker.ToString(),
			SpeakerFont,
			SpeakerScale);
		const float SpeakerChipWidth = FMath::Min(
			PanelWidth - HorizontalPadding * 2.0f,
			SpeakerTextWidth + 20.0f * ResolutionScale);
		FLinearColor SpeakerChipColor = Accent;
		SpeakerChipColor.R *= 0.24f;
		SpeakerChipColor.G *= 0.24f;
		SpeakerChipColor.B *= 0.24f;
		SpeakerChipColor.A = Alpha
			* Settings.CaptionBackgroundOpacity
			* 0.34f;
		DrawRoundedHudSurface(
			FVector2D(PanelX + HorizontalPadding, PenY),
			FVector2D(SpeakerChipWidth, SpeakerChipHeight),
			SpeakerChipHeight * 0.5f,
			SpeakerChipColor);
		FLinearColor SpeakerColor = Accent;
		SpeakerColor.A = Alpha * 0.96f;
		DrawLeftAlignedText(
			CurrentDialogue.Speaker,
			FVector2D(
				PanelX + HorizontalPadding + 10.0f * ResolutionScale,
				PenY + (SpeakerChipHeight - SpeakerHeight) * 0.5f),
			SpeakerColor,
			EIGHudTextRole::Speaker,
			SpeakerScale,
			bUseTextOutline);
		PenY += SpeakerChipHeight + HeaderGap;
	}
	FLinearColor BodyColor = CurrentDialogue.Channel == EIGDialogueChannel::InnerVoice
		? IGHorrorHUD::ThoughtBlue
		: IGHorrorHUD::DialogueIvory;
	BodyColor.A = Alpha;
	for (int32 LineIndex = 0; LineIndex < CurrentDialogueLines.Num(); ++LineIndex)
	{
		DrawLeftAlignedText(
			FText::FromString(CurrentDialogueLines[LineIndex]),
			FVector2D(
				PanelX + HorizontalPadding,
				PenY + LineIndex * LineStep),
			BodyColor,
			EIGHudTextRole::Dialogue,
			TextScale,
			bUseTextOutline);
	}
	if (bCurrentDialogueHasContinuation)
	{
		const FText ContinuationLabel = NSLOCTEXT(
			"IGHorrorHUD",
			"DialogueContinues",
			"이어짐");
		const float ContinuationScale = TextScale * 0.72f;
		float ContinuationRawWidth = 0.0f;
		float ContinuationRawHeight = 0.0f;
		if (SpeakerFont)
		{
			Canvas->StrLen(
				SpeakerFont,
				ContinuationLabel.ToString(),
				ContinuationRawWidth,
				ContinuationRawHeight,
				true);
		}
		FLinearColor ContinuationColor = Accent;
		ContinuationColor.A *= 0.72f;
		const float ContinuationY = PanelY + PanelHeight
			- ContinuationRawHeight * ContinuationScale
			- 7.0f * ResolutionScale;
		FCanvasTileItem ContinuationRule(
			FVector2D(
				PanelX + PanelWidth - HorizontalPadding
					- ContinuationRawWidth * ContinuationScale
					- 16.0f * ResolutionScale,
				ContinuationY + ContinuationRawHeight * ContinuationScale * 0.52f),
			FVector2D(9.0f * ResolutionScale, FMath::Max(1.0f, ResolutionScale)),
			ContinuationColor);
		ContinuationRule.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(ContinuationRule);
		DrawLeftAlignedText(
			ContinuationLabel,
			FVector2D(
				PanelX + PanelWidth - HorizontalPadding
					- ContinuationRawWidth * ContinuationScale,
				ContinuationY),
			ContinuationColor,
			EIGHudTextRole::Speaker,
			ContinuationScale,
			bUseTextOutline);
	}

	DialogueLastPanelMinimum = FVector2D(PanelX, PanelY);
	DialogueLastPanelMaximum = FVector2D(PanelX + PanelWidth, PanelY + PanelHeight);
	DialogueLastCanvasSize = FVector2D(Canvas->ClipX, Canvas->ClipY);
	DialogueLastLineCount = CurrentDialogueLines.Num();
	bDialogueLastSpeakerVisible = bHasSpeaker;
	bDialogueLastHasContinuation = bCurrentDialogueHasContinuation;
	bDialogueLastInsideSafeArea =
		PanelX >= SafeHorizontalInset - 1.0f
		&& PanelX + PanelWidth <= Canvas->ClipX - SafeHorizontalInset + 1.0f
		&& PanelY >= SafeVerticalInset - 1.0f
		&& PanelY + PanelHeight <= Canvas->ClipY - SafeVerticalInset + 1.0f;
	++DialogueLastRenderSerial;
	return true;
}

bool AIGHorrorHUD::DrawAudioCaption(
	const double CurrentTime,
	const float MaximumBottomY)
{
	if (!Canvas)
	{
		return false;
	}
	AdvanceAudioCaptionQueue(CurrentTime);
	if (CurrentAudioCaption.IsEmpty() || CurrentTime >= AudioCaptionEndTime)
	{
		return false;
	}
	const UGameInstance* GameInstance = GetWorld()
		? GetWorld()->GetGameInstance()
		: nullptr;
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	if (!Accessibility || !Accessibility->AreSoundCaptionsEnabled())
	{
		return false;
	}
	const double Elapsed = CurrentTime - AudioCaptionStartTime;
	const double Remaining = AudioCaptionEndTime - CurrentTime;
	const float Alpha = FMath::Clamp(
		FMath::Min(
			static_cast<float>(Elapsed / 0.12),
			static_cast<float>(Remaining / 0.28)),
		0.0f,
		1.0f);
	const FIGAccessibilitySettings Settings = Accessibility->GetSettings();
	const float ResolutionScale = FMath::Clamp(
		Canvas->ClipY / 1080.0f,
		0.85f,
		2.0f);
	const float CaptionScale = GetResolutionTextScale(Settings.CaptionSizeScale) * 0.88f;
	const float SafeAreaScale = Settings.CaptionSafeAreaScale;
	const float SafeWidth = Canvas->ClipX * SafeAreaScale;
	const float MaximumPanelWidth = FMath::Min(
		FMath::Clamp(
			Canvas->ClipX * 0.42f,
			280.0f * ResolutionScale,
			680.0f * ResolutionScale),
		FMath::Max(240.0f, SafeWidth - 48.0f * ResolutionScale));
	const float HorizontalPadding = 18.0f * ResolutionScale;
	const float IconLaneWidth = 28.0f * ResolutionScale;
	const float MaximumTextWidth = FMath::Max(
		180.0f,
		MaximumPanelWidth - HorizontalPadding * 2.0f - IconLaneWidth);
	const int32 MaximumCaptionLines = Settings.CaptionSizeScale > 1.25f ? 3 : 2;
	FString DisplayCaption = CurrentAudioCaption.ToString().TrimStartAndEnd();
	// Authored captions keep square brackets in data for transcripts and
	// fallback surfaces. This lane already has a waveform glyph, so repeating
	// the same semantic marker on screen adds noise without adding meaning.
	if (DisplayCaption.Len() >= 2
		&& DisplayCaption[0] == TEXT('[')
		&& DisplayCaption[DisplayCaption.Len() - 1] == TEXT(']'))
	{
		DisplayCaption = DisplayCaption.Mid(1, DisplayCaption.Len() - 2)
			.TrimStartAndEnd();
	}
	TArray<FString> Lines;
	FString Remainder;
	WrapHudText(
		DisplayCaption,
		GetFontForRole(EIGHudTextRole::Dialogue),
		CaptionScale,
		MaximumTextWidth,
		MaximumCaptionLines,
		Lines,
		Remainder);
	if (Lines.IsEmpty())
	{
		return false;
	}
	if (!Remainder.IsEmpty())
	{
		FIGAudioCaptionMessage Continuation;
		Continuation.Caption = FText::FromString(Remainder);
		Continuation.DurationSeconds = FMath::Max(
			1.2f,
			static_cast<float>(AudioCaptionEndTime - CurrentTime));
		Continuation.QueuedAt = CurrentTime;
		AudioCaptionQueue.Insert(MoveTemp(Continuation), 0);
		if (AudioCaptionQueue.Num() > IGHorrorHUD::MaximumAudioCaptionQueueDepth)
		{
			AudioCaptionQueue.RemoveAt(AudioCaptionQueue.Num() - 1);
		}
		CurrentAudioCaption = FText::FromString(FString::Join(Lines, TEXT("\n")));
	}
	UFont* CaptionFont = GetFontForRole(EIGHudTextRole::Dialogue);
	float RawWidth = 0.0f;
	float RawHeight = 19.0f;
	if (CaptionFont)
	{
		Canvas->StrLen(CaptionFont, TEXT("한Ag"), RawWidth, RawHeight, true);
	}
	float LongestLineWidth = 0.0f;
	for (const FString& Line : Lines)
	{
		LongestLineWidth = FMath::Max(
			LongestLineWidth,
			MeasureTextWidth(Line, CaptionFont, CaptionScale));
	}
	const float PanelWidth = FMath::Min(
		MaximumPanelWidth,
		FMath::Max(
			280.0f * ResolutionScale,
			LongestLineWidth + HorizontalPadding * 2.0f + IconLaneWidth));
	const float BodyHeight = FMath::Max(16.0f, RawHeight * CaptionScale);
	const float LineStep = BodyHeight * (Lines.Num() >= 3 ? 1.34f : 1.29f);
	const float VerticalPadding = 11.0f * ResolutionScale;
	const float PanelHeight = VerticalPadding * 2.0f + BodyHeight
		+ LineStep * FMath::Max(0, Lines.Num() - 1);
	const float SafeVerticalInset = Canvas->ClipY * (1.0f - SafeAreaScale) * 0.5f;
	const float MinimumPanelY = SafeVerticalInset + 28.0f * ResolutionScale;
	const float PanelY = FMath::Max(
		MinimumPanelY,
		MaximumBottomY - PanelHeight);
	const float PanelX = (Canvas->ClipX - PanelWidth) * 0.5f;
	const float SurfaceAlpha = Settings.CaptionBackgroundOpacity * Alpha;
	const float CornerRadius = PanelHeight * 0.22f;
	DrawRoundedHudSurface(
		FVector2D(PanelX, PanelY + 4.0f * ResolutionScale),
		FVector2D(PanelWidth, PanelHeight),
		CornerRadius,
		FLinearColor(0.0f, 0.0f, 0.0f, SurfaceAlpha * 0.34f));
	DrawRoundedHudSurface(
		FVector2D(PanelX, PanelY),
		FVector2D(PanelWidth, PanelHeight),
		CornerRadius,
		FLinearColor(0.018f, 0.026f, 0.024f, SurfaceAlpha));
	DrawDialogueFilm(
		FVector2D(PanelX, PanelY),
		FVector2D(PanelWidth, PanelHeight),
		CornerRadius,
		SurfaceAlpha * 0.11f);
	RecordLayoutValidationRect(
		FVector2D(PanelX, PanelY),
		FVector2D(PanelX + PanelWidth, PanelY + PanelHeight));
	FLinearColor WaveColor = IGHorrorHUD::DialogueTeal;
	WaveColor.A = Alpha * 0.78f;
	const float WaveCenterY = PanelY + PanelHeight * 0.5f;
	const float WaveHeights[] = {5.0f, 11.0f, 16.0f, 8.0f};
	for (int32 BarIndex = 0; BarIndex < UE_ARRAY_COUNT(WaveHeights); ++BarIndex)
	{
		const float BarHeight = WaveHeights[BarIndex] * ResolutionScale;
		FCanvasTileItem WaveBar(
			FVector2D(
				PanelX + HorizontalPadding + BarIndex * 4.0f * ResolutionScale,
				WaveCenterY - BarHeight * 0.5f),
			FVector2D(FMath::Max(1.0f, 1.5f * ResolutionScale), BarHeight),
			WaveColor);
		WaveBar.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(WaveBar);
	}
	FLinearColor CaptionColor = FLinearColor(0.80f, 0.81f, 0.77f, 1.0f);
	CaptionColor.A = Alpha;
	const bool bUseTextOutline = Settings.CaptionBackgroundOpacity < 0.42f;
	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		DrawLeftAlignedText(
			FText::FromString(Lines[LineIndex]),
			FVector2D(
				PanelX + HorizontalPadding + IconLaneWidth,
				PanelY + VerticalPadding + LineIndex * LineStep),
			CaptionColor,
			EIGHudTextRole::Dialogue,
			CaptionScale,
			bUseTextOutline);
	}
	return true;
}

void AIGHorrorHUD::DrawLensDroplet(const double CurrentTime)
{
	if (!Canvas || !LensDropletTexture
		|| CurrentTime < LensDropletStartTime
		|| CurrentTime >= LensDropletEndTime)
	{
		return;
	}

	const double Duration = FMath::Max(
		LensDropletEndTime - LensDropletStartTime,
		0.001);
	const float Age = FMath::Clamp(
		static_cast<float>((CurrentTime - LensDropletStartTime) / Duration),
		0.0f,
		1.0f);
	const float FadeIn = IGHorrorHUD::SmoothStep01(Age / 0.12f);
	const float FadeOut = 1.0f - IGHorrorHUD::SmoothStep01((Age - 0.68f) / 0.32f);
	const float Alpha = FadeIn * FadeOut * 0.72f;

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const bool bReducedMotion = Accessibility
		&& Accessibility->IsReducedCameraMotionEnabled();
	const float Travel = bReducedMotion
		? 0.0f
		: IGHorrorHUD::SmoothStep01(Age) * Canvas->ClipY * 0.028f;

	const float DropHeight = FMath::Clamp(
		FMath::Min(Canvas->ClipX, Canvas->ClipY) * 0.16f,
		84.0f,
		176.0f);
	const FVector2D DropSize(DropHeight * 0.60f, DropHeight);
	const FVector2D DropPosition(
		Canvas->ClipX * 0.75f - DropSize.X * 0.5f,
		Canvas->ClipY * 0.14f + Travel);
	LensDropletLastPosition = DropPosition;
	LensDropletLastSize = DropSize;
	LensDropletLastCanvasSize = FVector2D(Canvas->ClipX, Canvas->ClipY);
	LensDropletLastAlpha = Alpha;
	bLensDropletLastReducedMotion = bReducedMotion;
	LensDropletLastRenderTime = CurrentTime;

	FCanvasTileItem Droplet(
		DropPosition,
		LensDropletTexture->GetResource(),
		DropSize,
		FLinearColor(0.76f, 0.82f, 0.84f, Alpha));
	Droplet.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Droplet);
}

void AIGHorrorHUD::ValidateSettingsTextRect(
	const FVector2D& Minimum,
	const FVector2D& Maximum,
	const FVector2D& ContainerMinimum,
	const FVector2D& ContainerMaximum)
{
	if (!bLayoutValidationEnabled)
	{
		return;
	}
	constexpr float PixelTolerance = 1.5f;
	bLayoutValidationAllInsideSettingsContainers =
		bLayoutValidationAllInsideSettingsContainers
		&& Minimum.X >= ContainerMinimum.X - PixelTolerance
		&& Minimum.Y >= ContainerMinimum.Y - PixelTolerance
		&& Maximum.X <= ContainerMaximum.X + PixelTolerance
		&& Maximum.Y <= ContainerMaximum.Y + PixelTolerance;
}

void AIGHorrorHUD::DrawFirstPersonKnock(const double CurrentTime)
{
	if (!Canvas
		|| FirstPersonKnockFrames.Num()
			!= IGHorrorHUD::FirstPersonKnockFrameCount)
	{
		return;
	}
	for (const UTexture2D* Frame : FirstPersonKnockFrames)
	{
		if (!Frame || !Frame->GetResource())
		{
			return;
		}
	}

	const UIGInteractionComponent* Interaction = InteractionComponent.Get();
	const AActor* FocusedActor = Interaction
		? Interaction->GetFocusedActor()
		: nullptr;
	const bool bKnockReady = IsValid(FocusedActor)
		&& FocusedActor->ActorHasTag(FName(TEXT("MissingFloor.Verb.Knock")));
	const float ActionAge = FirstPersonKnockStartTime >= 0.0
		? static_cast<float>(CurrentTime - FirstPersonKnockStartTime)
		: -1.0f;
	const bool bActionActive = ActionAge >= 0.0f
		&& ActionAge < IGHorrorHUD::FirstPersonKnockDurationSeconds;
	if (!bActionActive && !bKnockReady)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const bool bReducedMotion = Accessibility
		&& Accessibility->IsReducedCameraMotionEnabled();

	const float SpriteSize = FMath::Clamp(
		Canvas->ClipY * 2.0f,
		960.0f,
		2880.0f);
	const FVector2D DrawSize(SpriteSize, SpriteSize);
	const FVector2D DrawPosition(
		Canvas->ClipX * 0.5f - SpriteSize * 0.17f,
		Canvas->ClipY * 0.5f - SpriteSize * 0.17f);

	auto DrawFrame = [this, &DrawPosition, &DrawSize](
		const int32 FrameIndex,
		const float Alpha)
	{
		if (Alpha <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		FCanvasTileItem FrameTile(
			DrawPosition,
			FirstPersonKnockFrames[FrameIndex]->GetResource(),
			DrawSize,
			FLinearColor(0.84f, 0.87f, 0.90f, FMath::Clamp(Alpha, 0.0f, 1.0f)));
		FrameTile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(FrameTile);
	};

	if (!bActionActive)
	{
		// The player sees their raised hand before committing the noisy verb.
		// A very small 0<->1 blend keeps it alive without becoming a weapon idle.
		const float ReadyBlend = bReducedMotion
			? 0.0f
			: 0.07f + 0.05f * (
				0.5f + 0.5f * FMath::Sin(static_cast<float>(CurrentTime) * 2.1f));
		const float ReadyAlpha = 0.72f * FMath::Clamp(
			FocusBracketAlpha,
			0.0f,
			1.0f);
		DrawFrame(0, ReadyAlpha * (1.0f - ReadyBlend));
		DrawFrame(1, ReadyAlpha * ReadyBlend);
		return;
	}

	const float NormalizedAge = FMath::Clamp(
		ActionAge / static_cast<float>(IGHorrorHUD::FirstPersonKnockDurationSeconds),
		0.0f,
		1.0f);
	const float Visibility = bKnockReady
		? 0.96f
		: 0.96f * (1.0f - IGHorrorHUD::SmoothStep01(
			(NormalizedAge - 0.68f) / 0.32f));
	if (bReducedMotion)
	{
		// Keep the tactile replacement cue but remove apparent arm travel.
		DrawFrame(2, Visibility);
		return;
	}

	// Audio and noise are emitted on the input frame, so the contact pose is
	// frame zero here. The authored recoil then blends back to the ready hand.
	if (NormalizedAge < 0.42f)
	{
		const float Blend = IGHorrorHUD::SmoothStep01(NormalizedAge / 0.42f);
		DrawFrame(2, Visibility * (1.0f - Blend));
		DrawFrame(3, Visibility * Blend);
	}
	else
	{
		const float Blend = IGHorrorHUD::SmoothStep01(
			(NormalizedAge - 0.42f) / 0.58f);
		DrawFrame(3, Visibility * (1.0f - Blend));
		DrawFrame(0, Visibility * Blend);
	}
}

bool AIGHorrorHUD::DrawCaptureEmbrace(const double CurrentTime)
{
	if (!Canvas
		|| CaptureEmbraceStartTime < 0.0
		|| CurrentTime < CaptureEmbraceStartTime
		|| CurrentTime >= CaptureEmbraceEndTime)
	{
		return false;
	}

	const double Duration = FMath::Max(
		CaptureEmbraceEndTime - CaptureEmbraceStartTime,
		0.001);
	const float NormalizedAge = FMath::Clamp(
		static_cast<float>((CurrentTime - CaptureEmbraceStartTime) / Duration),
		0.0f,
		1.0f);
	// 꼬리 페이드를 두지 않는다. 카메라 암전이 같은 길이로 돌기 때문에 팔을
	// 0.82부터 지우면 아직 덜 검은 화면에서 팔이 먼저 증발하고, 마지막에
	// 남는 그림이 빈 복도가 된다. 끝까지 불투명하게 두면 화면이 완전히
	// 검어지는 프레임에 맞춰 그리기가 멎으므로 잘림이 보이지 않는다.
	const float Visibility = IGHorrorHUD::SmoothStep01(NormalizedAge / 0.08f);

	// 어떤 화면 비율에서도 정사각 원본을 늘리지 않는다. 두 소매가 화면
	// 바깥에서 시작하도록 의도적으로 오버스캔한다.
	const float SpriteSize = FMath::Max(Canvas->ClipX, Canvas->ClipY);
	const FVector2D DrawSize(SpriteSize, SpriteSize);
	const FVector2D DrawPosition(
		(Canvas->ClipX - SpriteSize) * 0.5f,
		(Canvas->ClipY - SpriteSize) * 0.5f);
	auto DrawFrame = [this, &DrawPosition, &DrawSize](
		const int32 FrameIndex,
		const float Alpha)
	{
		if (!CaptureEmbraceFrames.IsValidIndex(FrameIndex)
			|| !CaptureEmbraceFrames[FrameIndex]
			|| !CaptureEmbraceFrames[FrameIndex]->GetResource()
			|| Alpha <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		FCanvasTileItem FrameTile(
			DrawPosition,
			CaptureEmbraceFrames[FrameIndex]->GetResource(),
			DrawSize,
			FLinearColor(0.88f, 0.90f, 0.90f, FMath::Clamp(Alpha, 0.0f, 1.0f)));
		FrameTile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(FrameTile);
	};

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const bool bReducedMotion = Accessibility
		&& Accessibility->IsReducedCameraMotionEnabled();
	if (bReducedMotion)
	{
		// 촉각을 대신할 정보는 남기되 팔이 이동하는 느낌은 제거한다.
		// 손이 잘려 나간 칸은 무엇이 닿았는지를 말해 주지 못하므로,
		// 정지 화면으로 세울 칸은 손이 오므라든 마지막 포즈다.
		DrawFrame(0, Visibility * 0.94f);
		return true;
	}

	constexpr float ClosingAnimationEnd = 0.72f;
	const float FramePosition = FMath::Clamp(
		NormalizedAge / ClosingAnimationEnd,
		0.0f,
		1.0f) * IGHorrorHUD::CaptureEmbraceFrameCount;
	// 시트 순서대로 틀면 팔이 안으로 모이는 게 아니라 바깥으로 벌어져
	// 화면을 빠져나가고, 마지막 칸은 손이 잘려 나간 팔뚝 두 개다. 포옹은
	// 안으로 닫히는 동작이므로 뒤에서부터 튼다 — 팔이 옆에서 들어와 올라오고
	// 벌어졌다가 손이 오므라들며 끝난다. 붙잡고 있을 마지막 포즈도 손이
	// 있는 칸이 된다.
	const int32 SheetIndex = FMath::Clamp(
		FMath::FloorToInt(FramePosition),
		0,
		IGHorrorHUD::CaptureEmbraceFrameCount - 1);
	const int32 FrameIndex =
		IGHorrorHUD::CaptureEmbraceFrameCount - 1 - SheetIndex;
	// 포즈 사이 실루엣 차이가 커서 교차 페이드는 팔이 네 개로 보인다.
	// 장면과의 알파 블렌드는 유지하되 애니메이션 셀은 한 장씩 전환한다.
	DrawFrame(FrameIndex, Visibility);
	return true;
}

bool AIGHorrorHUD::DrawCaptureWakeEcho(const double CurrentTime)
{
	if (!Canvas
		|| CaptureWakeEchoStartTime < 0.0
		|| CurrentTime < CaptureWakeEchoStartTime
		|| CurrentTime >= CaptureWakeEchoEndTime)
	{
		return false;
	}

	// 잔상이 먼저 사라져도 카메라 페이드와 입력 잠금이 끝날 때까지 일반 HUD는
	// 되살리지 않는다. 검은 화면 위에 목표/프롬프트만 먼저 뜨는 것을 막는다.
	if (CurrentTime >= CaptureWakeEchoVisualEndTime)
	{
		return true;
	}

	const double VisualDuration = FMath::Max(
		CaptureWakeEchoVisualEndTime - CaptureWakeEchoStartTime,
		0.001);
	const float NormalizedAge = FMath::Clamp(
		static_cast<float>((CurrentTime - CaptureWakeEchoStartTime) / VisualDuration),
		0.0f,
		1.0f);
	const float FadeIn = IGHorrorHUD::SmoothStep01(NormalizedAge / 0.12f);
	const float FadeOut = 1.0f - IGHorrorHUD::SmoothStep01(
		(NormalizedAge - 0.12f) / 0.88f);
	const float RepeatAttenuation = FMath::Lerp(
		1.0f,
		0.68f,
		FMath::Clamp((CaptureWakeEchoCount - 1) / 4.0f, 0.0f, 1.0f));
	const float Visibility = FadeIn * FadeOut * 0.34f * RepeatAttenuation;

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const bool bReducedMotion = Accessibility
		&& Accessibility->IsReducedCameraMotionEnabled();
	int32 FrameIndex = 3;
	if (!bReducedMotion)
	{
		// 닫힌 포옹이 한 단계씩 풀리는 방향으로만 재생한다. 셀 사이
		// 교차 페이드는 추가 팔처럼 보이므로 포획 때와 같이 사용하지 않는다.
		if (NormalizedAge >= 0.66f)
		{
			FrameIndex = 1;
		}
		else if (NormalizedAge >= 0.33f)
		{
			FrameIndex = 2;
		}
	}

	if (!CaptureEmbraceFrames.IsValidIndex(FrameIndex)
		|| !CaptureEmbraceFrames[FrameIndex]
		|| !CaptureEmbraceFrames[FrameIndex]->GetResource())
	{
		// 잔상 에셋이 없어도 입력 복귀와 밤 루프는 계속 진행한다.
		return true;
	}

	const float SpriteSize = FMath::Max(Canvas->ClipX, Canvas->ClipY);
	const FVector2D DrawSize(SpriteSize, SpriteSize);
	const FVector2D DrawPosition(
		(Canvas->ClipX - SpriteSize) * 0.5f,
		(Canvas->ClipY - SpriteSize) * 0.5f);
	FCanvasTileItem FrameTile(
		DrawPosition,
		CaptureEmbraceFrames[FrameIndex]->GetResource(),
		DrawSize,
		FLinearColor(
			0.62f,
			0.68f,
			0.72f,
			FMath::Clamp(Visibility, 0.0f, 1.0f)));
	FrameTile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(FrameTile);
	return true;
}

float AIGHorrorHUD::MeasureTextWidth(
	const FString& Text,
	UFont* Font,
	const float TextScale) const
{
	if (!Canvas || !Font || Text.IsEmpty())
	{
		return 0.0f;
	}

	float Width = 0.0f;
	float Height = 0.0f;
	Canvas->StrLen(Font, Text, Width, Height, true);
	return Width * FMath::Max(0.5f, TextScale);
}

float AIGHorrorHUD::MeasureTextHeight(
	const FString& Text,
	UFont* Font,
	const float TextScale) const
{
	if (!Canvas || !Font || Text.IsEmpty())
	{
		return 0.0f;
	}
	float Width = 0.0f;
	float Height = 0.0f;
	Canvas->StrLen(Font, Text, Width, Height, true);
	return Height * FMath::Max(0.5f, TextScale);
}

float AIGHorrorHUD::GetFittedTextScale(
	const FText& Text,
	const EIGHudTextRole TextRole,
	const float PreferredScale,
	const float MaximumWidth,
	const float MinimumScale) const
{
	UFont* Font = GetFontForRole(TextRole);
	if (!Font || Text.IsEmpty() || MaximumWidth <= 0.0f)
	{
		return FMath::Max(0.5f, PreferredScale);
	}

	const float SafePreferredScale = FMath::Max(0.5f, PreferredScale);
	const float RawWidth = MeasureTextWidth(Text.ToString(), Font, 1.0f);
	if (RawWidth <= KINDA_SMALL_NUMBER)
	{
		return SafePreferredScale;
	}
	return FMath::Clamp(
		MaximumWidth / RawWidth,
		FMath::Max(0.5f, MinimumScale),
		SafePreferredScale);
}

int32 AIGHorrorHUD::FindFittingCaptionPrefix(
	const FString& Text,
	UFont* Font,
	const float TextScale,
	const float MaximumWidth) const
{
	if (Text.IsEmpty() || !Font || MaximumWidth <= 0.0f)
	{
		return 0;
	}
	if (MeasureTextWidth(Text, Font, TextScale) <= MaximumWidth)
	{
		return Text.Len();
	}

	int32 BestLength = 0;
	int32 Low = 1;
	int32 High = Text.Len();
	while (Low <= High)
	{
		const int32 CandidateLength = Low + ((High - Low) / 2);
		if (MeasureTextWidth(Text.Left(CandidateLength), Font, TextScale)
			<= MaximumWidth)
		{
			BestLength = CandidateLength;
			Low = CandidateLength + 1;
		}
		else
		{
			High = CandidateLength - 1;
		}
	}
	return FMath::Max(1, BestLength);
}

void AIGHorrorHUD::WrapHudText(
	const FString& Source,
	UFont* Font,
	const float TextScale,
	const float MaximumWidth,
	const int32 MaximumLines,
	TArray<FString>& OutLines,
	FString& OutRemainder) const
{
	OutLines.Reset();
	OutRemainder.Reset();
	if (!Font || MaximumWidth <= 0.0f || MaximumLines <= 0)
	{
		OutRemainder = Source;
		return;
	}

	FString Remaining = Source;
	Remaining.ReplaceInline(TEXT("\r"), TEXT(""));
	Remaining = Remaining.TrimStartAndEnd();
	while (!Remaining.IsEmpty() && OutLines.Num() < MaximumLines)
	{
		const int32 NewlineIndex = Remaining.Find(TEXT("\n"));
		const int32 ParagraphLength = NewlineIndex == INDEX_NONE
			? Remaining.Len()
			: NewlineIndex;
		FString Paragraph = Remaining.Left(ParagraphLength).TrimStartAndEnd();
		if (Paragraph.IsEmpty())
		{
			Remaining = NewlineIndex == INDEX_NONE
				? FString()
				: Remaining.Mid(NewlineIndex + 1).TrimStart();
			continue;
		}

		if (MeasureTextWidth(Paragraph, Font, TextScale) <= MaximumWidth)
		{
			OutLines.Add(MoveTemp(Paragraph));
			Remaining = NewlineIndex == INDEX_NONE
				? FString()
				: Remaining.Mid(NewlineIndex + 1).TrimStart();
			continue;
		}

		int32 BreakIndex = FindFittingCaptionPrefix(
			Paragraph,
			Font,
			TextScale,
			MaximumWidth);
		BreakIndex = FMath::Clamp(BreakIndex, 1, Paragraph.Len());
		const int32 MinimumEditorialBreak = FMath::Max(1, BreakIndex / 2);
		int32 EditorialBreak = INDEX_NONE;
		for (int32 Index = BreakIndex - 1; Index >= MinimumEditorialBreak; --Index)
		{
			const TCHAR Character = Paragraph[Index];
			if (FChar::IsWhitespace(Character))
			{
				EditorialBreak = Index;
				break;
			}
			if (FCString::Strchr(TEXT(".,!?;:…。！？、，"), Character))
			{
				EditorialBreak = Index + 1;
				break;
			}
		}
		if (EditorialBreak > 0)
		{
			BreakIndex = EditorialBreak;
		}

		FString Line = Paragraph.Left(BreakIndex).TrimEnd();
		if (Line.IsEmpty())
		{
			Line = Paragraph.Left(1);
			BreakIndex = 1;
		}
		OutLines.Add(MoveTemp(Line));
		const FString ParagraphRemainder =
			Paragraph.Mid(BreakIndex).TrimStart();
		const FString FollowingParagraphs = NewlineIndex == INDEX_NONE
			? FString()
			: Remaining.Mid(NewlineIndex);
		Remaining = (ParagraphRemainder + FollowingParagraphs).TrimStart();
	}
	OutRemainder = Remaining.TrimStartAndEnd();
}

void AIGHorrorHUD::DrawFearDirection(const double CurrentTime)
{
	if (!Canvas || CurrentTime >= FearCueEndTime)
	{
		return;
	}
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector Direction =
		(FearCueWorldLocation - ViewLocation).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	const FVector Forward = ViewRotation.Vector();
	const FVector Right = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
	const float ForwardAmount = FVector::DotProduct(Direction, Forward);
	const float RightAmount = FVector::DotProduct(Direction, Right);
	const float Angle = FMath::Atan2(RightAmount, ForwardAmount);
	const FVector2D Radial(FMath::Sin(Angle), -FMath::Cos(Angle));
	const FVector2D Tangent(-Radial.Y, Radial.X);
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const FVector2D WaveCenter = Center + FVector2D(
		Radial.X * Canvas->ClipX * 0.42f,
		Radial.Y * Canvas->ClipY * 0.39f);

	const double Elapsed = CurrentTime - FearCueStartTime;
	const double Remaining = FearCueEndTime - CurrentTime;
	const float Alpha = FMath::Clamp(
		FMath::Min(
			static_cast<float>(Elapsed / 0.12),
			static_cast<float>(Remaining / 0.30)),
		0.0f,
		1.0f);
	const FLinearColor CueColor(0.72f, 0.74f, 0.72f, Alpha * 0.78f);
	constexpr int32 SegmentCount = 6;
	constexpr float SegmentLength = 8.0f;
	constexpr float WaveAmplitude = 4.0f;
	FVector2D Previous = WaveCenter
		- Tangent * (SegmentCount * SegmentLength * 0.5f);
	for (int32 Index = 1; Index <= SegmentCount; ++Index)
	{
		const float Across =
			(Index - SegmentCount * 0.5f) * SegmentLength;
		const float Wave = Index == SegmentCount
			? 0.0f
			: (Index % 2 == 0 ? WaveAmplitude : -WaveAmplitude);
		const FVector2D Next = WaveCenter
			+ Tangent * Across
			+ Radial * Wave;
		FCanvasLineItem Line(Previous, Next);
		Line.SetColor(CueColor);
		Line.LineThickness = 2.0f;
		Canvas->DrawItem(Line);
		Previous = Next;
	}
}

void AIGHorrorHUD::DrawNoiseRipple(const double CurrentTime)
{
	if (!Canvas || CurrentTime >= RippleEndTime || RippleLoudness <= 0.0f)
	{
		return;
	}
	const APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController)
	{
		return;
	}

	// Which way the sound went out. Deliberately duplicated from
	// DrawFearDirection rather than shared: the accessibility contract pins
	// that function's exact expressions.
	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FVector Direction = (RippleWorldLocation - ViewLocation).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		// A sound made exactly at the camera — a footstep, usually — still
		// deserves a ring; put it straight ahead.
		Direction = ViewRotation.Vector();
	}

	const FVector RippleForward = ViewRotation.Vector();
	const FVector RippleRight = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
	const float RippleAngle = FMath::Atan2(
		FVector::DotProduct(Direction, RippleRight),
		FVector::DotProduct(Direction, RippleForward));
	const FVector2D EdgeNormal(FMath::Sin(RippleAngle), -FMath::Cos(RippleAngle));
	const FVector2D ScreenCenter(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	// How far the sound carries, normalized: a footstep is a short scratch of
	// an arc, a hammer blow is a wide bow. Radius is post-masking, so a sound
	// swallowed by a fridge hum never reaches this function at all.
	const float CarryRatio = FMath::Clamp(
		RippleRadiusCentimeters / UIGNoiseSubsystem::CarryPerLoudness,
		0.0f,
		1.0f);
	const float SweepDegrees = FMath::Lerp(
		IGHorrorHUD::NoiseRippleMinimumSweepDegrees,
		IGHorrorHUD::NoiseRippleMaximumSweepDegrees,
		CarryRatio);

	const float Age = static_cast<float>(CurrentTime - RippleStartTime);
	const float Life = FMath::Clamp(
		Age / static_cast<float>(IGHorrorHUD::NoiseRippleDurationSeconds),
		0.0f,
		1.0f);
	const float FadeIn = IGHorrorHUD::SmoothStep01(Age / 0.1f);
	const float FadeOut = 1.0f - IGHorrorHUD::SmoothStep01((Life - 0.45f) / 0.55f);
	const float Alpha = FMath::Clamp(FadeIn * FadeOut, 0.0f, 1.0f);
	if (Alpha <= 0.01f)
	{
		return;
	}

	const UIGAccessibilitySubsystem* Accessibility = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			Accessibility = GameInstance->GetSubsystem<UIGAccessibilitySubsystem>();
		}
	}
	const bool bReducedMotion =
		Accessibility && Accessibility->IsReducedCameraMotionEnabled();

	// The expansion is the ring's whole grammar, so reduced motion pins it at
	// its final radius and keeps the fade instead of dropping the element:
	// suppressing it would remove the only channel that carries loudness.
	const float Expansion = bReducedMotion ? 1.0f : FMath::Sqrt(Life);
	const float RadiusX = Canvas->ClipX * FMath::Lerp(0.30f, 0.455f, Expansion);
	const float RadiusY = Canvas->ClipY * FMath::Lerp(0.28f, 0.425f, Expansion);

	const FLinearColor RippleColor(
		0.78f,
		0.80f,
		0.78f,
		Alpha * FMath::Lerp(0.34f, 0.70f, CarryRatio));
	const float BaseAngle = FMath::Atan2(EdgeNormal.X, -EdgeNormal.Y);
	const float HalfSweep = FMath::DegreesToRadians(SweepDegrees) * 0.5f;

	FVector2D Previous = FVector2D::ZeroVector;
	for (int32 Index = 0; Index <= IGHorrorHUD::NoiseRippleSegmentCount; ++Index)
	{
		const float T =
			static_cast<float>(Index) / IGHorrorHUD::NoiseRippleSegmentCount;
		const float Angle = BaseAngle + FMath::Lerp(-HalfSweep, HalfSweep, T);
		const FVector2D Point = ScreenCenter + FVector2D(
			FMath::Sin(Angle) * RadiusX,
			-FMath::Cos(Angle) * RadiusY);
		if (Index > 0)
		{
			// Thin toward the ends so the arc reads as a wave rather than a
			// gauge with hard stops.
			const float EndTaper = FMath::Sin(T * UE_PI);
			FCanvasLineItem Line(Previous, Point);
			Line.SetColor(RippleColor);
			Line.LineThickness = FMath::Max(
				1.0f,
				IGHorrorHUD::NoiseRippleMaximumThickness * EndTaper);
			Canvas->DrawItem(Line);
		}
		Previous = Point;
	}

	// Never recorded for layout validation: a screen-edge arc is outside the
	// caption-safe rect by design, and recording it would widen the bounds the
	// packaged frontend probe asserts.
}

void AIGHorrorHUD::DrawSettingsShell(
	const IGSettingsMenuLayout::FPanelMetrics& Metrics,
	const FText& Title,
	const FText& Subtitle,
	const FText& ContextLabel)
{
	if (!Canvas)
	{
		return;
	}

	const float Scale = Metrics.Scale;
	DrawRoundedHudSurface(
		Metrics.PanelPosition + FVector2D(0.0f, 10.0f * Scale),
		Metrics.PanelSize,
		Metrics.CornerRadius + 2.0f * Scale,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.56f));
	DrawRoundedHudSurface(
		Metrics.PanelPosition,
		Metrics.PanelSize,
		Metrics.CornerRadius,
		IGHorrorHUD::SettingsPanel);
	// The rail tint belongs to the body only. Extending it behind the header
	// and footer creates a false clipping boundary through otherwise valid copy.
	FCanvasTileItem RailFill(
		FVector2D(Metrics.PanelPosition.X, Metrics.HeaderBottom),
		FVector2D(
			Metrics.RailRight - Metrics.PanelPosition.X,
			Metrics.FooterTop - Metrics.HeaderBottom),
		IGHorrorHUD::SettingsRail);
	RailFill.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(RailFill);

	FCanvasTileItem HeaderLine(
		FVector2D(Metrics.PanelPosition.X, Metrics.HeaderBottom),
		FVector2D(Metrics.PanelSize.X, FMath::Max(1.0f, Scale)),
		IGHorrorHUD::SettingsDivider);
	HeaderLine.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(HeaderLine);
	FCanvasTileItem RailLine(
		FVector2D(Metrics.RailRight, Metrics.HeaderBottom),
		FVector2D(FMath::Max(1.0f, Scale), Metrics.FooterTop - Metrics.HeaderBottom),
		IGHorrorHUD::SettingsDivider);
	RailLine.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(RailLine);
	FCanvasTileItem FooterLine(
		FVector2D(Metrics.PanelPosition.X, Metrics.FooterTop),
		FVector2D(Metrics.PanelSize.X, FMath::Max(1.0f, Scale)),
		IGHorrorHUD::SettingsDivider);
	FooterLine.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(FooterLine);

	const float HeaderLeft = Metrics.PanelPosition.X + 32.0f * Scale;
	const float HeaderRight =
		Metrics.PanelPosition.X + Metrics.PanelSize.X - 32.0f * Scale;
	const float HeaderGap = 24.0f * Scale;
	const float ContextScale = GetFittedTextScale(
		ContextLabel,
		EIGHudTextRole::Hint,
		0.84f * Scale,
		FMath::Min(280.0f * Scale, Metrics.PanelSize.X * 0.28f),
		0.64f * Scale);
	const float ContextWidth = MeasureTextWidth(
		ContextLabel.ToString(),
		GetFontForRole(EIGHudTextRole::Hint),
		ContextScale);
	const float HeaderTextWidth = FMath::Max(
		180.0f * Scale,
		HeaderRight - ContextWidth - HeaderGap - HeaderLeft);
	const float TitleScale = GetFittedTextScale(
		Title,
		EIGHudTextRole::Objective,
		1.02f * Scale,
		HeaderTextWidth,
		0.74f * Scale);
	const float SubtitleScale = GetFittedTextScale(
		Subtitle,
		EIGHudTextRole::Hint,
		0.84f * Scale,
		HeaderTextWidth,
		0.62f * Scale);
	const FVector2D HeaderContainerMinimum(
		HeaderLeft,
		Metrics.PanelPosition.Y + 10.0f * Scale);
	const FVector2D HeaderContainerMaximum(
		HeaderLeft + HeaderTextWidth,
		Metrics.HeaderBottom - 8.0f * Scale);
	const FVector2D ContextContainerMinimum(
		HeaderRight - ContextWidth,
		Metrics.PanelPosition.Y + 10.0f * Scale);
	const FVector2D ContextContainerMaximum(
		HeaderRight,
		Metrics.HeaderBottom - 8.0f * Scale);
	UFont* TitleFont = GetFontForRole(EIGHudTextRole::Objective);
	UFont* SubtitleFont = GetFontForRole(EIGHudTextRole::Hint);
	const float TitleHeight = MeasureTextHeight(
		Title.ToString(),
		TitleFont,
		TitleScale);
	const float SubtitleHeight = MeasureTextHeight(
		Subtitle.ToString(),
		SubtitleFont,
		SubtitleScale);
	const float TitleY = Metrics.PanelPosition.Y + 19.0f * Scale;
	const float SubtitleY = FMath::Max(
		Metrics.PanelPosition.Y + 64.0f * Scale,
		TitleY + TitleHeight + 8.0f * Scale);
	ValidateSettingsTextRect(
		FVector2D(HeaderLeft, TitleY),
		FVector2D(
			HeaderLeft + MeasureTextWidth(
				Title.ToString(),
				TitleFont,
				TitleScale),
			TitleY + TitleHeight),
		HeaderContainerMinimum,
		HeaderContainerMaximum);
	ValidateSettingsTextRect(
		FVector2D(HeaderLeft, SubtitleY),
		FVector2D(
			HeaderLeft + MeasureTextWidth(
				Subtitle.ToString(),
				SubtitleFont,
				SubtitleScale),
			SubtitleY + SubtitleHeight),
		HeaderContainerMinimum,
		HeaderContainerMaximum);
	ValidateSettingsTextRect(
		FVector2D(HeaderRight - ContextWidth, Metrics.PanelPosition.Y + 34.0f * Scale),
		FVector2D(
			HeaderRight,
			Metrics.PanelPosition.Y + 34.0f * Scale
				+ IGHorrorHUD::SmallFontSize * ContextScale * 1.35f),
		ContextContainerMinimum,
		ContextContainerMaximum);

	DrawLeftAlignedText(
		Title,
		FVector2D(HeaderLeft, TitleY),
		IGHorrorHUD::SettingsPrimary,
		EIGHudTextRole::Objective,
		TitleScale);
	DrawLeftAlignedText(
		Subtitle,
		FVector2D(
			HeaderLeft + 1.0f * Scale,
			SubtitleY),
		IGHorrorHUD::SettingsSecondary,
		EIGHudTextRole::Hint,
		SubtitleScale);
	DrawRightAlignedText(
		ContextLabel,
		FVector2D(
			HeaderRight,
			Metrics.PanelPosition.Y + 34.0f * Scale),
		IGHorrorHUD::SettingsSecondary,
		EIGHudTextRole::Hint,
		ContextScale);
	DrawLeftAlignedText(
		SupportsKorean()
			? NSLOCTEXT("IGHUD", "SettingsCategoryHeading", "카테고리")
			: FText::FromString(TEXT("CATEGORIES")),
		FVector2D(
			Metrics.RailLeft + 10.0f * Scale,
			Metrics.HeaderBottom + 17.0f * Scale),
		IGHorrorHUD::SettingsSecondary,
		EIGHudTextRole::Hint,
		0.75f * Scale);
	DrawLeftAlignedText(
		SupportsKorean()
			? NSLOCTEXT("IGHUD", "SettingsOptionsHeading", "세부 설정")
			: FText::FromString(TEXT("OPTIONS")),
		FVector2D(
			Metrics.ContentLeft,
			Metrics.HeaderBottom + 17.0f * Scale),
		IGHorrorHUD::SettingsSecondary,
		EIGHudTextRole::Hint,
		0.75f * Scale);

	RecordLayoutValidationRect(
		Metrics.PanelPosition,
		Metrics.PanelPosition + Metrics.PanelSize);
}

void AIGHorrorHUD::DrawSettingsCategoryRow(
	const IGSettingsMenuLayout::FPanelMetrics& Metrics,
	const int32 CategoryIndex,
	const FText& Label,
	const bool bSelected)
{
	const float Scale = Metrics.Scale;
	const FVector2D Position(
		Metrics.RailLeft,
		Metrics.CategoryStartY + CategoryIndex * Metrics.CategoryRowHeight);
	const FVector2D Size(
		Metrics.RailRight - Metrics.RailLeft - 14.0f * Scale,
		Metrics.CategoryRowHeight - 6.0f * Scale);
	if (bSelected)
	{
		DrawRoundedHudSurface(
			Position,
			Size,
			7.0f * Scale,
			IGHorrorHUD::SettingsRaised);
		DrawRoundedHudSurface(
			Position + FVector2D(2.0f * Scale, 8.0f * Scale),
			FVector2D(3.0f * Scale, Size.Y - 16.0f * Scale),
			1.5f * Scale,
			IGHorrorHUD::SettingsAccent);
	}
	const float CategoryTextScale = GetFittedTextScale(
		Label,
		bSelected ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint,
		0.94f * Scale,
		Size.X - 32.0f * Scale,
		0.64f * Scale);
	UFont* CategoryFont = GetFontForRole(
		bSelected ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint);
	const float CategoryTextWidth = MeasureTextWidth(
		Label.ToString(),
		CategoryFont,
		CategoryTextScale);
	const float CategoryTextHeight = MeasureTextHeight(
		Label.ToString(),
		CategoryFont,
		CategoryTextScale);
	const float CategoryTextY = Position.Y
		+ FMath::Max(0.0f, (Size.Y - CategoryTextHeight) * 0.5f);
	ValidateSettingsTextRect(
		FVector2D(Position.X + 16.0f * Scale, CategoryTextY),
		FVector2D(
			Position.X + 16.0f * Scale + CategoryTextWidth,
			CategoryTextY + CategoryTextHeight),
		Position,
		Position + Size);
	DrawLeftAlignedText(
		Label,
		FVector2D(Position.X + 16.0f * Scale, CategoryTextY),
		bSelected
			? IGHorrorHUD::SettingsPrimary
			: IGHorrorHUD::SettingsSecondary,
		bSelected ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint,
		CategoryTextScale);
}

void AIGHorrorHUD::DrawSettingsOptionRow(
	const IGSettingsMenuLayout::FPanelMetrics& Metrics,
	const int32 LocalRow,
	const FText& Label,
	const FText& Value,
	const bool bSelected,
	const bool bAdjustable)
{
	const float Scale = Metrics.Scale;
	const FVector2D Position(
		Metrics.ContentLeft,
		Metrics.OptionStartY + LocalRow * Metrics.OptionRowHeight);
	const FVector2D Size(
		Metrics.ContentRight - Metrics.ContentLeft,
		Metrics.OptionRowHeight - 7.0f * Scale);
	if (bSelected)
	{
		DrawRoundedHudSurface(
			Position,
			Size,
			8.0f * Scale,
			IGHorrorHUD::SettingsSelected);
		DrawRoundedHudSurface(
			Position + FVector2D(2.0f * Scale, 9.0f * Scale),
			FVector2D(3.0f * Scale, Size.Y - 18.0f * Scale),
			1.5f * Scale,
			IGHorrorHUD::SettingsAccent);
	}
	else
	{
		FCanvasTileItem Divider(
			FVector2D(Position.X + 14.0f * Scale, Position.Y + Size.Y),
			FVector2D(Size.X - 28.0f * Scale, 1.0f),
			IGHorrorHUD::SettingsDivider);
		Divider.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Divider);
	}

	UFont* ValueFont = GetFontForRole(EIGHudTextRole::Hint);
	const FString ValueText = bAdjustable
		? FString::Printf(TEXT("−  %s  +"), *Value.ToString())
		: Value.ToString();
	const float ValueScale = 0.88f * Scale;
	const float ValueWidth = Value.IsEmpty() || !ValueFont
		? 0.0f
		: MeasureTextWidth(ValueText, ValueFont, ValueScale);
	const float LabelWidthLimit = FMath::Max(
		120.0f * Scale,
		Size.X - ValueWidth - 64.0f * Scale);
	const float LabelTextScale = GetFittedTextScale(
		Label,
		bSelected ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint,
		0.96f * Scale,
		LabelWidthLimit,
		0.62f * Scale);
	const float LabelWidth = MeasureTextWidth(
		Label.ToString(),
		GetFontForRole(
			bSelected ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint),
		LabelTextScale);
	UFont* LabelFont = GetFontForRole(
		bSelected ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint);
	const float LabelHeight = MeasureTextHeight(
		Label.ToString(),
		LabelFont,
		LabelTextScale);
	const float ValueHeight = MeasureTextHeight(
		ValueText,
		ValueFont,
		ValueScale);
	const float LabelY = Position.Y
		+ FMath::Max(0.0f, (Size.Y - LabelHeight) * 0.5f);
	const float ValueY = Position.Y
		+ FMath::Max(0.0f, (Size.Y - ValueHeight) * 0.5f);
	ValidateSettingsTextRect(
		FVector2D(Position.X + 18.0f * Scale, LabelY),
		FVector2D(
			Position.X + 18.0f * Scale + LabelWidth,
			LabelY + LabelHeight),
		Position,
		Position + Size);
	if (!Value.IsEmpty())
	{
		ValidateSettingsTextRect(
			FVector2D(
				Position.X + Size.X - 18.0f * Scale - ValueWidth,
				ValueY),
			FVector2D(
				Position.X + Size.X - 18.0f * Scale,
				ValueY + ValueHeight),
			Position,
			Position + Size);
	}
	DrawLeftAlignedText(
		Label,
		FVector2D(Position.X + 18.0f * Scale, LabelY),
		bSelected
			? IGHorrorHUD::SettingsPrimary
			: IGHorrorHUD::SettingsSecondary,
		bSelected ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint,
		LabelTextScale);
	if (!Value.IsEmpty())
	{
		DrawRightAlignedText(
			FText::FromString(ValueText),
			FVector2D(
				Position.X + Size.X - 18.0f * Scale,
				ValueY),
			bSelected
				? IGHorrorHUD::SettingsPrimary
				: IGHorrorHUD::SettingsSecondary,
			EIGHudTextRole::Hint,
			ValueScale);
	}
}

void AIGHorrorHUD::DrawSettingsDetailText(
	const FString& Text,
	const FVector2D& Position,
	const float MaximumWidth,
	const float TextScale,
	const FLinearColor& Color)
{
	UFont* Font = GetFontForRole(EIGHudTextRole::Hint);
	if (!Font || Text.IsEmpty() || MaximumWidth <= 0.0f)
	{
		return;
	}

	float FitScale = TextScale;
	TArray<FString> Lines;
	FString Remainder;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		Lines.Reset();
		Remainder.Reset();
		WrapHudText(Text, Font, FitScale, MaximumWidth, 2, Lines, Remainder);
		if (Remainder.IsEmpty())
		{
			break;
		}
		FitScale *= 0.90f;
	}
	if (!Remainder.IsEmpty())
	{
		// At the minimum supported scale, preserve the full explanation by
		// using a third line instead of clipping or silently dropping copy.
		Lines.Reset();
		Remainder.Reset();
		WrapHudText(Text, Font, FitScale, MaximumWidth, 3, Lines, Remainder);
	}

	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const float DetailLineHeight = MeasureTextHeight(
			Lines[Index],
			Font,
			FitScale);
		const FVector2D LinePosition = Position + FVector2D(
			0.0f,
			Index * FMath::Max(
				IGHorrorHUD::SmallFontSize * FitScale * 1.48f,
				DetailLineHeight + 6.0f * FitScale));
		const float LineWidth = MeasureTextWidth(
			Lines[Index],
			Font,
			FitScale);
		ValidateSettingsTextRect(
			LinePosition,
			LinePosition + FVector2D(
				LineWidth,
				DetailLineHeight),
			Position,
			FVector2D(
				Position.X + MaximumWidth,
				Position.Y + IGHorrorHUD::SmallFontSize * FitScale * 4.5f));
		DrawLeftAlignedText(
			FText::FromString(Lines[Index]),
			LinePosition,
			Color,
			EIGHudTextRole::Hint,
			FitScale);
	}
}

void AIGHorrorHUD::DrawSettingsFooterText(
	const IGSettingsMenuLayout::FPanelMetrics& Metrics,
	const FText& Text)
{
	const float Scale = Metrics.Scale;
	const float HorizontalPadding = 26.0f * Scale;
	const float MaximumWidth = Metrics.PanelSize.X - HorizontalPadding * 2.0f;
	const float FooterScale = GetFittedTextScale(
		Text,
		EIGHudTextRole::Hint,
		0.82f * Scale,
		MaximumWidth,
		0.58f * Scale);
	const float FooterTextHeight = MeasureTextHeight(
		Text.ToString(),
		GetFontForRole(EIGHudTextRole::Hint),
		FooterScale);
	const FVector2D FooterTextPosition(
		Metrics.PanelPosition.X + HorizontalPadding,
		Metrics.FooterTop + FMath::Max(
			0.0f,
			(Metrics.PanelPosition.Y + Metrics.PanelSize.Y
				- Metrics.FooterTop - FooterTextHeight) * 0.5f));
	ValidateSettingsTextRect(
		FooterTextPosition,
		FooterTextPosition + FVector2D(
			MeasureTextWidth(
				Text.ToString(),
				GetFontForRole(EIGHudTextRole::Hint),
				FooterScale),
			FooterTextHeight),
		FVector2D(
			Metrics.PanelPosition.X + HorizontalPadding,
			Metrics.FooterTop),
		FVector2D(
			Metrics.PanelPosition.X + Metrics.PanelSize.X - HorizontalPadding,
			Metrics.PanelPosition.Y + Metrics.PanelSize.Y));
	DrawLeftAlignedText(
		Text,
		FooterTextPosition,
		IGHorrorHUD::SettingsSecondary,
		EIGHudTextRole::Hint,
		FooterScale);
}

void AIGHorrorHUD::DrawAccessibilityPanel()
{
	if (!Canvas)
	{
		return;
	}
	const UGameInstance* GameInstance = GetGameInstance();
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	if (!Accessibility)
	{
		return;
	}
	const FIGAccessibilitySettings Settings = Accessibility->GetSettings();
	const bool bKorean = SupportsKorean();
	const IGSettingsMenuLayout::FPanelMetrics Metrics =
		IGSettingsMenuLayout::MakePanelMetrics(Canvas->ClipX, Canvas->ClipY);
	const float Scale = Metrics.Scale;
	const auto OnOff = [bKorean](const bool bEnabled)
	{
		return bKorean
			? FString(bEnabled ? TEXT("켬") : TEXT("끔"))
			: FString(bEnabled ? TEXT("ON") : TEXT("OFF"));
	};

	FString HintMode;
	switch (Settings.HintMode)
	{
	case EIGHintMode::Story:
		HintMode = bKorean ? TEXT("이야기") : TEXT("STORY");
		break;
	case EIGHintMode::Silent:
		HintMode = bKorean ? TEXT("침묵") : TEXT("SILENT");
		break;
	case EIGHintMode::Standard:
	default:
		HintMode = bKorean ? TEXT("기본") : TEXT("STANDARD");
		break;
	}

	const FString Labels[] =
	{
		bKorean ? TEXT("힌트 난이도") : TEXT("HINT MODE"),
		bKorean ? TEXT("카메라 흔들림 감소") : TEXT("REDUCED CAMERA MOTION"),
		bKorean ? TEXT("손전등 점멸 감소") : TEXT("REDUCED FLASHLIGHT FLICKER"),
		bKorean ? TEXT("공포음 방향 표시") : TEXT("FEAR SOUND DIRECTION"),
		bKorean ? TEXT("P5 단서 자동 연결") : TEXT("AUTO-CONNECT EVIDENCE"),
		bKorean ? TEXT("대사 음성 자막") : TEXT("VOICE SUBTITLES"),
		bKorean ? TEXT("핵심 소리 캡션") : TEXT("SOUND CAPTIONS"),
		bKorean ? TEXT("대사·캡션 크기") : TEXT("DIALOGUE + CAPTION SIZE"),
		bKorean ? TEXT("메시지 배경 농도") : TEXT("MESSAGE BACKGROUND"),
		bKorean ? TEXT("자막 안전 영역") : TEXT("CAPTION SAFE AREA"),
		bKorean ? TEXT("앉기 입력 방식") : TEXT("CROUCH INPUT"),
		bKorean ? TEXT("길게 누르기 방식") : TEXT("HOLD INPUT"),
		bKorean ? TEXT("홀드 길이") : TEXT("HOLD DURATION"),
		bKorean ? TEXT("컨트롤러 진동") : TEXT("CONTROLLER VIBRATION"),
		bKorean ? TEXT("마이크 소음 입력 (선택)") : TEXT("OPTIONAL MICROPHONE NOISE"),
		bKorean ? TEXT("기본값으로 초기화") : TEXT("RESET TO DEFAULTS"),
		bKorean ? TEXT("닫기") : TEXT("CLOSE")
	};
	const FString Values[] =
	{
		HintMode,
		OnOff(Settings.bReducedCameraMotion),
		OnOff(Settings.bReducedFlicker),
		OnOff(Settings.bDirectionalFearCues),
		OnOff(Settings.bAutoConnectEvidence),
		OnOff(Settings.bSubtitlesEnabled),
		OnOff(Settings.bSoundCaptionsEnabled),
		FString::Printf(
			TEXT("%d%%"),
			FMath::RoundToInt(Settings.CaptionSizeScale * 100.0f)),
		FString::Printf(
			TEXT("%d%%"),
			FMath::RoundToInt(Settings.CaptionBackgroundOpacity * 100.0f)),
		FString::Printf(
			TEXT("%d%%"),
			FMath::RoundToInt(Settings.CaptionSafeAreaScale * 100.0f)),
		bKorean
			? (Settings.bToggleCrouch ? TEXT("토글") : TEXT("누르는 동안"))
			: (Settings.bToggleCrouch ? TEXT("TOGGLE") : TEXT("HOLD")),
		bKorean
			? (Settings.bToggleHoldInteractions ? TEXT("토글") : TEXT("누르는 동안"))
			: (Settings.bToggleHoldInteractions ? TEXT("TOGGLE") : TEXT("HOLD")),
		FString::Printf(
			TEXT("%d%%"),
			FMath::RoundToInt(Settings.HoldDurationScale * 100.0f)),
		OnOff(Settings.bHapticsEnabled),
		OnOff(Settings.bMicrophoneNoiseEnabled),
		FString(),
		FString()
	};
	const FString Descriptions[] =
	{
		bKorean
			? TEXT("막혔을 때 제공되는 도움의 강도를 정합니다. 정답을 즉시 공개하지 않고 단계별 단서를 유지합니다.")
			: TEXT("SETS HOW MUCH HELP APPEARS WHEN PROGRESS STALLS WITHOUT REVEALING ANSWERS AT ONCE."),
		bKorean
			? TEXT("머리 흔들림과 추격 중 카메라 진폭을 낮춥니다. 이동 속도와 판정은 그대로 유지됩니다.")
			: TEXT("REDUCES HEAD BOB AND CHASE CAMERA AMPLITUDE WITHOUT CHANGING MOVEMENT OR GAMEPLAY."),
		bKorean
			? TEXT("손전등과 공포 연출의 빠른 점멸을 완화합니다. 필요한 위험 정보는 밝기 변화 대신 형태로 남깁니다.")
			: TEXT("SOFTENS RAPID FLASHES WHILE PRESERVING DANGER INFORMATION THROUGH SHAPE AND TIMING."),
		bKorean
			? TEXT("공포음이 들린 방향을 화면 가장자리의 절제된 표시로 함께 전달합니다.")
			: TEXT("ADDS A RESTRAINED SCREEN-EDGE CUE FOR THE DIRECTION OF IMPORTANT HORROR SOUNDS."),
		bKorean
			? TEXT("관찰한 P5 단서의 연결을 자동으로 정리합니다. 단서를 발견하는 과정은 건너뛰지 않습니다.")
			: TEXT("ORGANIZES OBSERVED P5 EVIDENCE AUTOMATICALLY WITHOUT SKIPPING DISCOVERY."),
		bKorean
			? TEXT("녹음된 인물 대사를 자막으로 표시합니다. 지운의 내면 독백은 이야기 전달을 위해 항상 유지됩니다.")
			: TEXT("SHOWS SUBTITLES FOR RECORDED VOICES; STORY-CRITICAL INNER MONOLOGUE REMAINS AVAILABLE."),
		bKorean
			? TEXT("노크, 발소리, 기계음처럼 진행에 필요한 비언어음을 별도의 캡션으로 표시합니다.")
			: TEXT("CAPTIONS IMPORTANT NON-SPEECH AUDIO SUCH AS KNOCKS, FOOTSTEPS, AND MACHINES."),
		bKorean
			? TEXT("대사와 소리 캡션의 글자 크기를 함께 조정합니다. 아래 미리 보기에 즉시 반영됩니다.")
			: TEXT("CHANGES DIALOGUE AND SOUND-CAPTION SIZE WITH AN IMMEDIATE PREVIEW BELOW."),
		bKorean
			? TEXT("밝은 배경에서도 읽히도록 자막 뒤 어두운 면의 농도를 조절합니다.")
			: TEXT("ADJUSTS THE DARK SURFACE BEHIND CAPTIONS FOR LEGIBILITY OVER BRIGHT SCENES."),
		bKorean
			? TEXT("자막이 화면 가장자리에 너무 가깝지 않도록 최대 너비와 여백을 조절합니다.")
			: TEXT("CONTROLS CAPTION WIDTH AND MARGINS SO TEXT STAYS AWAY FROM SCREEN EDGES."),
		bKorean
			? TEXT("앉기 키를 한 번 눌러 전환하거나, 누르고 있는 동안만 유지하도록 선택합니다.")
			: TEXT("CHOOSE BETWEEN TOGGLE CROUCH AND HOLD-TO-CROUCH."),
		bKorean
			? TEXT("상호작용을 길게 누르는 대신 한 번 눌러 시작하고 다시 눌러 취소할 수 있습니다.")
			: TEXT("ALLOWS HOLD INTERACTIONS TO START WITH ONE PRESS AND CANCEL WITH ANOTHER."),
		bKorean
			? TEXT("문 열기와 조사처럼 길게 누르는 상호작용의 요구 시간을 조정합니다.")
			: TEXT("ADJUSTS THE REQUIRED TIME FOR HOLD INTERACTIONS SUCH AS OPENING AND INSPECTING."),
		bKorean
			? TEXT("위험, 충돌, 상호작용 피드백에 사용되는 컨트롤러 진동을 켜거나 끕니다.")
			: TEXT("ENABLES OR DISABLES CONTROLLER VIBRATION FOR DANGER, IMPACTS, AND INTERACTIONS."),
		bKorean
			? TEXT("선택 기능입니다. 실제 마이크 소리를 소음 기믹에 사용하며, 끄면 마이크를 열지 않습니다.")
			: TEXT("OPTIONAL. USES LIVE MICROPHONE NOISE FOR GAMEPLAY; OFF KEEPS THE MICROPHONE CLOSED."),
		bKorean
			? TEXT("이 화면의 접근성 항목을 처음 설치했을 때의 값으로 되돌립니다.")
			: TEXT("RESTORES EVERY ACCESSIBILITY OPTION ON THIS SCREEN TO ITS INSTALL DEFAULT."),
		bKorean
			? TEXT("변경 내용은 즉시 저장됩니다. 이전 화면이나 게임으로 돌아갑니다.")
			: TEXT("CHANGES ARE SAVED IMMEDIATELY. RETURN TO THE PREVIOUS SCREEN OR THE GAME.")
	};
	const FString CategoryLabels[] =
	{
		bKorean ? TEXT("게임 진행") : TEXT("GAMEPLAY"),
		bKorean ? TEXT("움직임") : TEXT("MOTION"),
		bKorean ? TEXT("정보 안내") : TEXT("GUIDANCE"),
		bKorean ? TEXT("자막") : TEXT("CAPTIONS"),
		bKorean ? TEXT("입력") : TEXT("INPUT"),
		bKorean ? TEXT("관리") : TEXT("GENERAL")
	};

	FCanvasTileItem Scrim(
		FVector2D::ZeroVector,
		FVector2D(Canvas->ClipX, Canvas->ClipY),
		FLinearColor(0.0f, 0.0f, 0.0f, 0.94f));
	Scrim.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Scrim);
	DrawSettingsShell(
		Metrics,
		bKorean
			? NSLOCTEXT("IGHUD", "AccessibilityTitle", "접근성 설정")
			: FText::FromString(TEXT("ACCESSIBILITY")),
		bKorean
			? NSLOCTEXT(
				"IGHUD",
				"AccessibilitySubtitle",
				"필요한 정보는 더 또렷하게, 공포의 밀도는 그대로 유지합니다.")
			: FText::FromString(
				TEXT("CLEARER INFORMATION WITHOUT DILUTING THE HORROR.")),
		bKorean
			? NSLOCTEXT("IGHUD", "AccessibilitySavedImmediately", "변경 즉시 저장")
			: FText::FromString(TEXT("SAVES IMMEDIATELY")));

	const int32 ActiveCategory = IGSettingsMenuLayout::FindCategoryForRow(
		AccessibilitySelectedRow,
		IGSettingsMenuLayout::AccessibilityCategoryCount,
		IGSettingsMenuLayout::GetAccessibilityCategory);
	const IGSettingsMenuLayout::FCategoryRange ActiveRange =
		IGSettingsMenuLayout::GetAccessibilityCategory(ActiveCategory);
	for (int32 Category = 0;
		Category < IGSettingsMenuLayout::AccessibilityCategoryCount;
		++Category)
	{
		DrawSettingsCategoryRow(
			Metrics,
			Category,
			FText::FromString(CategoryLabels[Category]),
			Category == ActiveCategory);
	}
	for (int32 LocalRow = 0; LocalRow < ActiveRange.RowCount; ++LocalRow)
	{
		const int32 Row = ActiveRange.FirstRow + LocalRow;
		DrawSettingsOptionRow(
			Metrics,
			LocalRow,
			FText::FromString(Labels[Row]),
			FText::FromString(Values[Row]),
			Row == AccessibilitySelectedRow,
			Row < 15);
	}

	const float DetailTop = Metrics.OptionStartY
		+ ActiveRange.RowCount * Metrics.OptionRowHeight
		+ 18.0f * Scale;
	const float DetailBottom = Metrics.FooterTop - 18.0f * Scale;
	const float DetailHeight = FMath::Min(
		ActiveCategory == 3 ? 210.0f * Scale : 150.0f * Scale,
		DetailBottom - DetailTop);
	if (DetailTop < DetailBottom - 54.0f * Scale)
	{
		DrawRoundedHudSurface(
			FVector2D(Metrics.ContentLeft, DetailTop),
			FVector2D(
				Metrics.ContentRight - Metrics.ContentLeft,
				DetailHeight),
			8.0f * Scale,
			IGHorrorHUD::SettingsRaised);
		DrawLeftAlignedText(
			bKorean
				? NSLOCTEXT("IGHUD", "SettingsEffectHeading", "이 설정이 바꾸는 것")
				: FText::FromString(TEXT("WHAT THIS CHANGES")),
			FVector2D(
				Metrics.ContentLeft + 18.0f * Scale,
				DetailTop + 13.0f * Scale),
			IGHorrorHUD::SettingsSecondary,
			EIGHudTextRole::Hint,
			0.72f * Scale);
		DrawSettingsDetailText(
			Descriptions[AccessibilitySelectedRow],
			FVector2D(
				Metrics.ContentLeft + 18.0f * Scale,
				DetailTop + 41.0f * Scale),
			Metrics.ContentRight - Metrics.ContentLeft - 36.0f * Scale,
			0.82f * Scale,
			IGHorrorHUD::SettingsPrimary);
	}

	// 자막 카테고리에서는 설정 설명만으로 결과를 상상하게 하지 않는다.
	// 선택한 크기·배경·안전 영역을 같은 화면의 실제 렌더링으로 확인한다.
	const float PreviewTextScale = GetResolutionTextScale(Settings.CaptionSizeScale);
	const float PreviewWidth = FMath::Min(
		(Metrics.ContentRight - Metrics.ContentLeft - 36.0f * Scale)
			* Settings.CaptionSafeAreaScale,
		620.0f * Scale);
	const FText PreviewBodyText = bKorean
		? NSLOCTEXT("IGHUD", "CaptionPreviewBody", "[위층] 천천히 끌리는 발소리")
		: FText::FromString(TEXT("[ABOVE] SLOW, DRAGGING FOOTSTEPS"));
	UFont* PreviewFont = GetFontForRole(EIGHudTextRole::Dialogue);
	float PreviewRawWidth = 0.0f;
	float PreviewRawHeight = 19.0f;
	if (PreviewFont)
	{
		Canvas->StrLen(
			PreviewFont,
			TEXT("한Ag"),
			PreviewRawWidth,
			PreviewRawHeight,
			true);
	}
	TArray<FString> PreviewLines;
	FString PreviewRemainder;
	WrapHudText(
		PreviewBodyText.ToString(),
		PreviewFont,
		PreviewTextScale,
		PreviewWidth - 36.0f * Scale,
		2,
		PreviewLines,
		PreviewRemainder);
	if (PreviewLines.IsEmpty())
	{
		PreviewLines.Add(PreviewBodyText.ToString());
	}
	const float PreviewLineHeight = FMath::Max(
		16.0f,
		PreviewRawHeight * PreviewTextScale * 1.20f);
	const float PreviewBodyHeight =
		PreviewLineHeight * PreviewLines.Num();
	const float PreviewSpeakerHeight = 14.0f * PreviewTextScale * 0.78f;
	const float PreviewHeight = FMath::Max(
		48.0f * Scale,
		PreviewSpeakerHeight + PreviewBodyHeight + 23.0f * Scale);
	if (ActiveCategory == 3)
	{
		const float PreviewX = Metrics.ContentLeft
			+ (Metrics.ContentRight - Metrics.ContentLeft - PreviewWidth) * 0.5f;
		const float PreviewY = FMath::Max(
			DetailTop + 62.0f * Scale,
			DetailTop + DetailHeight - PreviewHeight - 12.0f * Scale);
		ValidateSettingsTextRect(
			FVector2D(PreviewX, PreviewY),
			FVector2D(
				PreviewX + PreviewWidth,
				PreviewY + PreviewHeight),
			FVector2D(Metrics.ContentLeft, DetailTop),
			FVector2D(Metrics.ContentRight, DetailTop + DetailHeight));
		DrawRoundedHudSurface(
			FVector2D(PreviewX, PreviewY),
			FVector2D(PreviewWidth, PreviewHeight),
			5.0f * Scale,
			FLinearColor(
				0.018f,
				0.021f,
				0.020f,
				Settings.CaptionBackgroundOpacity));
		RecordLayoutValidationRect(
			FVector2D(PreviewX, PreviewY),
			FVector2D(PreviewX + PreviewWidth, PreviewY + PreviewHeight));
		DrawLeftAlignedText(
			bKorean
				? NSLOCTEXT("IGHUD", "CaptionPreviewSpeaker", "미리 보기")
				: FText::FromString(TEXT("PREVIEW")),
			FVector2D(
				PreviewX + 18.0f * Scale,
				PreviewY + 7.0f * Scale),
			IGHorrorHUD::ThoughtBlue,
			EIGHudTextRole::Speaker,
			PreviewTextScale * 0.78f,
			true);
		for (int32 LineIndex = 0;
			LineIndex < PreviewLines.Num();
			++LineIndex)
		{
			const FVector2D PreviewLinePosition(
				PreviewX + 18.0f * Scale,
				PreviewY + 8.0f * Scale
					+ PreviewSpeakerHeight + 5.0f * Scale
					+ LineIndex * PreviewLineHeight);
			ValidateSettingsTextRect(
				PreviewLinePosition,
				PreviewLinePosition + FVector2D(
					MeasureTextWidth(
						PreviewLines[LineIndex],
						PreviewFont,
						PreviewTextScale),
					PreviewRawHeight * PreviewTextScale),
				FVector2D(PreviewX, PreviewY),
				FVector2D(
					PreviewX + PreviewWidth,
					PreviewY + PreviewHeight));
			DrawLeftAlignedText(
				FText::FromString(PreviewLines[LineIndex]),
				PreviewLinePosition,
				IGHorrorHUD::SettingsPrimary,
				EIGHudTextRole::Dialogue,
				PreviewTextScale,
				true);
		}
	}

	DrawSettingsFooterText(
		Metrics,
		bKorean
			? bUsingGamepad
				? NSLOCTEXT(
					"IGHUD",
					"AccessibilityControlsGamepad",
					"D-pad 이동·값 변경  ·  A 선택  ·  B 닫기")
				: NSLOCTEXT(
					"IGHUD",
					"AccessibilityControlsKeyboard",
					"방향키 이동·값 변경  ·  Enter 선택  ·  Esc/F10 닫기")
			: FText::FromString(
				bUsingGamepad
					? TEXT("D-PAD SELECT + CHANGE  |  A APPLY  |  B CLOSE")
					: TEXT("ARROWS SELECT + CHANGE  |  ENTER APPLY  |  ESC/F10 CLOSE")));
}

UTexture2D* AIGHorrorHUD::GetMissingFloorJournalThumbnail(
	const int32 ThumbnailType) const
{
	switch (static_cast<IGHorrorHUD::EJournalThumbnail>(ThumbnailType))
	{
	case IGHorrorHUD::EJournalThumbnail::Meter:
		return JournalMeterTexture;
	case IGHorrorHUD::EJournalThumbnail::Plaster:
		return JournalPlasterTexture;
	case IGHorrorHUD::EJournalThumbnail::Tank:
		return JournalTankTexture;
	case IGHorrorHUD::EJournalThumbnail::Metal:
		return JournalMetalTexture;
	case IGHorrorHUD::EJournalThumbnail::Document:
	default:
		return NotePaperTexture;
	}
}

void AIGHorrorHUD::DrawMissingFloorJournalPanel()
{
	if (!Canvas)
	{
		return;
	}

	const float ScreenWidth = Canvas->ClipX;
	const float ScreenHeight = Canvas->ClipY;
	const float ResolutionScale = FMath::Clamp(
		FMath::Min(ScreenWidth / 1920.0f, ScreenHeight / 1080.0f),
		0.68f,
		1.35f);
	DrawRect(
		FLinearColor(0.004f, 0.006f, 0.007f, 0.97f),
		0.0f,
		0.0f,
		ScreenWidth,
		ScreenHeight);

	const FVector2D PaperOrigin(ScreenWidth * 0.035f, ScreenHeight * 0.045f);
	const FVector2D PaperSize(ScreenWidth * 0.93f, ScreenHeight * 0.90f);
	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.50f),
		PaperOrigin.X + 7.0f * ResolutionScale,
		PaperOrigin.Y + 10.0f * ResolutionScale,
		PaperSize.X,
		PaperSize.Y);
	if (MissingFloorJournalTexture)
	{
		DrawTexture(
			MissingFloorJournalTexture,
			PaperOrigin.X,
			PaperOrigin.Y,
			PaperSize.X,
			PaperSize.Y,
			0.0f,
			0.0f,
			1.0f,
			1.0f,
			FLinearColor(0.79f, 0.77f, 0.70f, 1.0f),
			BLEND_Opaque);
	}
	else
	{
		DrawRect(
			FLinearColor(0.73f, 0.70f, 0.62f, 1.0f),
			PaperOrigin.X,
			PaperOrigin.Y,
			PaperSize.X,
			PaperSize.Y);
	}
	// The texture carries tactile variation; this wash makes the runtime text
	// pass contrast at 720p without bleaching that grain into a generic panel.
	DrawRect(
		FLinearColor(0.08f, 0.075f, 0.06f, 0.10f),
		PaperOrigin.X,
		PaperOrigin.Y,
		PaperSize.X,
		PaperSize.Y);

	const FLinearColor Ink(0.075f, 0.070f, 0.060f, 0.98f);
	const FLinearColor FaintInk(0.18f, 0.19f, 0.18f, 0.78f);
	const FLinearColor BlueRule(0.20f, 0.29f, 0.31f, 0.56f);
	const FLinearColor OxideRed(0.43f, 0.12f, 0.09f, 0.82f);
	auto DrawPaperText = [this](
		const FText& Text,
		const FVector2D& Position,
		const FLinearColor& Color,
		const EIGHudTextRole TextRole,
		const float TextScale)
	{
		UFont* Font = GetFontForRole(TextRole);
		if (!Canvas || !Font || Text.IsEmpty())
		{
			return;
		}
		const float SafeScale = FMath::Max(0.5f, TextScale);
		FCanvasTextItem Item(Position, Text, Font, Color);
		Item.Scale = FVector2D(SafeScale);
		Canvas->DrawItem(Item);
		if (bLayoutValidationEnabled)
		{
			float Width = 0.0f;
			float Height = 0.0f;
			Canvas->StrLen(Font, Text.ToString(), Width, Height, true);
			RecordLayoutValidationRect(
				Position,
				Position + FVector2D(Width * SafeScale, Height * SafeScale));
		}
	};
	auto DrawCenteredPaperText = [this, &DrawPaperText](
		const FText& Text,
		const float Y,
		const FLinearColor& Color,
		const EIGHudTextRole TextRole,
		const float TextScale)
	{
		UFont* Font = GetFontForRole(TextRole);
		float Width = 0.0f;
		float Height = 0.0f;
		if (!Canvas || !Font || Text.IsEmpty())
		{
			return;
		}
		Canvas->StrLen(Font, Text.ToString(), Width, Height, true);
		DrawPaperText(
			Text,
			FVector2D((Canvas->ClipX - Width * TextScale) * 0.5f, Y),
			Color,
			TextRole,
			TextScale);
	};
	const float OuterMargin = 30.0f * ResolutionScale;
	const float HeaderTop = PaperOrigin.Y + 20.0f * ResolutionScale;
	DrawPaperText(
		SupportsKorean()
			? NSLOCTEXT("IGHUD", "MissingFloorJournalTitle", "듣는 것들")
			: FText::FromString(TEXT("THINGS HEARD")),
		FVector2D(PaperOrigin.X + OuterMargin, HeaderTop),
		Ink,
		EIGHudTextRole::Objective,
		1.08f);
	DrawPaperText(
		SupportsKorean()
			? NSLOCTEXT(
				"IGHUD",
				"MissingFloorJournalSubtitle",
				"달빛빌라 · 관찰 기록")
			: FText::FromString(TEXT("DALBIT VILLA · OBSERVATION LOG")),
		FVector2D(
			PaperOrigin.X + OuterMargin,
			HeaderTop + 27.0f * ResolutionScale),
		FaintInk,
		EIGHudTextRole::Hint,
		0.78f);

	const UGameInstance* GameInstance = GetGameInstance();
	const UIGMissingFloorNarrativeSubsystem* Narrative = GameInstance
		? GameInstance->GetSubsystem<UIGMissingFloorNarrativeSubsystem>()
		: nullptr;
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const float UserTextScale = Accessibility
		? Accessibility->GetCaptionSizeScale()
		: 1.0f;
	TSet<FName> ObservedSources;
	if (Narrative)
	{
		for (const FIGMissingFloorTruthRecord& Record :
			Narrative->GetSnapshot().Truths)
		{
			for (const FName SourceId : Record.SourceIds)
			{
				ObservedSources.Add(SourceId);
			}
		}
	}

	TArray<const IGHorrorHUD::FJournalEntryDefinition*> Lanes[3];
	for (const IGHorrorHUD::FJournalEntryDefinition& Entry :
		IGHorrorHUD::JournalEntries())
	{
		if (ObservedSources.Contains(Entry.SourceId))
		{
			Lanes[static_cast<int32>(Entry.Lane)].Add(&Entry);
		}
	}
	const int32 PageCount = FMath::Max(1, GetMissingFloorJournalPageCount());
	const int32 SafePage = FMath::Clamp(
		MissingFloorJournalPageIndex,
		0,
		PageCount - 1);
	const int32 ObservedCount =
		Lanes[0].Num() + Lanes[1].Num() + Lanes[2].Num();
	const FString CountText = SupportsKorean()
		? FString::Printf(
			TEXT("%d개 기록  ·  %d / %d"),
			ObservedCount,
			SafePage + 1,
			PageCount)
		: FString::Printf(
			TEXT("%d RECORDS  ·  %d / %d"),
			ObservedCount,
			SafePage + 1,
			PageCount);
	DrawPaperText(
		FText::FromString(CountText),
		FVector2D(
			PaperOrigin.X + PaperSize.X - 190.0f * ResolutionScale,
			HeaderTop + 5.0f * ResolutionScale),
		FaintInk,
		EIGHudTextRole::Hint,
		0.75f);

	const float ContentLeft = PaperOrigin.X + OuterMargin;
	const float ContentRight = PaperOrigin.X + PaperSize.X - OuterMargin;
	const float ContentTop = PaperOrigin.Y + 82.0f * ResolutionScale;
	const float FooterHeight = 43.0f * ResolutionScale;
	const float ContentBottom = PaperOrigin.Y + PaperSize.Y - FooterHeight;
	const float LaneGap = 13.0f * ResolutionScale;
	const float LaneWidth =
		(ContentRight - ContentLeft - LaneGap * 2.0f) / 3.0f;
	const float LaneHeaderHeight = 30.0f * ResolutionScale;
	const float CardGap = 9.0f * ResolutionScale;
	const int32 CardsPerLanePerPage = UserTextScale > 1.50f
		? 1
		: UserTextScale > 1.15f ? 2 : 3;
	const float CardHeight =
		(ContentBottom - ContentTop - LaneHeaderHeight
			- CardGap * FMath::Max(0, CardsPerLanePerPage - 1))
		/ CardsPerLanePerPage;
	const FText LaneTitles[3] = {
		SupportsKorean()
			? NSLOCTEXT("IGHUD", "JournalLaneAdministration", "행정")
			: FText::FromString(TEXT("RECORD")),
		SupportsKorean()
			? NSLOCTEXT("IGHUD", "JournalLaneLife", "생활")
			: FText::FromString(TEXT("LIFE")),
		SupportsKorean()
			? NSLOCTEXT("IGHUD", "JournalLanePersonal", "개인")
			: FText::FromString(TEXT("PERSONAL")),
	};

	for (int32 LaneIndex = 0; LaneIndex < 3; ++LaneIndex)
	{
		const float LaneX = ContentLeft + LaneIndex * (LaneWidth + LaneGap);
		DrawPaperText(
			LaneTitles[LaneIndex],
			FVector2D(LaneX + 3.0f * ResolutionScale, ContentTop),
			LaneIndex == 0 ? OxideRed : Ink,
			EIGHudTextRole::Speaker,
			0.90f);
		DrawRect(
			LaneIndex == 0 ? OxideRed : BlueRule,
			LaneX,
			ContentTop + 23.0f * ResolutionScale,
			LaneWidth,
			LaneIndex == 0 ? 1.4f : 1.0f);
		if (LaneIndex > 0)
		{
			DrawRect(
				FLinearColor(0.10f, 0.16f, 0.17f, 0.16f),
				LaneX - LaneGap * 0.5f,
				ContentTop,
				1.0f,
				ContentBottom - ContentTop);
		}
	}

	TMap<FName, FVector2D> VisibleCardCenters;
	for (int32 LaneIndex = 0; LaneIndex < 3; ++LaneIndex)
	{
		const int32 FirstEntry = SafePage * CardsPerLanePerPage;
		for (int32 Slot = 0; Slot < CardsPerLanePerPage; ++Slot)
		{
			const int32 EntryIndex = FirstEntry + Slot;
			if (!Lanes[LaneIndex].IsValidIndex(EntryIndex))
			{
				continue;
			}
			const float LaneX = ContentLeft + LaneIndex * (LaneWidth + LaneGap);
			const float CardY = ContentTop + LaneHeaderHeight
				+ Slot * (CardHeight + CardGap);
			VisibleCardCenters.Add(
				Lanes[LaneIndex][EntryIndex]->SourceId,
				FVector2D(LaneX + LaneWidth * 0.5f, CardY + CardHeight * 0.5f));
		}
	}

	// A faint pencil stroke is the only deduction visualization. It appears
	// only after the router confirms a truth and only when both source cards
	// are on this page; no line itself reveals a missing record.
	if (Narrative)
	{
		for (const FIGMissingFloorTruthRecord& Record :
			Narrative->GetSnapshot().Truths)
		{
			if (!Record.bConfirmed)
			{
				continue;
			}
			TArray<FVector2D> Points;
			for (const FName SourceId : Record.SourceIds)
			{
				if (const FVector2D* Center = VisibleCardCenters.Find(SourceId))
				{
					Points.Add(*Center);
				}
			}
			if (Points.Num() >= 2)
			{
				FVector2D Direction = Points[1] - Points[0];
				Direction.Normalize();
				const FVector2D Start = Points[0] + Direction * LaneWidth * 0.40f;
				const FVector2D End = Points[1] - Direction * LaneWidth * 0.40f;
				DrawLine(
					Start.X,
					Start.Y,
					End.X,
					End.Y,
					FLinearColor(0.34f, 0.10f, 0.08f, 0.42f),
					1.6f * ResolutionScale);
			}
		}
	}

	UFont* BodyFont = GetFontForRole(EIGHudTextRole::Dialogue);
	const float CardTextScale = FMath::Clamp(
		ResolutionScale * 0.92f * UserTextScale,
		0.64f,
		1.45f);
	for (int32 LaneIndex = 0; LaneIndex < 3; ++LaneIndex)
	{
		const int32 FirstEntry = SafePage * CardsPerLanePerPage;
		for (int32 Slot = 0; Slot < CardsPerLanePerPage; ++Slot)
		{
			const int32 EntryIndex = FirstEntry + Slot;
			if (!Lanes[LaneIndex].IsValidIndex(EntryIndex))
			{
				continue;
			}
			const IGHorrorHUD::FJournalEntryDefinition& Entry =
				*Lanes[LaneIndex][EntryIndex];
			const float LaneX = ContentLeft + LaneIndex * (LaneWidth + LaneGap);
			const float CardY = ContentTop + LaneHeaderHeight
				+ Slot * (CardHeight + CardGap);
			DrawRoundedHudSurface(
				FVector2D(LaneX, CardY),
				FVector2D(LaneWidth, CardHeight),
				8.0f * ResolutionScale,
				FLinearColor(0.83f, 0.81f, 0.74f, 0.76f));
			DrawRect(
				FLinearColor(0.16f, 0.18f, 0.17f, 0.18f),
				LaneX,
				CardY + CardHeight - 1.0f,
				LaneWidth,
				1.0f);

			const float Padding = 10.0f * ResolutionScale;
			const float ThumbnailSize = FMath::Clamp(
				CardHeight * 0.38f,
				44.0f * ResolutionScale,
				82.0f * ResolutionScale);
			if (UTexture2D* Thumbnail = GetMissingFloorJournalThumbnail(
				static_cast<int32>(Entry.Thumbnail)))
			{
				DrawTexture(
					Thumbnail,
					LaneX + Padding,
					CardY + Padding,
					ThumbnailSize,
					ThumbnailSize,
					0.08f,
					0.08f,
					0.84f,
					0.84f,
					FLinearColor(0.57f, 0.56f, 0.51f, 0.90f),
					BLEND_Opaque);
				DrawRect(
					FLinearColor(0.08f, 0.08f, 0.07f, 0.26f),
					LaneX + Padding,
					CardY + Padding + ThumbnailSize - 1.0f,
					ThumbnailSize,
					1.0f);
			}

			const float TextX = LaneX + Padding + ThumbnailSize
				+ 9.0f * ResolutionScale;
			const float TextWidth = LaneWidth - (TextX - LaneX) - Padding;
			DrawPaperText(
				FText::FromString(Entry.Title),
				FVector2D(TextX, CardY + Padding - 1.0f * ResolutionScale),
				Ink,
				EIGHudTextRole::Speaker,
				CardTextScale * 0.86f);

			TArray<FString> ExcerptLines;
			FString ExcerptRemainder;
			WrapHudText(
				Entry.Excerpt,
				BodyFont,
				CardTextScale * 0.72f,
				TextWidth,
				2,
				ExcerptLines,
				ExcerptRemainder);
			float TextY = CardY + Padding + 23.0f * ResolutionScale;
			for (const FString& Line : ExcerptLines)
			{
				DrawPaperText(
					FText::FromString(Line),
					FVector2D(TextX, TextY),
					FaintInk,
					EIGHudTextRole::Dialogue,
					CardTextScale * 0.72f);
				TextY += 17.0f * ResolutionScale;
			}
			DrawPaperText(
				FText::FromString(Entry.WhereWhen),
				FVector2D(
					LaneX + Padding,
					CardY + CardHeight - 22.0f * ResolutionScale),
				FaintInk,
				EIGHudTextRole::Hint,
				CardTextScale * 0.62f);
		}
	}

	if (ObservedCount == 0)
	{
		DrawCenteredPaperText(
			SupportsKorean()
				? NSLOCTEXT(
					"IGHUD",
					"MissingFloorJournalEmpty",
					"아직 옮겨 적은 것이 없다.")
				: FText::FromString(TEXT("NOTHING HAS BEEN COPIED DOWN YET.")),
			ContentTop + (ContentBottom - ContentTop) * 0.48f,
			FaintInk,
			EIGHudTextRole::Dialogue,
			0.90f);
	}

	DrawCenteredPaperText(
		SupportsKorean()
			? bUsingGamepad
				? NSLOCTEXT(
					"IGHUD",
					"MissingFloorJournalControlsGamepad",
					"D-pad  기록 넘기기  ·  Y 닫기")
				: NSLOCTEXT(
					"IGHUD",
					"MissingFloorJournalControlsKeyboard",
					"← / →  기록 넘기기  ·  Tab 닫기")
			: FText::FromString(
				bUsingGamepad
					? TEXT("D-PAD PAGES  ·  Y CLOSE")
					: TEXT("LEFT / RIGHT PAGES  ·  TAB CLOSE")),
		PaperOrigin.Y + PaperSize.Y - 29.0f * ResolutionScale,
		FaintInk,
		EIGHudTextRole::Hint,
		0.78f);
	RecordLayoutValidationRect(PaperOrigin, PaperOrigin + PaperSize);
}

void AIGHorrorHUD::DrawAudioCalibrationPanel()
{
	if (!Canvas)
	{
		return;
	}
	const bool bKorean = SupportsKorean();
	const float Scale = FMath::Clamp(
		FMath::Min(Canvas->ClipY / 1080.0f, Canvas->ClipX / 1920.0f),
		0.67f,
		2.0f);
	const FVector2D PanelSize(
		FMath::Min(Canvas->ClipX * 0.76f, 980.0f * Scale),
		FMath::Min(Canvas->ClipY * 0.70f, 690.0f * Scale));
	const FVector2D PanelOrigin(
		(Canvas->ClipX - PanelSize.X) * 0.5f,
		(Canvas->ClipY - PanelSize.Y) * 0.5f + 12.0f * Scale);

	DrawRoundedHudSurface(
		PanelOrigin,
		PanelSize,
		18.0f * Scale,
		FLinearColor(0.016f, 0.019f, 0.020f, 0.98f));
	if (AudioCalibrationWallTexture
		&& AudioCalibrationWallTexture->GetResource())
	{
		FCanvasTileItem Wall(
			PanelOrigin + FVector2D(2.0f, 2.0f) * Scale,
			AudioCalibrationWallTexture->GetResource(),
			PanelSize - FVector2D(4.0f, 4.0f) * Scale,
			FLinearColor(0.60f, 0.62f, 0.63f, 0.34f));
		Wall.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Wall);
	}
	FCanvasTileItem Divider(
		PanelOrigin + FVector2D(PanelSize.X * 0.50f, 122.0f * Scale),
		FVector2D(1.0f, PanelSize.Y - 168.0f * Scale),
		FLinearColor(0.42f, 0.45f, 0.45f, 0.24f));
	Divider.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Divider);

	DrawCenteredText(
		bKorean
			? NSLOCTEXT("IGHUD", "AudioCalibrationTitle", "소리 · 밝기 보정")
			: FText::FromString(TEXT("AUDIO + BRIGHTNESS CALIBRATION")),
		PanelOrigin.Y + 30.0f * Scale,
		IGHorrorHUD::PaleGray,
		EIGHudTextRole::Objective,
		1.08f * Scale);
	DrawCenteredText(
		bSystemMenuAudioCalibrationFirstRun
			? bKorean
				? NSLOCTEXT(
					"IGHUD", "AudioCalibrationFirstRunSubtitle",
					"없는 층은 소리로 먼저 드러납니다. 시작 전에 한 번만 맞춰 주세요.")
				: FText::FromString(
					TEXT("THE MISSING FLOOR IS HEARD FIRST. TUNE IT ONCE BEFORE PLAY."))
			: bKorean
				? NSLOCTEXT(
					"IGHUD", "AudioCalibrationSubtitle",
					"플레이 환경이 바뀌었다면 여기서 다시 맞출 수 있습니다.")
				: FText::FromString(
					TEXT("RECALIBRATE WHEN YOUR HEADPHONES OR DISPLAY CHANGE.")),
		PanelOrigin.Y + 74.0f * Scale,
		IGHorrorHUD::MutedGray,
		EIGHudTextRole::Hint,
		0.90f * Scale);

	const float LeftX = PanelOrigin.X + 54.0f * Scale;
	const float RightX = PanelOrigin.X + PanelSize.X * 0.55f;
	const float ContentY = PanelOrigin.Y + 144.0f * Scale;
	DrawLeftAlignedText(
		bKorean
			? NSLOCTEXT("IGHUD", "AudioCalibrationKnockLabel", "위쪽 노크")
			: FText::FromString(TEXT("KNOCK ABOVE")),
		FVector2D(LeftX, ContentY),
		IGHorrorHUD::ThoughtBlue,
		EIGHudTextRole::Prompt,
		Scale);
	DrawLeftAlignedText(
		bKorean
			? NSLOCTEXT(
				"IGHUD", "AudioCalibrationKnockInstruction",
				"노크가 겨우 들리면서\n위쪽에서 온다고 느껴질 정도로 맞추세요.")
			: FText::FromString(
				TEXT("LOWER IT UNTIL THE KNOCK IS BARELY AUDIBLE\nAND STILL FEELS ABOVE YOU.")),
		FVector2D(LeftX, ContentY + 38.0f * Scale),
		IGHorrorHUD::PaleGray,
		EIGHudTextRole::Hint,
		0.86f * Scale);

	const float MeterY = ContentY + 116.0f * Scale;
	const float MeterGap = 10.0f * Scale;
	const float MeterWidth = FMath::Min(
		38.0f * Scale,
		(PanelSize.X * 0.39f - MeterGap * 6.0f) / 7.0f);
	for (int32 Step = 0; Step < 7; ++Step)
	{
		const bool bFilled = Step <= AudioCalibrationVolumeStep;
		FCanvasTileItem Bar(
			FVector2D(LeftX + Step * (MeterWidth + MeterGap), MeterY),
			FVector2D(MeterWidth, 8.0f * Scale),
			bFilled
				? FLinearColor(0.62f, 0.70f, 0.72f, 0.92f)
				: FLinearColor(0.20f, 0.22f, 0.22f, 0.72f));
		Bar.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Bar);
	}

	DrawLeftAlignedText(
		bKorean
			? NSLOCTEXT("IGHUD", "AudioCalibrationShadowLabel", "어두운 곳의 경계")
			: FText::FromString(TEXT("SHADOW DETAIL")),
		FVector2D(RightX, ContentY),
		IGHorrorHUD::ThoughtBlue,
		EIGHudTextRole::Prompt,
		Scale);
	DrawLeftAlignedText(
		bKorean
			? NSLOCTEXT(
				"IGHUD", "AudioCalibrationShadowInstruction",
				"가운데 칸은 겨우 보이고\n왼쪽 칸은 배경에 묻히게 맞추세요.")
			: FText::FromString(
				TEXT("THE MIDDLE PATCH SHOULD BE BARELY VISIBLE;\nTHE LEFT PATCH SHOULD DISAPPEAR.")),
		FVector2D(RightX, ContentY + 38.0f * Scale),
		IGHorrorHUD::PaleGray,
		EIGHudTextRole::Hint,
		0.86f * Scale);
	const FLinearColor ShadowPatches[] =
	{
		FLinearColor(0.006f, 0.007f, 0.008f, 1.0f),
		FLinearColor(0.018f, 0.020f, 0.021f, 1.0f),
		FLinearColor(0.042f, 0.045f, 0.046f, 1.0f)
	};
	// 첫 칸과 같은 기준 블랙을 아래에 이어 붙인다. 벽면 무늬의 경계로
	// 첫 칸을 찾는 편법을 막고, 실제로 둘째 계조가 보이는지만 판단하게 한다.
	FCanvasTileItem ShadowBacking(
		FVector2D(RightX, ContentY + 100.0f * Scale),
		FVector2D(242.0f, 70.0f) * Scale,
		ShadowPatches[0]);
	ShadowBacking.BlendMode = SE_BLEND_Opaque;
	Canvas->DrawItem(ShadowBacking);
	for (int32 PatchIndex = 0; PatchIndex < 3; ++PatchIndex)
	{
		FCanvasTileItem Patch(
			FVector2D(
				RightX + PatchIndex * 86.0f * Scale,
				ContentY + 100.0f * Scale),
			FVector2D(70.0f, 70.0f) * Scale,
			ShadowPatches[PatchIndex]);
		Patch.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(Patch);
	}

	const FString Labels[] =
	{
		bKorean ? TEXT("노크 음량") : TEXT("KNOCK VOLUME"),
		bKorean ? TEXT("음악") : TEXT("MUSIC"),
		bKorean ? TEXT("환경음") : TEXT("AMBIENCE"),
		bKorean ? TEXT("듣는 방식") : TEXT("LISTENING ON"),
		bKorean ? TEXT("화면 밝기") : TEXT("DISPLAY BRIGHTNESS"),
		bKorean ? TEXT("노크 다시 듣기") : TEXT("PLAY KNOCK AGAIN"),
		bSystemMenuAudioCalibrationFirstRun
			? bKorean ? TEXT("저장하고 타이틀로") : TEXT("SAVE AND CONTINUE")
			: bKorean ? TEXT("저장하고 돌아가기") : TEXT("SAVE AND BACK")
	};
	const float RowStartY = PanelOrigin.Y + PanelSize.Y - 308.0f * Scale;
	const float RowSpacing = 42.0f * Scale;
	for (int32 Row = 0; Row < UE_ARRAY_COUNT(Labels); ++Row)
	{
		const bool bSelected = Row == AudioCalibrationSelectedRow;
		FString Label = FString(bSelected ? TEXT(">  ") : TEXT("   ")) + Labels[Row];
		if (Row == 0 || Row == 1 || Row == 2 || Row == 4)
		{
			const int32 Step =
				Row == 0 ? AudioCalibrationVolumeStep
				: Row == 1 ? SystemMenuAudioCalibrationMusicStep
				: Row == 2 ? SystemMenuAudioCalibrationAmbienceStep
				: AudioCalibrationBrightnessStep;
			const int32 Count = Row == 0 ? 7 : 5;
			FString Dots;
			for (int32 Dot = 0; Dot < Count; ++Dot)
			{
				Dots += Dot == Step ? TEXT("●") : TEXT("○");
			}
			Label += FString::Printf(TEXT("    < %s >"), *Dots);
		}
		else if (Row == 3)
		{
			Label += FString::Printf(
				TEXT("    < %s >"),
				bSystemMenuHeadphoneOutput
					? (bKorean ? TEXT("헤드폰") : TEXT("HEADPHONES"))
					: (bKorean ? TEXT("스피커") : TEXT("SPEAKERS")));
		}
		DrawCenteredText(
			FText::FromString(Label),
			RowStartY + Row * RowSpacing,
			bSelected ? IGHorrorHUD::RedAccent : IGHorrorHUD::PaleGray,
			bSelected ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint,
			0.92f * Scale);
	}
	if (AudioCalibrationSelectedRow == 1 || AudioCalibrationSelectedRow == 2)
	{
		DrawCenteredText(
			bKorean
				? NSLOCTEXT(
					"IGHUD", "AudioCalibrationBusNote",
					"존재가 내는 소리는 줄지 않습니다. 놓친 노크는 설정으로 되돌릴 수 없습니다.")
				: FText::FromString(
					TEXT("THE PRESENCE NEVER GETS QUIETER. A MISSED KNOCK CANNOT BE UNDONE.")),
			PanelOrigin.Y + PanelSize.Y - 52.0f * Scale,
			IGHorrorHUD::PaleGray,
			EIGHudTextRole::Hint,
			0.78f * Scale);
	}
	if (AudioCalibrationSelectedRow == 3)
	{
		DrawCenteredText(
			bKorean
				? bSystemMenuHeadphoneOutput
					? NSLOCTEXT(
						"IGHUD", "AudioCalibrationOutputHeadphones",
						"위아래에서 나는 소리를 그대로 씁니다. 이 게임이 기대하는 방식입니다.")
					: NSLOCTEXT(
						"IGHUD", "AudioCalibrationOutputSpeakers",
						"스피커에서는 위아래 구분이 흐려집니다. 자막을 켜면 방향을 함께 적습니다.")
				: FText::FromString(
					bSystemMenuHeadphoneOutput
						? TEXT("FULL VERTICAL IMAGING. THIS IS WHAT THE GAME EXPECTS.")
						: TEXT("SPEAKERS BLUR UP AND DOWN. SUBTITLES WILL NAME THE DIRECTION.")),
			PanelOrigin.Y + PanelSize.Y - 52.0f * Scale,
			IGHorrorHUD::PaleGray,
			EIGHudTextRole::Hint,
			0.78f * Scale);
	}
	DrawCenteredText(
		bKorean
			? bUsingGamepad
				? NSLOCTEXT(
					"IGHUD", "AudioCalibrationControlsGamepad",
					"D-pad 항목·조정  ·  A 선택  ·  B 취소")
				: NSLOCTEXT(
					"IGHUD", "AudioCalibrationControlsKeyboard",
					"방향키/WASD 항목·조정  ·  Enter 선택  ·  Esc 취소  ·  마우스 선택")
			: FText::FromString(
				bUsingGamepad
					? TEXT("D-PAD SELECT + ADJUST  |  A APPLY  |  B CANCEL")
					: TEXT("ARROWS/WASD ADJUST  |  ENTER APPLY  |  ESC CANCEL  |  MOUSE SELECT")),
		PanelOrigin.Y + PanelSize.Y - 26.0f * Scale,
		IGHorrorHUD::MutedGray,
		EIGHudTextRole::Hint,
		0.78f * Scale);
	RecordLayoutValidationRect(PanelOrigin, PanelOrigin + PanelSize);
}

void AIGHorrorHUD::DrawDisplaySettingsPanel()
{
	if (!Canvas)
	{
		return;
	}
	const bool bKorean = SupportsKorean();
	const IGSettingsMenuLayout::FPanelMetrics Metrics =
		IGSettingsMenuLayout::MakePanelMetrics(Canvas->ClipX, Canvas->ClipY);
	const float Scale = Metrics.Scale;
	const FString WindowModes[] =
	{
		bKorean ? TEXT("전체 화면") : TEXT("FULLSCREEN"),
		bKorean ? TEXT("테두리 없는 창") : TEXT("BORDERLESS"),
		bKorean ? TEXT("창 모드") : TEXT("WINDOWED")
	};
	const FString Resolutions[] =
	{
		TEXT("1280 x 720"),
		TEXT("1920 x 1080"),
		TEXT("2560 x 1440")
	};
	const FString Qualities[] =
	{
		bKorean ? TEXT("낮음") : TEXT("LOW"),
		bKorean ? TEXT("높음") : TEXT("HIGH")
	};
	const FString FrameLimits[] =
	{
		TEXT("30 FPS"),
		TEXT("60 FPS"),
		bKorean ? TEXT("제한 없음") : TEXT("UNLIMITED")
	};
	const FString Labels[] =
	{
		bKorean ? TEXT("화면 모드") : TEXT("DISPLAY MODE"),
		bKorean ? TEXT("해상도") : TEXT("RESOLUTION"),
		bKorean ? TEXT("그래픽 품질") : TEXT("GRAPHICS QUALITY"),
		bKorean ? TEXT("수직 동기화") : TEXT("V-SYNC"),
		bKorean ? TEXT("프레임 제한") : TEXT("FRAME LIMIT"),
		bKorean ? TEXT("접근성 설정") : TEXT("ACCESSIBILITY"),
		bKorean ? TEXT("소리 · 밝기 보정") : TEXT("AUDIO + BRIGHTNESS"),
		bKorean ? TEXT("조작 · 키 다시 묶기") : TEXT("CONTROLS + REBINDING"),
		bDisplaySettingsAwaitingConfirmation
			? bKorean ? TEXT("이 설정 유지") : TEXT("KEEP THESE SETTINGS")
			: bKorean ? TEXT("화면 설정 다시 적용") : TEXT("REAPPLY DISPLAY"),
		bDisplaySettingsAwaitingConfirmation
			? bKorean ? TEXT("이전 설정으로 되돌리기") : TEXT("REVERT SETTINGS")
			: bKorean ? TEXT("돌아가기") : TEXT("BACK")
	};
	const FString Values[] =
	{
		WindowModes[DisplayWindowModeIndex],
		Resolutions[DisplayResolutionIndex],
		Qualities[DisplayQualityIndex],
		bKorean
			? FString(bSystemMenuVSync ? TEXT("켬") : TEXT("끔"))
			: FString(bSystemMenuVSync ? TEXT("ON") : TEXT("OFF")),
		FrameLimits[DisplayFrameLimitIndex],
		bKorean ? TEXT("열기") : TEXT("OPEN"),
		bKorean ? TEXT("열기") : TEXT("OPEN"),
		bKorean ? TEXT("열기") : TEXT("OPEN"),
		FString(),
		FString()
	};
	const FString Descriptions[] =
	{
		bKorean
			? TEXT("전체 화면, 테두리 없는 창, 창 모드 중 출력 방식을 선택합니다. 바꾸면 바로 적용되고, 10초 안에 유지할지 정합니다.")
			: TEXT("CHOOSE FULLSCREEN, BORDERLESS, OR WINDOWED OUTPUT. IT APPLIES AT ONCE; YOU HAVE 10 SECONDS TO KEEP IT."),
		bKorean
			? TEXT("화면에 출력할 픽셀 수를 정합니다. 디스플레이의 기본 해상도와 같을 때 가장 선명합니다.")
			: TEXT("SETS THE OUTPUT PIXEL COUNT. MATCHING THE DISPLAY'S NATIVE RESOLUTION GIVES THE SHARPEST IMAGE."),
		bKorean
			? TEXT("그림자, 후처리, 반사 품질을 한 번에 조정합니다. 낮음은 GPU 부하와 프레임 흔들림을 줄입니다.")
			: TEXT("CHANGES SHADOW, POST-PROCESS, AND REFLECTION QUALITY TOGETHER. LOW REDUCES GPU LOAD."),
		bKorean
			? TEXT("모니터 주사에 프레임 출력을 맞춰 화면 찢어짐을 줄입니다. 입력 지연은 환경에 따라 소폭 늘 수 있습니다.")
			: TEXT("SYNCHRONIZES FRAMES TO THE DISPLAY TO REDUCE TEARING, WITH A POSSIBLE SMALL LATENCY COST."),
		bKorean
			? TEXT("초당 최대 프레임 수를 정합니다. 안정적인 상한은 발열과 순간적인 프레임 편차를 줄이는 데 도움이 됩니다.")
			: TEXT("SETS THE MAXIMUM FRAME RATE. A STABLE CAP CAN REDUCE HEAT AND FRAME-TIME VARIANCE."),
		bKorean
			? TEXT("자막, 움직임, 소리 방향, 입력 보조처럼 플레이 방식에 영향을 주는 항목을 엽니다.")
			: TEXT("OPENS CAPTION, MOTION, SOUND-DIRECTION, AND INPUT ASSISTANCE OPTIONS."),
		bKorean
			? TEXT("게임의 핵심인 위층 노크가 들리는 크기와 어두운 복도의 기준 밝기를 다시 맞춥니다.")
			: TEXT("RECALIBRATES THE UPSTAIRS KNOCK LEVEL AND THE REFERENCE BRIGHTNESS FOR DARK CORRIDORS."),
		bKorean
			? TEXT("달리기·앉기·두드리기 같은 동사를 다른 키로 옮깁니다. Esc와 F10은 나가는 길이라 고정입니다.")
			: TEXT("MOVES SPRINT, CROUCH, KNOCK AND THE REST ONTO OTHER KEYS. ESC AND F10 STAY FIXED."),
		bKorean
			? TEXT("항목을 바꾸면 그 자리에서 적용되고 저장됩니다. 이 줄은 같은 값을 한 번 더 적용할 때만 쓰입니다.")
			: TEXT("CHANGES APPLY AND SAVE AS YOU MAKE THEM. THIS ROW ONLY REAPPLIES THE SAME VALUES."),
		bKorean
			? TEXT("이전 화면으로 돌아갑니다.")
			: TEXT("RETURNS TO THE PREVIOUS SCREEN.")
	};
	const FString CategoryLabels[] =
	{
		bKorean ? TEXT("화면") : TEXT("DISPLAY"),
		bKorean ? TEXT("성능") : TEXT("PERFORMANCE"),
		bKorean ? TEXT("플레이 보조") : TEXT("PLAY ASSISTS"),
		bKorean ? TEXT("변경 사항") : TEXT("CHANGES")
	};

	DrawSettingsShell(
		Metrics,
		bKorean
			? NSLOCTEXT("IGHUD", "DisplaySettingsTitle", "화면 설정")
			: FText::FromString(TEXT("DISPLAY SETTINGS")),
		bKorean
			? NSLOCTEXT(
				"IGHUD",
				"DisplaySettingsSubtitle",
				"공포 연출의 가독성과 프레임 안정성을 함께 조정합니다.")
			: FText::FromString(
				TEXT("BALANCE HORROR LEGIBILITY WITH STABLE FRAME DELIVERY.")),
		bKorean
			? NSLOCTEXT(
				"IGHUD",
				"DisplayImmediateStatus",
				"바꾸는 즉시 적용되고 저장됩니다")
			: FText::FromString(TEXT("CHANGES APPLY AND SAVE INSTANTLY")));

	const int32 ActiveCategory = IGSettingsMenuLayout::FindCategoryForRow(
		DisplaySettingsSelectedRow,
		IGSettingsMenuLayout::DisplayCategoryCount,
		IGSettingsMenuLayout::GetDisplayCategory);
	const IGSettingsMenuLayout::FCategoryRange ActiveRange =
		IGSettingsMenuLayout::GetDisplayCategory(ActiveCategory);
	for (int32 Category = 0;
		Category < IGSettingsMenuLayout::DisplayCategoryCount;
		++Category)
	{
		DrawSettingsCategoryRow(
			Metrics,
			Category,
			FText::FromString(CategoryLabels[Category]),
			Category == ActiveCategory);
	}
	for (int32 LocalRow = 0; LocalRow < ActiveRange.RowCount; ++LocalRow)
	{
		const int32 Row = ActiveRange.FirstRow + LocalRow;
		DrawSettingsOptionRow(
			Metrics,
			LocalRow,
			FText::FromString(Labels[Row]),
			FText::FromString(Values[Row]),
			Row == DisplaySettingsSelectedRow,
			Row <= 4);
	}

	const float DetailTop = Metrics.OptionStartY
		+ ActiveRange.RowCount * Metrics.OptionRowHeight
		+ 22.0f * Scale;
	const float DetailBottom = Metrics.FooterTop - 22.0f * Scale;
	const float DetailHeight = FMath::Min(
		150.0f * Scale,
		DetailBottom - DetailTop);
	if (DetailTop < DetailBottom - 62.0f * Scale)
	{
		DrawRoundedHudSurface(
			FVector2D(Metrics.ContentLeft, DetailTop),
			FVector2D(
				Metrics.ContentRight - Metrics.ContentLeft,
				DetailHeight),
			8.0f * Scale,
			IGHorrorHUD::SettingsRaised);
		DrawLeftAlignedText(
			bKorean
				? NSLOCTEXT("IGHUD", "DisplaySettingEffect", "선택한 항목")
				: FText::FromString(TEXT("SELECTED OPTION")),
			FVector2D(
				Metrics.ContentLeft + 18.0f * Scale,
				DetailTop + 15.0f * Scale),
			IGHorrorHUD::SettingsSecondary,
			EIGHudTextRole::Hint,
			0.72f * Scale);
		DrawSettingsDetailText(
			Descriptions[DisplaySettingsSelectedRow],
			FVector2D(
				Metrics.ContentLeft + 18.0f * Scale,
				DetailTop + 45.0f * Scale),
			Metrics.ContentRight - Metrics.ContentLeft - 36.0f * Scale,
			0.84f * Scale,
			IGHorrorHUD::SettingsPrimary);
	}

	if (bDisplaySettingsAwaitingConfirmation)
	{
		DrawLeftAlignedText(
			bKorean
				? FText::Format(
					NSLOCTEXT(
						"IGHUD",
						"DisplaySettingsConfirmCountdown",
						"이 화면 설정을 유지할까요? {0}초 뒤 자동으로 되돌립니다."),
					FText::AsNumber(DisplayConfirmationSecondsRemaining))
				: FText::Format(
					FText::FromString(
						TEXT("KEEP THESE DISPLAY SETTINGS? REVERTING IN {0} SECONDS.")),
					FText::AsNumber(DisplayConfirmationSecondsRemaining)),
			FVector2D(
				Metrics.ContentLeft + 18.0f * Scale,
				DetailTop + DetailHeight - 31.0f * Scale),
			IGHorrorHUD::SettingsAccent,
			EIGHudTextRole::Hint,
			0.82f * Scale);
	}
	else if (bDisplaySettingsApplied)
	{
		DrawLeftAlignedText(
			bKorean
				? NSLOCTEXT("IGHUD", "DisplaySettingsApplied", "설정을 적용했습니다.")
				: FText::FromString(TEXT("SETTINGS APPLIED.")),
			FVector2D(
				Metrics.ContentLeft + 18.0f * Scale,
				DetailTop + DetailHeight - 31.0f * Scale),
			IGHorrorHUD::SettingsSuccess,
			EIGHudTextRole::Hint,
			0.82f * Scale);
	}
	else if (!SystemMenuStatusText.IsEmpty())
	{
		DrawLeftAlignedText(
			SystemMenuStatusText,
			FVector2D(
				Metrics.ContentLeft + 18.0f * Scale,
				DetailTop + DetailHeight - 31.0f * Scale),
			bSystemMenuStatusIsError
				? IGHorrorHUD::SettingsAccent
				: IGHorrorHUD::SettingsPrimary,
			EIGHudTextRole::Hint,
			0.82f * Scale);
	}

	DrawSettingsFooterText(
		Metrics,
		bDisplaySettingsAwaitingConfirmation
			? bUsingGamepad
				? bKorean
					? NSLOCTEXT(
						"IGHUD",
						"DisplaySettingsConfirmControlsGamepad",
						"A 유지  ·  B/View 자동 복원  ·  아래 항목에서 되돌리기")
					: FText::FromString(
						TEXT("A KEEP  |  B/VIEW AUTO-REVERT  |  SELECT REVERT BELOW"))
				: bKorean
					? NSLOCTEXT(
						"IGHUD",
						"DisplaySettingsConfirmControlsKeyboard",
						"Enter/클릭 유지  ·  Esc 자동 복원  ·  아래 항목에서 되돌리기")
					: FText::FromString(
						TEXT("ENTER/CLICK KEEP  |  ESC AUTO-REVERT  |  SELECT REVERT BELOW"))
			: bKorean
			? bUsingGamepad
				? NSLOCTEXT(
					"IGHUD",
					"DisplaySettingsControlsGamepad",
					"D-pad 이동·값 변경  ·  A 선택  ·  B/View 취소")
				: NSLOCTEXT(
					"IGHUD",
					"DisplaySettingsControlsKeyboard",
					"방향키/WASD 이동·값 변경  ·  Enter 선택  ·  Esc 취소  ·  마우스 선택")
			: FText::FromString(
				bUsingGamepad
					? TEXT("D-PAD SELECT + CHANGE  |  A APPLY  |  B/VIEW CANCEL")
					: TEXT("ARROWS/WASD CHANGE  |  ENTER APPLY  |  ESC CANCEL  |  MOUSE SELECT")));
}

namespace
{
	FText MakeBindingColumnHeader(
		const bool bKorean,
		const bool bGamepadColumn,
		const bool bSelectedIsGamepad)
	{
		// 고른 칸에 꺾쇠를 붙인다. 색맹 프로필에서도 어느 칸인지 읽힌다.
		const bool bSelected = bGamepadColumn == bSelectedIsGamepad;
		const FText Base = bGamepadColumn
			? (bKorean
				? NSLOCTEXT("IGHUD", "KeyBindingsColumnPad", "게임패드")
				: FText::FromString(TEXT("GAMEPAD")))
			: (bKorean
				? NSLOCTEXT("IGHUD", "KeyBindingsColumnKeys", "키보드")
				: FText::FromString(TEXT("KEYBOARD")));
		return bSelected
			? FText::Format(
				NSLOCTEXT("IGHUD", "KeyBindingsColumnActive", "[ {0} ]"), Base)
			: Base;
	}
}

void AIGHorrorHUD::DrawKeyBindingsPanel()
{
	if (!Canvas)
	{
		return;
	}
	const bool bKorean = SupportsKorean();
	const bool bPadHints = bUsingGamepad;
	const IGFrontendMenuLayout::FMetrics Metrics =
		IGFrontendMenuLayout::MakeMetrics(Canvas->ClipX, Canvas->ClipY);
	const float Scale = Metrics.Scale;
	const UGameInstance* GameInstance = GetGameInstance();
	const UIGInputBindingSubsystem* Bindings = GameInstance
		? GameInstance->GetSubsystem<UIGInputBindingSubsystem>()
		: nullptr;
	if (!Bindings)
	{
		return;
	}

	DrawLeftAlignedText(
		bKorean
			? NSLOCTEXT("IGHUD", "KeyBindingsContext", "설정 · 조작")
			: FText::FromString(TEXT("SETTINGS · CONTROLS")),
		FVector2D(Metrics.ContentLeft, Metrics.TitleTop),
		IGHorrorHUD::SettingsSecondary,
		EIGHudTextRole::Hint,
		0.82f * Scale);
	DrawLeftAlignedText(
		bKorean
			? NSLOCTEXT("IGHUD", "KeyBindingsTitle", "키 다시 묶기")
			: FText::FromString(TEXT("REBIND CONTROLS")),
		FVector2D(Metrics.ContentLeft, Metrics.TitleTop + 26.0f * Scale),
		IGHorrorHUD::SettingsPrimary,
		EIGHudTextRole::Prompt,
		1.05f * Scale);

	// 시점 셋. 동사 목록보다 위에 둔다 — 1인칭에서 손에 가장 먼저 걸리는
	// 것이 감도이고, 여기서 못 맞추면 그 뒤의 어떤 키 배열도 소용없다.
	const float LookRowStride = 28.0f * Scale;
	const float LookFirstY = Metrics.TitleTop + 62.0f * Scale;
	const float ValueX = Metrics.ContentLeft + 300.0f * Scale;
	FNumberFormattingOptions TwoDecimals;
	TwoDecimals.MinimumFractionalDigits = 2;
	TwoDecimals.MaximumFractionalDigits = 2;
	for (int32 Row = 0; Row < UIGInputBindingSubsystem::LookRowCount; ++Row)
	{
		const bool bSelected = Row == SystemMenuKeyBindingSelection;
		const float RowY = LookFirstY + Row * LookRowStride;
		FText Label;
		FText Value;
		switch (Row)
		{
		case 0:
			Label = bKorean
				? NSLOCTEXT("IGHUD", "LookMouse", "마우스 감도")
				: FText::FromString(TEXT("MOUSE SENSITIVITY"));
			Value = FText::AsNumber(Bindings->GetMouseSensitivity(), &TwoDecimals);
			break;
		case 1:
			Label = bKorean
				? NSLOCTEXT("IGHUD", "LookPad", "패드 시점 감도")
				: FText::FromString(TEXT("GAMEPAD LOOK SENSITIVITY"));
			Value = FText::AsNumber(Bindings->GetGamepadSensitivity(), &TwoDecimals);
			break;
		default:
			Label = bKorean
				? NSLOCTEXT("IGHUD", "LookInvert", "시점 상하 반전")
				: FText::FromString(TEXT("INVERT LOOK Y"));
			Value = Bindings->IsLookInverted()
				? (bKorean
					? NSLOCTEXT("IGHUD", "LookInvertOn", "켬")
					: FText::FromString(TEXT("ON")))
				: (bKorean
					? NSLOCTEXT("IGHUD", "LookInvertOff", "끔")
					: FText::FromString(TEXT("OFF")));
			break;
		}
		DrawLeftAlignedText(
			bSelected ? FText::FromString(TEXT(">")) : FText::GetEmpty(),
			FVector2D(Metrics.ContentLeft - 16.0f * Scale, RowY),
			IGHorrorHUD::SettingsAccent,
			EIGHudTextRole::Hint,
			0.88f * Scale);
		DrawLeftAlignedText(
			Label,
			FVector2D(Metrics.ContentLeft, RowY),
			bSelected ? IGHorrorHUD::SettingsPrimary : IGHorrorHUD::SettingsSecondary,
			EIGHudTextRole::Hint,
			0.88f * Scale);
		// 고른 행에만 꺾쇠를 붙여 좌우로 움직이는 행임을 알린다.
		DrawLeftAlignedText(
			bSelected
				? FText::Format(
					NSLOCTEXT("IGHUD", "LookValueSelected", "‹ {0} ›"),
					Value)
				: Value,
			FVector2D(ValueX, RowY),
			bSelected ? IGHorrorHUD::SettingsPrimary : IGHorrorHUD::SettingsSecondary,
			EIGHudTextRole::Hint,
			0.88f * Scale);
	}

	// 열 머리글. 지금 고른 칸을 밝게 둔다 — 색만으로 알리지 않기 위해
	// 고른 칸에는 꺾쇠를 함께 그린다(§24 즉시 차단 22).
	const float ColumnKeyboardX = Metrics.ContentLeft + 300.0f * Scale;
	const float ColumnGamepadX = Metrics.ContentLeft + 470.0f * Scale;
	const float HeaderY = LookFirstY
		+ UIGInputBindingSubsystem::LookRowCount * LookRowStride
		+ 16.0f * Scale;
	DrawLeftAlignedText(
		MakeBindingColumnHeader(bKorean, false, bSystemMenuKeyBindingColumnGamepad),
		FVector2D(ColumnKeyboardX, HeaderY),
		bSystemMenuKeyBindingColumnGamepad
			? IGHorrorHUD::SettingsSecondary
			: IGHorrorHUD::SettingsAccent,
		EIGHudTextRole::Hint,
		0.84f * Scale);
	DrawLeftAlignedText(
		MakeBindingColumnHeader(bKorean, true, bSystemMenuKeyBindingColumnGamepad),
		FVector2D(ColumnGamepadX, HeaderY),
		bSystemMenuKeyBindingColumnGamepad
			? IGHorrorHUD::SettingsAccent
			: IGHorrorHUD::SettingsSecondary,
		EIGHudTextRole::Hint,
		0.84f * Scale);

	const int32 ActionCount = UIGInputBindingSubsystem::GetActionCount();
	const float RowStride = 30.0f * Scale;
	const float FirstRowY = HeaderY + 26.0f * Scale;
	for (int32 Row = 0; Row < ActionCount; ++Row)
	{
		const FIGBindableActionInfo& Info =
			UIGInputBindingSubsystem::GetActionInfo(Row);
		const bool bSelected =
			Row + UIGInputBindingSubsystem::LookRowCount
				== SystemMenuKeyBindingSelection;
		const float RowY = FirstRowY + Row * RowStride;
		// 선택 표시는 색이 아니라 모양이다.
		DrawLeftAlignedText(
			bSelected
				? FText::FromString(TEXT(">"))
				: FText::GetEmpty(),
			FVector2D(Metrics.ContentLeft - 16.0f * Scale, RowY),
			IGHorrorHUD::SettingsAccent,
			EIGHudTextRole::Hint,
			0.88f * Scale);
		DrawLeftAlignedText(
			Info.Label,
			FVector2D(Metrics.ContentLeft, RowY),
			bSelected ? IGHorrorHUD::SettingsPrimary : IGHorrorHUD::SettingsSecondary,
			EIGHudTextRole::Hint,
			0.88f * Scale);

		for (int32 Column = 0; Column < 2; ++Column)
		{
			const bool bGamepadColumn = Column == 1;
			const FKey Bound = Bindings->GetBoundKey(Row, bGamepadColumn);
			const bool bCapturingHere = bSystemMenuKeyBindingCapturing
				&& bSelected
				&& bGamepadColumn == bSystemMenuKeyBindingColumnGamepad;
			FText Shown;
			if (bCapturingHere)
			{
				Shown = bKorean
					? NSLOCTEXT("IGHUD", "KeyBindingsPressNow", "[ 누르세요 ]")
					: FText::FromString(TEXT("[ PRESS ]"));
			}
			else if (!Bound.IsValid())
			{
				// 빈 칸은 「없음」이라고 적는다. 비워 두면 고장으로 읽힌다.
				Shown = bKorean
					? NSLOCTEXT("IGHUD", "KeyBindingsNone", "없음")
					: FText::FromString(TEXT("NONE"));
			}
			else
			{
				Shown = Bound.GetDisplayName();
			}
			const bool bOverridden = !Bindings->IsDefaultBinding(Row, bGamepadColumn);
			DrawLeftAlignedText(
				bOverridden
					? FText::Format(
						NSLOCTEXT("IGHUD", "KeyBindingsChanged", "{0} *"),
						Shown)
					: Shown,
				FVector2D(bGamepadColumn ? ColumnGamepadX : ColumnKeyboardX, RowY),
				bCapturingHere
					? IGHorrorHUD::SettingsAccent
					: bSelected
						? IGHorrorHUD::SettingsPrimary
						: IGHorrorHUD::SettingsSecondary,
				EIGHudTextRole::Hint,
				0.88f * Scale);
		}
	}

	// 마지막 행: 전부 기본값으로.
	const int32 ResetRow =
		ActionCount + UIGInputBindingSubsystem::LookRowCount;
	const float ResetY = FirstRowY + ActionCount * RowStride + 8.0f * Scale;
	const bool bResetSelected = SystemMenuKeyBindingSelection == ResetRow;
	DrawLeftAlignedText(
		bResetSelected ? FText::FromString(TEXT(">")) : FText::GetEmpty(),
		FVector2D(Metrics.ContentLeft - 16.0f * Scale, ResetY),
		IGHorrorHUD::SettingsAccent,
		EIGHudTextRole::Hint,
		0.88f * Scale);
	DrawLeftAlignedText(
		bKorean
			? NSLOCTEXT("IGHUD", "KeyBindingsResetRow", "전부 기본값으로")
			: FText::FromString(TEXT("RESET ALL TO DEFAULTS")),
		FVector2D(Metrics.ContentLeft, ResetY),
		bResetSelected ? IGHorrorHUD::SettingsPrimary : IGHorrorHUD::SettingsSecondary,
		EIGHudTextRole::Hint,
		0.88f * Scale);

	// 고른 행의 설명. §18.1이 그 동사에 대해 말하는 것을 그대로 보여 준다.
	const float DetailY = ResetY + 34.0f * Scale;
	const int32 DetailAction =
		SystemMenuKeyBindingSelection - UIGInputBindingSubsystem::LookRowCount;
	if (SystemMenuKeyBindingSelection < UIGInputBindingSubsystem::LookRowCount)
	{
		DrawLeftAlignedText(
			bKorean
				? NSLOCTEXT(
					"IGHUD",
					"LookDetail",
					"마우스와 패드는 곡선이 달라 따로 맞춥니다. 좌우로 0.10씩.")
				: FText::FromString(
					TEXT("MOUSE AND PAD TUNE SEPARATELY. LEFT/RIGHT BY 0.10.")),
			FVector2D(Metrics.ContentLeft, DetailY),
			IGHorrorHUD::SettingsSecondary,
			EIGHudTextRole::Hint,
			0.82f * Scale);
	}
	else if (DetailAction >= 0 && DetailAction < ActionCount)
	{
		DrawLeftAlignedText(
			UIGInputBindingSubsystem::GetActionInfo(DetailAction).Description,
			FVector2D(Metrics.ContentLeft, DetailY),
			IGHorrorHUD::SettingsSecondary,
			EIGHudTextRole::Hint,
			0.82f * Scale);
	}
	if (!SystemMenuKeyBindingStatus.IsEmpty())
	{
		DrawLeftAlignedText(
			SystemMenuKeyBindingStatus,
			FVector2D(Metrics.ContentLeft, DetailY + 24.0f * Scale),
			bSystemMenuKeyBindingStatusIsError
				? IGHorrorHUD::SettingsAccent
				: IGHorrorHUD::SettingsSuccess,
			EIGHudTextRole::Hint,
			0.82f * Scale);
	}

	DrawLeftAlignedText(
		bKorean
			? NSLOCTEXT(
				"IGHUD",
				"KeyBindingsFixedNote",
				"Esc와 F10은 고정입니다 — 일시정지와 접근성으로 돌아갈 길은 남겨 둡니다.")
			: FText::FromString(
				TEXT("ESC AND F10 STAY FIXED SO YOU CAN ALWAYS GET BACK OUT.")),
		FVector2D(Metrics.ContentLeft, DetailY + 48.0f * Scale),
		IGHorrorHUD::SettingsSecondary,
		EIGHudTextRole::Hint,
		0.78f * Scale);

	DrawLeftAlignedText(
		bKorean
			? bPadHints
				? NSLOCTEXT(
					"IGHUD",
					"KeyBindingsControlsPad",
					"D-pad 상하 이동 · 좌우 값과 칸 · A 바꾸기 · B 돌아가기")
				: NSLOCTEXT(
					"IGHUD",
					"KeyBindingsControlsKeys",
					"방향키 상하 이동 · 좌우 값과 칸 · Enter 바꾸기 · Esc 돌아가기")
			: FText::FromString(
				bPadHints
					? TEXT("D-PAD MOVE  |  A REBIND  |  B BACK")
					: TEXT("ARROWS MOVE  |  ENTER REBIND  |  ESC BACK")),
		FVector2D(Metrics.ContentLeft, Metrics.FooterTop),
		IGHorrorHUD::SettingsSecondary,
		EIGHudTextRole::Hint,
		0.84f * Scale);
	RecordLayoutValidationRect(
		FVector2D(Metrics.ContentLeft, Metrics.TitleTop),
		FVector2D(
			Metrics.ContentLeft + Metrics.ContentWidth,
			FMath::Min(Metrics.FooterTop + 24.0f, Canvas->ClipY)));
}

void AIGHorrorHUD::DrawSystemMenuPanel()
{
	if (!Canvas)
	{
		return;
	}

	const bool bKorean = SupportsKorean();
	// §9 「밤 5」. 로딩 없이 검정 화면. 타이틀 키아트도 메뉴 행도 그리지 않고,
	// 비언어음 자막 레인만 남긴다 — 30초 동안 화면에 있는 것은 그것뿐이다.
	// 여기서 검정을 직접 칠하는 이유는 타이틀에는 페이드할 카메라가 없다는 것.
	if (bSystemMenuNightFivePlaying)
	{
		FCanvasTileItem NightScrim(
			FVector2D::ZeroVector,
			FVector2D(Canvas->ClipX, Canvas->ClipY),
			FLinearColor::Black);
		NightScrim.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(NightScrim);
		DrawAudioCaption(
			GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0,
			Canvas->ClipY - 24.0f);
		return;
	}
	if (bSystemMenuIsKeyBindings)
	{
		FCanvasTileItem BindingScrim(
			FVector2D::ZeroVector,
			FVector2D(Canvas->ClipX, Canvas->ClipY),
			FLinearColor(0.004f, 0.006f, 0.007f, 0.985f));
		BindingScrim.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(BindingScrim);
		DrawKeyBindingsPanel();
		return;
	}
	if (bSystemMenuIsAudioCalibration || bSystemMenuIsDisplaySettings)
	{
		// These two tools own their complete visual hierarchy and deliberately
		// avoid inheriting the title screen's floor-datum ornament.
		FCanvasTileItem ToolScrim(
			FVector2D::ZeroVector,
			FVector2D(Canvas->ClipX, Canvas->ClipY),
			FLinearColor(0.004f, 0.006f, 0.007f, 0.985f));
		ToolScrim.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(ToolScrim);
		if (bSystemMenuIsAudioCalibration)
		{
			DrawAudioCalibrationPanel();
		}
		else
		{
			DrawDisplaySettingsPanel();
		}
		return;
	}

	const IGFrontendMenuLayout::FMetrics Metrics =
		IGFrontendMenuLayout::MakeMetrics(Canvas->ClipX, Canvas->ClipY);
	const UGameInstance* GameInstance = GetGameInstance();
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	const bool bReducedMotion = Accessibility
		&& Accessibility->IsReducedCameraMotionEnabled();
	const double Now = FPlatformTime::Seconds();
	const float EntranceAlpha = bReducedMotion || SystemMenuOpenedAt < 0.0
		? 1.0f
		: IGHorrorHUD::SmoothStep01(static_cast<float>(
			(Now - SystemMenuOpenedAt) / 0.32));

	auto WithAlpha = [EntranceAlpha](FLinearColor Color, const float Multiplier = 1.0f)
	{
		Color.A *= EntranceAlpha * Multiplier;
		return Color;
	};

	// The title uses one text-free environmental still. Other menu modes retain
	// the frozen playfield so pausing never destroys the player's spatial memory.
	if (bSystemMenuUseTitleBackdrop)
	{
		FCanvasTileItem Base(
			FVector2D::ZeroVector,
			FVector2D(Canvas->ClipX, Canvas->ClipY),
			IGHorrorHUD::FrontendInk);
		Canvas->DrawItem(Base);
		if (FrontendTitleBackgroundTexture
			&& FrontendTitleBackgroundTexture->GetResource())
		{
			const float ScreenAspect = Canvas->ClipX / FMath::Max(Canvas->ClipY, 1.0f);
			constexpr float SourceAspect = 16.0f / 9.0f;
			FVector2D Uv0(0.0f, 0.0f);
			FVector2D Uv1(1.0f, 1.0f);
			if (ScreenAspect < SourceAspect)
			{
				const float VisibleWidth = ScreenAspect / SourceAspect;
				const float LeftBias = (1.0f - VisibleWidth) * 0.20f;
				Uv0.X = LeftBias;
				Uv1.X = LeftBias + VisibleWidth;
			}
			else if (ScreenAspect > SourceAspect)
			{
				const float VisibleHeight = SourceAspect / ScreenAspect;
				Uv0.Y = (1.0f - VisibleHeight) * 0.5f;
				Uv1.Y = Uv0.Y + VisibleHeight;
			}

			FCanvasTileItem KeyArt(
				FVector2D::ZeroVector,
				FrontendTitleBackgroundTexture->GetResource(),
				FVector2D(Canvas->ClipX, Canvas->ClipY),
				Uv0,
				Uv1,
				FLinearColor(1.0f, 1.0f, 1.0f, EntranceAlpha));
			KeyArt.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(KeyArt);
		}

		FCanvasTileItem ArtTint(
			FVector2D::ZeroVector,
			FVector2D(Canvas->ClipX, Canvas->ClipY),
			FLinearColor(0.01f, 0.014f, 0.015f, 0.16f));
		ArtTint.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(ArtTint);
	}
	else
	{
		FCanvasTileItem PauseScrim(
			FVector2D::ZeroVector,
			FVector2D(Canvas->ClipX, Canvas->ClipY),
			FLinearColor(0.008f, 0.011f, 0.012f, 0.68f));
		PauseScrim.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(PauseScrim);
	}

	// A single bilinear alpha ramp protects legibility without the visible bands
	// produced by overlapping rectangles or the cost of a blur/retainer pass.
	if (FrontendShadeTexture && FrontendShadeTexture->GetResource())
	{
		FCanvasTileItem LeftShade(
			FVector2D::ZeroVector,
			FrontendShadeTexture->GetResource(),
			FVector2D(Canvas->ClipX * 0.64f, Canvas->ClipY),
			FLinearColor(
				1.0f,
				1.0f,
				1.0f,
				bSystemMenuUseTitleBackdrop ? 0.68f : 0.58f));
		LeftShade.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(LeftShade);
	}
	else
	{
		FCanvasTileItem LeftShadeFallback(
			FVector2D::ZeroVector,
			FVector2D(Canvas->ClipX * 0.48f, Canvas->ClipY),
			FLinearColor(0.006f, 0.009f, 0.010f, 0.38f));
		LeftShadeFallback.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(LeftShadeFallback);
	}

	const float SupportScale = FMath::Max(0.90f, Metrics.Scale);
	const float TitleScale = FMath::Max(0.76f, Metrics.Scale);
	auto DrawDisplayTitle = [
		this,
		&WithAlpha](
			const FText& Text,
			const FVector2D& Position,
			const float Scale)
	{
		UFont* Font = KoreanFrontendTitleFont
			? KoreanFrontendTitleFont.Get()
			: GetFontForRole(EIGHudTextRole::Objective);
		if (!Canvas || !Font || Text.IsEmpty())
		{
			return 0.0f;
		}
		const float EffectiveScale = KoreanFrontendTitleFont
			? Scale
			: Scale * 2.45f;
		float Width = 0.0f;
		float Height = 0.0f;
		Canvas->StrLen(Font, Text.ToString(), Width, Height, true);
		FCanvasTextItem Item(
			Position,
			Text,
			Font,
			WithAlpha(IGHorrorHUD::FrontendIvory));
		Item.Scale = FVector2D(EffectiveScale);
		Item.EnableShadow(
			WithAlpha(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f)),
			FVector2D(1.0f, 2.0f));
		if (bLayoutValidationEnabled)
		{
			RecordLayoutValidationRect(
				Position,
				Position + FVector2D(
					Width * EffectiveScale,
					Height * EffectiveScale));
		}
		Canvas->DrawItem(Item);
		return Height * EffectiveScale;
	};

	const FVector2D HeaderOrigin(Metrics.ContentLeft, Metrics.TitleTop);
	if (bSystemMenuIsContentNotice)
	{
		// 일반 경고는 사용자가 아니라 책임을 보호한다. 그래서 「점멸이
		// 있습니다」로 끝내지 않고 **무엇이 나오는지**와 **그것을 어느
		// 설정으로 줄일 수 있는지**를 같은 화면에서 말한다.
		DrawLeftAlignedText(
			bKorean
				? NSLOCTEXT("IGHUD", "NoticeContext", "플레이 전에")
				: FText::FromString(TEXT("BEFORE YOU PLAY")),
			HeaderOrigin,
			WithAlpha(IGHorrorHUD::FrontendMuted),
			EIGHudTextRole::Hint,
			0.82f * SupportScale);
		const FVector2D NoticeTitleOrigin =
			HeaderOrigin + FVector2D(0.0f, 24.0f * Metrics.Scale);
		DrawDisplayTitle(
			bKorean
				? NSLOCTEXT("IGHUD", "NoticeTitle", "이 게임에 나오는 것")
				: FText::FromString(TEXT("WHAT THIS GAME CONTAINS")),
			NoticeTitleOrigin,
			0.78f * TitleScale);

		struct FNoticeLine
		{
			FText Text;
			bool bHeading;
		};
		const FNoticeLine NoticeLines[] =
		{
			{bKorean
				? NSLOCTEXT("IGHUD", "NoticeSensory", "감각")
				: FText::FromString(TEXT("SENSORY")), true},
			{bKorean
				? NSLOCTEXT(
					"IGHUD", "NoticeSensory1",
					"암전, 갑작스러운 접근, 화면 흔들림과 조명 점멸이 있습니다.")
				: FText::FromString(
					TEXT("BLACKOUTS, SUDDEN APPROACH, CAMERA SHAKE AND FLICKER.")), false},
			{bKorean
				? NSLOCTEXT(
					"IGHUD", "NoticeSensory2",
					"3Hz를 넘는 점멸과 전대역 플래시는 쓰지 않았습니다.")
				: FText::FromString(
					TEXT("NO FLASHING ABOVE 3HZ AND NO FULL-SCREEN FLASH.")), false},
			{bKorean
				? NSLOCTEXT("IGHUD", "NoticeThemes", "소재")
				: FText::FromString(TEXT("THEMES")), true},
			{bKorean
				? NSLOCTEXT(
					"IGHUD", "NoticeThemes1",
					"장기간 은폐된 유해를 연상시키는 장면이 나옵니다. 직접적인"
					" 신체 훼손 묘사는 없습니다.")
				: FText::FromString(
					TEXT("LONG-CONCEALED REMAINS ARE IMPLIED. NO GRAPHIC GORE.")), false},
			{bKorean
				? NSLOCTEXT(
					"IGHUD", "NoticeThemes2",
					"층간소음, 무단증축, 산업재해 은폐를 다룹니다. 실제 사건이나"
					" 인물과는 무관합니다.")
				: FText::FromString(
					TEXT("NOISE DISPUTES, ILLEGAL BUILDING, A COVERED-UP DEATH."
						" NOT BASED ON REAL EVENTS.")), false},
			{bKorean
				? NSLOCTEXT("IGHUD", "NoticeControls", "줄일 수 있는 것")
				: FText::FromString(TEXT("WHAT YOU CAN TURN DOWN")), true},
			{bKorean
				? NSLOCTEXT(
					"IGHUD", "NoticeControls1",
					"흔들림 감소와 점멸 감소를 켜면 카메라 떨림과 임의 암전이"
					" 사라집니다. 판정은 그대로입니다.")
				: FText::FromString(
					TEXT("REDUCED SHAKE AND REDUCED FLICKER REMOVE BOTH."
						" GAMEPLAY IS UNCHANGED.")), false},
			{bKorean
				? NSLOCTEXT(
					"IGHUD", "NoticeControls2",
					"쫓기는 압박이 부담스러우면 난이도에서 「듣기만 하는 밤」을"
					" 고르세요. 결말은 전부 같습니다.")
				: FText::FromString(
					TEXT("PICK THE LISTENING-ONLY NIGHT IF PURSUIT IS TOO MUCH."
						" EVERY ENDING STAYS REACHABLE.")), false},
		};

		float NoticePenY = Metrics.MenuTop - 18.0f * SupportScale;
		for (const FNoticeLine& Line : NoticeLines)
		{
			if (Line.bHeading)
			{
				NoticePenY += 10.0f * SupportScale;
			}
			DrawLeftAlignedText(
				Line.Text,
				FVector2D(Metrics.ContentLeft, NoticePenY),
				WithAlpha(
					Line.bHeading
						? IGHorrorHUD::FrontendIvory
						: IGHorrorHUD::FrontendMuted),
				Line.bHeading ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint,
				(Line.bHeading ? 0.94f : 0.88f) * SupportScale);
			NoticePenY += (Line.bHeading ? 30.0f : 27.0f) * SupportScale;
		}

		DrawLeftAlignedText(
			bKorean
				? bUsingGamepad
					? NSLOCTEXT(
						"IGHUD", "NoticeControlsGamepad",
						"A  계속  ·  Y  접근성 설정 열기")
					: NSLOCTEXT(
						"IGHUD", "NoticeControlsKeyboard",
						"Enter  계속  ·  F10  접근성 설정 열기")
				: FText::FromString(
					bUsingGamepad
						? TEXT("A  CONTINUE  |  Y  ACCESSIBILITY")
						: TEXT("ENTER  CONTINUE  |  F10  ACCESSIBILITY")),
			FVector2D(Metrics.ContentLeft, Metrics.FooterTop),
			WithAlpha(IGHorrorHUD::FrontendIvory),
			EIGHudTextRole::Hint,
			0.86f * SupportScale);
		RecordLayoutValidationRect(
			FVector2D(Metrics.ContentLeft, HeaderOrigin.Y),
			FVector2D(
				Metrics.ContentLeft + Metrics.ContentWidth,
				FMath::Min(Metrics.FooterTop + 24.0f, Canvas->ClipY)));
		return;
	}
	if (bSystemMenuIsCredits)
	{
		DrawLeftAlignedText(
			bKorean
				? NSLOCTEXT("IGHUD", "CreditsContext", "제작 정보 · 2026")
				: FText::FromString(TEXT("CREDITS · 2026")),
			HeaderOrigin,
			WithAlpha(IGHorrorHUD::FrontendMuted),
			EIGHudTextRole::Hint,
			0.82f * SupportScale);
		const FVector2D CreditsTitleOrigin =
			HeaderOrigin + FVector2D(0.0f, 24.0f * Metrics.Scale);
		const float CreditsTitleHeight = DrawDisplayTitle(
			bKorean
				? NSLOCTEXT("IGHUD", "CreditsTitle", "만든 사람")
				: FText::FromString(TEXT("CREDITS")),
			CreditsTitleOrigin,
			0.78f * TitleScale);
		DrawLeftAlignedText(
			bKorean
				? NSLOCTEXT("IGHUD", "CreditsGameTitle", "없는 층")
				: FText::FromString(TEXT("THE MISSING FLOOR")),
			FVector2D(
				HeaderOrigin.X + 2.0f,
				CreditsTitleOrigin.Y
					+ CreditsTitleHeight
					+ 2.0f * Metrics.Scale),
			WithAlpha(IGHorrorHUD::FrontendMuted),
			EIGHudTextRole::Hint,
			0.82f * SupportScale);

		const FText CreditLines[] =
		{
			bKorean
				? NSLOCTEXT("IGHUD", "CreditsDeveloper", "기획 · 개발    easygap")
				: FText::FromString(TEXT("DESIGN + DEVELOPMENT    easygap")),
			bKorean
				? NSLOCTEXT("IGHUD", "CreditsEngine", "제작 도구    Unreal Engine 5.8")
				: FText::FromString(TEXT("POWERED BY    UNREAL ENGINE 5.8")),
			bKorean
				? NSLOCTEXT("IGHUD", "CreditsMaterials", "일부 재질    ambientCG · CC0")
				: FText::FromString(TEXT("SELECT MATERIALS    ambientCG · CC0")),
			bKorean
				? NSLOCTEXT("IGHUD", "CreditsProps", "일부 소품    Poly Haven · CC0")
				: FText::FromString(TEXT("SELECT PROPS    POLY HAVEN · CC0")),
			FText::FromString(TEXT("Copyright 2026 easygap. All rights reserved."))
		};
		for (int32 Line = 0; Line < UE_ARRAY_COUNT(CreditLines); ++Line)
		{
			DrawLeftAlignedText(
				CreditLines[Line],
				FVector2D(
					Metrics.ContentLeft,
					Metrics.MenuTop + Line * 34.0f * SupportScale),
				WithAlpha(
					Line == 0
						? IGHorrorHUD::FrontendIvory
						: IGHorrorHUD::FrontendMuted),
				Line == 0
					? EIGHudTextRole::Prompt
					: EIGHudTextRole::Hint,
				Line == 0
					? SupportScale
					: SupportScale * 0.94f);
		}
		DrawLeftAlignedText(
			bKorean
				? bUsingGamepad
					? NSLOCTEXT("IGHUD", "CreditsBackGamepad", "B  돌아가기")
					: NSLOCTEXT("IGHUD", "CreditsBackKeyboard", "Esc 또는 Enter  돌아가기")
				: FText::FromString(
					bUsingGamepad ? TEXT("B  BACK") : TEXT("ESC OR ENTER  BACK")),
			FVector2D(Metrics.ContentLeft, Metrics.FooterTop),
			WithAlpha(IGHorrorHUD::FrontendMuted),
			EIGHudTextRole::Hint,
			0.86f * SupportScale);
		return;
	}

	DrawLeftAlignedText(
		bSystemMenuIsTitle
			? bKorean
				? NSLOCTEXT("IGHUD", "TitleContext", "무영로 · 04:30")
				: FText::FromString(TEXT("MUYEONG-RO · 04:30"))
			: FText::FromString(TEXT("SYSTEM · PAUSE")),
		HeaderOrigin,
		WithAlpha(IGHorrorHUD::FrontendMuted),
		EIGHudTextRole::Hint,
		0.82f * SupportScale);
	const FVector2D MainTitleOrigin =
		HeaderOrigin + FVector2D(0.0f, 24.0f * Metrics.Scale);
	const float MainTitleHeight = DrawDisplayTitle(
		bSystemMenuIsTitle
			? bKorean
				? NSLOCTEXT("IGHUD", "MainTitle", "없는 층")
				: FText::FromString(TEXT("THE MISSING FLOOR"))
			: bKorean
				? NSLOCTEXT("IGHUD", "PauseTitle", "잠시 멈춤")
				: FText::FromString(TEXT("PAUSED")),
		MainTitleOrigin,
		TitleScale);
	DrawLeftAlignedText(
		bSystemMenuIsTitle
			? FText::FromString(TEXT("THE MISSING FLOOR"))
			: FText::FromString(TEXT("PAUSED")),
		FVector2D(
			HeaderOrigin.X + 2.0f,
			MainTitleOrigin.Y
				+ MainTitleHeight
				+ 2.0f * Metrics.Scale),
		WithAlpha(IGHorrorHUD::FrontendMuted),
		EIGHudTextRole::Hint,
		0.76f * SupportScale);

	TArray<FString> MessageLines;
	FLinearColor MessageColor = IGHorrorHUD::FrontendIvory;
	if (bSystemMenuIsTitle && bSystemMenuHeadphoneRecommendation)
	{
		MessageLines = {
			bKorean
				? TEXT("이 게임은 헤드폰으로 듣도록 만들어졌다.")
				: TEXT("THIS GAME IS MADE TO BE HEARD ON HEADPHONES."),
			bKorean ? TEXT("아무 키를 누르면 건너뜁니다.") : TEXT("PRESS ANY KEY TO SKIP.")
		};
		MessageColor = IGHorrorHUD::ThoughtBlue;
	}
	else if (bSystemMenuIsTitle && bSystemMenuConfirmNewGame)
	{
		MessageLines = {
			bKorean ? TEXT("자동 저장을 덮어씁니다.") : TEXT("THIS OVERWRITES YOUR AUTOSAVE."),
			bKorean ? TEXT("다시 선택하면 새 게임 시작") : TEXT("SELECT NEW GAME AGAIN TO START.")
		};
		MessageColor = IGHorrorHUD::FrontendOxide;
	}
	else if (!SystemMenuStatusText.IsEmpty())
	{
		UFont* HintFont = GetFontForRole(EIGHudTextRole::Hint);
		FString Remainder;
		if (HintFont)
		{
			WrapHudText(
				SystemMenuStatusText.ToString(),
				HintFont,
				SupportScale,
				Metrics.ContentWidth,
				2,
				MessageLines,
				Remainder);
		}
		if (MessageLines.IsEmpty())
		{
			MessageLines.Add(SystemMenuStatusText.ToString());
		}
		MessageColor = bSystemMenuStatusIsError
			? IGHorrorHUD::FrontendOxide
			: IGHorrorHUD::FrontendIvory;
	}
	if (!MessageLines.IsEmpty())
	{
		FCanvasTileItem MessageMark(
			FVector2D(
				Metrics.ContentLeft - 13.0f * Metrics.Scale,
				Metrics.MessageTop + 2.0f * Metrics.Scale),
			FVector2D(
				FMath::Max(2.0f, 2.0f * Metrics.Scale),
				MessageLines.Num() * 20.0f * SupportScale),
			WithAlpha(MessageColor, 0.86f));
		MessageMark.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(MessageMark);
		for (int32 Line = 0; Line < MessageLines.Num(); ++Line)
		{
			DrawLeftAlignedText(
				FText::FromString(MessageLines[Line]),
				FVector2D(
					Metrics.ContentLeft,
					Metrics.MessageTop + Line * 21.0f * SupportScale),
				WithAlpha(MessageColor),
				EIGHudTextRole::Hint,
				SupportScale);
		}
	}

	const FString TitleRows[] =
	{
		bKorean ? TEXT("이어하기") : TEXT("CONTINUE"),
		bSystemMenuCanContinue
			? bKorean ? TEXT("새 게임") : TEXT("NEW GAME")
			: bKorean ? TEXT("게임 시작") : TEXT("START GAME"),
		bKorean ? TEXT("설정") : TEXT("SETTINGS"),
		bKorean ? TEXT("제작 정보") : TEXT("CREDITS"),
		bKorean ? TEXT("게임 종료") : TEXT("QUIT"),
		// §9. 있을 수 없는 슬롯. 라벨은 밤 이름 하나뿐이고 아무 설명도 달지
		// 않는다 — 발견한 사람만 아는 것이 이 30초의 전부다.
		bKorean ? TEXT("밤 5") : TEXT("NIGHT 5")
	};
	const FString PauseRows[] =
	{
		bKorean ? TEXT("계속하기") : TEXT("RESUME"),
		bKorean ? TEXT("최근 자동 저장 불러오기") : TEXT("LOAD LATEST AUTOSAVE"),
		bKorean ? TEXT("설정") : TEXT("SETTINGS"),
		bKorean ? TEXT("제작 정보") : TEXT("CREDITS"),
		bKorean ? TEXT("게임 종료") : TEXT("QUIT"),
		// 일시정지 메뉴에는 밤 5가 없다. 자리만 채운다.
		FString()
	};

	const int32 VisibleRowCount = IGFrontendMenuLayout::GetVisibleActionCount(
		bSystemMenuIsTitle,
		bSystemMenuCanContinue,
		bSystemMenuNightFiveAvailable);
	const float GuideX = Metrics.ContentLeft - 20.0f * Metrics.Scale;
	const float GuideTop = Metrics.MenuTop + Metrics.RowHeight * 0.5f;
	const float GuideBottom = Metrics.MenuTop
		+ (VisibleRowCount - 1) * Metrics.GetRowStride()
		+ Metrics.RowHeight * 0.5f;
	FCanvasTileItem FloorGuide(
		FVector2D(GuideX, GuideTop),
		FVector2D(
			FMath::Max(1.0f, Metrics.Scale),
			FMath::Max(1.0f, GuideBottom - GuideTop)),
		WithAlpha(IGHorrorHUD::FrontendGuide));
	FloorGuide.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(FloorGuide);

	const int32 LoadRow = bSystemMenuIsTitle ? 0 : 1;
	for (int32 ActionRow = 0;
		ActionRow < IGFrontendMenuLayout::ActionCount;
		++ActionRow)
	{
		const int32 VisibleSlot =
			IGFrontendMenuLayout::GetVisibleSlotForAction(
				ActionRow,
				bSystemMenuIsTitle,
				bSystemMenuCanContinue,
				bSystemMenuNightFiveAvailable);
		if (VisibleSlot == INDEX_NONE)
		{
			continue;
		}

		const bool bEnabled =
			ActionRow != LoadRow || bSystemMenuCanContinue;
		const bool bSelected = ActionRow == SystemMenuSelectedRow;
		// §9: 한 번 재생하면 흐려진다. 사라지지는 않는다 — 다시 들을 수 있다.
		const bool bDimmed = ActionRow == IGFrontendMenuLayout::NightFiveAction
			&& bSystemMenuNightFiveSpent;
		FString Label =
			bSystemMenuIsTitle ? TitleRows[ActionRow] : PauseRows[ActionRow];
		if (bSystemMenuIsTitle
			&& ActionRow == 1
			&& bSystemMenuConfirmNewGame)
		{
			Label = bKorean
				? TEXT("새 게임 확인")
				: TEXT("CONFIRM NEW GAME");
		}
		if (!bEnabled)
		{
			Label += bKorean ? TEXT("  · 저장 없음") : TEXT("  · NO SAVE");
		}

		const FVector2D RowPosition = Metrics.GetRowPosition(VisibleSlot);
		if (bSelected)
		{
			FCanvasTileItem FocusSurface(
				RowPosition - FVector2D(4.0f * Metrics.Scale, 0.0f),
				FVector2D(
					Metrics.ContentWidth + 4.0f * Metrics.Scale,
					Metrics.RowHeight),
				WithAlpha(IGHorrorHUD::FrontendFocus));
			FocusSurface.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(FocusSurface);
		}

		const float TickWidth = bSelected
			? 30.0f * Metrics.Scale
			: 9.0f * Metrics.Scale;
		const float TickHeight = bSelected
			? FMath::Max(2.0f, 2.0f * Metrics.Scale)
			: FMath::Max(1.0f, Metrics.Scale);
		FCanvasTileItem FloorTick(
			FVector2D(
				GuideX - (bSelected ? 7.0f * Metrics.Scale : 0.0f),
				RowPosition.Y + Metrics.RowHeight * 0.5f - TickHeight * 0.5f),
			FVector2D(TickWidth, TickHeight),
			WithAlpha(
				bSelected
					? IGHorrorHUD::FrontendOxide
					: IGHorrorHUD::FrontendGuide));
		FloorTick.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(FloorTick);

		const FLinearColor LabelColor = bEnabled
			? IGHorrorHUD::FrontendIvory
			: IGHorrorHUD::FrontendMuted;
		// 흐려진 밤 5는 여전히 선택 가능하므로 비활성 색이 아니라 밝기만 낮춘다.
		const float DisabledMultiplier =
			(bEnabled ? 1.0f : 0.52f) * (bDimmed ? 0.55f : 1.0f);
		const float RowTextScale = bSelected
			? SupportScale
			: SupportScale * (20.0f / 18.0f);
		DrawLeftAlignedText(
			FText::FromString(Label),
			FVector2D(
				Metrics.ContentLeft + 13.0f * Metrics.Scale,
				RowPosition.Y + FMath::Max(9.0f, 13.0f * Metrics.Scale)),
			WithAlpha(LabelColor, DisabledMultiplier),
			bSelected
				? EIGHudTextRole::Prompt
				: EIGHudTextRole::Hint,
			RowTextScale);

		const FBox2D HitBox = Metrics.GetRowHitBox(VisibleSlot);
		RecordLayoutValidationRect(HitBox.Min, HitBox.Max);
	}

	FText SystemControls;
	if (bKorean)
	{
		SystemControls = bUsingGamepad
			? bSystemMenuIsTitle
				? bSystemMenuConfirmNewGame
					? NSLOCTEXT("IGHUD", "TitleConfirmControlsGamepad", "A 시작  ·  View 취소")
					: NSLOCTEXT("IGHUD", "TitleControlsGamepad", "D-pad 이동  ·  A 선택  ·  Menu 접근성")
				: NSLOCTEXT("IGHUD", "SystemMenuControlsGamepad", "D-pad 이동  ·  A 선택  ·  View 돌아가기")
			: bSystemMenuIsTitle
				? bSystemMenuConfirmNewGame
					? NSLOCTEXT("IGHUD", "TitleConfirmControlsKeyboard", "Enter 시작  ·  Esc 취소")
					: NSLOCTEXT("IGHUD", "TitleControlsKeyboard", "↑↓ 이동  ·  Enter 선택  ·  F10 접근성")
				: NSLOCTEXT("IGHUD", "SystemMenuControlsKeyboard", "↑↓ 이동  ·  Enter 선택  ·  Esc 돌아가기");
	}
	else
	{
		SystemControls = FText::FromString(
			bUsingGamepad
				? bSystemMenuIsTitle
					? TEXT("D-PAD MOVE  ·  A SELECT  ·  MENU ACCESSIBILITY")
					: TEXT("D-PAD MOVE  ·  A SELECT  ·  VIEW BACK")
				: bSystemMenuIsTitle
					? TEXT("ARROWS MOVE  ·  ENTER SELECT  ·  F10 ACCESSIBILITY")
					: TEXT("ARROWS MOVE  ·  ENTER SELECT  ·  ESC BACK"));
	}
	DrawLeftAlignedText(
		SystemControls,
		FVector2D(Metrics.ContentLeft, Metrics.FooterTop),
		WithAlpha(IGHorrorHUD::FrontendMuted),
		EIGHudTextRole::Hint,
		0.84f * SupportScale);
}


void AIGHorrorHUD::ResolveInteractionComponent()
{
	APawn* Pawn = GetOwningPawn();
	InteractionComponent = Pawn
		? Pawn->FindComponentByClass<UIGInteractionComponent>()
		: nullptr;
}

void AIGHorrorHUD::ResolveDirectors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!WakeDirector.IsValid())
	{
		for (TActorIterator<AIGWakeUpDirector> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				WakeDirector = *It;
				break;
			}
		}
	}

	if (!ObjectiveProvider.IsValid())
	{
		for (TActorIterator<AIGMorningRoutineDirector> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				SetMorningDirector(*It);
				break;
			}
		}
	}
}

FText AIGHorrorHUD::GetObjectiveText() const
{
	const AIGWakeUpDirector* Wake = WakeDirector.Get();
	const IIGObjectiveProvider* Provider =
		Cast<IIGObjectiveProvider>(ObjectiveProvider.Get());
	if (!Wake)
	{
		// Later chapters retire the reusable wake director once the player is
		// already standing. Their chapter director still owns the objective,
		// so do not make that HUD line depend on a CH01/02-only actor.
		return Provider ? Provider->GetObjectiveText() : FText::GetEmpty();
	}

	switch (Wake->GetWakeState())
	{
	case EIGWakeState::NotStarted:
	case EIGWakeState::FadeIn:
	case EIGWakeState::AwaitAlarm:
		return NSLOCTEXT("IGHUD", "ObjAlarm", "알람을 끄자");

	case EIGWakeState::BedLocked:
	case EIGWakeState::GettingUp:
		return NSLOCTEXT("IGHUD", "ObjGetUp", "몸을 일으키자");

	case EIGWakeState::FreeRoam:
	{
		if (Provider)
		{
			// An empty objective can be intentional between story beats.
			// Only use the generic exploration copy when no director exists.
			return Provider->GetObjectiveText();
		}
		return NSLOCTEXT("IGHUD", "ObjExplore", "방 안을 둘러보자");
	}

	default:
		return FText::GetEmpty();
	}
}

FString AIGHorrorHUD::GetObjectiveTextAscii() const
{
	const AIGWakeUpDirector* Wake = WakeDirector.Get();
	const IIGObjectiveProvider* Provider =
		Cast<IIGObjectiveProvider>(ObjectiveProvider.Get());
	if (!Wake)
	{
		return Provider ? Provider->GetObjectiveTextAscii() : FString();
	}

	switch (Wake->GetWakeState())
	{
	case EIGWakeState::NotStarted:
	case EIGWakeState::FadeIn:
	case EIGWakeState::AwaitAlarm:
		return TEXT("Alarm ringing: Turn off alarm");

	case EIGWakeState::BedLocked:
	case EIGWakeState::GettingUp:
		return TEXT("Alarm stopped: Get up");

	case EIGWakeState::FreeRoam:
	{
		if (Provider)
		{
			return Provider->GetObjectiveTextAscii();
		}
		return TEXT("Explore the room");
	}

	default:
		return FString();
	}
}

float AIGHorrorHUD::GetObjectiveProgress() const
{
	const IIGObjectiveProvider* Provider =
		Cast<IIGObjectiveProvider>(ObjectiveProvider.Get());
	return Provider
		? FMath::Clamp(Provider->GetObjectiveProgress(), 0.0f, 1.0f)
		: 0.0f;
}

bool AIGHorrorHUD::DrawChapterCard(const double CurrentTime)
{
	if (!Canvas || ChapterCardTitle.IsEmpty() || CurrentTime >= ChapterCardEndTime)
	{
		if (CurrentTime >= ChapterCardEndTime)
		{
			ChapterCardEyebrow = FText::GetEmpty();
			ChapterCardTitle = FText::GetEmpty();
			ChapterCardSubtitle = FText::GetEmpty();
		}
		return false;
	}

	constexpr double FadeInSeconds = 0.65;
	constexpr double FadeOutSeconds = 0.8;
	const double Elapsed = CurrentTime - ChapterCardStartTime;
	const double Remaining = ChapterCardEndTime - CurrentTime;
	const float FadeAlpha = FMath::Clamp(
		static_cast<float>(FMath::Min(Elapsed / FadeInSeconds, Remaining / FadeOutSeconds)),
		0.0f,
		1.0f);
	const float SmoothAlpha = FadeAlpha * FadeAlpha * (3.0f - (2.0f * FadeAlpha));

	FCanvasTileItem Scrim(
		FVector2D::ZeroVector,
		FVector2D(Canvas->ClipX, Canvas->ClipY),
		FLinearColor(0.0f, 0.0f, 0.0f, 0.96f * SmoothAlpha));
	Scrim.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Scrim);

	FLinearColor EyebrowColor = IGHorrorHUD::MutedGray;
	EyebrowColor.A = SmoothAlpha;
	FLinearColor TitleColor = IGHorrorHUD::PaleGray;
	TitleColor.A = SmoothAlpha;
	FLinearColor SubtitleColor = IGHorrorHUD::ThoughtBlue;
	SubtitleColor.A = SmoothAlpha;

	if (!ChapterCardEyebrow.IsEmpty())
	{
		DrawCenteredText(
			ChapterCardEyebrow,
			(Canvas->ClipY * 0.5f) - 52.0f,
			EyebrowColor,
			EIGHudTextRole::Hint);
	}
	DrawCenteredText(
		ChapterCardTitle,
		(Canvas->ClipY * 0.5f) - 12.0f,
		TitleColor,
		EIGHudTextRole::Objective);
	if (!ChapterCardSubtitle.IsEmpty())
	{
		DrawCenteredText(
			ChapterCardSubtitle,
			(Canvas->ClipY * 0.5f) + 34.0f,
			SubtitleColor,
			EIGHudTextRole::Thought);
	}

	return true;
}

void AIGHorrorHUD::DrawCenteredText(
	const FText& Text,
	const float ScreenY,
	const FLinearColor& Color,
	const EIGHudTextRole TextRole,
	const float TextScale)
{
	if (!Canvas || Text.IsEmpty())
	{
		return;
	}

	UFont* Font = GetFontForRole(TextRole);
	if (!Font)
	{
		return;
	}

	FCanvasTextItem TextItem(
		FVector2D(Canvas->ClipX * 0.5f, ScreenY),
		Text,
		Font,
		Color);
	TextItem.bCentreX = true;
	TextItem.Scale = FVector2D(FMath::Max(0.5f, TextScale));
	if (bLayoutValidationEnabled)
	{
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		Canvas->StrLen(Font, Text.ToString(), TextWidth, TextHeight, true);
		const FVector2D ScaledSize(
			TextWidth * TextItem.Scale.X,
			TextHeight * TextItem.Scale.Y);
		RecordLayoutValidationRect(
			FVector2D(
				(Canvas->ClipX - ScaledSize.X) * 0.5f,
				ScreenY),
			FVector2D(
				(Canvas->ClipX + ScaledSize.X) * 0.5f,
				ScreenY + ScaledSize.Y));
	}

	// A one-pixel outline keeps small Hangul legible on bright surfaces;
	// larger text reads better with a soft drop shadow instead.
	FLinearColor EffectColor = IGHorrorHUD::Shadow;
	EffectColor.A *= Color.A;
	if (TextRole == EIGHudTextRole::Hint || TextRole == EIGHudTextRole::Prompt)
	{
		TextItem.bOutlined = true;
		TextItem.OutlineColor = EffectColor;
	}
	else
	{
		TextItem.EnableShadow(EffectColor, FVector2D(1.0f, 1.0f));
	}

	Canvas->DrawItem(TextItem);
}

void AIGHorrorHUD::DrawLeftAlignedText(
	const FText& Text,
	const FVector2D& Position,
	const FLinearColor& Color,
	const EIGHudTextRole TextRole,
	const float TextScale,
	const bool bUseOutline)
{
	if (!Canvas || Text.IsEmpty())
	{
		return;
	}
	UFont* Font = GetFontForRole(TextRole);
	if (!Font)
	{
		return;
	}

	const float SafeTextScale = FMath::Max(0.5f, TextScale);
	FCanvasTextItem TextItem(Position, Text, Font, Color);
	TextItem.Scale = FVector2D(SafeTextScale);
	FLinearColor EffectColor = IGHorrorHUD::Shadow;
	EffectColor.A *= Color.A;
	if (bUseOutline)
	{
		TextItem.bOutlined = true;
		TextItem.OutlineColor = EffectColor;
	}
	else
	{
		TextItem.EnableShadow(EffectColor, FVector2D(1.0f, 1.0f));
	}

	if (bLayoutValidationEnabled)
	{
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		Canvas->StrLen(Font, Text.ToString(), TextWidth, TextHeight, true);
		RecordLayoutValidationRect(
			Position,
			Position + FVector2D(
				TextWidth * SafeTextScale,
				TextHeight * SafeTextScale));
	}
	Canvas->DrawItem(TextItem);
}

void AIGHorrorHUD::DrawRightAlignedText(
	const FText& Text,
	const FVector2D& Position,
	const FLinearColor& Color,
	const EIGHudTextRole TextRole,
	const float TextScale,
	const bool bUseOutline)
{
	if (!Canvas || Text.IsEmpty())
	{
		return;
	}
	UFont* Font = GetFontForRole(TextRole);
	if (!Font)
	{
		return;
	}

	const float SafeTextScale = FMath::Max(0.5f, TextScale);
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	Canvas->StrLen(Font, Text.ToString(), TextWidth, TextHeight, true);
	const FVector2D DrawPosition(
		Position.X - TextWidth * SafeTextScale,
		Position.Y);
	FCanvasTextItem TextItem(DrawPosition, Text, Font, Color);
	TextItem.Scale = FVector2D(SafeTextScale);
	FLinearColor EffectColor = IGHorrorHUD::Shadow;
	EffectColor.A *= Color.A;
	if (bUseOutline)
	{
		TextItem.bOutlined = true;
		TextItem.OutlineColor = EffectColor;
	}
	else
	{
		TextItem.EnableShadow(EffectColor, FVector2D(1.0f, 1.0f));
	}

	if (bLayoutValidationEnabled)
	{
		const FVector2D Size(
			TextWidth * SafeTextScale,
			TextHeight * SafeTextScale);
		RecordLayoutValidationRect(
			DrawPosition,
			DrawPosition + Size);
	}
	Canvas->DrawItem(TextItem);
}

void AIGHorrorHUD::DrawCrosshair(const FLinearColor& Color)
{
	if (!Canvas)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	constexpr float InnerRadius = 7.0f;
	constexpr float OuterRadius = 11.0f;
	constexpr float Thickness = 1.5f;

	auto DrawBrackets = [this, CenterX, CenterY](const FLinearColor& LineColor, const float Offset)
	{
		DrawLine(CenterX - OuterRadius + Offset, CenterY - InnerRadius + Offset,
			CenterX - InnerRadius + Offset, CenterY - InnerRadius + Offset, LineColor, Thickness);
		DrawLine(CenterX - OuterRadius + Offset, CenterY - InnerRadius + Offset,
			CenterX - OuterRadius + Offset, CenterY + InnerRadius + Offset, LineColor, Thickness);
		DrawLine(CenterX + InnerRadius + Offset, CenterY - InnerRadius + Offset,
			CenterX + OuterRadius + Offset, CenterY - InnerRadius + Offset, LineColor, Thickness);
		DrawLine(CenterX + OuterRadius + Offset, CenterY - InnerRadius + Offset,
			CenterX + OuterRadius + Offset, CenterY + InnerRadius + Offset, LineColor, Thickness);
		DrawLine(CenterX - OuterRadius + Offset, CenterY + InnerRadius + Offset,
			CenterX - InnerRadius + Offset, CenterY + InnerRadius + Offset, LineColor, Thickness);
		DrawLine(CenterX + InnerRadius + Offset, CenterY + InnerRadius + Offset,
			CenterX + OuterRadius + Offset, CenterY + InnerRadius + Offset, LineColor, Thickness);
	};

	DrawBrackets(IGHorrorHUD::Shadow, 1.0f);
	DrawRect(IGHorrorHUD::Shadow, CenterX, CenterY, 3.0f, 3.0f);
	DrawBrackets(Color, 0.0f);
	DrawRect(Color, CenterX - 1.5f, CenterY - 1.5f, 3.0f, 3.0f);
}

void AIGHorrorHUD::UpdateFocusBracket(AActor* FocusedActor, const float DeltaSeconds)
{
	if (!Canvas)
	{
		return;
	}

	if (!IsValid(FocusedActor))
	{
		FocusBracketTarget.Reset();
		FocusBracketAcquireElapsed = 0.0f;
		FocusBracketAlpha = FMath::FInterpTo(FocusBracketAlpha, 0.0f, DeltaSeconds, 12.0f);
		return;
	}
	if (FocusBracketTarget.Get() != FocusedActor)
	{
		FocusBracketTarget = FocusedActor;
		FocusBracketAcquireElapsed = 0.0f;
		FocusBracketAlpha = 0.0f;
	}
	FocusBracketAcquireElapsed += FMath::Max(0.0f, DeltaSeconds);

	// Project the focused actor's bounds and fit a box around them, so the
	// reticle visibly grabs the object instead of just recolouring a dot.
	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	FocusedActor->GetActorBounds(true, Origin, Extent);

	FVector2D ScreenMin(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D ScreenMax(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
	bool bAnyCornerVisible = false;
	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FVector Corner(
			Origin.X + ((CornerIndex & 1) ? Extent.X : -Extent.X),
			Origin.Y + ((CornerIndex & 2) ? Extent.Y : -Extent.Y),
			Origin.Z + ((CornerIndex & 4) ? Extent.Z : -Extent.Z));
		const FVector Projected = Canvas->Project(Corner);
		if (Projected.Z <= 0.0f)
		{
			continue; // behind the camera
		}
		bAnyCornerVisible = true;
		ScreenMin.X = FMath::Min(ScreenMin.X, Projected.X);
		ScreenMin.Y = FMath::Min(ScreenMin.Y, Projected.Y);
		ScreenMax.X = FMath::Max(ScreenMax.X, Projected.X);
		ScreenMax.Y = FMath::Max(ScreenMax.Y, Projected.Y);
	}

	if (!bAnyCornerVisible)
	{
		FocusBracketAlpha = FMath::FInterpTo(FocusBracketAlpha, 0.0f, DeltaSeconds, 12.0f);
		return;
	}

	// Keep the bracket readable: never smaller than the crosshair, never so
	// large that it frames the whole screen.
	const FVector2D Center = (ScreenMin + ScreenMax) * 0.5f;
	FVector2D HalfSize = (ScreenMax - ScreenMin) * 0.5f + FVector2D(6.0f, 6.0f);
	HalfSize.X = FMath::Clamp(HalfSize.X, 14.0f, Canvas->ClipX * 0.30f);
	HalfSize.Y = FMath::Clamp(HalfSize.Y, 14.0f, Canvas->ClipY * 0.34f);

	const FVector2D TargetMin = Center - HalfSize;
	const FVector2D TargetMax = Center + HalfSize;
	if (FocusBracketAlpha <= 0.01f)
	{
		FocusBracketMin = TargetMin;
		FocusBracketMax = TargetMax;
	}
	else
	{
		FocusBracketMin = FMath::Vector2DInterpTo(FocusBracketMin, TargetMin, DeltaSeconds, 18.0f);
		FocusBracketMax = FMath::Vector2DInterpTo(FocusBracketMax, TargetMax, DeltaSeconds, 18.0f);
	}
	FocusBracketAlpha = FMath::Clamp(
		(FocusBracketAcquireElapsed - IGHorrorHUD::FocusAcquireDelaySeconds)
			/ IGHorrorHUD::FocusAcquireRevealSeconds,
		0.0f,
		1.0f);
}

void AIGHorrorHUD::DrawFocusBracket(const FLinearColor& Color, const float Progress)
{
	if (!Canvas)
	{
		return;
	}

	FLinearColor BracketColor = Color;
	BracketColor.A *= FMath::Clamp(FocusBracketAlpha, 0.0f, 1.0f);
	const float CloseAlpha = FMath::Clamp(Progress, 0.0f, 1.0f);
	const FVector2D Center = (FocusBracketMin + FocusBracketMax) * 0.5f;
	const FVector2D ClosedHalfSize(7.0f, 7.0f);
	const FVector2D DrawMin = FMath::Lerp(
		FocusBracketMin,
		Center - ClosedHalfSize,
		CloseAlpha);
	const FVector2D DrawMax = FMath::Lerp(
		FocusBracketMax,
		Center + ClosedHalfSize,
		CloseAlpha);
	const float CornerX = FMath::Min(18.0f, (DrawMax.X - DrawMin.X) * 0.34f);
	const float CornerY = FMath::Min(18.0f, (DrawMax.Y - DrawMin.Y) * 0.34f);
	constexpr float Thickness = 2.0f;

	auto DrawCorner = [this, &BracketColor](
		const FVector2D& Pivot, const float DirectionX, const float DirectionY,
		const float LengthX, const float LengthY)
	{
		DrawLine(Pivot.X, Pivot.Y, Pivot.X + DirectionX * LengthX, Pivot.Y,
			BracketColor, Thickness);
		DrawLine(Pivot.X, Pivot.Y, Pivot.X, Pivot.Y + DirectionY * LengthY,
			BracketColor, Thickness);
	};

	DrawCorner(DrawMin, 1.0f, 1.0f, CornerX, CornerY);
	DrawCorner(FVector2D(DrawMax.X, DrawMin.Y), -1.0f, 1.0f, CornerX, CornerY);
	DrawCorner(FVector2D(DrawMin.X, DrawMax.Y), 1.0f, -1.0f, CornerX, CornerY);
	DrawCorner(DrawMax, -1.0f, -1.0f, CornerX, CornerY);
}

void AIGHorrorHUD::DrawNotePanel()
{
	const AIGReadableNote* Note = AIGReadableNote::GetOpenNote();
	if (!Note || !Canvas)
	{
		return;
	}

	if (Note->UsesPhoneNotificationPresentation())
	{
		DrawPhoneNotificationPanel(*Note);
		return;
	}

	if (Note->UsesThermalReceiptPresentation())
	{
		DrawThermalReceiptPanel(*Note);
		return;
	}

	const float ScreenWidth = Canvas->ClipX;
	const float ScreenHeight = Canvas->ClipY;

	// Scrim: the room is still there, just pushed back behind the paper.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), 0.0f, 0.0f, ScreenWidth, ScreenHeight);

	// The sheet itself: A4 proportions, sized to the shorter screen axis.
	const float PaperHeight = FMath::Min(ScreenHeight * 0.78f, 720.0f);
	const float PaperWidth = PaperHeight * 0.707f;
	const FVector2D PaperOrigin(
		(ScreenWidth - PaperWidth) * 0.5f,
		(ScreenHeight - PaperHeight) * 0.5f);

	// A hairline drop shadow lifts the sheet off the scrim.
	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.55f),
		PaperOrigin.X + 6.0f, PaperOrigin.Y + 8.0f, PaperWidth, PaperHeight);

	// The sheet is real aged paper when the texture is present — creases,
	// tape marks, a water stain. A flat fill only ever looked like a dialog
	// box, and a note the player is meant to believe in cannot look like UI.
	if (NotePaperTexture)
	{
		DrawTexture(
			NotePaperTexture,
			PaperOrigin.X, PaperOrigin.Y, PaperWidth, PaperHeight,
			0.0f, 0.0f, 1.0f, 1.0f,
			FLinearColor::White, BLEND_Opaque);
	}
	else
	{
		DrawRect(
			FLinearColor(0.88f, 0.87f, 0.83f, 1.0f),
			PaperOrigin.X, PaperOrigin.Y, PaperWidth, PaperHeight);
	}

	UFont* TitleFont = GetFontForRole(EIGHudTextRole::Objective);
	UFont* BodyFont = GetFontForRole(EIGHudTextRole::Prompt);
	UFont* HintFont = GetFontForRole(EIGHudTextRole::Hint);
	if (!TitleFont || !BodyFont)
	{
		return;
	}

	const FLinearColor Ink(0.11f, 0.10f, 0.09f, 1.0f);
	const float LeftMargin = PaperOrigin.X + PaperWidth * 0.09f;
	float PenY = PaperOrigin.Y + PaperHeight * 0.09f;

	// Heading, then the rule under it.
	const FString TitleString = Note->GetTitle().ToString();
	if (!TitleString.IsEmpty())
	{
		FCanvasTextItem TitleItem(
			FVector2D(LeftMargin, PenY), FText::FromString(TitleString), TitleFont, Ink);
		Canvas->DrawItem(TitleItem);
		PenY += TitleFont->GetMaxCharHeight() * 1.15f;

		DrawRect(
			FLinearColor(0.35f, 0.33f, 0.30f, 1.0f),
			LeftMargin, PenY, PaperWidth * 0.82f, 1.5f);
		PenY += BodyFont->GetMaxCharHeight() * 1.1f;
	}

	// The body is authored with explicit line breaks: Korean has no spaces to
	// break on in the places that matter, so automatic wrapping produces worse
	// line endings than simply writing the copy to fit.
	const float LineHeight = BodyFont->GetMaxCharHeight() * 1.45f;
	for (const FText& Line : Note->GetBodyLines())
	{
		// Empty entries are deliberate paragraph breaks.
		if (!Line.IsEmpty())
		{
			FCanvasTextItem LineItem(FVector2D(LeftMargin, PenY), Line, BodyFont, Ink);
			Canvas->DrawItem(LineItem);
		}
		PenY += LineHeight;
	}

	// Dismiss hint at the foot of the sheet.
	if (HintFont)
	{
		const FText Hint = SupportsKorean()
			? bUsingGamepad
				? NSLOCTEXT("IGHUD", "NoteCloseGamepad", "[ A ]  덮기")
				: NSLOCTEXT("IGHUD", "NoteCloseKeyboard", "[ E ]  덮기")
			: FText::FromString(
				bUsingGamepad ? TEXT("[ A ]  Close") : TEXT("[ E ]  Close"));
		const FString HintString = Hint.ToString();
		float HintWidth = 0.0f;
		float HintHeight = 0.0f;
		Canvas->StrLen(HintFont, HintString, HintWidth, HintHeight);
		FCanvasTextItem HintItem(
			FVector2D(
				PaperOrigin.X + (PaperWidth - HintWidth) * 0.5f,
				PaperOrigin.Y + PaperHeight - HintHeight * 2.2f),
			Hint,
			HintFont,
			FLinearColor(0.42f, 0.39f, 0.35f, 1.0f));
		Canvas->DrawItem(HintItem);
	}
}

void AIGHorrorHUD::DrawPhoneNotificationPanel(const AIGReadableNote& Note)
{
	if (!Canvas)
	{
		return;
	}

	const float ScreenWidth = Canvas->ClipX;
	const float ScreenHeight = Canvas->ClipY;
	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.84f),
		0.0f,
		0.0f,
		ScreenWidth,
		ScreenHeight);

	const float PhoneHeight = FMath::Clamp(ScreenHeight * 0.82f, 500.0f, 700.0f);
	const float PhoneWidth = FMath::Clamp(PhoneHeight * 0.58f, 280.0f, 400.0f);
	const FVector2D PhoneOrigin(
		(ScreenWidth - PhoneWidth) * 0.5f,
		(ScreenHeight - PhoneHeight) * 0.5f);
	constexpr float Bezel = 8.0f;
	const FVector2D ScreenOrigin = PhoneOrigin + FVector2D(Bezel, Bezel);
	const float InnerWidth = PhoneWidth - Bezel * 2.0f;
	const float InnerHeight = PhoneHeight - Bezel * 2.0f;

	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.62f),
		PhoneOrigin.X + 7.0f,
		PhoneOrigin.Y + 10.0f,
		PhoneWidth,
		PhoneHeight);
	DrawRect(
		FLinearColor(0.025f, 0.029f, 0.032f, 1.0f),
		PhoneOrigin.X,
		PhoneOrigin.Y,
		PhoneWidth,
		PhoneHeight);
	DrawRect(
		FLinearColor(0.035f, 0.055f, 0.064f, 1.0f),
		ScreenOrigin.X,
		ScreenOrigin.Y,
		InnerWidth,
		InnerHeight);
	DrawRect(
		FLinearColor(0.12f, 0.15f, 0.16f, 1.0f),
		ScreenWidth * 0.5f - 18.0f,
		PhoneOrigin.Y + 4.0f,
		36.0f,
		3.0f);
	DrawRect(
		FLinearColor(0.005f, 0.008f, 0.010f, 1.0f),
		ScreenWidth * 0.5f - 3.0f,
		ScreenOrigin.Y + 7.0f,
		6.0f,
		6.0f);

	UFont* BodyFont = GetFontForRole(EIGHudTextRole::Prompt);
	UFont* MetaFont = KoreanPhoneMetaFont
		? KoreanPhoneMetaFont.Get()
		: GetFontForRole(EIGHudTextRole::Hint);
	if (!BodyFont || !MetaFont)
	{
		return;
	}

	const FLinearColor PrimaryText(0.91f, 0.94f, 0.94f, 1.0f);
	const FLinearColor SecondaryText(0.63f, 0.69f, 0.70f, 1.0f);
	const FLinearColor Accent(0.18f, 0.66f, 0.57f, 1.0f);
	auto DrawPhoneText = [this](
		const FText& Text,
		UFont* Font,
		const float X,
		const float Y,
		const FLinearColor& Color)
	{
		FCanvasTextItem Item(FVector2D(X, Y), Text, Font, Color);
		Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f), FVector2D(1.0f, 1.0f));
		Canvas->DrawItem(Item);
	};

	const float ContentLeft = ScreenOrigin.X + 16.0f;
	DrawPhoneText(
		FText::FromString(TEXT("04:44")),
		MetaFont,
		ContentLeft,
		ScreenOrigin.Y + 9.0f,
		PrimaryText);
	const FText StatusText = FText::FromString(TEXT("LTE   76%"));
	float StatusWidth = 0.0f;
	float StatusHeight = 0.0f;
	Canvas->StrLen(MetaFont, StatusText.ToString(), StatusWidth, StatusHeight);
	DrawPhoneText(
		StatusText,
		MetaFont,
		ScreenOrigin.X + InnerWidth - StatusWidth - 14.0f,
		ScreenOrigin.Y + 9.0f,
		SecondaryText);

	const float NotificationX = ScreenOrigin.X + 11.0f;
	const float NotificationY = ScreenOrigin.Y + 48.0f;
	const float NotificationWidth = InnerWidth - 22.0f;
	const float NotificationHeight = FMath::Min(250.0f, InnerHeight * 0.47f);
	DrawRect(
		FLinearColor(0.065f, 0.086f, 0.092f, 0.98f),
		NotificationX,
		NotificationY,
		NotificationWidth,
		NotificationHeight);
	DrawRect(
		Accent,
		NotificationX,
		NotificationY,
		4.0f,
		NotificationHeight);
	DrawRect(
		Accent,
		NotificationX + 15.0f,
		NotificationY + 15.0f,
		28.0f,
		28.0f);
	// 읽지 않은 상태는 색 면이나 확대 대신 작은 푸른 점 하나로만 남긴다.
	DrawRect(
		FLinearColor(0.20f, 0.55f, 0.95f, 1.0f),
		NotificationX + NotificationWidth - 20.0f,
		NotificationY + 18.0f,
		6.0f,
		6.0f);
	DrawPhoneText(
		NSLOCTEXT("IGHUD", "PhoneApprovalApp", "해온카드"),
		MetaFont,
		NotificationX + 51.0f,
		NotificationY + 13.0f,
		PrimaryText);
	DrawPhoneText(
		NSLOCTEXT("IGHUD", "PhoneApprovalJustNow", "방금 전"),
		MetaFont,
		NotificationX + 51.0f,
		NotificationY + 31.0f,
		SecondaryText);

	DrawPhoneText(
		Note.GetTitle(),
		BodyFont,
		NotificationX + 16.0f,
		NotificationY + 60.0f,
		PrimaryText);
	float PenY = NotificationY + 94.0f;
	const float LineHeight = FMath::Clamp(NotificationHeight / 7.8f, 24.0f, 30.0f);
	const TArray<FText>& Lines = Note.GetBodyLines();
	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		const bool bStatusLine = LineIndex == Lines.Num() - 1;
		DrawPhoneText(
			Lines[LineIndex],
			BodyFont,
			NotificationX + 16.0f,
			PenY,
			bStatusLine ? Accent : PrimaryText);
		PenY += LineHeight;
	}

	DrawPhoneText(
		NSLOCTEXT("IGHUD", "PhoneApprovalHistory", "알림 기록"),
		MetaFont,
		ContentLeft,
		ScreenOrigin.Y + InnerHeight - 31.0f,
		SecondaryText);
	const FText Hint = SupportsKorean()
		? bUsingGamepad
			? NSLOCTEXT("IGHUD", "PhoneCloseGamepad", "[ A ]  휴대폰 내려놓기")
			: NSLOCTEXT("IGHUD", "PhoneCloseKeyboard", "[ E ]  휴대폰 내려놓기")
		: FText::FromString(
			bUsingGamepad ? TEXT("[ A ]  Put phone down") : TEXT("[ E ]  Put phone down"));
	DrawCenteredText(
		Hint,
		FMath::Min(ScreenHeight - 26.0f, PhoneOrigin.Y + PhoneHeight + 16.0f),
		IGHorrorHUD::PaleGray,
		EIGHudTextRole::Hint);
}

void AIGHorrorHUD::DrawThermalReceiptPanel(const AIGReadableNote& Note)
{
	if (!Canvas)
	{
		return;
	}

	const FIGThermalReceiptData& Receipt = Note.GetThermalReceiptData();
	const float ScreenWidth = Canvas->ClipX;
	const float ScreenHeight = Canvas->ClipY;

	// A real convenience-store receipt is an 80 mm thermal roll, not A4.
	// Keep it tall and narrow while leaving a small strip of scrim below for
	// the interaction hint, so the hint never looks printed on the receipt.
	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.78f),
		0.0f, 0.0f, ScreenWidth, ScreenHeight);

	const float PaperHeight = FMath::Min(ScreenHeight * 0.91f, 760.0f);
	const float PaperWidth = FMath::Min(PaperHeight * 0.465f, ScreenWidth * 0.32f);
	const FVector2D PaperOrigin(
		(ScreenWidth - PaperWidth) * 0.5f,
		(ScreenHeight - PaperHeight) * 0.5f);

	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.62f),
		PaperOrigin.X + 5.0f,
		PaperOrigin.Y + 7.0f,
		PaperWidth,
		PaperHeight);

	if (ReceiptPaperTexture)
	{
		DrawTexture(
			ReceiptPaperTexture,
			PaperOrigin.X, PaperOrigin.Y, PaperWidth, PaperHeight,
			0.0f, 0.0f, 1.0f, 1.0f,
			FLinearColor(0.97f, 0.965f, 0.925f, 1.0f), BLEND_Opaque);
	}
	else
	{
		DrawRect(
			FLinearColor(0.945f, 0.94f, 0.885f, 1.0f),
			PaperOrigin.X, PaperOrigin.Y, PaperWidth, PaperHeight);
	}

	// Uneven heat and the low-grade roll stock leave faint horizontal bands.
	// Two different intervals prevent the surface from looking like ruled
	// notebook paper while remaining visible at a 1280x720 capture.
	for (float BandY = PaperOrigin.Y + 9.0f;
		BandY < PaperOrigin.Y + PaperHeight - 8.0f;
		BandY += 13.0f)
	{
		DrawRect(
			FLinearColor(0.24f, 0.225f, 0.19f, 0.012f),
			PaperOrigin.X + 1.0f,
			BandY,
			PaperWidth - 2.0f,
			1.0f);
	}
	for (float BandY = PaperOrigin.Y + 31.0f;
		BandY < PaperOrigin.Y + PaperHeight - 8.0f;
		BandY += 47.0f)
	{
		DrawRect(
			FLinearColor(1.0f, 0.995f, 0.95f, 0.04f),
			PaperOrigin.X + 3.0f,
			BandY,
			PaperWidth - 6.0f,
			1.0f);
	}
	// A soft compression crease and slightly dirty roll edges sell physical
	// paper without making a brand-new receipt look like an antique note.
	const float CreaseY = PaperOrigin.Y + PaperHeight * 0.585f;
	DrawRect(
		FLinearColor(0.23f, 0.21f, 0.17f, 0.035f),
		PaperOrigin.X + 2.0f, CreaseY, PaperWidth - 4.0f, 2.0f);
	DrawRect(
		FLinearColor(1.0f, 0.995f, 0.96f, 0.10f),
		PaperOrigin.X + 3.0f, CreaseY + 2.0f, PaperWidth - 6.0f, 1.0f);
	DrawRect(
		FLinearColor(0.24f, 0.22f, 0.18f, 0.36f),
		PaperOrigin.X,
		PaperOrigin.Y,
		1.0f,
		PaperHeight);
	DrawRect(
		FLinearColor(0.24f, 0.22f, 0.18f, 0.36f),
		PaperOrigin.X + PaperWidth - 1.0f,
		PaperOrigin.Y,
		1.0f,
		PaperHeight);

	UFont* HeaderFont = KoreanReceiptHeaderFont
		? KoreanReceiptHeaderFont.Get()
		: GetFontForRole(EIGHudTextRole::Prompt);
	UFont* ReceiptFont = KoreanReceiptFont
		? KoreanReceiptFont.Get()
		: GetFontForRole(EIGHudTextRole::Hint);
	if (!HeaderFont || !ReceiptFont)
	{
		return;
	}

	const FLinearColor ThermalInk(0.015f, 0.012f, 0.009f, 0.98f);
	const FLinearColor FaintInk(0.045f, 0.040f, 0.032f, 0.84f);
	const float ContentLeft = PaperOrigin.X + PaperWidth * 0.065f;
	const float ContentRight = PaperOrigin.X + PaperWidth * 0.935f;
	const float ContentWidth = ContentRight - ContentLeft;
	// Thermal printers pack rows much more tightly than normal UI text. Font
	// ascent metrics are intentionally not used here: Malgun Gothic reports a
	// generous line box that made the first pass look like a document again.
	const float ReceiptLineHeight =
		FMath::Clamp(PaperHeight / 39.0f, 15.5f, 17.5f);
	float PenY = PaperOrigin.Y + PaperHeight * 0.027f;

	auto MeasureText = [this](UFont* Font, const FText& Text)
	{
		FVector2D Size = FVector2D::ZeroVector;
		if (Canvas && Font && !Text.IsEmpty())
		{
			Canvas->StrLen(Font, Text.ToString(), Size.X, Size.Y);
		}
		return Size;
	};
	auto DrawTextAt = [this, &ThermalInk](
		const FText& Text,
		UFont* Font,
		const float X,
		const float Y,
		const FLinearColor* Color = nullptr)
	{
		if (!Canvas || !Font || Text.IsEmpty())
		{
			return;
		}
		FCanvasTextItem Item(
			FVector2D(X, Y),
			Text,
			Font,
			Color ? *Color : ThermalInk);
		Canvas->DrawItem(Item);
	};
	auto DrawCentered = [&](
		const FText& Text,
		UFont* Font,
		const float Y,
		const FLinearColor* Color = nullptr)
	{
		const FVector2D Size = MeasureText(Font, Text);
		DrawTextAt(
			Text,
			Font,
			PaperOrigin.X + (PaperWidth - Size.X) * 0.5f,
			Y,
			Color);
	};
	auto DrawRight = [&](
		const FText& Text,
		UFont* Font,
		const float RightX,
		const float Y,
		const FLinearColor* Color = nullptr)
	{
		const FVector2D Size = MeasureText(Font, Text);
		DrawTextAt(Text, Font, RightX - Size.X, Y, Color);
	};
	auto DrawRule = [&](const bool bHeavy = false)
	{
		const float SegmentWidth = bHeavy ? 5.0f : 3.0f;
		const float GapWidth = bHeavy ? 2.5f : 3.0f;
		for (float X = ContentLeft; X < ContentRight; X += SegmentWidth + GapWidth)
		{
			DrawRect(
				ThermalInk,
				X,
				PenY,
				FMath::Min(SegmentWidth, ContentRight - X),
				bHeavy ? 1.5f : 1.0f);
		}
		PenY += bHeavy ? 7.0f : 6.0f;
	};
	auto FormatAmount = [](const int32 Amount)
	{
		const bool bNegative = Amount < 0;
		FString Digits = FString::FromInt(FMath::Abs(Amount));
		for (int32 Index = Digits.Len() - 3; Index > 0; Index -= 3)
		{
			Digits.InsertAt(Index, TEXT(','));
		}
		return FText::FromString(bNegative ? TEXT("-") + Digits : Digits);
	};

	DrawCentered(Receipt.StoreName, HeaderFont, PenY);
	// A second sub-pixel impression mimics the heavy one-colour logo pass
	// common at the top of convenience-store thermal receipts.
	const FVector2D StoreNameSize = MeasureText(HeaderFont, Receipt.StoreName);
	DrawTextAt(
		Receipt.StoreName,
		HeaderFont,
		PaperOrigin.X + (PaperWidth - StoreNameSize.X) * 0.5f + 0.65f,
		PenY);
	PenY += FMath::Max(23.0f, HeaderFont->GetMaxCharHeight() * 1.05f);
	DrawCentered(Receipt.StoreSubtitle, ReceiptFont, PenY, &FaintInk);
	// Gulim's descenders extend slightly beyond its reported compact line
	// box; keep the first perforated rule visibly below the branch subtitle.
	PenY += ReceiptLineHeight + 4.0f;
	DrawRule(true);

	for (const FText& DetailLine : Receipt.StoreDetailLines)
	{
		DrawTextAt(DetailLine, ReceiptFont, ContentLeft, PenY);
		PenY += ReceiptLineHeight;
	}
	if (!Receipt.StoreDetailLines.IsEmpty())
	{
		PenY += 1.0f;
	}

	for (const FText& PolicyLine : Receipt.PolicyLines)
	{
		DrawTextAt(PolicyLine, ReceiptFont, ContentLeft, PenY, &FaintInk);
		PenY += ReceiptLineHeight;
	}
	PenY += 1.0f;
	DrawRule();

	// Keep identifiers and the timestamp on adjacent dense rows. This avoids
	// collisions on 720p while matching Korean POS layouts that print a
	// receipt/transaction number immediately before the dated sales line.
	DrawTextAt(
		FText::Format(
			NSLOCTEXT("IGHUD", "ReceiptTransactionNumber", "거래NO {0}"),
			Receipt.ReceiptNumber),
		ReceiptFont,
		ContentLeft,
		PenY);
	DrawRight(Receipt.PosLabel, ReceiptFont, ContentRight, PenY);
	PenY += ReceiptLineHeight;
	DrawCentered(Receipt.TransactionDateTime, ReceiptFont, PenY);
	PenY += ReceiptLineHeight;
	DrawRule();

	// Give Korean product names a true left column. The former 49% boundary
	// made "새벽샘물500mL" touch the quantity at 1280x720.
	const float QuantityCenterX = ContentLeft + ContentWidth * 0.53f;
	const float UnitPriceRightX = ContentLeft + ContentWidth * 0.76f;
	DrawTextAt(
		NSLOCTEXT("IGHUD", "ReceiptProductHeading", "상품명"),
		ReceiptFont,
		ContentLeft,
		PenY,
		&FaintInk);
	const FText QuantityHeading = NSLOCTEXT("IGHUD", "ReceiptQuantityHeading", "수량");
	const FVector2D QuantityHeadingSize = MeasureText(ReceiptFont, QuantityHeading);
	DrawTextAt(
		QuantityHeading,
		ReceiptFont,
		QuantityCenterX - QuantityHeadingSize.X * 0.5f,
		PenY,
		&FaintInk);
	DrawRight(
		NSLOCTEXT("IGHUD", "ReceiptUnitPriceHeading", "단가"),
		ReceiptFont,
		UnitPriceRightX,
		PenY,
		&FaintInk);
	DrawRight(
		NSLOCTEXT("IGHUD", "ReceiptAmountHeading", "금액"),
		ReceiptFont,
		ContentRight,
		PenY,
		&FaintInk);
	PenY += ReceiptLineHeight;

	for (const FIGReceiptItemLine& Item : Receipt.Items)
	{
		DrawTextAt(Item.ProductName, ReceiptFont, ContentLeft, PenY);
		const FText QuantityText = FText::AsNumber(Item.Quantity);
		const FVector2D QuantitySize = MeasureText(ReceiptFont, QuantityText);
		DrawTextAt(
			QuantityText,
			ReceiptFont,
			QuantityCenterX - QuantitySize.X * 0.5f,
			PenY);
		DrawRight(
			FormatAmount(Item.UnitPrice > 0 ? Item.UnitPrice : Item.Amount),
			ReceiptFont,
			UnitPriceRightX,
			PenY);
		DrawRight(FormatAmount(Item.Amount), ReceiptFont, ContentRight, PenY);
		PenY += ReceiptLineHeight + 1.0f;
	}
	DrawRule();

	auto DrawAmountRow = [&](
		const FText& Label,
		const int32 Amount,
		UFont* Font,
		const FLinearColor* Color = nullptr)
	{
		DrawTextAt(Label, Font, ContentLeft, PenY, Color);
		DrawRight(FormatAmount(Amount), Font, ContentRight, PenY, Color);
		PenY += FMath::Max(
			ReceiptLineHeight,
			static_cast<float>(Font->GetMaxCharHeight()) * 1.02f);
	};

	DrawAmountRow(
		NSLOCTEXT("IGHUD", "ReceiptSubtotal", "총 구 매 액"),
		Receipt.Subtotal,
		HeaderFont);
	DrawAmountRow(
		NSLOCTEXT("IGHUD", "ReceiptTaxable", "과세물품가액"),
		Receipt.TaxableSupply,
		ReceiptFont,
		&FaintInk);
	DrawAmountRow(
		NSLOCTEXT("IGHUD", "ReceiptVat", "부 가 세"),
		Receipt.Vat,
		ReceiptFont,
		&FaintInk);
	DrawAmountRow(
		NSLOCTEXT("IGHUD", "ReceiptTotal", "결 제 금 액"),
		Receipt.Total,
		HeaderFont);
	DrawRule(true);

	if (!Receipt.PaymentHeading.IsEmpty())
	{
		DrawCentered(
			Receipt.PaymentHeading,
			ReceiptFont,
			PenY,
			&FaintInk);
		PenY += ReceiptLineHeight;
	}
	for (const FIGReceiptKeyValueLine& PaymentLine : Receipt.PaymentLines)
	{
		DrawTextAt(PaymentLine.Label, ReceiptFont, ContentLeft, PenY);
		DrawRight(PaymentLine.Value, ReceiptFont, ContentRight, PenY);
		PenY += ReceiptLineHeight;
	}
	PenY += 2.0f;
	DrawRule();

	for (const FText& FooterLine : Receipt.FooterLines)
	{
		DrawCentered(FooterLine, ReceiptFont, PenY, &FaintInk);
		PenY += ReceiptLineHeight;
	}

	// EAN-13-like layout: start/centre/end guards, six left digits with L/G
	// parity, and six right digits. Story data deliberately supplies an
	// invalid check digit, so this has authentic proportions without becoming
	// a usable identifier for a real product.
	FString BarcodeDigits = Receipt.BarcodeDigits;
	bool bValidBarcodeDigits = BarcodeDigits.Len() == 13;
	for (int32 Index = 0; bValidBarcodeDigits && Index < BarcodeDigits.Len(); ++Index)
	{
		bValidBarcodeDigits =
			BarcodeDigits[Index] >= TEXT('0') && BarcodeDigits[Index] <= TEXT('9');
	}
	if (!bValidBarcodeDigits)
	{
		BarcodeDigits = TEXT("2904440711004");
	}

	static const TCHAR* LeftOddPatterns[10] = {
		TEXT("0001101"), TEXT("0011001"), TEXT("0010011"), TEXT("0111101"),
		TEXT("0100011"), TEXT("0110001"), TEXT("0101111"), TEXT("0111011"),
		TEXT("0110111"), TEXT("0001011"),
	};
	static const TCHAR* LeftEvenPatterns[10] = {
		TEXT("0100111"), TEXT("0110011"), TEXT("0011011"), TEXT("0100001"),
		TEXT("0011101"), TEXT("0111001"), TEXT("0000101"), TEXT("0010001"),
		TEXT("0001001"), TEXT("0010111"),
	};
	static const TCHAR* RightPatterns[10] = {
		TEXT("1110010"), TEXT("1100110"), TEXT("1101100"), TEXT("1000010"),
		TEXT("1011100"), TEXT("1001110"), TEXT("1010000"), TEXT("1000100"),
		TEXT("1001000"), TEXT("1110100"),
	};
	static const TCHAR* LeftParity[10] = {
		TEXT("LLLLLL"), TEXT("LLGLGG"), TEXT("LLGGLG"), TEXT("LLGGGL"),
		TEXT("LGLLGG"), TEXT("LGGLLG"), TEXT("LGGGLL"), TEXT("LGLGLG"),
		TEXT("LGLGGL"), TEXT("LGGLGL"),
	};

	FString BarcodeModules(TEXT("101"));
	const int32 LeadingDigit = BarcodeDigits[0] - TEXT('0');
	for (int32 Index = 1; Index <= 6; ++Index)
	{
		const int32 Digit = BarcodeDigits[Index] - TEXT('0');
		BarcodeModules += LeftParity[LeadingDigit][Index - 1] == TEXT('L')
			? LeftOddPatterns[Digit]
			: LeftEvenPatterns[Digit];
	}
	BarcodeModules += TEXT("01010");
	for (int32 Index = 7; Index <= 12; ++Index)
	{
		BarcodeModules += RightPatterns[BarcodeDigits[Index] - TEXT('0')];
	}
	BarcodeModules += TEXT("101");

	// Leave one printer row between the service line and the first guard bar.
	const float BarcodeTop = PenY + 5.0f;
	const float BarcodeHeight = FMath::Max(
		26.0f,
		FMath::Min(35.0f, PaperOrigin.Y + PaperHeight - BarcodeTop - 26.0f));
	const float UnitWidth = ContentWidth / 95.0f;
	float BarcodeX = ContentLeft;
	for (int32 ModuleIndex = 0; ModuleIndex < BarcodeModules.Len(); ++ModuleIndex)
	{
		if (BarcodeModules[ModuleIndex] == TEXT('1'))
		{
			const bool bGuard =
				ModuleIndex <= 2
				|| (ModuleIndex >= 45 && ModuleIndex <= 49)
				|| ModuleIndex >= 92;
			DrawRect(
				ThermalInk,
				BarcodeX,
				BarcodeTop,
				FMath::Max(1.0f, UnitWidth * 0.86f),
				BarcodeHeight + (bGuard ? 4.0f : 0.0f));
		}
		BarcodeX += UnitWidth;
	}
	PenY = BarcodeTop + BarcodeHeight + 3.0f;
	const FString HumanReadableBarcode = FString::Printf(
		TEXT("%c  %s  %s"),
		BarcodeDigits[0],
		*BarcodeDigits.Mid(1, 6),
		*BarcodeDigits.Mid(7, 6));
	DrawCentered(
		FText::FromString(HumanReadableBarcode),
		ReceiptFont,
		PenY,
		&FaintInk);

	const FText Hint = SupportsKorean()
		? bUsingGamepad
			? NSLOCTEXT("IGHUD", "ReceiptCloseGamepad", "[ A ]  영수증 내려놓기")
			: NSLOCTEXT("IGHUD", "ReceiptCloseKeyboard", "[ E ]  영수증 내려놓기")
		: FText::FromString(
			bUsingGamepad
				? TEXT("[ A ]  Put receipt down")
				: TEXT("[ E ]  Put receipt down"));
	const FVector2D HintSize = MeasureText(ReceiptFont, Hint);
	const float HintY = FMath::Min(
		ScreenHeight - HintSize.Y - 5.0f,
		PaperOrigin.Y + PaperHeight + 8.0f);
	DrawTextAt(
		Hint,
		ReceiptFont,
		(ScreenWidth - HintSize.X) * 0.5f,
		HintY,
		&IGHorrorHUD::PaleGray);
}
