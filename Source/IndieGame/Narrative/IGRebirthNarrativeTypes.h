#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "IGRebirthNarrativeTypes.generated.h"

UENUM(BlueprintType)
enum class EIGRebirthPurchaseProfile : uint8
{
	Unset = 0,
	ProfileA500MlX2 = 1,
	ProfileB1LX1 = 2,
	ProfileC2LX2 = 3
};

UENUM(BlueprintType)
enum class EIGRebirthPaymentMethod : uint8
{
	Unset = 0,
	WalletCard = 1,
	PocketCard = 2
};

UENUM(BlueprintType)
enum class EIGRebirthCatWaterState : uint8
{
	Unset = 0,
	BottleCap = 1,
	PaperCup = 2,
	PassedBy = 3
};

UENUM(BlueprintType)
enum class EIGRebirthBottleClosureState : uint8
{
	Unset = 0,
	MissingCap = 1,
	Resealed = 2
};

UENUM(BlueprintType)
enum class EIGRebirthConvergencePoint : uint8
{
	C1StorePurchase = 0,
	C2FirstReturn = 1,
	C3SecondMorning = 2,
	C4RoofReached = 3,
	C5FinalChoice = 4,
	C6AfterDiscovery = 5
};

/** Stable evidence ids; never serialize the director's interaction enum. */
UENUM(BlueprintType)
enum class EIGRebirthEvidenceId : uint8
{
	None = 0,
	CatEntered = 1,
	CatExited = 2,
	HosePaw = 3,
	HoseImpact = 4,
	Bag = 5,
	WetRung = 6,
	HandSmear = 7,
	Glasses = 8,
	TankClothing = 9,
	CurrentSleeve = 10,
	SearchPoster = 11
};

/** The ending is one exclusive choice, not two independently persisted flags. */
UENUM(BlueprintType)
enum class EIGRebirthEndingChoice : uint8
{
	None = 0,
	EndingA = 1,
	EndingB = 2
};

USTRUCT(BlueprintType)
struct INDIEGAME_API FIGRebirthTruthRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	FGameplayTag TruthTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FName> SourceIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bConfirmed = false;
};

USTRUCT(BlueprintType)
struct INDIEGAME_API FIGRebirthChoiceState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	EIGRebirthPurchaseProfile PurchaseProfile = EIGRebirthPurchaseProfile::Unset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	EIGRebirthPaymentMethod PaymentMethod = EIGRebirthPaymentMethod::Unset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	EIGRebirthCatWaterState CatWaterState = EIGRebirthCatWaterState::Unset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	EIGRebirthBottleClosureState BottleClosureState =
		EIGRebirthBottleClosureState::Unset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bHasPaperCup = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bWaitedForCat = false;
};

USTRUCT(BlueprintType)
struct INDIEGAME_API FIGRebirthP3State
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bDirectInletClosed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bReserveInletClosed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bPressureReleaseOpen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bPressureZero = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bCompleted = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	float PressureKPa = 60.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 MistakeCount = 0;

	/**
	 * The gauge must audibly settle at zero twice before the drain is safe.
	 * Keeping this count makes a quit between the two ticks resume exactly.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 ZeroConfirmationTicks = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	float HintElapsedSeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 HintStage = 0;
};

USTRUCT(BlueprintType)
struct INDIEGAME_API FIGRebirthChapterThreeState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	FIGRebirthP3State P3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bTankOpened = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	EIGRebirthEvidenceId FocusedEvidence = EIGRebirthEvidenceId::None;

	/**
	 * Clues the player has physically inspected. These are deliberately
	 * separate from truth-record SourceIds, which are committed only after an
	 * explicit valid comparison.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FName> ObservedP5Sources;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bP5CatSafeConfirmed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bP5HoseCauseConfirmed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bP5FallConfirmed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bP5IdentityConfirmed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 AccidentScratchCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bAccidentScratchTailSettled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bLookedAwayAfterFirstScratch = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bActedAfterSecondScratch = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	EIGRebirthEndingChoice EndingChoice = EIGRebirthEndingChoice::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bCommonDiscoveryCommitted = false;
};

USTRUCT(BlueprintType)
struct INDIEGAME_API FIGRebirthNarrativeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 SchemaVersion = 3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FIGRebirthTruthRecord> TruthRecords;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	FGameplayTagContainer NarrativeDebt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	FIGRebirthChoiceState Choices;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FName> VisitedLocations;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FName> ResolvedPuzzles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FName> SkippedPuzzles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FName> EquippedOutfitChapters;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	TArray<FName> PlayedOneShotBeats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bTankOpenedEarly = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bHasMemoryFlashlight = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative")
	FIGRebirthChapterThreeState ChapterThree;
};
