#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Player/IGSettingsMenuLayout.h"
#include "IGHorrorHUD.generated.h"

class AIGMorningRoutineDirector;
class AIGReadableNote;
class AIGWakeUpDirector;
class IIGObjectiveProvider;
class UFont;
class UTexture2D;
class UFontFace;
class UIGInteractionComponent;

/** HUD text roles; each maps to a font rasterized at its native pixel size. */
enum class EIGHudTextRole : uint8
{
	Objective,
	Prompt,
	Thought,
	Dialogue,
	Speaker,
	Hint
};

/** Visual and accessibility contract for a line presented in the lower HUD. */
enum class EIGDialogueChannel : uint8
{
	/** Ji-woon's unvoiced inner monologue. This is story text, not a subtitle. */
	InnerVoice,
	/** Text-first exchange whose copy is the primary delivery channel. */
	Conversation,
	/** Subtitle paired with recorded speech; follows the subtitle toggle. */
	VoiceSubtitle,
	/** Diegetic machine or phone response. */
	Device
};

/**
 * §9 에필로그의 한 장면. 디렉터가 시각표를 들고, HUD는 지금 어느 장면인지만
 * 받아 그린다. 화면은 카메라 페이드가 이미 검게 만들어 두었으므로 여기서
 * 하는 일은 그 검정 위에 무엇을 얹느냐가 전부다.
 */
enum class EIGMissingFloorEpilogueScene : uint8
{
	None,
	/** 소리만 지나간다. 글자도 그림도 없다. */
	Montage,
	/** 에필로그 1 — 도하의 공방. 화면은 손과 현만(엔딩 A). */
	Workshop,
	/** 에필로그 2 — 가을의 달빛빌라(엔딩 A). */
	Autumn,
	/** 마지막 신 — 비어 있는 서비스 베이(엔딩 B). */
	ServiceBay,
	/** 두 엔딩 공통. 목격한 만큼 문장이 선명해진다(§22.3). */
	News,
	/** 마지막 카드 한 줄. */
	Card
};

/** Higher-priority lines may briefly interrupt and then resume a lower one. */
enum class EIGDialoguePriority : uint8
{
	Ambient,
	Story,
	Critical
};

/** Small value object kept outside UObject reflection to avoid per-line allocation churn. */
struct FIGDialogueMessage
{
	FText Speaker;
	FText Line;
	EIGDialogueChannel Channel = EIGDialogueChannel::InnerVoice;
	EIGDialoguePriority Priority = EIGDialoguePriority::Story;
	float MinimumDurationSeconds = 0.0f;
	double QueuedAt = 0.0;
	bool bContinuation = false;
};

/** Queued non-dialogue audio description; authored timings remain the source of truth. */
struct FIGAudioCaptionMessage
{
	FText Caption;
	float DurationSeconds = 0.0f;
	double QueuedAt = 0.0;
};

/** Snapshot of controller-owned front-end state consumed by the native HUD. */
struct FIGSystemMenuPresentation
{
	bool bVisible = false;
	bool bTitle = false;
	/** Title key art is also retained when credits were opened from the title. */
	bool bUseTitleBackdrop = false;
	bool bCredits = false;
	/** 첫 실행 콘텐츠 고지. 제작 정보와 같은 전체 화면 계층을 쓴다. */
	bool bContentNotice = false;
	bool bAudioCalibration = false;
	bool bDisplaySettings = false;
	bool bCanContinue = false;
	/** §9 「밤 5」: 엔딩 B를 본 세이브가 있을 때만 그 행이 존재한다. */
	bool bNightFiveAvailable = false;
	/** 한 번 재생하면 흐려진다. 다시 들을 수는 있다. */
	bool bNightFiveSpent = false;
	/** 검정 화면 30초. 재생 중에는 메뉴를 그리지 않는다. */
	bool bNightFivePlaying = false;
	bool bConfirmNewGame = false;
	bool bHeadphoneRecommendation = false;
	bool bVSync = true;
	bool bDisplaySettingsApplied = false;
	bool bDisplaySettingsAwaitingConfirmation = false;
	bool bStatusIsError = false;
	int32 SelectedRow = 0;
	int32 DisplaySelectedRow = 0;
	int32 AudioCalibrationSelectedRow = 0;
	int32 AudioCalibrationVolumeStep = 6;
	int32 AudioCalibrationBrightnessStep = 2;
	bool bAudioCalibrationFirstRun = false;
	int32 WindowModeIndex = 0;
	int32 ResolutionIndex = 1;
	int32 QualityIndex = 1;
	int32 FrameLimitIndex = 1;
	int32 ConfirmationSecondsRemaining = 0;
	FText StatusText;
};

