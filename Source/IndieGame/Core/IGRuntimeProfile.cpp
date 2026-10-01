#include "Core/IGRuntimeProfile.h"

#include "DynamicRHI.h"
#include "Engine/GameInstance.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GPUProfiler.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RenderingThread.h"
#include "RHIStats.h"
#include "Scalability.h"

struct FIGRuntimeProfile::FGPUState
{
	struct FSample
	{
		double ReceivedElapsed = 0;
		double GpuMs = 0;
		int32 Stage = -1;
		bool Disjoint = false;
	};
	FRHIGPUFrameTimeHistory::FState Reader;
	TArray<FSample> Samples;
};

FIGRuntimeProfile::FIGRuntimeProfile() = default;

FIGRuntimeProfile::~FIGRuntimeProfile()
{
	Stop();
}

void FIGRuntimeProfile::Start(UGameInstance* GameInstance)
{
	if (FrameHandle.IsValid() || !GameInstance
		|| !FParse::Param(FCommandLine::Get(), TEXT("IGRuntimeProfile"))) return;
	Owner = GameInstance;
	OutputPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Profiling/MissingFloorRuntime.csv"));
	Frames.Reserve(16384);
	Memory = MakeShared<FMemorySample, ESPMode::ThreadSafe>();
	GPUState = MakeUnique<FGPUState>();
	GPUState->Samples.Reserve(16384);
	StartTime = LastFrameTime = FPlatformTime::Seconds();
#if !UE_BUILD_SHIPPING
	FString ProfileStages;
	if (FParse::Value(FCommandLine::Get(), TEXT("IGProfileGPUStages="), ProfileStages, false))
	{
		TArray<FString> Parts;
		ProfileStages.ParseIntoArray(Parts, TEXT(","));
		for (const FString& Part : Parts) { ProfileGPUStages.Add(FCString::Atoi(*Part)); }
	}
#endif
	FrameHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FIGRuntimeProfile::RecordFrame);
	ExitHandle = FCoreDelegates::OnPreExit.AddRaw(this, &FIGRuntimeProfile::Stop);
}

void FIGRuntimeProfile::SetStage(const int32 InStage)
{
	if (InStage != Stage) { StageEnteredAt = FPlatformTime::Seconds(); }
	Stage = InStage;
}

void FIGRuntimeProfile::RecordFrame()
{
	const double Now = FPlatformTime::Seconds();
#if !UE_BUILD_SHIPPING
	// 장면이 자리 잡은 뒤의 한 프레임을 패스·광원별로 기록한다.
	if (ProfileGPUStages.Contains(Stage) && !ProfiledGPUStages.Contains(Stage) && Now - StageEnteredAt > 0.5 && GEngine)
	{
		ProfiledGPUStages.Add(Stage);
		UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_PROFILEGPU stage=%d"), Stage);
		GEngine->Exec(Owner ? Owner->GetWorld() : nullptr, TEXT("ProfileGPU"));
		LogActiveLights();
	}
#endif
	if (Now >= NextMemorySampleTime)
	{
		// OS와 드라이버 조회는 초당 한 번만 한다. 이 값은 순간 최대치가 아닌 표본이다.
		CommittedMiB = FPlatformMemory::GetStats().UsedVirtual / (1024.0 * 1024.0);
		if (GDynamicRHI)
		{
			const auto SharedMemory = Memory;
			ENQUEUE_RENDER_COMMAND(MissingFloorProfileMemory)(
				[SharedMemory](FRHICommandListImmediate&)
				{
					FRHIMemoryStats Stats;
					RHIGetMemoryStats(Stats);
					SharedMemory->LocalBytes.Store(Stats.UsedLocal);
					SharedMemory->Valid.Store(Stats.IsValid());
				});
		}
		NextMemorySampleTime = Now + 1.0;
	}
	FFrame& Frame = Frames.AddDefaulted_GetRef();
	Frame.EngineFrame = GFrameCounter;
	Frame.Elapsed = Now - StartTime;
	Frame.Stage = Stage;
#if CSV_PROFILER
	// 개발 빌드의 csvprofile에 장면 번호를 같이 남겨 패스별 GPU 시간을 장면별로 나눈다.
	CSV_CUSTOM_STAT_GLOBAL(MissingFloorStage, Stage, ECsvCustomStatOp::Set);
#endif
	Frame.FrameMs = (Now - LastFrameTime) * 1000.0;
	RecordGPUFrames(Frame.Elapsed);
	Frame.CommittedMiB = CommittedMiB;
	Frame.LocalMiB = Memory->Valid.Load() ? Memory->LocalBytes.Load() / (1024.0 * 1024.0) : -1;
	if (const UWorld* World = Owner->GetWorld())
	{
		Frame.WorldSeconds = World->GetTimeSeconds();
		Frame.Paused = World->IsPaused();
	}
	if (const UGameViewportClient* Viewport = Owner->GetGameViewportClient())
	{
		FVector2D Size;
		Viewport->GetViewportSize(Size);
		Frame.Width = FMath::RoundToInt(Size.X);
		Frame.Height = FMath::RoundToInt(Size.Y);
	}
	static const IConsoleVariable* Percentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	static const IConsoleVariable* SecondaryPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport"));
	static const IConsoleVariable* DynamicResolution = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.OperationMode"));
	static const IConsoleVariable* AA = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
	static const IConsoleVariable* VSync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	static const IConsoleVariable* FrameLimit = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
	Frame.ScreenPercentage = Percentage ? Percentage->GetFloat() : -1;
	Frame.SecondaryScreenPercentage = SecondaryPercentage ? SecondaryPercentage->GetFloat() : -1;
	Frame.DynamicResolutionMode = DynamicResolution ? DynamicResolution->GetInt() : -1;
	Frame.AntiAliasing = AA ? AA->GetInt() : -1;
	Frame.VSync = VSync ? VSync->GetInt() : -1;
	Frame.FrameLimit = FrameLimit ? FrameLimit->GetFloat() : -1;
	Frame.Quality = Scalability::GetQualityLevels().GetSingleQualityLevel();
	LastFrameTime = Now;
}

