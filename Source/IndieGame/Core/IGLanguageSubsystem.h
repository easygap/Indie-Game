#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IGLanguageSubsystem.generated.h"

/**
 * 표시 언어. 한국어가 원문이고 영어, 일본어, 중국어 간체와 번체를 싣는다.
 *
 * 처음 켤 때는 운영체제 언어를 따르고, 지원하지 않는 언어면 영어로 둔다.
 * 원문 문화권(ko)으로 떨어지게 두면 프랑스어 운영체제에서 한국어가 나온다.
 * 고른 언어는 프로필(GameUserSettings)에 남긴다.
 */
UCLASS()
class INDIEGAME_API UIGLanguageSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 지원하는 문화권 이름. 설정 화면이 이 순서로 돈다. */
	static const TArray<FString>& GetSupportedCultures();
	/** 그 언어로 쓴 언어 이름. 지금 표시 언어와 상관없이 늘 그 언어로 적는다. */
	static FText GetNativeLanguageName(const FString& Culture);

	FString GetCurrentCulture() const;
	/** Direction만큼 다음 언어로 넘기고 프로필에 저장한다. */
	void CycleCulture(int32 Direction);
	void ApplyCulture(const FString& Culture, bool bPersist);

private:
	static FString ResolveSupportedCulture(const FString& Requested);
	FString LoadSavedCulture() const;
};
