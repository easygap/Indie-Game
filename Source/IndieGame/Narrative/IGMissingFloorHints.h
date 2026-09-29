#pragma once

#include "CoreMinimal.h"

/**
 * 힌트 키가 지금 떠올릴 생각. 뒤로 갈수록 구체적이다. 첫 단계는 어디를 볼지,
 * 다음은 무엇을 볼지, 마지막은 무엇을 할지다.
 */
struct FIGHintStep
{
	/** 진행 단계의 이름. 이것이 바뀌면 힌트는 첫 단계부터 다시 센다. */
	FName GoalId;
	TArray<FText> Tiers;

	bool IsValid() const { return !GoalId.IsNone() && Tiers.Num() > 0; }
};

/**
 * 저장된 진행 상태(진실, 출처, 비트, 퍼즐)만 읽고 지금 걸린 자리를 고른다.
 * 정답 UI를 두지 않는다는 원칙(§19.4)은 그대로다. 힌트는 유담의 속말로만
 * 나오고, 플레이어가 누를 때만 나온다.
 */
namespace IGMissingFloorHints
{
	INDIEGAME_API FIGHintStep Resolve(const UObject* WorldContext);
}
