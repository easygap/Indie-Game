#pragma once

#include "CoreMinimal.h"
#include "Narrative/IGRebirthNarrativeTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IGRebirthEvidenceSubsystem.generated.h"

class IConsoleObject;

struct FIGRebirthEndingTimelineEntry
{
	int32 Sequence = 0;
	FName EventId;
	EIGRebirthEndingChoice EndingChoice = EIGRebirthEndingChoice::None;
	bool bActualStateRestored = false;
	bool bFound0731 = false;
	bool bCommonDiscoveryCommitted = false;
};

/**
 * Opt-in runtime evidence recorder for the REBIRTH manual release paths.
 *
 * Normal play only owns the subsystem object. Timeline collection and disk
 * writes stay disabled until a QA command-line switch or console dump enables
 * them.
 */
UCLASS()
class INDIEGAME_API UIGRebirthEvidenceSubsystem final
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Records a named S4 milestone when evidence capture is active. */
	static void RecordEndingEvent(
		UObject* WorldContextObject,
		FName EventId);

	/** Records the otherwise director-local P4 observation state. */
	static void RecordPuzzleFourObservation(
		UObject* WorldContextObject,
		int32 DownwardLoopCount,
		bool bUpwardRouteRevealed);

private:
	void HandleDumpConsoleCommand(const TArray<FString>& Arguments);
	void RecordEndingEventInternal(FName EventId);
	void RecordPuzzleFourObservationInternal(
		int32 DownwardLoopCount,
		bool bUpwardRouteRevealed);
	bool DumpEvidence(
		const FString& CaptureLabel,
		const FString& OutputPathOverride = FString());
	FString BuildDeterministicJson(const FString& CaptureLabel) const;
	FString ResolveOutputPath(
		const FString& CaptureLabel,
		const FString& OutputPathOverride) const;
	void AutoDumpAfterEvent(FName EventId);

	IConsoleObject* DumpConsoleCommand = nullptr;
	TArray<FIGRebirthEndingTimelineEntry> EndingTimeline;
	FString ConfiguredOutputPath;
	int32 NextTimelineSequence = 1;
	int32 NextAutoDumpSequence = 1;
	int32 PuzzleFourDownwardLoopCount = 0;
	bool bPuzzleFourObserved = false;
	bool bPuzzleFourUpwardRouteRevealed = false;
	bool bCaptureEnabled = false;
	bool bAutoDumpOnEvent = false;
};