/**
 * Lightweight native HUD for the prologue.
 * It intentionally avoids widget assets so the first playable build always has
 * interaction feedback, objectives, inner-voice lines and control hints.
 *
 * Korean text renders through bundled runtime composite fonts so typography
 * and glyph metrics stay identical in Editor and packaged builds. A system
 * font remains a development-only fallback for partial source checkouts.
 */
UCLASS()
class INDIEGAME_API AIGHorrorHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Pushes a short inner-voice line onto the local player's HUD. */
	static void PushThought(
		const UObject* WorldContext,
		const FText& Thought,
		float DurationSeconds = 3.5f);

	/**
	 * Presents a speaker-aware line without taking movement or camera control.
	 * Text-first channels remain visible because hiding them would remove story;
	 * only VoiceSubtitle follows the player's subtitle toggle.
	 */
	static void PushDialogue(
		const UObject* WorldContext,
		const FText& Speaker,
		const FText& Line,
		EIGDialogueChannel Channel = EIGDialogueChannel::Conversation,
		float MinimumDurationSeconds = 0.0f,
		EIGDialoguePriority Priority = EIGDialoguePriority::Story);

	/** Shows a non-dialogue sound caption when the accessibility option is on. */
	static void PushAudioCaption(
		const UObject* WorldContext,
		const FText& Caption,
		float DurationSeconds = 2.0f);

	/** Shows a short grayscale edge wave pointing toward an authored fear cue. */
	static void PushFearDirection(
		const UObject* WorldContext,
		const FVector& WorldLocation,
		float DurationSeconds = 1.1f);

	/** Shows the single authored CH03 lens droplet without requiring a cooked UI asset. */
	static void PushLensDroplet(
		const UObject* WorldContext,
		float DurationSeconds = 3.0f);

	/** Starts the authored first-person contact/recoil sequence for a valid knock. */
	void PlayFirstPersonKnock();

	/**
	 * 포획 암전 동안 위층 사람의 절제된 1인칭 포옹을 재생한다.
	 * ImageGen 파생 4프레임은 화면 연출만 맡고 실제 포획 판정은
	 * 밤 루프 디렉터가 계속 소유한다.
	 */
	void PlayCaptureEmbrace(float DurationSeconds = 1.2f);

	/**
	 * 침대에서 시야가 돌아올 때 포옹의 마지막 자세를 짧은 잔상으로 남긴다.
	 * 잔상은 VisualDurationSeconds에 사라지지만, 입력이 돌아오는
	 * OwnershipDurationSeconds까지 일반 HUD가 먼저 나타나지 않게 프레임을 점유한다.
	 */
	void PlayCaptureWakeEcho(
		int32 CaptureCount,
		float VisualDurationSeconds = 0.68f,
		float OwnershipDurationSeconds = 0.68f);

	/**
	 * Returns the most recent frame that actually drew the CH03 lens droplet.
	 * The Shipping visual probe uses this instead of trusting a screenshot
	 * request alone, so reduced-motion and HUD-safe placement remain measurable.
	 */
	bool GetLensDropletRenderSample(
		FVector2D& OutPosition,
		FVector2D& OutSize,
		FVector2D& OutCanvasSize,
		float& OutAlpha,
		bool& bOutReducedMotion,
		double& OutWorldTime) const;

	/**
	 * Returns text bounds from the most recently completed HUD frame. The
	 * packaged frontend probe uses this to prove that native menu copy stayed
	 * inside the real Shipping canvas at every supported resolution.
	 */
	bool GetLayoutValidationSample(
		FVector2D& OutCanvasSize,
		FVector2D& OutBoundsMin,
		FVector2D& OutBoundsMax,
		int32& OutElementCount,
		bool& bOutAllInsideCanvas,
		bool& bOutAllInsideSettingsContainers,
		uint64& OutFrameSerial) const;

	/** Last lower-third dialogue layout actually drawn by the Shipping probe. */
	bool GetDialogueRenderSample(
		FVector2D& OutPanelMinimum,
		FVector2D& OutPanelMaximum,
		FVector2D& OutCanvasSize,
		int32& OutLineCount,
		bool& bOutSpeakerVisible,
		bool& bOutHasContinuation,
		bool& bOutInsideSafeArea,
		uint64& OutFrameSerial) const;

	/**
	 * Shows a reusable story-transition card over a fading black scrim.
	 * All copy is supplied by the caller so the HUD remains chapter-agnostic.
	 */
	static void ShowChapterCard(
		const UObject* WorldContext,
		const FText& Eyebrow,
		const FText& Title,
		const FText& Subtitle,
		float DurationSeconds = 4.2f);

	void ShowThought(const FText& Thought, float DurationSeconds);
	void ShowDialogue(
		const FText& Speaker,
		const FText& Line,
		EIGDialogueChannel Channel,
		float MinimumDurationSeconds,
		EIGDialoguePriority Priority);
	void ShowAudioCaption(const FText& Caption, float DurationSeconds);
	void ShowFearDirection(const FVector& WorldLocation, float DurationSeconds);
	void ShowLensDroplet(float DurationSeconds);
	void PresentChapterCard(
		const FText& Eyebrow,
		const FText& Title,
		const FText& Subtitle,
		float DurationSeconds);

	/**
	 * Binds the objective source used after the wake-up sequence reaches free
	 * roam. The object must implement IIGObjectiveProvider.
	 */
	void SetObjectiveProvider(UObject* InObjectiveProvider);

	/** Backward-compatible CH01 binding; delegates to SetObjectiveProvider. */
	void SetMorningDirector(AIGMorningRoutineDirector* InMorningDirector);

	/** Progress reported by the currently bound story objective provider. */
	UFUNCTION(BlueprintPure, Category = "HUD")
	float GetObjectiveProgress() const;

	UFUNCTION(BlueprintPure, Category = "HUD")
	bool SupportsKoreanText() const { return KoreanFontMedium != nullptr; }

	/**
	 * 없는 층 night austerity (§11 V4): during the hour the objective line is
	 * not shown at all. Pushed by the night director; off leaves every legacy
	 * chapter's HUD byte-identical.
	 */
	void SetNightPresentation(bool bInNightPresentation)
	{
		bNightPresentation = bInNightPresentation;
	}

	UFUNCTION(BlueprintPure, Category = "HUD")
	bool IsNightPresentation() const { return bNightPresentation; }

	/**
	 * The fifth-dawn interlude owns a black frame. Ordinary crosshair, focus,
	 * objective and dialogue stay out; directional accessibility captions may
	 * still draw because sound is the only image in that scene.
	 */
	void SetSensoryInterludePresentation(bool bEnabled)
	{
		bSensoryInterludePresentation = bEnabled;
	}
	void SetSensoryInterludeSkipState(
		bool bAvailable,
		float Progress,
		bool bInProgress,
		float RequiredHoldSeconds,
		bool bToggleMode)
	{
		bSensoryInterludeSkipAvailable = bAvailable;
		SensoryInterludeSkipProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
		bSensoryInterludeSkipInProgress = bInProgress;
		SensoryInterludeSkipHoldSeconds = FMath::Max(RequiredHoldSeconds, 0.25f);
		bSensoryInterludeSkipToggleMode = bToggleMode;
	}
	/** 서사가 소유하는 실패 연출이며 입력 처리는 밤 디렉터에 남긴다. */
	void BeginMissingFloorFailureEnding(float InitialElapsedSeconds = 0.0f);
	void SetMissingFloorFailureRetryEnabled(bool bEnabled)
	{
		bMissingFloorFailureRetryEnabled = bEnabled;
	}
	void EndMissingFloorFailureEnding();
	bool IsMissingFloorFailureEndingVisible() const
	{
		return bMissingFloorFailureEndingVisible;
	}

	/**
	 * §9 에필로그의 장면 하나를 건다. 디렉터가 장면이 바뀔 때마다 부르고,
	 * 흐른 시간은 HUD가 직접 잰다 — 실패 엔딩과 같은 규칙이라 프레임마다
	 * 상태를 밀어 넣는 경로가 하나도 늘지 않는다.
	 */
	void BeginMissingFloorEpilogueScene(
		EIGMissingFloorEpilogueScene Scene,
		const FText& Heading,
		const TArray<FText>& BodyLines,
		const FText& Footnote);
	void EndMissingFloorEpilogue();
	bool IsMissingFloorEpilogueVisible() const
	{
		return MissingFloorEpilogueScene != EIGMissingFloorEpilogueScene::None;
	}
	EIGMissingFloorEpilogueScene GetMissingFloorEpilogueScene() const
	{
		return MissingFloorEpilogueScene;
	}
	/** 계약 스크립트가 지금 화면의 문장을 그대로 읽는다. */
	const TArray<FText>& GetMissingFloorEpilogueLinesForTesting() const
	{
		return MissingFloorEpilogueBodyLines;
	}

	/** Native, asset-independent accessibility panel driven by the controller. */
	void SetAccessibilityMenuState(bool bVisible, int32 SelectedRow);
	/** Native title, pause and credits presentation shared by packaged builds. */
	void SetSystemMenuState(const FIGSystemMenuPresentation& Presentation);
	/**
	 * Daylight-only evidence journal for 없는 층. The controller owns pause and
	 * input; the HUD only renders the requested page from the saved provenance.
	 */
	void SetMissingFloorJournalState(bool bVisible, int32 PageIndex);
	bool IsMissingFloorJournalVisible() const { return bMissingFloorJournalVisible; }
	/**
	 * Harness hook: the line currently on screen, so a probe can assert *which*
	 * refusal the game gave instead of inferring it from an absence. §24's
	 * 즉시 차단 19 is about the sealed hour turning F9 and the journal down, and
	 * "nothing happened" is also what a broken binding looks like.
	 */
	FText GetActiveDialogueLineForTesting() const
	{
		return bHasCurrentDialogue ? CurrentDialogue.Line : FText::GetEmpty();
	}
	/**
	 * PushThought queues; it does not preempt. Reading only the line on screen
	 * returns whatever was already speaking, so a probe that wants to know
	 * whether the refusal was *given* has to look at the queue too.
	 */
	bool HasDialogueLineForTesting(const FString& Line) const
	{
		if (bHasCurrentDialogue && CurrentDialogue.Line.ToString().Equals(Line))
		{
			return true;
		}
		for (const FIGDialogueMessage& Queued : DialogueQueue)
		{
			if (Queued.Line.ToString().Equals(Line))
			{
				return true;
			}
		}
		return false;
	}
	int32 GetMissingFloorJournalPageCount() const;
	void SetInputDevicePresentation(bool bInUsingGamepad)
	{
		bUsingGamepad = bInUsingGamepad;
	}

