#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sequence/IGObjectiveProvider.h"
#include "IGNightPhaseDirector.generated.h"

class AIGPrologueWorldScene;
class AIGPlayerCharacter;
class AIGListenerEntity;
class APlayerController;
class UIGMissingFloorNarrativeSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FIGHourActiveSignature, bool /*bActive*/);

/**
 * Owner of '그 시간' — the hour from half past four to half past five in which
 * 없는 층 actually takes place (STORY_BIBLE_MISSING_FLOOR.md §1).
 *
 * There is no clock anywhere in this project: every "04:44" in the codebase is
 * a string on a note or a receipt. So this director keeps its own elapsed
 * seconds and maps them onto the hour, and it is the single authority for when
 * the building is shut.
 *
 * While the hour holds, the envelope is sealed (the common entrance refuses,
 * the lift is dead, the connector shutter is down), the phone has no signal,
 * and the HUD shows no objective — the player is told nothing and has to
 * listen. Morning arrives either when the hour runs out or when the night's
 * goal is met, and it arrives as the world opening rather than as a notice.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGNightPhaseDirector
	: public AActor
	, public IIGObjectiveProvider
{
	GENERATED_BODY()

public:
	AIGNightPhaseDirector();

	/** Binds the scene whose envelope this director seals. */
	void Configure(AIGPrologueWorldScene* InScene, AIGPlayerCharacter* InPlayer);

	/** Starts the hour: seals the building and begins counting. */
	UFUNCTION(BlueprintCallable, Category = "Night")
	void BeginTheHour(int32 NightIndex);

	/** 진행 시간을 초기화하지 않고 봉인된 야간 자동 저장을 복원한다. */
	void ResumeTheHour(int32 NightIndex, float ElapsedSeconds);

	/** 봉인된 건물을 열지 않고 엔딩 C 연출 동안 서사 시간을 멈춘다. */
	void SuspendForFailureEnding();
	/** 밤 4 범위의 서사 상태를 되돌린 뒤 같은 밤을 다시 시작한다. */
	void RestartTheHour(int32 NightIndex);

	/**
	 * Ends the hour early because the night's goal was met. Morning is the
	 * same exit as the timeout, so both routes leave identical world state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Night")
	void CompleteNightGoal();
	/** 채운 밤의 기록. 못 채운 밤은 다음 저녁에 같은 밤이 온다(§5.4). */
	static FName GoalBeatId(int32 NightIndex);
	/**
	 * 다음 새벽은 눈을 감기지도, 아침 독백을 하지도 않는다. 엔딩 길에서
	 * 에필로그가 제 암전을 갖고 오므로 두 번 깜빡이면 안 된다.
	 */
	void SuppressNextMorningPresentation() { bMorningPresentationSuppressed = true; }
	/** 벽 안의 2분 40초는 이 밤의 시간이 아니다. 막간 동안 05:30이 서 있는다. */
	void SetHourPaused(bool bPaused) { bHourPaused = bPaused; }
	bool IsHourPaused() const { return bHourPaused; }
	/**
	 * 공동현관 잠금 소리만 낸다. 엔딩 B는 벽 앞에 앉은 채 05:30을 먼저 맞고,
	 * 진짜 새벽은 에필로그 아래서 소리 없이 온다.
	 */
	void PlayDawnLatchCue() { PlayEntranceLatch(); }

	UFUNCTION(BlueprintPure, Category = "Night")
	bool IsHourActive() const { return bHourActive; }
	bool IsFailureEndingSuspended() const { return bFailureEndingSuspended; }
	/**
	 * 05:30에 눈이 감기기 시작해서 다시 뜨기 시작할 때까지 참이다. 세계는 그
	 * 한가운데, 검은 화면 아래서 낮으로 바뀐다. 낮 전환을 받는 쪽은 이 동안
	 * 카메라 페이드를 건드리지 않는다.
	 */
	bool IsDawnTransitionInProgress() const { return bDawnTransitionInProgress; }

	UFUNCTION(BlueprintPure, Category = "Night")
	float GetHourElapsedSeconds() const { return HourElapsedSeconds; }

	/** Minutes left in story time, for the objective line outside the hour. */
	UFUNCTION(BlueprintPure, Category = "Night")
	int32 GetStoryMinutesRemaining() const;

	// IIGObjectiveProvider
	virtual FText GetObjectiveText() const override;
	virtual FString GetObjectiveTextAscii() const override;
	virtual float GetObjectiveProgress() const override;

	/**
	 * Fires on every hour boundary: true when the night seals, false at dawn.
	 * The composition root routes this to everything the boundary touches —
	 * entity dormancy, the booth lock, the day interactions.
	 */
	FIGHourActiveSignature OnHourActiveChanged;

	/**
	 * Real seconds the hour lasts. Story time runs 04:30 to 05:30, but the
	 * story hour compresses to twenty real minutes: the timeout is a mercy
	 * for a stuck player, and sixty real minutes of fallback would be a wall,
	 * not a mercy. Nights normally end early through CompleteNightGoal.
	 */
	static constexpr float HourDurationSeconds = 1200.0f;
	static constexpr int32 StoryStartMinutes = 4 * 60 + 30;
	static constexpr int32 StoryEndMinutes = 5 * 60 + 30;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void TickHour();
	/**
	 * 새벽의 시각표. 그가 그 자리에 멈추고(숨기지 않는다), 눈이 감기고, 검은
	 * 화면 아래서 건물이 낮으로 바뀌고 잠금이 풀리는 소리가 아래에서 올라온
	 * 뒤에 눈을 뜬다. 엔딩 C의 새벽, 에필로그가 화면을 가진 엔딩 길, 무인
	 * 검증은 같은 결과를 즉시 만든다.
	 */
	void ReleaseAtDawn();
	/** 봉쇄를 풀고 낮을 알린다. 이 브로드캐스트가 그를 재운다. */
	void ApplyDawnWorld();
	/** 눈이 다 감긴 뒤. 세계를 바꾸고 낮의 자동 저장을 건다. */
	void FlipWorldUnderBlack();
	/** 공동현관 잠금. 1층의 딸깍과, 계단실을 타고 올라온 걸쇠 소리. */
	void PlayEntranceLatch();
	/** 눈을 뜬다. 목표 줄과 아침 독백이 같이 온다. */
	void FinishDawnPresentation();
	void PlayMorningLine();
	/** 눈을 감는 동안 그를 그 자리에 세운다. 포획도 이동도 없다. */
	void HoldListenersForDawn();
	void ReleaseHeldListeners();
	bool IsCaptureResetInFlight() const;
	APlayerController* GetDawnController() const;
	void ApplySealedPresentation(bool bSealed);
	void RequestMissingFloorAutosave(bool bAtNight);
	UIGMissingFloorNarrativeSubsystem* GetNarrative() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPlayerCharacter> Player;

	float HourElapsedSeconds = 0.0f;
	bool bHourActive = false;
	bool bGoalComplete = false;
	bool bFailureEndingSuspended = false;
	bool bRestoringHour = false;
	bool bMorningPresentationSuppressed = false;
	bool bHourPaused = false;
	/** -IGListenerGreyboxProbe / -IGNightCapture: 새벽 직후의 세계를 같은 프레임에 본다. */
	bool bImmediateDawn = false;
	/** 새벽의 눈 감기가 화면을 쥐고 있다. FinishDawnPresentation이 놓는다. */
	bool bDawnTransitionInProgress = false;
	FTimerHandle HourTimer;
	FTimerHandle DawnTimer;
	FTimerHandle DawnWorldTimer;
	FTimerHandle DawnLatchTimer;
	TArray<TWeakObjectPtr<AIGListenerEntity>> DawnHeldListeners;
};
