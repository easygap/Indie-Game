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
	Hint
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

protected:
	virtual void BeginPlay() override;

private:
	void ResolveInteractionComponent();
	void ResolveDirectors();
	void InitializeKoreanFont();
	UFont* MakeRuntimeFont(UFontFace* FontFace, int32 PixelSize, const TCHAR* FontName);
	UFont* GetFontForRole(EIGHudTextRole TextRole) const;
	FText GetObjectiveText() const;
	FString GetObjectiveTextAscii() const;
	void DrawCenteredText(
		const FText& Text,
		float ScreenY,
		const FLinearColor& Color,
		EIGHudTextRole TextRole);
	void DrawCrosshair(const FLinearColor& Color);
	bool DrawChapterCard(double CurrentTime);
	/** Full-screen reading panel for whatever note is currently open. */
	void DrawNotePanel();
	/** Narrow, dense convenience-store thermal receipt presentation. */
	void DrawThermalReceiptPanel(const AIGReadableNote& Note);
	void DrawHoldProgress(float Progress);
	/** Screen-space bracket that snaps around whatever is currently focused. */
	void UpdateFocusBracket(const AActor* FocusedActor, float DeltaSeconds);
	void DrawFocusBracket(const FLinearColor& Color, float Progress);

	bool SupportsKorean() const { return KoreanFontMedium != nullptr; }

	/** Aged-paper sheet the reading panel is printed on. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> NotePaperTexture;

	/** Cleaner paper grain used for thermal receipts. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ReceiptPaperTexture;

	/** Per-role fonts rasterized at native size so Hangul stays crisp. */
	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanFontLarge;

	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanFontMedium;

	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanFontSmall;

	TWeakObjectPtr<UIGInteractionComponent> InteractionComponent;
	TWeakObjectPtr<AIGWakeUpDirector> WakeDirector;
	TWeakObjectPtr<UObject> ObjectiveProvider;
	double NextDirectorSearchTime = 0.0;

	FText CurrentThought;
	double ThoughtStartTime = 0.0;
	double ThoughtEndTime = -1.0;

	FText ChapterCardEyebrow;
	FText ChapterCardTitle;
	FText ChapterCardSubtitle;
	double ChapterCardStartTime = 0.0;
	double ChapterCardEndTime = -1.0;

	FVector2D FocusBracketMin = FVector2D::ZeroVector;
	FVector2D FocusBracketMax = FVector2D::ZeroVector;
	float FocusBracketAlpha = 0.0f;
	double LastHudDrawTime = 0.0;
};