protected:
	virtual void BeginPlay() override;
	/** Unbinds the noise-bus listener; HUDs are recreated per controller. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveInteractionComponent();
	void ResolveDirectors();
	void InitializeKoreanFont();
	void InitializeFrontendMenuTextures();
	void InitializeLensDropletTexture();
	void InitializeDialogueSurfaceTextures();
	void InitializeAudioCalibrationTexture();
	void InitializeMissingFloorJournalTextures();
	void InitializeFirstPersonActionTextures();
	UFontFace* LoadBundledFontFace(
		const TCHAR* RelativePath,
		const TCHAR* FontFaceName);
	UFont* MakeRuntimeFont(UFontFace* FontFace, int32 PixelSize, const TCHAR* FontName);
	UFont* GetFontForRole(EIGHudTextRole TextRole) const;
	FText GetObjectiveText() const;
	FString GetObjectiveTextAscii() const;
	void DrawCenteredText(
		const FText& Text,
		float ScreenY,
		const FLinearColor& Color,
		EIGHudTextRole TextRole,
		float TextScale = 1.0f);
	void DrawLeftAlignedText(
		const FText& Text,
		const FVector2D& Position,
		const FLinearColor& Color,
		EIGHudTextRole TextRole,
		float TextScale = 1.0f,
		bool bUseOutline = false);
	void DrawRightAlignedText(
		const FText& Text,
		const FVector2D& Position,
		const FLinearColor& Color,
		EIGHudTextRole TextRole,
		float TextScale = 1.0f,
		bool bUseOutline = false);
	void BeginLayoutValidationSample();
	void RecordLayoutValidationRect(
		const FVector2D& Minimum,
		const FVector2D& Maximum);
	void ValidateSettingsTextRect(
		const FVector2D& Minimum,
		const FVector2D& Maximum,
		const FVector2D& ContainerMinimum,
		const FVector2D& ContainerMaximum);
	void FinalizeLayoutValidationSample();
	void DrawCrosshair(const FLinearColor& Color);
	bool DrawChapterCard(double CurrentTime);
	/** Full-screen reading panel for whatever note is currently open. */
	void DrawNotePanel();
	/** Narrow, dense convenience-store thermal receipt presentation. */
	void DrawThermalReceiptPanel(const AIGReadableNote& Note);
	/** Dark, portrait phone screen used for the CH02 card approval record. */
	void DrawPhoneNotificationPanel(const AIGReadableNote& Note);
	void DrawFearDirection(double CurrentTime);
	void DrawLensDroplet(double CurrentTime);
	void DrawFirstPersonKnock(double CurrentTime);
	/** 포획 연출이 HUD 전체 프레임을 점유하는 동안 true를 반환한다. */
	bool DrawCaptureEmbrace(double CurrentTime);
	/** 포획 뒤 기상 잔상이 HUD 전체 프레임을 점유하는 동안 true를 반환한다. */
	bool DrawCaptureWakeEcho(double CurrentTime);
	/** 재관람 전용 우회 안내. 자막은 기존 하단 안전 영역을 그대로 사용한다. */
	void DrawSensoryInterludeSkip();
	bool DrawMissingFloorFailureEnding(double CurrentTime);
	/** §9 에필로그가 HUD 전체 프레임을 점유하는 동안 true를 반환한다. */
	bool DrawMissingFloorEpilogue(double CurrentTime);
	/**
	 * One expanding arc at the screen edge, sized by how far the sound the
	 * player just made actually carries (§5.1). No numbers, no meter: the ring
	 * is the entire channel, which is why reduced motion keeps it (pinned at
	 * its final radius) instead of suppressing it.
	 */
	void DrawNoiseRipple(double CurrentTime);
	void HandleNoiseReported(const struct FIGNoiseEvent& Event);
	/** Draws a scalable anti-aliased surface without allocating a Slate widget. */
	void DrawRoundedHudSurface(
		const FVector2D& Position,
		const FVector2D& Size,
		float CornerRadius,
		const FLinearColor& Color) const;
	/** Adds the authored optical-film grain while keeping rounded corners clean. */
	void DrawDialogueFilm(
		const FVector2D& Position,
		const FVector2D& Size,
		float CornerRadius,
		float Alpha) const;
	bool DrawDialoguePanel(double CurrentTime, float& OutPanelTop);
	bool DrawAudioCaption(double CurrentTime, float MaximumBottomY);
	void EnqueueDialogue(FIGDialogueMessage&& Message, double CurrentTime);
	void ActivateDialogue(FIGDialogueMessage&& Message, double CurrentTime);
	void AdvanceDialogueQueue(double CurrentTime);
	void SuspendDialoguePresentation(double CurrentTime);
	void ResumeDialoguePresentation(double CurrentTime);
	float CalculateDialogueDuration(
		const FString& Line,
		float MinimumDurationSeconds) const;
	float GetResolutionTextScale(float UserScale) const;
	void PrepareDialoguePage(
		float TextScale,
		float MaximumWidth,
		int32 MaximumLines,
		double CurrentTime);
	void ActivateAudioCaption(FIGAudioCaptionMessage&& Message, double CurrentTime);
	void AdvanceAudioCaptionQueue(double CurrentTime);
	float MeasureTextWidth(const FString& Text, UFont* Font, float TextScale) const;
	float MeasureTextHeight(const FString& Text, UFont* Font, float TextScale) const;
	float GetFittedTextScale(
		const FText& Text,
		EIGHudTextRole TextRole,
		float PreferredScale,
		float MaximumWidth,
		float MinimumScale = 0.5f) const;
	int32 FindFittingCaptionPrefix(
		const FString& Text,
		UFont* Font,
		float TextScale,
		float MaximumWidth) const;
	void WrapHudText(
		const FString& Source,
		UFont* Font,
		float TextScale,
		float MaximumWidth,
		int32 MaximumLines,
		TArray<FString>& OutLines,
		FString& OutRemainder) const;
	void DrawAccessibilityPanel();
	void DrawSystemMenuPanel();
	void DrawAudioCalibrationPanel();
	void DrawDisplaySettingsPanel();
	void DrawSettingsShell(
		const IGSettingsMenuLayout::FPanelMetrics& Metrics,
		const FText& Title,
		const FText& Subtitle,
		const FText& ContextLabel);
	void DrawSettingsCategoryRow(
		const IGSettingsMenuLayout::FPanelMetrics& Metrics,
		int32 CategoryIndex,
		const FText& Label,
		bool bSelected);
	void DrawSettingsOptionRow(
		const IGSettingsMenuLayout::FPanelMetrics& Metrics,
		int32 LocalRow,
		const FText& Label,
		const FText& Value,
		bool bSelected,
		bool bAdjustable);
	void DrawSettingsDetailText(
		const FString& Text,
		const FVector2D& Position,
		float MaximumWidth,
		float TextScale,
		const FLinearColor& Color);
	void DrawSettingsFooterText(
		const IGSettingsMenuLayout::FPanelMetrics& Metrics,
		const FText& Text);
	void DrawMissingFloorJournalPanel();
	UTexture2D* GetMissingFloorJournalThumbnail(int32 ThumbnailType) const;
	/** Screen-space bracket that snaps around whatever is currently focused. */
	void UpdateFocusBracket(AActor* FocusedActor, float DeltaSeconds);
	void DrawFocusBracket(const FLinearColor& Color, float Progress);

	bool SupportsKorean() const { return KoreanFontMedium != nullptr; }

	/** Aged-paper sheet the reading panel is printed on. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> NotePaperTexture;

	/** Cleaner paper grain used for thermal receipts. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ReceiptPaperTexture;

	/** Procedural low-cost proxy for the one authored camera-lens droplet. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LensDropletTexture;

	/** ImageGen-derived, low-contrast optical grain used by dialogue surfaces. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DialogueFilmTexture;

	/** Text-free ImageGen key art; every title/menu glyph stays runtime-localized. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FrontendTitleBackgroundTexture;

	/** One 256x1 alpha ramp replaces stepped shade strips and extra draw calls. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FrontendShadeTexture;

	/** ImageGen 파생 저조도 벽면. 보정 안내와 눈금은 런타임에서 그린다. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> AudioCalibrationWallTexture;

	/**
	 * §9 에필로그의 세 정지 화면. 없으면 글자만 남는다 — 에필로그의 뜻은
	 * 문장에 있으므로 그림이 빠져도 장면이 무너지지 않아야 한다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> EpilogueWorkshopTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> EpilogueAutumnTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> EpilogueServiceBayTexture;

	/** ImageGen-derived blank ledger paper. All Korean copy remains runtime text. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MissingFloorJournalTexture;

	/** ImageGen-derived M0 hand phases; screen-space only, never world geometry. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> FirstPersonKnockFrames;

	/** 카메라 위에 합성하는 ImageGen 파생 M1 포옹 단계. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> CaptureEmbraceFrames;

	/** Existing world textures sampled as restrained evidence-card thumbnails. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> JournalMeterTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> JournalPlasterTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> JournalTankTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> JournalMetalTexture;

	/** Runtime 9-slice mask; one 64 px allocation shared by every HUD surface. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> HudRoundedMaskTexture;

	/** Bundled faces keep typography and glyph metrics stable after packaging. */
	UPROPERTY(Transient)
	TObjectPtr<UFontFace> KoreanBodyFontFace;

	UPROPERTY(Transient)
	TObjectPtr<UFontFace> KoreanEmphasisFontFace;

	UPROPERTY(Transient)
	TObjectPtr<UFontFace> KoreanDisplayFontFace;

	/** Per-role fonts rasterized at native size so Hangul stays crisp. */
	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanFontLarge;

	/** Dedicated 64 px display face; scaling the 24 px HUD role looked soft. */
	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanFrontendTitleFont;

	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanFontMedium;

	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanFontSmall;

	/** 15 px sender/status face used by the phone presentation contract. */
	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanPhoneMetaFont;

	/** Dense Gulim/Dotum-style faces reserved for narrow thermal receipts. */
	UPROPERTY(Transient)
	TObjectPtr<UFontFace> KoreanReceiptFontFace;

	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanReceiptHeaderFont;

	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanReceiptFont;

	TWeakObjectPtr<UIGInteractionComponent> InteractionComponent;
	TWeakObjectPtr<AIGWakeUpDirector> WakeDirector;
	TWeakObjectPtr<UObject> ObjectiveProvider;
	double NextDirectorSearchTime = 0.0;

	TWeakObjectPtr<class UIGNoiseSubsystem> NoiseSubsystem;
	FDelegateHandle NoiseReportedHandle;

	FIGDialogueMessage CurrentDialogue;
	TArray<FIGDialogueMessage> DialogueQueue;
	TArray<FString> CurrentDialogueLines;
	bool bHasCurrentDialogue = false;
	bool bCurrentDialogueHasContinuation = false;
	double DialogueStartTime = 0.0;
	double DialogueEndTime = -1.0;
	double DialogueOccludedAt = -1.0;
	float DialogueLayoutScale = -1.0f;
	float DialogueLayoutWidth = -1.0f;
	int32 DialogueLayoutMaximumLines = 0;

	FVector2D DialogueLastPanelMinimum = FVector2D::ZeroVector;
	FVector2D DialogueLastPanelMaximum = FVector2D::ZeroVector;
	FVector2D DialogueLastCanvasSize = FVector2D::ZeroVector;
	int32 DialogueLastLineCount = 0;
	bool bDialogueLastSpeakerVisible = false;
	bool bDialogueLastHasContinuation = false;
	bool bDialogueLastInsideSafeArea = false;
	uint64 DialogueLastRenderSerial = 0;

	FText CurrentAudioCaption;
	TArray<FIGAudioCaptionMessage> AudioCaptionQueue;
	double AudioCaptionStartTime = 0.0;
	double AudioCaptionEndTime = -1.0;

	FVector FearCueWorldLocation = FVector::ZeroVector;
	double FearCueStartTime = 0.0;
	double FearCueEndTime = -1.0;

	/**
	 * Single-slot ripple state. One slot on purpose: footsteps report every
	 * footfall and a panicking heart every beat, so a per-event ring would
	 * strobe the screen edge. The louder event wins; quieter ones are dropped.
	 */
	FVector RippleWorldLocation = FVector::ZeroVector;
	float RippleRadiusCentimeters = 0.0f;
	float RippleLoudness = 0.0f;
	double RippleStartTime = 0.0;
	double RippleEndTime = -1.0;

	double LensDropletStartTime = 0.0;
	double LensDropletEndTime = -1.0;
	FVector2D LensDropletLastPosition = FVector2D::ZeroVector;
	FVector2D LensDropletLastSize = FVector2D::ZeroVector;
	FVector2D LensDropletLastCanvasSize = FVector2D::ZeroVector;
	float LensDropletLastAlpha = 0.0f;
	bool bLensDropletLastReducedMotion = false;
	double LensDropletLastRenderTime = -1.0;
	double FirstPersonKnockStartTime = -1.0;
	double CaptureEmbraceStartTime = -1.0;
	double CaptureEmbraceEndTime = -1.0;
	double CaptureWakeEchoStartTime = -1.0;
	double CaptureWakeEchoVisualEndTime = -1.0;
	double CaptureWakeEchoEndTime = -1.0;
	int32 CaptureWakeEchoCount = 0;
