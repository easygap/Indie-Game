#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

/**
 * 플레이어가 이미 익힌 조작을 프로필(GameUserSettings)에 기억한다.
 * 한 번 익힌 안내가 불러오기·새 게임마다 다시 뜨면 화면이 계속 말을 건다.
 * 저장 파일이 아니라 프로필에 두는 것은 콘텐츠 경고·소리 보정과 같은 이유다.
 */
namespace IGOnboardingMemory
{
	inline const TCHAR* Section() { return TEXT("IndieGame.Onboarding"); }

	/** 같은 종류의 안내를 이만큼 쓰고 나면 키 이름을 떼고 대상 이름만 남긴다. */
	constexpr int32 PromptLearnedUses = 3;

	/**
	 * 검사와 개발 실행(-IGSkipFrontend, -IGFreshOnboarding)은 프로필에 쓰지 않고
	 * 이번 실행 동안만 기억한다. 검사가 한 번 돌 때마다 안내가 줄어들면 다음 검사가
	 * 다른 화면을 찍는다.
	 */
	inline bool IsSessionOnly()
	{
		static const bool bSessionOnly =
			FParse::Param(FCommandLine::Get(), TEXT("IGFreshOnboarding"))
			|| FParse::Param(FCommandLine::Get(), TEXT("IGSkipFrontend"));
		return bSessionOnly;
	}

	inline TMap<FString, int32>& SessionValues()
	{
		static TMap<FString, int32> Values;
		return Values;
	}

	inline int32 ReadInt(const TCHAR* Key)
	{
		if (IsSessionOnly())
		{
			const int32* Value = SessionValues().Find(Key);
			return Value ? *Value : 0;
		}
		int32 Value = 0;
		if (GConfig)
		{
			GConfig->GetInt(Section(), Key, Value, GGameUserSettingsIni);
		}
		return Value;
	}

	inline void WriteInt(const TCHAR* Key, const int32 Value)
	{
		if (IsSessionOnly())
		{
			SessionValues().Add(Key, Value);
			return;
		}
		if (GConfig)
		{
			GConfig->SetInt(Section(), Key, Value, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	inline bool ReadBool(const TCHAR* Key) { return ReadInt(Key) != 0; }
	inline void WriteBool(const TCHAR* Key, const bool bValue) { WriteInt(Key, bValue ? 1 : 0); }

	/** 입주 저녁의 기본 조작 안내를 끝까지 봤거나 첫 조사를 마쳤다. */
	inline bool IsControlsIntroDone() { return ReadBool(TEXT("ControlsIntroDone")); }
	inline void MarkControlsIntroDone()
	{
		if (!IsControlsIntroDone())
		{
			WriteBool(TEXT("ControlsIntroDone"), true);
		}
	}

	/** Kind: Interact, Knock, Listen. 쓸 때마다 하나씩, 익힌 뒤에는 더 세지 않는다. */
	inline int32 GetPromptUses(const TCHAR* Kind)
	{
		return ReadInt(*FString::Printf(TEXT("PromptUses.%s"), Kind));
	}
	inline bool HasLearnedPrompt(const TCHAR* Kind)
	{
		return GetPromptUses(Kind) >= PromptLearnedUses;
	}
	inline void AddPromptUse(const TCHAR* Kind)
	{
		const int32 Uses = GetPromptUses(Kind);
		if (Uses < PromptLearnedUses)
		{
			WriteInt(*FString::Printf(TEXT("PromptUses.%s"), Kind), Uses + 1);
		}
	}

	/** 문을 길게 눌러 조용히 연 적이 있다. 그 뒤로는 문 안내에서 방법 설명을 뺀다. */
	inline bool IsQuietDoorLearned() { return ReadBool(TEXT("QuietDoorLearned")); }
	inline void MarkQuietDoorLearned()
	{
		if (!IsQuietDoorLearned())
		{
			WriteBool(TEXT("QuietDoorLearned"), true);
		}
	}

	/**
	 * 본 결말. 세이브가 아니라 프로필에 둔다. 새 게임이 자동 저장을 지워도
	 * 다섯째 밤 줄은 남아야 하고, 결말 뒤에는 새 자동 저장을 남기지 않는다.
	 */
	inline bool HasSeenEnding(const FName EndingId)
	{
		return !EndingId.IsNone()
			&& ReadBool(*FString::Printf(TEXT("EndingSeen.%s"), *EndingId.ToString()));
	}
	inline void MarkEndingSeen(const FName EndingId)
	{
		if (!EndingId.IsNone() && !HasSeenEnding(EndingId))
		{
			WriteBool(*FString::Printf(TEXT("EndingSeen.%s"), *EndingId.ToString()), true);
		}
	}

	/** 상황 안내(앉기, 숨 참기 같은 것)를 한 번 끝까지 보여 줬다. 같은 안내는 다시 띄우지 않는다. */
	inline bool WasTipShown(const TCHAR* Tip)
	{
		return ReadBool(*FString::Printf(TEXT("Tip.%s"), Tip));
	}
	inline void MarkTipShown(const TCHAR* Tip)
	{
		if (!WasTipShown(Tip))
		{
			WriteBool(*FString::Printf(TEXT("Tip.%s"), Tip), true);
		}
	}
}
