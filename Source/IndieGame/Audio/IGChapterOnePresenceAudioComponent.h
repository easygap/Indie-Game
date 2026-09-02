#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "IGChapterOnePresenceAudioComponent.generated.h"

class UAudioComponent;

/**
 * Quiet, spatial traces of ordinary people used only by the first morning.
 *
 * Unit 401's closed-door radio and the convenience-store backroom scrape are
 * deliberately non-verbal. They establish that the building and shop appear
 * occupied in CH01, so their complete removal in CH02 becomes meaningful.
 * The component owns every timer/component it creates and is allocation-free
 * between the sparse cardboard events.
 */
UCLASS(ClassGroup = (IndieGame), meta = (BlueprintSpawnableComponent))
class INDIEGAME_API UIGChapterOnePresenceAudioComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UIGChapterOnePresenceAudioComponent();

	/**
	 * Enables or removes the CH01 human-presence bed. Safe to call repeatedly;
	 * disabling clears future events and stops the radio immediately.
	 */
	void SetPresenceEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void StartPrayerRadio();
	void StartStorePresence();
	void ScheduleNextCardboardDrag(float DelaySeconds);
	void PlayCardboardDrag();

	UFUNCTION()
	void HandleStoryStateChanged(FGameplayTag StateTag, bool bAdded);

	/**
	 * 401호 안, 북향 강철 현관문 뒤. 기본값은 생성자에서 씬이 든
	 * 자리로 채운다 — 챕터 2가 같은 라디오를 소리 내므로 숫자를
	 * 여기 다시 적으면 둘이 갈라진다. 배치에서 만지는 것은 그대로다.
	 */
	UPROPERTY(EditAnywhere, Category = "Chapter One Presence|Placement")
	FVector PrayerRadioWorldLocation = FVector::ZeroVector;

	/** Behind the convenience-store counter, toward the implied stock room. */
	UPROPERTY(EditAnywhere, Category = "Chapter One Presence|Placement")
	FVector StoreBackroomWorldLocation = FVector(2865.0f, -205.0f, 72.0f);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> PrayerRadioComponent;

	FTimerHandle CardboardTimerHandle;
	FGameplayTag EnteredStoreTag;
	int32 CardboardIntervalIndex = 0;
	bool bPresenceEnabled = false;
	bool bCardboardScheduleStarted = false;
};
