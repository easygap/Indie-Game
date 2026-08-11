#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
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
	bool bCredits = false;
	bool bDisplaySettings = false;
	bool bCanContinue = false;
	bool bConfirmNewGame = false;
	bool bHeadphoneRecommendation = false;
	bool bVSync = true;
	bool bDisplaySettingsApplied = false;
	bool bDisplaySettingsAwaitingConfirmation = false;
	bool bStatusIsError = false;
	int32 SelectedRow = 0;
	int32 DisplaySelectedRow = 0;
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
 * Korean text renders through a runtime composite font built from a system
 * font (Malgun Gothic and friends); when none is available the HUD falls back
 * to the engine font with ASCII strings.
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
	void InitializeLensDropletTexture();
	void InitializeDialogueSurfaceTextures();
	void InitializeMissingFloorJournalTextures();
	void InitializeFirstPersonActionTextures();
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
	void BeginLayoutValidationSample();
	void RecordLayoutValidationRect(
		const FVector2D& Minimum,
		const FVector2D& Maximum);
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
	void DrawDisplaySettingsPanel();
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

	/** Per-role fonts rasterized at native size so Hangul stays crisp. */
	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanFontLarge;

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
	bool bLayoutValidationSampleReady = false;
	bool bLayoutValidationEnabled = false;
	int32 AccessibilitySelectedRow = 0;
	int32 SystemMenuSelectedRow = 0;
	int32 MissingFloorJournalPageIndex = 0;
	bool bNightPresentation = false;
	bool bSensoryInterludePresentation = false;
	bool bAccessibilityMenuVisible = false;
	bool bSystemMenuVisible = false;
	bool bMissingFloorJournalVisible = false;
	bool bSystemMenuIsTitle = false;
	bool bSystemMenuIsCredits = false;
	bool bSystemMenuIsDisplaySettings = false;
	bool bSystemMenuCanContinue = false;
	bool bSystemMenuConfirmNewGame = false;
	bool bSystemMenuHeadphoneRecommendation = false;
	bool bSystemMenuVSync = true;
	bool bDisplaySettingsApplied = false;
	bool bDisplaySettingsAwaitingConfirmation = false;
	bool bSystemMenuStatusIsError = false;
	int32 DisplaySettingsSelectedRow = 0;
	int32 DisplayWindowModeIndex = 0;
	int32 DisplayResolutionIndex = 1;
	int32 DisplayQualityIndex = 1;
	int32 DisplayFrameLimitIndex = 1;
	int32 DisplayConfirmationSecondsRemaining = 0;
	FText SystemMenuStatusText;
	bool bUsingGamepad = false;
};
