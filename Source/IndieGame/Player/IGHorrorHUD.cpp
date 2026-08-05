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
#include "Fonts/CompositeFont.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "IndieGame.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interaction/IGReadableNote.h"
#include "Player/IGInteractionComponent.h"
#include "Player/IGPlayerController.h"
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
	constexpr int32 PhoneMetaFontSize = 15;

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
	if (const AIGPlayerController* IndieController =
		Cast<AIGPlayerController>(GetOwningPlayerController()))
	{
		IndieController->RefreshMenuHud();
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
	if (!Accessibility || !Accessibility->AreSubtitlesEnabled())
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
	CurrentAudioCaption = Caption;
	AudioCaptionStartTime = World->GetTimeSeconds();
	AudioCaptionEndTime =
		AudioCaptionStartTime + FMath::Max(0.8f, DurationSeconds);
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
	AccessibilitySelectedRow = FMath::Clamp(SelectedRow, 0, 11);
}

void AIGHorrorHUD::SetSystemMenuState(
	const FIGSystemMenuPresentation& Presentation)
{
	bSystemMenuVisible = Presentation.bVisible;
	bSystemMenuIsTitle = Presentation.bTitle;
	bSystemMenuIsCredits = Presentation.bCredits;
	bSystemMenuIsDisplaySettings = Presentation.bDisplaySettings;
	SystemMenuSelectedRow = FMath::Clamp(Presentation.SelectedRow, 0, 4);
	bSystemMenuCanContinue = Presentation.bCanContinue;
	bSystemMenuConfirmNewGame = Presentation.bConfirmNewGame;
	DisplaySettingsSelectedRow = FMath::Clamp(
		Presentation.DisplaySelectedRow,
		0,
		7);
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
	if (bAccessibilityMenuVisible)
	{
		DrawAccessibilityPanel();
		return;
	}
	if (bSystemMenuVisible)
	{
		DrawSystemMenuPanel();
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
	DrawFearDirection(CurrentTime);

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
				bUsingGamepad
					? NSLOCTEXT("IGHUD", "PromptFormatGamepad", "[ A ]  {0}")
					: NSLOCTEXT("IGHUD", "PromptFormatKeyboard", "[ E ]  {0}"),
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
	DrawAudioCaption(CurrentTime);

	// Control hints.
	const FText Hints = SupportsKorean()
		? bUsingGamepad
			? NSLOCTEXT(
				"IGHUD",
				"HintsGamepad",
				"LS 이동  ·  RS 시점  ·  A 상호작용  ·  RB 힌트  ·  Menu 접근성")
			: NSLOCTEXT(
				"IGHUD",
				"HintsKeyboard",
				"WASD 이동  ·  마우스 시점  ·  E 상호작용  ·  H 힌트  ·  F10 접근성")
		: FText::FromString(
			bUsingGamepad
				? TEXT("LS MOVE  |  RS LOOK  |  A INTERACT  |  RB HINT  |  MENU ACCESSIBILITY")
				: TEXT("WASD MOVE  |  MOUSE LOOK  |  E INTERACT  |  H HINT  |  F10 ACCESSIBILITY"));
	DrawCenteredText(
		Hints,
		FMath::Max(0.0f, Canvas->ClipY - 34.0f),
		IGHorrorHUD::MutedGray,
		EIGHudTextRole::Hint);
}

void AIGHorrorHUD::DrawAudioCaption(const double CurrentTime)
{
	if (!Canvas
		|| CurrentAudioCaption.IsEmpty()
		|| CurrentTime >= AudioCaptionEndTime)
	{
		return;
	}
	const UGameInstance* GameInstance = GetWorld()
		? GetWorld()->GetGameInstance()
		: nullptr;
	const UIGAccessibilitySubsystem* Accessibility = GameInstance
		? GameInstance->GetSubsystem<UIGAccessibilitySubsystem>()
		: nullptr;
	if (!Accessibility || !Accessibility->AreSubtitlesEnabled())
	{
		return;
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
	const float CaptionScale = Settings.CaptionSizeScale;
	const float SafeAreaScale = Settings.CaptionSafeAreaScale;
	const float SafeWidth = Canvas->ClipX * SafeAreaScale;
	const float PanelWidth = FMath::Min(
		FMath::Clamp(Canvas->ClipX * 0.62f, 320.0f, 760.0f),
		FMath::Max(260.0f, SafeWidth - 32.0f));
	const float MaximumTextWidth = FMath::Max(220.0f, PanelWidth - 38.0f);
	FString FirstLine;
	FString SecondLine;
	WrapAudioCaption(
		CurrentAudioCaption.ToString(),
		GetFontForRole(EIGHudTextRole::Hint),
		CaptionScale,
		MaximumTextWidth,
		FirstLine,
		SecondLine);
	const bool bTwoLines = !SecondLine.IsEmpty();
	const float PanelHeight = (bTwoLines ? 58.0f : 38.0f) * CaptionScale;
	const float SafeVerticalInset = Canvas->ClipY * (1.0f - SafeAreaScale) * 0.5f;
	const float PanelY = FMath::Clamp(
		Canvas->ClipY * 0.76f,
		SafeVerticalInset + 28.0f,
		Canvas->ClipY - SafeVerticalInset - PanelHeight - 22.0f);
	FCanvasTileItem Backdrop(
		FVector2D((Canvas->ClipX - PanelWidth) * 0.5f, PanelY),
		FVector2D(PanelWidth, PanelHeight),
		FLinearColor(0.015f, 0.018f, 0.017f, 0.82f * Alpha));
	Backdrop.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Backdrop);
	FLinearColor CaptionColor = IGHorrorHUD::PaleGray;
	CaptionColor.A = Alpha;
	DrawCenteredText(
		FText::FromString(FirstLine),
		PanelY + 8.0f * CaptionScale,
		CaptionColor,
		EIGHudTextRole::Hint,
		CaptionScale);
	if (bTwoLines)
	{
		DrawCenteredText(
			FText::FromString(SecondLine),
			PanelY + 29.0f * CaptionScale,
			CaptionColor,
			EIGHudTextRole::Hint,
			CaptionScale);
	}
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

void AIGHorrorHUD::WrapAudioCaption(
	const FString& Caption,
	UFont* Font,
	const float TextScale,
	const float MaximumWidth,
	FString& OutFirstLine,
	FString& OutSecondLine) const
{
	OutFirstLine = Caption.TrimStartAndEnd();
	OutSecondLine.Reset();
	if (OutFirstLine.IsEmpty()
		|| MeasureTextWidth(OutFirstLine, Font, TextScale) <= MaximumWidth)
	{
		return;
	}

	int32 BreakIndex = FindFittingCaptionPrefix(
		OutFirstLine,
		Font,
		TextScale,
		MaximumWidth);
	const int32 SpaceIndex = OutFirstLine.Left(BreakIndex).Find(
		TEXT(" "),
		ESearchCase::CaseSensitive,
		ESearchDir::FromEnd);
	if (SpaceIndex >= BreakIndex / 2)
	{
		BreakIndex = SpaceIndex;
	}

	OutSecondLine = OutFirstLine.Mid(BreakIndex).TrimStartAndEnd();
	OutFirstLine = OutFirstLine.Left(BreakIndex).TrimEnd();
	if (OutSecondLine.IsEmpty()
		|| MeasureTextWidth(OutSecondLine, Font, TextScale) <= MaximumWidth)
	{
		return;
	}

	const FString Ellipsis = TEXT("…");
	const float EllipsisWidth = MeasureTextWidth(Ellipsis, Font, TextScale);
	const int32 VisibleLength = FindFittingCaptionPrefix(
		OutSecondLine,
		Font,
		TextScale,
		FMath::Max(1.0f, MaximumWidth - EllipsisWidth));
	OutSecondLine = OutSecondLine.Left(VisibleLength).TrimEnd() + Ellipsis;
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
		bKorean ? TEXT("핵심 소리 자막") : TEXT("SOUND CAPTIONS"),
		bKorean ? TEXT("소리 자막 크기") : TEXT("CAPTION SIZE"),
		bKorean ? TEXT("자막 안전 영역") : TEXT("CAPTION SAFE AREA"),
		bKorean ? TEXT("길게 누르기 방식") : TEXT("HOLD INPUT"),
		bKorean ? TEXT("홀드 길이") : TEXT("HOLD DURATION"),
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
		FString::Printf(
			TEXT("%d%%"),
			FMath::RoundToInt(Settings.CaptionSizeScale * 100.0f)),
		FString::Printf(
			TEXT("%d%%"),
			FMath::RoundToInt(Settings.CaptionSafeAreaScale * 100.0f)),
		bKorean
			? (Settings.bToggleHoldInteractions ? TEXT("토글") : TEXT("누르는 동안"))
			: (Settings.bToggleHoldInteractions ? TEXT("TOGGLE") : TEXT("HOLD")),
		FString::Printf(
			TEXT("%d%%"),
			FMath::RoundToInt(Settings.HoldDurationScale * 100.0f)),
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

	const float RowStartY = FMath::Max(116.0f, Canvas->ClipY * 0.18f);
	const float RowSpacing = FMath::Clamp(Canvas->ClipY * 0.047f, 28.0f, 38.0f);
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
		FMath::Max(RowStartY + 12.5f * RowSpacing, Canvas->ClipY - 48.0f),
		IGHorrorHUD::MutedGray,
		EIGHudTextRole::Hint);
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
				? NSLOCTEXT("IGHUD", "CreditsGameTitle", "4시 44분")
				: FText::FromString(TEXT("4:44 AM")),
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
				? NSLOCTEXT("IGHUD", "MainTitle", "4시 44분")
				: FText::FromString(TEXT("4:44 AM"))
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
					"다음 날 새벽, 냉장고에는 또 물이 없다.")
				: FText::FromString(
					TEXT("THE NEXT MORNING, THE FRIDGE IS EMPTY AGAIN."))
			: bKorean
				? NSLOCTEXT(
					"IGHUD",
					"PauseSubtitle",
					"숨을 고르고, 기억을 이어 간다.")
				: FText::FromString(TEXT("CATCH YOUR BREATH. CONTINUE THE MEMORY.")),
		142.0f,
		IGHorrorHUD::MutedGray,
		EIGHudTextRole::Hint);
	if (bSystemMenuIsTitle && bSystemMenuConfirmNewGame)
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
