#include "Player/IGHorrorHUD.h"

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
#include "IndieGame.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Interaction/IGReadableNote.h"
#include "Narrative/IGMissingFloorNarrativeSubsystem.h"
#include "Player/IGInteractionComponent.h"
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
	constexpr int32 LargeFontSize = 22;
	constexpr int32 MediumFontSize = 19;
	constexpr int32 SmallFontSize = 14;
	constexpr int32 PhoneMetaFontSize = 15;

	const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.9f);
	const FLinearColor PaleGray(0.82f, 0.84f, 0.82f, 0.95f);
	const FLinearColor MutedGray(0.62f, 0.64f, 0.62f, 0.9f);
	const FLinearColor RedAccent(0.72f, 0.08f, 0.06f, 1.0f);
	const FLinearColor ThoughtBlue(0.74f, 0.78f, 0.86f, 1.0f);
	const FLinearColor DialogueIvory(0.88f, 0.87f, 0.81f, 1.0f);
	const FLinearColor DialogueTeal(0.42f, 0.64f, 0.59f, 1.0f);

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
			TEXT("IGAudioCalibrationPreview"));
	InitializeKoreanFont();

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
	JournalMetalTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Prototype/Textures/T_MetalBrushed_D.T_MetalBrushed_D"));
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

UFont* AIGHorrorHUD::MakeRuntimeFont(UFontFace* FontFace, const int32 PixelSize, const TCHAR* FontName)
{
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
		KoreanFontMedium = MakeRuntimeFont(
			FontFace, IGHorrorHUD::MediumFontSize, TEXT("KoreanFontMedium"));
		KoreanFontSmall = MakeRuntimeFont(
			FontFace, IGHorrorHUD::SmallFontSize, TEXT("KoreanFontSmall"));
		KoreanPhoneMetaFont = MakeRuntimeFont(
			FontFace,
			IGHorrorHUD::PhoneMetaFontSize,
			TEXT("KoreanPhoneMetaFont"));

		// Receipt printers use a compact, almost fixed-width bitmap face. Keep
		// the normal UI on Malgun Gothic, but prefer the narrower Gulim/Dotum
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
		case EIGHudTextRole::Thought:
		case EIGHudTextRole::Dialogue:
			return KoreanFontMedium.Get();
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
	bSystemMenuVisible = Presentation.bVisible;
	bSystemMenuIsTitle = Presentation.bTitle;
	bSystemMenuIsCredits = Presentation.bCredits;
	bSystemMenuIsAudioCalibration = Presentation.bAudioCalibration;
	bSystemMenuIsDisplaySettings = Presentation.bDisplaySettings;
	SystemMenuSelectedRow = FMath::Clamp(Presentation.SelectedRow, 0, 4);
	bSystemMenuCanContinue = Presentation.bCanContinue;
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
				"LS 이동  ·  L3 달리기  ·  R3 앉기  ·  A 상호작용  ·  B 두드리기  ·  X 손전등")
			: NSLOCTEXT(
				"IGHUD",
				"HintsKeyboard",
				"WASD 이동  ·  Shift 달리기  ·  C 앉기  ·  E 상호작용  ·  Q 두드리기  ·  F 손전등")
		: FText::FromString(
			bUsingGamepad
				? TEXT("LS MOVE  |  L3 SPRINT  |  R3 CROUCH  |  A INTERACT  |  B KNOCK  |  X FLASHLIGHT")
				: TEXT("WASD MOVE  |  SHIFT SPRINT  |  C CROUCH  |  E INTERACT  |  Q KNOCK  |  F FLASHLIGHT"));
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
	const float Visibility = IGHorrorHUD::SmoothStep01(NormalizedAge / 0.08f)
		* (1.0f - IGHorrorHUD::SmoothStep01((NormalizedAge - 0.82f) / 0.18f));

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
		DrawFrame(2, Visibility * 0.94f);
		return true;
	}

	constexpr float ClosingAnimationEnd = 0.72f;
	const float FramePosition = FMath::Clamp(
		NormalizedAge / ClosingAnimationEnd,
		0.0f,
		1.0f) * IGHorrorHUD::CaptureEmbraceFrameCount;
	const int32 FrameIndex = FMath::Clamp(
		FMath::FloorToInt(FramePosition),
		0,
		IGHorrorHUD::CaptureEmbraceFrameCount - 1);
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

	FCanvasTileItem Scrim(
		FVector2D::ZeroVector,
		FVector2D(Canvas->ClipX, Canvas->ClipY),
		FLinearColor(0.0f, 0.0f, 0.0f, 0.94f));
	Scrim.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Scrim);
	DrawCenteredText(
		bKorean
			? NSLOCTEXT("IGHUD", "AccessibilityTitle", "접근성 설정")
			: FText::FromString(TEXT("ACCESSIBILITY")),
		72.0f,
		IGHorrorHUD::PaleGray,
		EIGHudTextRole::Objective);

	const float RowStartY = FMath::Max(96.0f, Canvas->ClipY * 0.15f);
	const float RowSpacing = FMath::Clamp(Canvas->ClipY * 0.038f, 24.0f, 34.0f);
	for (int32 Row = 0; Row < UE_ARRAY_COUNT(Labels); ++Row)
	{
		const bool bSelected = Row == AccessibilitySelectedRow;
		FString RowText = Values[Row].IsEmpty()
			? Labels[Row]
			: FString::Printf(TEXT("%s    < %s >"), *Labels[Row], *Values[Row]);
		RowText = FString(bSelected ? TEXT(">  ") : TEXT("   ")) + RowText;
		DrawCenteredText(
			FText::FromString(RowText),
			RowStartY + Row * RowSpacing,
			bSelected ? IGHorrorHUD::RedAccent : IGHorrorHUD::PaleGray,
			bSelected ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint);
	}

	// Keep a live sample in the same screen where size, surface opacity and
	// safe area are changed. The preview is text-only and never mutates the
	// actual story queue.
	const float ResolutionScale = FMath::Clamp(
		Canvas->ClipY / 1080.0f,
		0.85f,
		2.0f);
	const float PreviewTextScale = GetResolutionTextScale(Settings.CaptionSizeScale);
	const float PreviewWidth = FMath::Min(
		Canvas->ClipX * Settings.CaptionSafeAreaScale - 32.0f * ResolutionScale,
		780.0f * ResolutionScale);
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
	const float PreviewBodyHeight = FMath::Max(
		16.0f,
		PreviewRawHeight * PreviewTextScale);
	const float PreviewSpeakerHeight = 14.0f * PreviewTextScale * 0.78f;
	const float PreviewHeight = FMath::Max(
		48.0f * ResolutionScale,
		PreviewSpeakerHeight + PreviewBodyHeight + 23.0f * ResolutionScale);
	const float PreviewX = (Canvas->ClipX - PreviewWidth) * 0.5f;
	const float PreviewY = Canvas->ClipY - 148.0f * ResolutionScale;
	FCanvasTileItem PreviewSurface(
		FVector2D(PreviewX, PreviewY),
		FVector2D(PreviewWidth, PreviewHeight),
		FLinearColor(
			0.018f,
			0.021f,
			0.020f,
			Settings.CaptionBackgroundOpacity));
	PreviewSurface.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(PreviewSurface);
	RecordLayoutValidationRect(
		FVector2D(PreviewX, PreviewY),
		FVector2D(PreviewX + PreviewWidth, PreviewY + PreviewHeight));
	DrawLeftAlignedText(
		bKorean
			? NSLOCTEXT("IGHUD", "CaptionPreviewSpeaker", "미리 보기")
			: FText::FromString(TEXT("PREVIEW")),
		FVector2D(
			PreviewX + 18.0f * ResolutionScale,
			PreviewY + 7.0f * ResolutionScale),
		IGHorrorHUD::ThoughtBlue,
		EIGHudTextRole::Speaker,
		PreviewTextScale * 0.78f,
		true);
	DrawLeftAlignedText(
		bKorean
			? NSLOCTEXT("IGHUD", "CaptionPreviewBody", "대사·소리 캡션 예시입니다.")
			: FText::FromString(TEXT("DIALOGUE + SOUND CAPTION PREVIEW.")),
		FVector2D(
			PreviewX + 18.0f * ResolutionScale,
			PreviewY + 8.0f * ResolutionScale
				+ PreviewSpeakerHeight + 5.0f * ResolutionScale),
		IGHorrorHUD::PaleGray,
		EIGHudTextRole::Dialogue,
		PreviewTextScale,
		true);

	DrawCenteredText(
		bKorean
			? bUsingGamepad
				? NSLOCTEXT(
					"IGHUD",
					"AccessibilityControlsGamepad",
					"D-pad 항목·변경  ·  A 선택  ·  B 닫기")
				: NSLOCTEXT(
					"IGHUD",
					"AccessibilityControlsKeyboard",
					"방향키 항목·변경  ·  Enter 선택  ·  Esc/F10 닫기")
			: FText::FromString(
				bUsingGamepad
					? TEXT("D-PAD SELECT + CHANGE  |  A APPLY  |  B CLOSE")
					: TEXT("ARROWS SELECT + CHANGE  |  ENTER APPLY  |  ESC/F10 CLOSE")),
		FMath::Max(RowStartY + 16.5f * RowSpacing, Canvas->ClipY - 48.0f),
		IGHorrorHUD::MutedGray,
		EIGHudTextRole::Hint);
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
		bKorean ? TEXT("화면 밝기") : TEXT("DISPLAY BRIGHTNESS"),
		bKorean ? TEXT("노크 다시 듣기") : TEXT("PLAY KNOCK AGAIN"),
		bSystemMenuAudioCalibrationFirstRun
			? bKorean ? TEXT("저장하고 타이틀로") : TEXT("SAVE AND CONTINUE")
			: bKorean ? TEXT("저장하고 돌아가기") : TEXT("SAVE AND BACK")
	};
	const float RowStartY = PanelOrigin.Y + PanelSize.Y - 224.0f * Scale;
	const float RowSpacing = 42.0f * Scale;
	for (int32 Row = 0; Row < UE_ARRAY_COUNT(Labels); ++Row)
	{
		const bool bSelected = Row == AudioCalibrationSelectedRow;
		FString Label = FString(bSelected ? TEXT(">  ") : TEXT("   ")) + Labels[Row];
		if (Row == 0 || Row == 1)
		{
			const int32 Step = Row == 0
				? AudioCalibrationVolumeStep
				: AudioCalibrationBrightnessStep;
			const int32 Count = Row == 0 ? 7 : 5;
			FString Dots;
			for (int32 Dot = 0; Dot < Count; ++Dot)
			{
				Dots += Dot == Step ? TEXT("●") : TEXT("○");
			}
			Label += FString::Printf(TEXT("    < %s >"), *Dots);
		}
		DrawCenteredText(
			FText::FromString(Label),
			RowStartY + Row * RowSpacing,
			bSelected ? IGHorrorHUD::RedAccent : IGHorrorHUD::PaleGray,
			bSelected ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint,
			0.92f * Scale);
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
		bDisplaySettingsAwaitingConfirmation
			? bKorean ? TEXT("이 설정 유지") : TEXT("KEEP THESE SETTINGS")
			: bKorean ? TEXT("변경 적용") : TEXT("APPLY CHANGES"),
		bDisplaySettingsAwaitingConfirmation
			? bKorean ? TEXT("이전 설정으로 되돌리기") : TEXT("REVERT SETTINGS")
			: bKorean ? TEXT("변경 취소하고 돌아가기") : TEXT("CANCEL AND BACK")
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
		FString(),
		FString(),
		FString(),
		FString()
	};

	DrawCenteredText(
		bKorean
			? NSLOCTEXT("IGHUD", "DisplaySettingsTitle", "화면 설정")
			: FText::FromString(TEXT("DISPLAY SETTINGS")),
		72.0f,
		IGHorrorHUD::PaleGray,
		EIGHudTextRole::Objective);
	DrawCenteredText(
		bKorean
			? NSLOCTEXT(
				"IGHUD",
				"DisplaySettingsSubtitle",
				"Windows-v1 지원 범위 안에서 화면과 성능을 조정합니다.")
			: FText::FromString(
				TEXT("ADJUST DISPLAY AND PERFORMANCE WITHIN WINDOWS-V1 SUPPORT.")),
		112.0f,
		IGHorrorHUD::MutedGray,
		EIGHudTextRole::Hint);
	if (bDisplaySettingsAwaitingConfirmation)
	{
		DrawCenteredText(
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
			140.0f,
			IGHorrorHUD::RedAccent,
			EIGHudTextRole::Hint);
	}
	else if (bDisplaySettingsApplied)
	{
		DrawCenteredText(
			bKorean
				? NSLOCTEXT("IGHUD", "DisplaySettingsApplied", "설정을 적용했습니다.")
				: FText::FromString(TEXT("SETTINGS APPLIED.")),
			140.0f,
			FLinearColor(0.62f, 0.72f, 0.66f, 1.0f),
			EIGHudTextRole::Hint);
	}
	else if (!SystemMenuStatusText.IsEmpty())
	{
		DrawCenteredText(
			SystemMenuStatusText,
			140.0f,
			bSystemMenuStatusIsError
				? IGHorrorHUD::RedAccent
				: IGHorrorHUD::PaleGray,
			EIGHudTextRole::Hint);
	}

	const float RowStartY = FMath::Max(174.0f, Canvas->ClipY * 0.24f);
	const float RowSpacing = FMath::Clamp(Canvas->ClipY * 0.055f, 34.0f, 42.0f);
	for (int32 Row = 0; Row < UE_ARRAY_COUNT(Labels); ++Row)
	{
		const bool bSelected = Row == DisplaySettingsSelectedRow;
		FString RowText = Values[Row].IsEmpty()
			? Labels[Row]
			: FString::Printf(TEXT("%s    < %s >"), *Labels[Row], *Values[Row]);
		RowText = FString(bSelected ? TEXT(">  ") : TEXT("   ")) + RowText;
		DrawCenteredText(
			FText::FromString(RowText),
			RowStartY + Row * RowSpacing,
			bSelected ? IGHorrorHUD::RedAccent : IGHorrorHUD::PaleGray,
			bSelected ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint);
	}

	DrawCenteredText(
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
					"D-pad 항목·변경  ·  A 선택  ·  B/View 취소")
				: NSLOCTEXT(
					"IGHUD",
					"DisplaySettingsControlsKeyboard",
					"방향키/WASD 항목·변경  ·  Enter 선택  ·  Esc 취소  ·  마우스 선택")
			: FText::FromString(
				bUsingGamepad
					? TEXT("D-PAD SELECT + CHANGE  |  A APPLY  |  B/VIEW CANCEL")
					: TEXT("ARROWS/WASD CHANGE  |  ENTER APPLY  |  ESC CANCEL  |  MOUSE SELECT")),
		FMath::Max(0.0f, Canvas->ClipY - 48.0f),
		IGHorrorHUD::MutedGray,
		EIGHudTextRole::Hint);
}

