#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGInteractableActor.h"
#include "IGReadableNote.generated.h"

class AIGReadableNote;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

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

	/** Builds the paper: a thin quad with the note artwork on its face. */
	void ConfigurePrototypeVisuals(
		UStaticMesh* CubeMesh,
		UMaterialInterface* PaperMaterial,
		const FVector& PaperSize);

	/**
	 * Sets the heading and body shown in the reading panel.
	 *
	 * The body is a list of lines rather than one string with embedded
	 * breaks. Korean copy has to be broken by hand anyway — there is nothing
	 * sensible to wrap on mid-clause — and keeping the lines separate means
	 * the note text contains no escape sequences at all.
	 */
	void SetNoteText(const FText& InTitle, TArray<FText> InBodyLines);

	/** Sets the prompt shown before it has been read (e.g. "공지 읽기"). */
	void SetInteractionPrompt(const FText& InPrompt) { OpenPrompt = InPrompt; }

	UFUNCTION(BlueprintPure, Category = "Note")
	bool IsOpen() const { return bOpen; }

	UFUNCTION(BlueprintPure, Category = "Note")
	const FText& GetTitle() const { return NoteTitle; }

	UFUNCTION(BlueprintPure, Category = "Note")
	const TArray<FText>& GetBodyLines() const { return NoteBodyLines; }

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

private:
	static TWeakObjectPtr<AIGReadableNote> OpenNote;

	bool bOpen = false;
	bool bEverRead = false;
};
