#include "Player/IGHorrorHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "IndieGame.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interaction/IGReadableNote.h"
#include "Player/IGInteractionComponent.h"
#include "Sequence/IGMorningRoutineDirector.h"
#include "Sequence/IGObjectiveProvider.h"
#include "Sequence/IGWakeUpDirector.h"

namespace IGHorrorHUD
{
	constexpr double DirectorSearchInterval = 2.0;

	// Native pixel sizes per text role; glyphs are never scaled after rasterization.
	constexpr int32 LargeFontSize = 22;
	constexpr int32 MediumFontSize = 19;
	constexpr int32 SmallFontSize = 14;

	const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.9f);
	const FLinearColor PaleGray(0.82f, 0.84f, 0.82f, 0.95f);
	const FLinearColor MutedGray(0.62f, 0.64f, 0.62f, 0.9f);
	const FLinearColor RedAccent(0.72f, 0.08f, 0.06f, 1.0f);
	const FLinearColor ThoughtBlue(0.74f, 0.78f, 0.86f, 1.0f);
}

void AIGHorrorHUD::BeginPlay()
{
	Super::BeginPlay();
	InitializeKoreanFont();

	// Optional: absent until Scripts/Prepare-AIArt.ps1 has produced it, in
	// which case the reading panel falls back to a flat fill.
	NotePaperTexture = LoadObject<UTexture2D>(
		nullptr, TEXT("/Game/Prototype/Textures/T_PaperOld_V2_D.T_PaperOld_V2_D"));
	ReceiptPaperTexture = LoadObject<UTexture2D>(
		nullptr, TEXT("/Game/Prototype/Textures/T_PaperClean_V2_D.T_PaperClean_V2_D"));

	ResolveInteractionComponent();
	ResolveDirectors();

	if (const UWorld* World = GetWorld())
	{
		NextDirectorSearchTime = World->GetTimeSeconds() + IGHorrorHUD::DirectorSearchInterval;
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
			return KoreanFontMedium.Get();
		case EIGHudTextRole::Hint:
		default:
			return KoreanFontSmall.Get();
		}
	}

	if (!GEngine)
	{
		return nullptr;
	}
	return TextRole == EIGHudTextRole::Hint ? GEngine->GetSmallFont() : GEngine->GetMediumFont();
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
		HorrorHUD->ShowThought(Thought, DurationSeconds);
	}
}

