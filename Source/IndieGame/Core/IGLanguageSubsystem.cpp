#include "Core/IGLanguageSubsystem.h"

#include "HAL/PlatformMisc.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

namespace IGLanguage
{
	const TCHAR* ConfigSection = TEXT("IndieGame.Language");
	const TCHAR* ConfigKey = TEXT("Culture");
}

void UIGLanguageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 검사와 캡처는 -IGCulture=en 처럼 이번 실행만 언어를 바꾼다. 프로필은 건드리지 않는다.
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("IGCulture="), Override))
	{
		ApplyCulture(ResolveSupportedCulture(Override), false);
		return;
	}
	// 편집기 안에서 돌리는 PIE는 편집기 UI의 언어까지 같이 바꾼다. 게임 실행과
	// 패키지에서만 프로필과 운영체제 언어를 따른다.
	if (GIsEditor && !IsRunningGame())
	{
		return;
	}
	const FString Saved = LoadSavedCulture();
	if (!Saved.IsEmpty())
	{
		ApplyCulture(ResolveSupportedCulture(Saved), false);
		return;
	}
	// 처음 켰다. 운영체제가 쓰는 언어를 따른다.
	ApplyCulture(ResolveSupportedCulture(FPlatformMisc::GetDefaultLanguage()), false);
}

const TArray<FString>& UIGLanguageSubsystem::GetSupportedCultures()
{
	static const TArray<FString> Cultures = {
		TEXT("ko"), TEXT("en"), TEXT("ja"), TEXT("zh-Hans"), TEXT("zh-Hant")
	};
	return Cultures;
}

FText UIGLanguageSubsystem::GetNativeLanguageName(const FString& Culture)
{
	// 언어 이름은 번역하지 않는다. 읽을 수 없는 언어로 켜진 사람도 자기 말은 찾는다.
	if (Culture == TEXT("en")) return INVTEXT("English");
	if (Culture == TEXT("ja")) return INVTEXT("日本語");
	if (Culture == TEXT("zh-Hans")) return INVTEXT("简体中文");
	if (Culture == TEXT("zh-Hant")) return INVTEXT("繁體中文");
	return INVTEXT("한국어");
}

FString UIGLanguageSubsystem::GetCurrentCulture() const
{
	return ResolveSupportedCulture(FInternationalization::Get().GetCurrentLanguage()->GetName());
}

void UIGLanguageSubsystem::CycleCulture(const int32 Direction)
{
	const TArray<FString>& Cultures = GetSupportedCultures();
	const int32 Current = FMath::Max(0, Cultures.IndexOfByKey(GetCurrentCulture()));
	const int32 Step = Direction < 0 ? -1 : 1;
	const int32 Next = (Current + Step + Cultures.Num()) % Cultures.Num();
	ApplyCulture(Cultures[Next], true);
}

void UIGLanguageSubsystem::ApplyCulture(const FString& Culture, const bool bPersist)
{
	FInternationalization& I18N = FInternationalization::Get();
	if (I18N.GetCurrentCulture()->GetName() != Culture)
	{
		I18N.SetCurrentCulture(Culture);
	}
	if (bPersist && GConfig)
	{
		GConfig->SetString(IGLanguage::ConfigSection, IGLanguage::ConfigKey, *Culture, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

FString UIGLanguageSubsystem::ResolveSupportedCulture(const FString& Requested)
{
	const FString Lower = Requested.ToLower();
	if (Lower.StartsWith(TEXT("ko")))
	{
		return TEXT("ko");
	}
	if (Lower.StartsWith(TEXT("ja")))
	{
		return TEXT("ja");
	}
	if (Lower.StartsWith(TEXT("zh")))
	{
		// 대만, 홍콩, 마카오와 번체 표기는 번체로, 나머지 중국어는 간체로 간다.
		const bool bTraditional = Lower.Contains(TEXT("hant"))
			|| Lower.Contains(TEXT("-tw")) || Lower.Contains(TEXT("_tw"))
			|| Lower.Contains(TEXT("-hk")) || Lower.Contains(TEXT("_hk"))
			|| Lower.Contains(TEXT("-mo")) || Lower.Contains(TEXT("_mo"));
		return bTraditional ? TEXT("zh-Hant") : TEXT("zh-Hans");
	}
	// 영어와 지원하지 않는 모든 언어.
	return TEXT("en");
}

FString UIGLanguageSubsystem::LoadSavedCulture() const
{
	FString Saved;
	if (GConfig)
	{
		GConfig->GetString(IGLanguage::ConfigSection, IGLanguage::ConfigKey, Saved, GGameUserSettingsIni);
	}
	return Saved;
}