void FIGRuntimeProfile::LogActiveLights() const
{
#if !UE_BUILD_SHIPPING
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World) { return; }
	FVector Eye = FVector::ZeroVector;
	FRotator View;
	if (APlayerController* Controller = World->GetFirstPlayerController()) { Controller->GetPlayerViewPoint(Eye, View); }
	for (const TCHAR* Name : {TEXT("r.Shadow.Virtual.SMRT.RayCountLocal"), TEXT("r.Shadow.Virtual.SMRT.SamplesPerRayLocal"),
		TEXT("r.Shadow.Virtual.OnePassProjection"), TEXT("sg.ShadowQuality"), TEXT("r.Shadow.Virtual.ResolutionLodBiasLocal")})
	{
		if (const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_CVAR stage=%d %s=%s"), Stage, Name, *Variable->GetString());
		}
	}
	for (TObjectIterator<ULocalLightComponent> It; It; ++It)
	{
		const ULocalLightComponent* Light = *It;
		if (!Light || Light->GetWorld() != World || !Light->IsRegistered() || !Light->IsVisible() || Light->Intensity <= 0.f) { continue; }
		const float Distance = FVector::Dist(Eye, Light->GetComponentLocation());
		UE_LOG(LogTemp, Display, TEXT("MISSINGFLOOR_LIGHT stage=%d name=%s owner=%s type=%s shadows=%d intensity=%.1f radius=%.0f distance=%.0f maxdraw=%.0f loc=%s"),
			Stage, *Light->GetName(), Light->GetOwner() ? *Light->GetOwner()->GetName() : TEXT("-"),
			Light->IsA<USpotLightComponent>() ? TEXT("spot") : Light->IsA<UPointLightComponent>() ? TEXT("point") : TEXT("rect"),
			Light->CastShadows ? 1 : 0, Light->Intensity, Light->AttenuationRadius, Distance, Light->MaxDrawDistance,
			*Light->GetComponentLocation().ToCompactString());
	}
#endif
}