#if !UE_BUILD_SHIPPING
	double FirstPersonKnockPreviewNextTime = 0.0;
	double CaptureEmbracePreviewNextTime = 0.0;
	double CaptureWakeEchoPreviewNextTime = 0.0;
	bool bFirstPersonKnockPreview = false;
	bool bCaptureEmbracePreview = false;
	bool bCaptureWakeEchoPreview = false;
#endif

	FText ChapterCardEyebrow;
	FText ChapterCardTitle;
	FText ChapterCardSubtitle;
	double ChapterCardStartTime = 0.0;
	double ChapterCardEndTime = -1.0;

	FVector2D FocusBracketMin = FVector2D::ZeroVector;
	FVector2D FocusBracketMax = FVector2D::ZeroVector;
	TWeakObjectPtr<AActor> FocusBracketTarget;
	float FocusBracketAlpha = 0.0f;
	float FocusBracketAcquireElapsed = 0.0f;
	double LastHudDrawTime = 0.0;
	FVector2D LayoutValidationCanvasSize = FVector2D::ZeroVector;
	FVector2D LayoutValidationBoundsMin = FVector2D::ZeroVector;
	FVector2D LayoutValidationBoundsMax = FVector2D::ZeroVector;
	int32 LayoutValidationElementCount = 0;
	uint64 LayoutValidationFrameSerial = 0;
	bool bLayoutValidationAllInsideCanvas = false;
	bool bLayoutValidationAllInsideSettingsContainers = false;
	bool bLayoutValidationSampleReady = false;
	bool bLayoutValidationEnabled = false;
	int32 AccessibilitySelectedRow = 0;
	int32 SystemMenuSelectedRow = 0;
	int32 MissingFloorJournalPageIndex = 0;
	bool bNightPresentation = false;
	bool bSensoryInterludePresentation = false;
	bool bSensoryInterludeSkipAvailable = false;
	bool bSensoryInterludeSkipInProgress = false;
	bool bSensoryInterludeSkipToggleMode = false;
	float SensoryInterludeSkipProgress = 0.0f;
	float SensoryInterludeSkipHoldSeconds = 2.0f;
	bool bMissingFloorFailureEndingVisible = false;
	bool bMissingFloorFailureRetryEnabled = false;
	double MissingFloorFailureEndingStartedAt = 0.0;
	EIGMissingFloorEpilogueScene MissingFloorEpilogueScene =
		EIGMissingFloorEpilogueScene::None;
	double MissingFloorEpilogueSceneStartedAt = 0.0;
	FText MissingFloorEpilogueHeading;
	TArray<FText> MissingFloorEpilogueBodyLines;
	FText MissingFloorEpilogueFootnote;
	bool bAccessibilityMenuVisible = false;
	bool bSystemMenuVisible = false;
	bool bMissingFloorJournalVisible = false;
	bool bSystemMenuIsTitle = false;
	bool bSystemMenuUseTitleBackdrop = false;
	bool bSystemMenuIsCredits = false;
	bool bSystemMenuIsContentNotice = false;
	bool bSystemMenuIsAudioCalibration = false;
	bool bSystemMenuIsDisplaySettings = false;
	bool bSystemMenuCanContinue = false;
	bool bSystemMenuNightFiveAvailable = false;
	bool bSystemMenuNightFiveSpent = false;
	bool bSystemMenuNightFivePlaying = false;
	bool bSystemMenuConfirmNewGame = false;
	bool bSystemMenuHeadphoneRecommendation = false;
	bool bSystemMenuVSync = true;
	bool bDisplaySettingsApplied = false;
	bool bDisplaySettingsAwaitingConfirmation = false;
	bool bSystemMenuStatusIsError = false;
	int32 DisplaySettingsSelectedRow = 0;
	int32 AudioCalibrationSelectedRow = 0;
	int32 AudioCalibrationVolumeStep = 6;
	int32 AudioCalibrationBrightnessStep = 2;
	bool bSystemMenuAudioCalibrationFirstRun = false;
	int32 DisplayWindowModeIndex = 0;
	int32 DisplayResolutionIndex = 1;
	int32 DisplayQualityIndex = 1;
	int32 DisplayFrameLimitIndex = 1;
	int32 DisplayConfirmationSecondsRemaining = 0;
	FText SystemMenuStatusText;
	double SystemMenuOpenedAt = -1.0;
	bool bUsingGamepad = false;
};
