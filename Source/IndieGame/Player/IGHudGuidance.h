#pragma once

#include "CoreMinimal.h"

/** 목표는 갱신 때만, 기본 조작은 첫 입주 때만 잠깐 보여 준다. */
struct FIGHudGuidance
{
	static constexpr float ObjectiveSeconds = 7.0f;
	static constexpr float TutorialSeconds = 20.0f;
	static constexpr float RecallSeconds = 10.0f;
	static constexpr float FadeSeconds = 0.45f;

	void Update(float DeltaSeconds, const FString& Objective, bool bTutorialAllowed)
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
		if (!bTutorialFinished)
		{
			TutorialRemaining = FMath::Max(0.0f, TutorialRemaining - Delta);
			bTutorialFinished = TutorialRemaining <= 0.0f;
		}
	}

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
};
