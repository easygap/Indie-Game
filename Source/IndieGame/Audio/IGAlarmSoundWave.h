#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "IGAlarmSoundWave.generated.h"

/**
 * Allocation-stable procedural PCM source for the bedside alarm.
 *
 * The sample cursor is owned exclusively by the audio render thread through
 * OnGeneratePCMAudio; game-thread code should treat this object as immutable.
 */
UCLASS()
class INDIEGAME_API UIGAlarmSoundWave final : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	explicit UIGAlarmSoundWave(const FObjectInitializer& ObjectInitializer);

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	uint64 GeneratedSampleCount = 0;
};
