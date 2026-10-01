#pragma once

#include "CoreMinimal.h"

class UGameInstance;

/** 명시적으로 요청한 배포본 성능 검사에만 연결하는 프레임 기록기. */
class FIGRuntimeProfile
{
public:
	FIGRuntimeProfile();
	~FIGRuntimeProfile();
	void Start(UGameInstance* GameInstance);
	void Stop();
	void SetStage(int32 InStage);

private:
	void RecordFrame();
	void RecordGPUFrames(double ReceivedElapsed);
	void LogActiveLights() const;
	struct FGPUState;
	TUniquePtr<FGPUState> GPUState;
	struct FMemorySample
	{
		TAtomic<uint64> LocalBytes{0};
		TAtomic<bool> Valid{false};
	};
	struct FFrame
	{
		uint64 EngineFrame = 0;
		double Elapsed = 0;
		double FrameMs = 0;
		double WorldSeconds = 0;
		double CommittedMiB = 0;
		double LocalMiB = -1;
		float ScreenPercentage = 100;
		float SecondaryScreenPercentage = 0;
		int32 DynamicResolutionMode = 0;
		int32 AntiAliasing = 0;
		int32 Quality = 0;
		int32 Width = 0;
		int32 Height = 0;
		int32 Stage = -1;
		int32 VSync = 0;
		float FrameLimit = 0;
		bool Paused = false;
	};
	UGameInstance* Owner = nullptr;
	FDelegateHandle FrameHandle;
	FDelegateHandle ExitHandle;
	TArray<FFrame> Frames;
	TSharedPtr<FMemorySample, ESPMode::ThreadSafe> Memory;
	double StartTime = 0;
	double LastFrameTime = 0;
	double NextMemorySampleTime = 0;
	double CommittedMiB = 0;
	FString OutputPath;
	int32 Stage = -1;
	double StageEnteredAt = 0;
	// 개발 빌드 진단용. -IGProfileGPUStages=0,10,13 에 든 장면마다 ProfileGPU를 한 번 남긴다.
	TSet<int32> ProfileGPUStages;
	TSet<int32> ProfiledGPUStages;
};
