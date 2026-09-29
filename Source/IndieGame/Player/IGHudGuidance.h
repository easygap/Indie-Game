#pragma once

#include "CoreMinimal.h"

/** 목표는 갱신 때만, 기본 조작은 첫 입주 때만 잠깐 보여 준다. */
struct FIGHudGuidance
{
	static constexpr float ObjectiveSeconds = 7.0f;
	static constexpr float TutorialSeconds = 20.0f;
	static constexpr float RecallSeconds = 10.0f;
	static constexpr float FadeSeconds = 0.45f;

	/**
	 * bTutorialVisible: 조작 안내가 실제로 화면에 나와 있었는지. 대사나 소리 자막에
	 * 가려진 동안은 20초에서 빼지 않는다. 가려진 채 시간이 다 가면 한 번도 못 본
	 * 안내가 끝난 것으로 기억된다.
	 */
	void Update(float DeltaSeconds, const FString& Objective, bool bTutorialAllowed, bool bTutorialVisible = true)
	{
		const float Delta = FMath::Max(0.0f, DeltaSeconds);
		ObjectiveRemaining = FMath::Max(0.0f, ObjectiveRemaining - Delta);
		RecallRemaining = FMath::Max(0.0f, RecallRemaining - Delta);
		if (LastObjective != Objective)
		{
			LastObjective = Objective;
			ObjectiveRemaining = Objective.IsEmpty() ? 0.0f : ObjectiveSeconds;
		}
		if (!bTutorialAllowed) bTutorialFinished = true;
		if (!bTutorialFinished && (bTutorialVisible || bTutorialEnding))
		{
			TutorialRemaining = FMath::Max(0.0f, TutorialRemaining - Delta);
			if (TutorialRemaining <= 0.0f)
			{
				bTutorialFinished = true;
				bTutorialCompleted = true;
			}
		}
	}

	/** 첫 조사를 마쳤다. 더 가르칠 것이 없으니 지금부터 페이드로 내린다. */
	void FinishTutorial()
	{
		if (bTutorialFinished || bTutorialEnding) return;
		bTutorialEnding = true;
		TutorialRemaining = FMath::Min(TutorialRemaining, FadeSeconds);
	}
	/** 이 프로필이 이미 익혔다. 처음부터 보이지 않는다(기억은 새로 남기지 않는다). */
	void SkipTutorial() { bTutorialFinished = true; }
	/** 안내가 끝까지 보였거나 첫 조사로 끝났다. 프로필에 기억해도 된다. */
	bool WasTutorialCompleted() const { return bTutorialCompleted; }
	bool IsTutorialRunning() const { return !bTutorialFinished; }

	void ToggleRecall()
	{
		RecallRemaining = RecallRemaining > 0.0f ? 0.0f : RecallSeconds;
		// 직접 안내를 닫은 뒤 튜토리얼 줄이 뒤에 남지 않는다.
		bTutorialFinished = true;
		ObjectiveRemaining = 0.0f;
	}
	void Interrupt()
	{
		RecallRemaining = ObjectiveRemaining = 0.0f;
		bTutorialFinished = true;
	}
	float ObjectiveAlpha() const { return Alpha(FMath::Max(ObjectiveRemaining, RecallRemaining)); }
	float ControlsAlpha() const { return Alpha(FMath::Max(bTutorialFinished ? 0.0f : TutorialRemaining, RecallRemaining)); }
	bool IsRecalled() const { return RecallRemaining > 0.0f; }
	void DismissRecall() { RecallRemaining = 0.0f; }

private:
	static float Alpha(float Remaining) { return FMath::SmoothStep(0.0f, FadeSeconds, Remaining); }
	FString LastObjective;
	float ObjectiveRemaining = 0.0f;
	float TutorialRemaining = TutorialSeconds;
	float RecallRemaining = 0.0f;
	bool bTutorialFinished = false;
	bool bTutorialEnding = false;
	bool bTutorialCompleted = false;
};
