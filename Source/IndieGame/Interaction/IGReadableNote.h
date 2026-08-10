#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGInteractableActor.h"
#include "IGReadableNote.generated.h"

class AIGReadableNote;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** One product row on a thermal receipt. Prices are expressed in won. */
USTRUCT(BlueprintType)
struct INDIEGAME_API FIGReceiptItemLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	FText ProductName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	int32 Quantity = 1;

	/** Price for one unit, printed separately on Korean POS receipts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	int32 UnitPrice = 0;

	/** Quantity multiplied by unit price after any line discount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	int32 Amount = 0;
};

/** A label/value row such as card approval or points information. */
USTRUCT(BlueprintType)
struct INDIEGAME_API FIGReceiptKeyValueLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	FText Value;
};

/**
 * Structured thermal-receipt data.
 *
 * Keeping columns as data instead of padding localized strings with spaces
 * lets the HUD align Korean product names, quantities and prices correctly.
 */
USTRUCT(BlueprintType)
struct INDIEGAME_API FIGThermalReceiptData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	FText StoreName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	FText StoreSubtitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	TArray<FText> StoreDetailLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	TArray<FText> PolicyLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	FText TransactionDateTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	FText PosLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	FText ReceiptNumber;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	TArray<FIGReceiptItemLine> Items;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	int32 Subtotal = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	int32 TaxableSupply = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	int32 Vat = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	int32 Total = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	FText PaymentHeading;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	TArray<FIGReceiptKeyValueLine> PaymentLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	TArray<FText> FooterLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receipt")
	FString BarcodeDigits;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FIGNoteReadSignature,
	AIGReadableNote*, Note,
	bool, bOpened);

/**
 * A piece of paper the player can stop and read: a building-management
 * notice taped to the lift doors, a memo on the fridge, a torn ledger page.
 *
 * Notes carry the story. Nothing in this game explains itself out loud, so
 * everything the player is allowed to learn is either seen or read.
 *
 * Reading opens a full-screen panel drawn by the HUD; the world keeps
 * running behind it, which is the point — you are standing in a dark
 * corridor with your face in a piece of paper.
 */
UCLASS(Blueprintable)
class INDIEGAME_API AIGReadableNote : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGReadableNote();

	/** Builds the physical reading prop; bound ledgers may opt into contact shadow. */
	void ConfigurePrototypeVisuals(
		UStaticMesh* CubeMesh,
		UMaterialInterface* PaperMaterial,
		const FVector& PaperSize,
		bool bCastPresentationShadow = false);

	/**
	 * Sets the heading and body shown in the reading panel.
	 *
	 * The body is a list of lines rather than one string with embedded
	 * breaks. Korean copy has to be broken by hand anyway — there is nothing
	 * sensible to wrap on mid-clause — and keeping the lines separate means
	 * the note text contains no escape sequences at all.
	 */
	void SetNoteText(const FText& InTitle, TArray<FText> InBodyLines);

	/**
	 * Switches this document to the narrow thermal-receipt presentation.
	 * Generic notes keep the existing full-sheet layout.
	 */
	void SetThermalReceiptData(FIGThermalReceiptData InReceiptData);

	/** Uses a dark smartphone notification screen instead of a paper sheet. */
	void SetPhoneNotificationPresentation();

	/** Sets the prompt shown before it has been read (e.g. "공지 읽기"). */
	void SetInteractionPrompt(const FText& InPrompt) { OpenPrompt = InPrompt; }

	UFUNCTION(BlueprintPure, Category = "Note")
	bool IsOpen() const { return bOpen; }

	UFUNCTION(BlueprintPure, Category = "Note")
	const FText& GetTitle() const { return NoteTitle; }

	UFUNCTION(BlueprintPure, Category = "Note")
	const TArray<FText>& GetBodyLines() const { return NoteBodyLines; }

	UFUNCTION(BlueprintPure, Category = "Note|Receipt")
	bool UsesThermalReceiptPresentation() const { return bUsesThermalReceiptPresentation; }

	UFUNCTION(BlueprintPure, Category = "Note|Phone")
	bool UsesPhoneNotificationPresentation() const
	{
		return bUsesPhoneNotificationPresentation;
	}

	UFUNCTION(BlueprintPure, Category = "Note|Receipt")
	const FIGThermalReceiptData& GetThermalReceiptData() const { return ThermalReceiptData; }

	/** Closes the panel; the HUD calls this when the player dismisses it. */
	UFUNCTION(BlueprintCallable, Category = "Note")
	void Close();

	/**
	 * The note currently being read, if any. Only one can be open at a time,
	 * so a single weak pointer is enough for the HUD to find it without the
	 * HUD and the note having to know about each other.
	 */
	static AIGReadableNote* GetOpenNote();

	/** Broadcast when the panel opens (true) or closes (false). */
	UPROPERTY(BlueprintAssignable, Category = "Note|Events")
	FIGNoteReadSignature OnReadStateChanged;

	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual void CompleteInteraction_Implementation(const FIGInteractionContext& Context) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Note|Components")
	TObjectPtr<UStaticMeshComponent> PaperMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FText NoteTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	TArray<FText> NoteBodyLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
	FText OpenPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note|Receipt")
	bool bUsesThermalReceiptPresentation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note|Receipt")
	FIGThermalReceiptData ThermalReceiptData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note|Phone")
	bool bUsesPhoneNotificationPresentation = false;

private:
	static TWeakObjectPtr<AIGReadableNote> OpenNote;

	bool bOpen = false;
	bool bEverRead = false;
};
