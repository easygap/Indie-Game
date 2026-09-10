#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Narrative/IGMissingFloorNarrativeTypes.h"
#include "IGMissingFloorPuzzleTwoDirector.generated.h"

class AIGCctvChannelFive;
class AIGMissingFloorEvidence;
class AIGPrologueWorldScene;
class AIGReadableNote;
class AIGSwingDoor;
class UIGMissingFloorNarrativeSubsystem;

DECLARE_MULTICAST_DELEGATE(FIGPuzzleTwoSolvedSignature);

/**
 * P2 「먹지 원장」 — the night the paper contradicts itself
 * (STORY_BIBLE_MISSING_FLOOR.md §7 P2, §8 밤2).
 *
 * The management booth's fair-copy ledger says "물탱크 배관 소음. 조치
 * 완료." Under it sits the carbon pad, and pressure does not lie: three
 * passes of frottage — each a sustained sound the one upstairs can hear —
 * restore the original complaints, dated after the day the tenant
 * "moved out". Crossed with the estate agent's move-out message, that is
 * T7: whatever was in the wall was still alive.
 *
 * The booth also holds the two beats that need no puzzle: the CCTV monitor
 * whose channel selector has one button too many, and the inner room's
 * door gap lined with egg-crate foam. Neither files evidence — they are
 * the night's images, played once.
 *
 * The booth door is the day/night valve: locked while Mok Hansu keeps his
 * daytime post, open in the hour when he hides in the soundproofed room.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGMissingFloorPuzzleTwoDirector : public AActor
{
	GENERATED_BODY()

public:
	AIGMissingFloorPuzzleTwoDirector();

	/** Spawns the booth door and its contents against a built lobby. */
	bool Configure(AIGPrologueWorldScene* InScene);

	/** Day locks the booth; the hour opens it. */
	void SetHourActive(bool bHourActive);

	/**
	 * Fired once, when the carbon original is restored — the night-2 goal.
	 * T7 itself waits for night 3: the realtor's message turns up beside the
	 * keyring, so the date contradiction closes in the same night as the answer.
	 */
	FIGPuzzleTwoSolvedSignature OnSolved;

	/** Probe queries. */
	bool ValidateFixtures() const;
	AIGMissingFloorEvidence* GetCarbonLedger() const { return CarbonLedger; }
	AIGReadableNote* GetAgentMessageNote() const { return AgentMessageNote; }
	AIGMissingFloorEvidence* GetCctvSelector() const { return CctvSelector; }
	AIGCctvChannelFive* GetCctvChannelFive() const { return CctvChannelFive; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleCarbonRestored(AIGMissingFloorEvidence* Evidence);
	void HandleCctvExamined(AIGMissingFloorEvidence* Evidence);

	/**
	 * §5.5 비트 2-1과 2-6, on one prop. During the hour the phone arms itself
	 * against the door; in the morning the same prop plays the take back and she
	 * hears her own footsteps and exactly as much silence as there was knocking.
	 */
	void HandlePhoneRecorder(AIGMissingFloorEvidence* Evidence);

	/** Keeps the phone's prompt honest about which of the two it is offering. */
	void RefreshPhonePrompt();
	/** 부동산 문자 사본은 밤3부터 책상에 있다. 밤2의 책상에는 없다. */
	void RefreshAgentNoteAvailability();
	void HandleFoamExamined(AIGMissingFloorEvidence* Evidence);
	void HandleWallCalendarExamined(AIGMissingFloorEvidence* Evidence);
	void HandleRecorderBayExamined(AIGMissingFloorEvidence* Evidence);
	void HandleInnerRoomListenExamined(AIGMissingFloorEvidence* Evidence);
	UFUNCTION()
	void HandleBoardReceiptsRead(class AIGReadableNote* Note, bool bOpened);
	void HandleTruthConfirmed(EIGMissingFloorTruth Truth);

	UFUNCTION()
	void HandleAgentNoteRead(AIGReadableNote* Note, bool bOpened);

	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TObjectPtr<AIGSwingDoor> BoothDoor;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> FairCopyLedger;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> CarbonLedger;

	/** §5.5's phone: armed at the door by night, played back in the morning. */
	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> PhoneRecorder;

	bool bPhonePlayedBack = false;
	/** 채널 5가 찢어진 뒤에야 글이 온다. 보는 동안은 읽게 하지 않는다. */
	FTimerHandle CctvThoughtTimer;

	UPROPERTY(Transient)
	TObjectPtr<AIGReadableNote> AgentMessageNote;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> CctvSelector;

	/** §14's one cut: the render-target channel behind the fifth button. */
	UPROPERTY(Transient)
	TObjectPtr<AIGCctvChannelFive> CctvChannelFive;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> FoamGap;

	/** T5 첫 번째 출처. 같은 품목이 두 날짜로 두 번 실려 왔다. */
	UPROPERTY(Transient)
	TObjectPtr<class AIGReadableNote> BoardReceipts;

	/** §22.3 선택적 목격 둘. 진실을 열지 않고 문장만 구체화한다. */
	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> WallCalendar;

	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> RecorderBay;

	/** 같은 문에서 보는 것과 듣는 것. 문틈은 눈이고 이쪽은 귀다. */
	UPROPERTY(Transient)
	TObjectPtr<AIGMissingFloorEvidence> InnerRoomListen;

	FDelegateHandle TruthHandle;
	bool bSolvedAnnounced = false;
};