void AIGHorrorHUD::DrawSystemMenuPanel()
{
	if (!Canvas)
	{
		return;
	}

	const bool bKorean = SupportsKorean();
	FCanvasTileItem Scrim(
		FVector2D::ZeroVector,
		FVector2D(Canvas->ClipX, Canvas->ClipY),
		FLinearColor(0.004f, 0.006f, 0.007f, 0.985f));
	Scrim.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Scrim);
	if (bSystemMenuIsAudioCalibration)
	{
		// 보정판은 자체 계조와 중앙 구분선을 갖는다. 타이틀 장식선을 뒤에
		// 남기면 화면 결함이나 네 번째 밝기 칸처럼 보이므로 여기서 분기한다.
		DrawAudioCalibrationPanel();
		return;
	}

	// A single reflected strip is enough to suggest the rooftop tank without
	// placing a literal spoiler behind the first screen.
	const float CenterX = Canvas->ClipX * 0.5f;
	FCanvasTileItem Reflection(
		FVector2D(CenterX - 0.5f, 0.0f),
		FVector2D(1.0f, Canvas->ClipY),
		FLinearColor(0.22f, 0.25f, 0.26f, 0.16f));
	Reflection.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Reflection);
	FCanvasTileItem Accent(
		FVector2D(CenterX - 44.0f, 174.0f),
		FVector2D(88.0f, 1.0f),
		IGHorrorHUD::RedAccent);
	Accent.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Accent);
	if (bSystemMenuIsDisplaySettings)
	{
		DrawDisplaySettingsPanel();
		return;
	}
	if (bSystemMenuIsCredits)
	{
		DrawCenteredText(
			bKorean
				? NSLOCTEXT("IGHUD", "CreditsTitle", "만든 사람")
				: FText::FromString(TEXT("CREDITS")),
			96.0f,
			IGHorrorHUD::PaleGray,
			EIGHudTextRole::Objective,
			1.15f);
		DrawCenteredText(
			bKorean
				? NSLOCTEXT("IGHUD", "CreditsGameTitle", "없는 층")
				: FText::FromString(TEXT("THE MISSING FLOOR")),
			136.0f,
			IGHorrorHUD::MutedGray,
			EIGHudTextRole::Hint);

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
		const float CreditStartY = FMath::Max(230.0f, Canvas->ClipY * 0.34f);
		const float CreditSpacing = FMath::Clamp(
			Canvas->ClipY * 0.058f,
			32.0f,
			42.0f);
		for (int32 Line = 0; Line < UE_ARRAY_COUNT(CreditLines); ++Line)
		{
			DrawCenteredText(
				CreditLines[Line],
				CreditStartY + Line * CreditSpacing,
				Line == 0 ? IGHorrorHUD::PaleGray : IGHorrorHUD::MutedGray,
				Line == 0 ? EIGHudTextRole::Prompt : EIGHudTextRole::Hint);
		}
		DrawCenteredText(
			bKorean
				? bUsingGamepad
					? NSLOCTEXT("IGHUD", "CreditsBackGamepad", "B  돌아가기")
					: NSLOCTEXT("IGHUD", "CreditsBackKeyboard", "Esc 또는 Enter  돌아가기")
				: FText::FromString(
					bUsingGamepad ? TEXT("B  BACK") : TEXT("ESC OR ENTER  BACK")),
			FMath::Max(0.0f, Canvas->ClipY - 48.0f),
			IGHorrorHUD::MutedGray,
			EIGHudTextRole::Hint);
		return;
	}

	DrawCenteredText(
		bSystemMenuIsTitle
			? bKorean
				? NSLOCTEXT("IGHUD", "MainTitle", "없는 층")
				: FText::FromString(TEXT("THE MISSING FLOOR"))
			: bKorean
				? NSLOCTEXT("IGHUD", "PauseTitle", "잠시 멈춤")
				: FText::FromString(TEXT("PAUSED")),
		92.0f,
		IGHorrorHUD::PaleGray,
		EIGHudTextRole::Objective,
		1.25f);
	DrawCenteredText(
		bSystemMenuIsTitle
			? bKorean
				? NSLOCTEXT(
					"IGHUD",
					"MainSubtitle",
					"존재하지 않는 층은 소리로 먼저 드러난다.")
				: FText::FromString(
					TEXT("A FLOOR THAT ISN'T THERE IS HEARD FIRST."))
			: bKorean
				? NSLOCTEXT(
					"IGHUD",
					"PauseSubtitle",
					"숨을 고르고, 기억을 이어 간다.")
				: FText::FromString(TEXT("CATCH YOUR BREATH. CONTINUE THE MEMORY.")),
		142.0f,
		IGHorrorHUD::MutedGray,
		EIGHudTextRole::Hint);
	if (bSystemMenuIsTitle && bSystemMenuHeadphoneRecommendation)
	{
		DrawCenteredText(
			bKorean
				? NSLOCTEXT(
					"IGHUD",
					"HeadphoneRecommendation",
					"이 게임은 헤드폰으로 듣도록 만들어졌다.  ·  아무 키로 건너뛰기")
				: FText::FromString(
					TEXT("THIS GAME IS MADE TO BE HEARD ON HEADPHONES.  ·  ANY KEY TO SKIP")),
			194.0f,
			IGHorrorHUD::ThoughtBlue,
			EIGHudTextRole::Hint);
	}
	else if (bSystemMenuIsTitle && bSystemMenuConfirmNewGame)
	{
		DrawCenteredText(
			bKorean
				? NSLOCTEXT(
					"IGHUD",
					"NewGameDeleteWarning",
					"기존 자동 저장이 삭제됩니다. 새 게임을 한 번 더 선택하세요.")
				: FText::FromString(
					TEXT("AUTOSAVES WILL BE DELETED. SELECT NEW GAME AGAIN.")),
			194.0f,
			IGHorrorHUD::RedAccent,
			EIGHudTextRole::Hint);
	}
	else if (!SystemMenuStatusText.IsEmpty())
	{
		DrawCenteredText(
			SystemMenuStatusText,
			194.0f,
			bSystemMenuStatusIsError
				? IGHorrorHUD::RedAccent
				: IGHorrorHUD::PaleGray,
			EIGHudTextRole::Hint);
	}

	const FString TitleRows[] =
	{
		bKorean ? TEXT("이어하기") : TEXT("CONTINUE"),
		bKorean ? TEXT("새 게임") : TEXT("NEW GAME"),
		bKorean ? TEXT("설정") : TEXT("SETTINGS"),
		bKorean ? TEXT("제작 정보") : TEXT("CREDITS"),
		bKorean ? TEXT("게임 종료") : TEXT("QUIT")
	};
	const FString PauseRows[] =
	{
		bKorean ? TEXT("계속하기") : TEXT("RESUME"),
		bKorean ? TEXT("최근 자동 저장 불러오기") : TEXT("LOAD LATEST AUTOSAVE"),
		bKorean ? TEXT("설정") : TEXT("SETTINGS"),
		bKorean ? TEXT("제작 정보") : TEXT("CREDITS"),
		bKorean ? TEXT("게임 종료") : TEXT("QUIT")
	};
	const float RowStartY = FMath::Max(244.0f, Canvas->ClipY * 0.36f);
	const float RowSpacing = FMath::Clamp(Canvas->ClipY * 0.062f, 36.0f, 46.0f);
	const int32 LoadRow = bSystemMenuIsTitle ? 0 : 1;
	for (int32 Row = 0; Row < UE_ARRAY_COUNT(TitleRows); ++Row)
	{
		const bool bEnabled = Row != LoadRow || bSystemMenuCanContinue;
		const bool bSelected = Row == SystemMenuSelectedRow;
		FString Label = bSystemMenuIsTitle ? TitleRows[Row] : PauseRows[Row];
		if (bSystemMenuIsTitle && Row == 1 && bSystemMenuConfirmNewGame)
		{
			Label = bKorean
				? TEXT("새 게임 확인")
				: TEXT("CONFIRM NEW GAME");
		}
		if (!bEnabled)
		{
			Label += bKorean ? TEXT("  (저장 없음)") : TEXT("  (NO SAVE)");
		}
		Label = FString(bSelected ? TEXT(">  ") : TEXT("   ")) + Label;
		DrawCenteredText(
			FText::FromString(Label),
			RowStartY + Row * RowSpacing,
			!bEnabled
				? FLinearColor(0.28f, 0.29f, 0.28f, 0.82f)
				: bSelected
					? IGHorrorHUD::RedAccent
					: IGHorrorHUD::PaleGray,
			bSelected && bEnabled
				? EIGHudTextRole::Prompt
				: EIGHudTextRole::Hint);
	}

	FText SystemControls;
	if (bKorean)
	{
		SystemControls = bUsingGamepad
			? bSystemMenuIsTitle
				? bSystemMenuConfirmNewGame
					? NSLOCTEXT(
						"IGHUD",
						"TitleConfirmControlsGamepad",
						"A 새 게임 시작  ·  View 취소")
					: NSLOCTEXT(
						"IGHUD",
						"TitleControlsGamepad",
						"D-pad 항목  ·  A 선택  ·  Menu 접근성")
				: NSLOCTEXT(
					"IGHUD",
					"SystemMenuControlsGamepad",
					"D-pad 항목  ·  A 선택  ·  View 돌아가기  ·  Menu 접근성")
			: bSystemMenuIsTitle
				? bSystemMenuConfirmNewGame
					? NSLOCTEXT(
						"IGHUD",
						"TitleConfirmControlsKeyboard",
						"Enter 새 게임 시작  ·  Esc 취소")
					: NSLOCTEXT(
						"IGHUD",
						"TitleControlsKeyboard",
						"W/S 또는 방향키 항목  ·  Enter/마우스 선택  ·  F10 접근성")
				: NSLOCTEXT(
					"IGHUD",
					"SystemMenuControlsKeyboard",
					"W/S 또는 방향키 항목  ·  Enter/마우스 선택  ·  Esc 돌아가기  ·  F10 접근성");
	}
	else
	{
		SystemControls = FText::FromString(
			bUsingGamepad
				? bSystemMenuIsTitle
					? bSystemMenuConfirmNewGame
						? TEXT("A START NEW GAME  |  VIEW CANCEL")
						: TEXT("D-PAD SELECT  |  A APPLY  |  MENU ACCESSIBILITY")
					: TEXT("D-PAD SELECT  |  A APPLY  |  VIEW BACK  |  MENU ACCESSIBILITY")
				: bSystemMenuIsTitle
					? bSystemMenuConfirmNewGame
						? TEXT("ENTER START NEW GAME  |  ESC CANCEL")
						: TEXT("W/S OR ARROWS SELECT  |  ENTER/MOUSE APPLY  |  F10 ACCESSIBILITY")
					: TEXT("W/S OR ARROWS SELECT  |  ENTER/MOUSE APPLY  |  ESC BACK  |  F10 ACCESSIBILITY"));
	}
	DrawCenteredText(
		SystemControls,
		FMath::Max(0.0f, Canvas->ClipY - 48.0f),
		IGHorrorHUD::MutedGray,
		EIGHudTextRole::Hint);
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