void AIGHorrorHUD::ShowThought(const FText& Thought, const float DurationSeconds)
{
	const UWorld* World = GetWorld();
	if (!World || Thought.IsEmpty())
	{
		return;
	}

	CurrentThought = Thought;
	ThoughtStartTime = World->GetTimeSeconds();
	ThoughtEndTime = ThoughtStartTime + FMath::Max(1.0f, DurationSeconds);
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
	if (DrawChapterCard(CurrentTime))
	{
		LastHudDrawTime = CurrentTime;
		return;
	}

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
	DrawCrosshair(bHasFocus ? IGHorrorHUD::RedAccent : IGHorrorHUD::PaleGray);
	if (FocusBracketAlpha > 0.01f)
	{
		DrawFocusBracket(
			IGHorrorHUD::RedAccent,
			Interaction && Interaction->IsInteracting() ? Interaction->GetHoldProgress() : 0.0f);
	}

	// Objective line.
	if (SupportsKorean())
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
		DrawNotePanel();
		return;
	}

	// Focused interaction prompt and hold progress.
	if (bHasFocus)
	{
		const FText FocusedPrompt = Interaction->GetFocusedPrompt();
		if (!FocusedPrompt.IsEmpty())
		{
			const FText Prompt = FText::Format(
				NSLOCTEXT("IGHUD", "PromptFormat", "[ E ]  {0}"),
				FocusedPrompt);
			DrawCenteredText(
				Prompt,
				(Canvas->ClipY * 0.5f) + 54.0f,
				IGHorrorHUD::RedAccent,
				EIGHudTextRole::Prompt);
		}
	}

	if (Interaction && Interaction->IsInteracting())
	{
		const float HoldProgress = Interaction->GetHoldProgress();
		if (HoldProgress > 0.0f && HoldProgress < 1.0f)
		{
			DrawHoldProgress(HoldProgress);
		}
	}

	// Inner-voice line with fade in/out.
	if (!CurrentThought.IsEmpty() && CurrentTime < ThoughtEndTime)
	{
		const double Elapsed = CurrentTime - ThoughtStartTime;
		const double Remaining = ThoughtEndTime - CurrentTime;
		const float Alpha = FMath::Clamp(
			FMath::Min(static_cast<float>(Elapsed / 0.25), static_cast<float>(Remaining / 0.6)),
			0.0f,
			1.0f);
		FLinearColor ThoughtColor = IGHorrorHUD::ThoughtBlue;
		ThoughtColor.A = Alpha * 0.95f;
		DrawCenteredText(
			CurrentThought, Canvas->ClipY * 0.66f, ThoughtColor, EIGHudTextRole::Thought);
	}

	// Control hints.
	const FText Hints = SupportsKorean()
		? NSLOCTEXT("IGHUD", "Hints", "WASD 이동  ·  마우스 시점  ·  E 상호작용  ·  Esc 커서")
		: FText::FromString(TEXT("WASD Move  |  Mouse Look  |  E Interact  |  Esc Release Cursor"));
	DrawCenteredText(
		Hints,
		FMath::Max(0.0f, Canvas->ClipY - 34.0f),
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
	if (!Wake)
	{
		return FText::GetEmpty();
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
		const IIGObjectiveProvider* Provider =
			Cast<IIGObjectiveProvider>(ObjectiveProvider.Get());
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
	if (!Wake)
	{
		return FString();
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
		const IIGObjectiveProvider* Provider =
			Cast<IIGObjectiveProvider>(ObjectiveProvider.Get());
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
	const EIGHudTextRole TextRole)
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

void AIGHorrorHUD::UpdateFocusBracket(const AActor* FocusedActor, const float DeltaSeconds)
{
	if (!Canvas)
	{
		return;
	}

	if (!IsValid(FocusedActor))
	{
		FocusBracketAlpha = FMath::FInterpTo(FocusBracketAlpha, 0.0f, DeltaSeconds, 12.0f);
		return;
	}

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
	FocusBracketAlpha = FMath::FInterpTo(FocusBracketAlpha, 1.0f, DeltaSeconds, 14.0f);
}

void AIGHorrorHUD::DrawFocusBracket(const FLinearColor& Color, const float Progress)
{
	if (!Canvas)
	{
		return;
	}

	FLinearColor BracketColor = Color;
	BracketColor.A *= FMath::Clamp(FocusBracketAlpha, 0.0f, 1.0f);
	const float CornerX = FMath::Min(18.0f, (FocusBracketMax.X - FocusBracketMin.X) * 0.34f);
	const float CornerY = FMath::Min(18.0f, (FocusBracketMax.Y - FocusBracketMin.Y) * 0.34f);
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

	DrawCorner(FocusBracketMin, 1.0f, 1.0f, CornerX, CornerY);
	DrawCorner(FVector2D(FocusBracketMax.X, FocusBracketMin.Y), -1.0f, 1.0f, CornerX, CornerY);
	DrawCorner(FVector2D(FocusBracketMin.X, FocusBracketMax.Y), 1.0f, -1.0f, CornerX, CornerY);
	DrawCorner(FocusBracketMax, -1.0f, -1.0f, CornerX, CornerY);

	// Hold interactions fill the bracket's bottom edge as they charge.
	if (Progress > 0.0f)
	{
		const float Width = (FocusBracketMax.X - FocusBracketMin.X) * FMath::Clamp(Progress, 0.0f, 1.0f);
		DrawRect(
			BracketColor,
			FocusBracketMin.X,
			FocusBracketMax.Y - Thickness,
			Width,
			Thickness);
	}
}

void AIGHorrorHUD::DrawNotePanel()
{
	const AIGReadableNote* Note = AIGReadableNote::GetOpenNote();
	if (!Note || !Canvas)
	{
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
			? NSLOCTEXT("IGHUD", "NoteClose", "[ E ]  덮기")
			: FText::FromString(TEXT("[ E ]  Close"));
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

	const float PaperHeight = FMath::Min(ScreenHeight * 0.88f, 760.0f);
	const float PaperWidth = FMath::Min(PaperHeight * 0.47f, ScreenWidth * 0.34f);
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
			FLinearColor(1.0f, 1.0f, 0.97f, 1.0f), BLEND_Opaque);
	}
	else
	{
		DrawRect(
			FLinearColor(0.94f, 0.94f, 0.90f, 1.0f),
			PaperOrigin.X, PaperOrigin.Y, PaperWidth, PaperHeight);
	}

	// Very faint horizontal thermal banding keeps the surface from reading as
	// a flat UI card without turning clean receipt stock into aged stationery.
	for (float BandY = PaperOrigin.Y + 12.0f;
		BandY < PaperOrigin.Y + PaperHeight - 8.0f;
		BandY += 19.0f)
	{
		DrawRect(
			FLinearColor(0.28f, 0.27f, 0.24f, 0.035f),
			PaperOrigin.X + 1.0f,
			BandY,
			PaperWidth - 2.0f,
			1.0f);
	}
	DrawRect(
		FLinearColor(0.27f, 0.26f, 0.23f, 0.45f),
		PaperOrigin.X,
		PaperOrigin.Y,
		1.0f,
		PaperHeight);
	DrawRect(
		FLinearColor(0.27f, 0.26f, 0.23f, 0.45f),
		PaperOrigin.X + PaperWidth - 1.0f,
		PaperOrigin.Y,
		1.0f,
		PaperHeight);

	UFont* HeaderFont = GetFontForRole(EIGHudTextRole::Prompt);
	UFont* ReceiptFont = GetFontForRole(EIGHudTextRole::Hint);
	if (!HeaderFont || !ReceiptFont)
	{
		return;
	}

	const FLinearColor ThermalInk(0.055f, 0.052f, 0.048f, 0.96f);
	const FLinearColor FaintInk(0.12f, 0.115f, 0.105f, 0.88f);
	const float ContentLeft = PaperOrigin.X + PaperWidth * 0.075f;
	const float ContentRight = PaperOrigin.X + PaperWidth * 0.925f;
	const float ContentWidth = ContentRight - ContentLeft;
	// Thermal printers pack rows much more tightly than normal UI text. Font
	// ascent metrics are intentionally not used here: Malgun Gothic reports a
	// generous line box that made the first pass look like a document again.
	const float ReceiptLineHeight =
		FMath::Clamp(PaperHeight / 31.0f, 18.0f, 21.0f);
	float PenY = PaperOrigin.Y + PaperHeight * 0.038f;

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
	PenY += FMath::Max(22.0f, HeaderFont->GetMaxCharHeight() * 1.18f);
	DrawCentered(Receipt.StoreSubtitle, ReceiptFont, PenY, &FaintInk);
	PenY += ReceiptLineHeight + 2.0f;
	DrawRule(true);

	for (const FText& DetailLine : Receipt.StoreDetailLines)
	{
		DrawTextAt(DetailLine, ReceiptFont, ContentLeft, PenY);
		PenY += ReceiptLineHeight;
	}
	if (!Receipt.StoreDetailLines.IsEmpty())
	{
		PenY += 2.0f;
	}

	for (const FText& PolicyLine : Receipt.PolicyLines)
	{
		DrawTextAt(PolicyLine, ReceiptFont, ContentLeft, PenY, &FaintInk);
		PenY += ReceiptLineHeight;
	}
	PenY += 2.0f;
	DrawRule();

	DrawTextAt(Receipt.TransactionDateTime, ReceiptFont, ContentLeft, PenY);
	DrawRight(Receipt.PosLabel, ReceiptFont, ContentRight, PenY);
	PenY += ReceiptLineHeight;
	DrawTextAt(
		NSLOCTEXT("IGHUD", "ReceiptNumberLabel", "영수증번호"),
		ReceiptFont,
		ContentLeft,
		PenY,
		&FaintInk);
	DrawRight(Receipt.ReceiptNumber, ReceiptFont, ContentRight, PenY);
	PenY += ReceiptLineHeight + 2.0f;
	DrawRule();

	const float QuantityCenterX = ContentLeft + ContentWidth * 0.62f;
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
		DrawRight(FormatAmount(Item.Amount), ReceiptFont, ContentRight, PenY);
		PenY += ReceiptLineHeight + 2.0f;
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
			static_cast<float>(Font->GetMaxCharHeight()) * 1.08f);
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
	PenY += 1.0f;
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
	PenY += 6.0f;
	DrawRule();

	for (const FText& FooterLine : Receipt.FooterLines)
	{
		DrawCentered(FooterLine, ReceiptFont, PenY, &FaintInk);
		PenY += ReceiptLineHeight;
	}

	// A deterministic, deliberately non-standard visual barcode. It reads as
	// POS output but is not a scannable copy of a real receipt.
	const float BarcodeTop = PenY + 2.0f;
	const float BarcodeHeight = FMath::Max(
		24.0f,
		FMath::Min(36.0f, PaperOrigin.Y + PaperHeight - BarcodeTop - 29.0f));
	const int32 PatternUnits = FMath::Max(1, Receipt.BarcodeDigits.Len() * 8);
	const float UnitWidth = FMath::Min(1.8f, ContentWidth / PatternUnits);
	float BarcodeX = ContentLeft + (ContentWidth - PatternUnits * UnitWidth) * 0.5f;
	for (int32 DigitIndex = 0; DigitIndex < Receipt.BarcodeDigits.Len(); ++DigitIndex)
	{
		const int32 Digit = FMath::Clamp(
			Receipt.BarcodeDigits[DigitIndex] - TEXT('0'),
			0,
			9);
		const uint8 Pattern = static_cast<uint8>(
			((Digit + 1) * 37 + (DigitIndex + 3) * 19) & 0x7f);
		for (int32 Bit = 0; Bit < 7; ++Bit)
		{
			if ((Pattern & (1 << Bit)) != 0)
			{
				const float BarHeight = (Bit == 0 || Bit == 6)
					? BarcodeHeight
					: BarcodeHeight - 3.0f;
				DrawRect(
					ThermalInk,
					BarcodeX,
					BarcodeTop,
					FMath::Max(1.0f, UnitWidth * 0.72f),
					BarHeight);
			}
			BarcodeX += UnitWidth;
		}
		BarcodeX += UnitWidth;
	}
	PenY = BarcodeTop + BarcodeHeight + 1.0f;
	DrawCentered(
		FText::FromString(Receipt.BarcodeDigits),
		ReceiptFont,
		PenY,
		&FaintInk);

	const FText Hint = SupportsKorean()
		? NSLOCTEXT("IGHUD", "ReceiptClose", "[ E ]  영수증 내려놓기")
		: FText::FromString(TEXT("[ E ]  Put receipt down"));
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

void AIGHorrorHUD::DrawHoldProgress(const float Progress)
{
	if (!Canvas)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float BarY = Canvas->ClipY * 0.5f + 30.0f;
	constexpr float BarWidth = 64.0f;
	constexpr float BarHeight = 4.0f;

	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.65f),
		CenterX - BarWidth * 0.5f - 1.0f,
		BarY - 1.0f,
		BarWidth + 2.0f,
		BarHeight + 2.0f);
	DrawRect(
		IGHorrorHUD::RedAccent,
		CenterX - BarWidth * 0.5f,
		BarY,
		BarWidth * FMath::Clamp(Progress, 0.0f, 1.0f),
		BarHeight);
}