void FIGRuntimeProfile::RecordGPUFrames(const double ReceivedElapsed)
{
	// 마지막 값만 반복해서 읽지 않고 새로 도착한 결과를 순서대로 모두 꺼낸다.
	// 엔진의 보관 한도를 넘겨 놓친 결과가 있으면 Disjoint를 원본에 남긴다.
	uint64 Cycles64 = 0;
	for (;;)
	{
		const auto Result = GPUState->Reader.PopFrameCycles(Cycles64);
		if (Result == FRHIGPUFrameTimeHistory::EResult::Empty) { break; }
		auto& Sample = GPUState->Samples.AddDefaulted_GetRef();
		Sample.ReceivedElapsed = ReceivedElapsed;
		Sample.GpuMs = FPlatformTime::ToMilliseconds64(Cycles64);
		// GPU 결과가 도착한 시점의 장면이다. 렌더링한 프레임의 장면 번호는 아니다.
		Sample.Stage = Stage;
		Sample.Disjoint = Result == FRHIGPUFrameTimeHistory::EResult::Disjoint;
	}
}

void FIGRuntimeProfile::Stop()
{
	if (!FrameHandle.IsValid()) return;
	FCoreDelegates::OnEndFrame.Remove(FrameHandle);
	FCoreDelegates::OnPreExit.Remove(ExitHandle);
	FrameHandle.Reset();
	ExitHandle.Reset();
	// 기다리거나 GPU를 동기화하지 않는다. 종료 시 아직 도착하지 않은 결과는 제외된다.
	RecordGPUFrames(FPlatformTime::Seconds() - StartTime);
	FString Csv = TEXT("EngineFrame,ElapsedSeconds,FrameTime,WorldSeconds,Paused,Process/CommittedMiB,GPUMem/LocalUsedMB,ScreenPercentage,SecondaryScreenPercentage,DynamicResolutionMode,AntiAliasingMethod,QualityLevel,Width,Height,VSync,FrameLimit,Stage\n");
	Csv.Reserve(Frames.Num() * 110);
	for (const FFrame& Frame : Frames)
	{
		Csv += FString::Printf(TEXT("%llu,%.6f,%.6f,%.6f,%d,%.3f,%.3f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%.2f,%d\n"),
			static_cast<unsigned long long>(Frame.EngineFrame), Frame.Elapsed, Frame.FrameMs, Frame.WorldSeconds, Frame.Paused ? 1 : 0,
			Frame.CommittedMiB, Frame.LocalMiB, Frame.ScreenPercentage, Frame.SecondaryScreenPercentage,
			Frame.DynamicResolutionMode, Frame.AntiAliasing,
			Frame.Quality, Frame.Width, Frame.Height, Frame.VSync, Frame.FrameLimit, Frame.Stage);
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);
	FFileHelper::SaveStringToFile(Csv, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FString GPUCsv = TEXT("SampleIndex,ReceivedElapsedSeconds,GPUTime,Stage,Disjoint\n");
	GPUCsv.Reserve(GPUState->Samples.Num() * 50);
	for (int32 Index = 0; Index < GPUState->Samples.Num(); ++Index)
	{
		const auto& Sample = GPUState->Samples[Index];
		GPUCsv += FString::Printf(TEXT("%d,%.6f,%.6f,%d,%d\n"), Index,
			Sample.ReceivedElapsed, Sample.GpuMs, Sample.Stage, Sample.Disjoint ? 1 : 0);
	}
	FFileHelper::SaveStringToFile(GPUCsv, *FPaths::ChangeExtension(OutputPath, TEXT("gpu.csv")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const FString Metadata = FString::Printf(
		TEXT("cpu=%s\ngpu=%s\ndriver=%s\nos=%s\nrhi=%s\nframes=%d\nram_mib=%.0f\nmemory_sampling_seconds=1\ngpu_timing=history_queue\ngpu_samples=%d\ngpu_stage=receipt_time\ngpu_tail_waited=0\n"),
		*FPlatformMisc::GetCPUBrand(), *GRHIAdapterName, *GRHIAdapterUserDriverVersion,
		*FPlatformMisc::GetOSVersion(), GDynamicRHI ? GDynamicRHI->GetName() : TEXT("unavailable"), Frames.Num(),
		FPlatformMemory::GetConstants().TotalPhysical / (1024.0 * 1024.0), GPUState->Samples.Num());
	FFileHelper::SaveStringToFile(Metadata, *(OutputPath + TEXT(".txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	Frames.Empty();
	GPUState.Reset();
	Memory.Reset();
	Owner = nullptr;
}
