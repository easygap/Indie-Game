#pragma once

#include "CoreMinimal.h"
#include "Interaction/IGInteractableActor.h"
#include "IGTimeEntryPuzzle.generated.h"

class AIGSecondMorningDirector;
class AIGTimeEntryPuzzle;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class EIGTimeEntryPuzzleId : uint8
{
	None,
	P1Alarm,
	P2Transaction
};

UENUM()
enum class EIGTimeEntryButtonAction : uint8
{
	Hour,
	Minute,
	Confirm
};

/** 시·분·확인 입력을 각각 독립 조사 표면으로 노출한다. */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGTimeEntryButton final : public AIGInteractableActor
{
	GENERATED_BODY()

public:
	AIGTimeEntryButton();

	void Configure(
		AIGTimeEntryPuzzle* InPuzzle,
		EIGTimeEntryButtonAction InAction,
		UStaticMesh* CubeMesh,
		UMaterialInterface* Material,
		const FText& Prompt);
	void SetAvailable(bool bAvailable);
	EIGTimeEntryButtonAction GetAction() const { return Action; }

	virtual void CompleteInteraction_Implementation(
		const FIGInteractionContext& Context) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Time Entry")
	TObjectPtr<UStaticMeshComponent> ButtonMesh;

	UPROPERTY(Transient)
	TObjectPtr<AIGTimeEntryPuzzle> Puzzle;

	EIGTimeEntryButtonAction Action = EIGTimeEntryButtonAction::Hour;
};

/**
 * CH02 P1/P2에서 같은 물리 입력 규칙을 쓰는 24시간 단말이다.
 * 디지털 표시와 세 버튼은 전부 실제 메시이며, 정답 판정만 장 디렉터에 위임한다.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGTimeEntryPuzzle final : public AActor
{
	GENERATED_BODY()

public:
	AIGTimeEntryPuzzle();

	void Configure(
		AIGSecondMorningDirector* InDirector,
		EIGTimeEntryPuzzleId InPuzzleId,
		UStaticMesh* CubeMesh,
		UStaticMesh* AuthoredHousingMesh,
		UMaterialInterface* BodyMaterial,
		UMaterialInterface* DisplayOffMaterial,
		UMaterialInterface* DisplayGlassMaterial,
		UMaterialInterface* AlarmDisplayOnMaterial,
		UMaterialInterface* PosDisplayOnMaterial,
		UMaterialInterface* ButtonMaterial);
	void RestoreState(
		int32 InHour,
		int32 InMinute,
		int32 InWrongAttempts,
		bool bInSolved);
	void SetAvailable(bool bAvailable);
	void HandleButton(EIGTimeEntryButtonAction Action);
	/** Adds one presentation-pressure stage without submitting a wrong answer. */
	bool AdvancePressureStage();
	bool RunAutomatedSolution();

	EIGTimeEntryPuzzleId GetPuzzleId() const { return PuzzleId; }
	int32 GetHour() const { return Hour; }
	int32 GetMinute() const { return Minute; }
	int32 GetWrongAttempts() const { return WrongAttempts; }
	int32 GetPressureStage() const { return WrongAttempts; }
	bool IsSolved() const { return bSolved; }
	bool IsAnswerCorrect() const;
	bool HasPhysicalContract() const;
	int32 GetPhysicalMeshComponentCount() const;
	bool UsesAuthoredHousing() const { return bUsesAuthoredHousing; }
	bool UsesLayeredDisplay() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UStaticMeshComponent* AddPart(
		const TCHAR* BaseName,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& RelativeLocation,
		const FVector& SizeCentimeters);
	UStaticMeshComponent* AddPresentationPart(
		const TCHAR* BaseName,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& RelativeLocation,
		const FVector& SizeCentimeters);
	UStaticMeshComponent* AddAuthoredPresentationMesh(
		const TCHAR* BaseName,
		UStaticMesh* Mesh,
		UMaterialInterface* MaterialOverride,
		const FVector& RelativeBoundsCenter,
		const FRotator& RelativeRotation,
		float UniformScale);
	UInstancedStaticMeshComponent* AddInstancedPart(
		const TCHAR* BaseName,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		bool bCastShadow);
	void BuildAlarmHousing(
		UStaticMesh* CubeMesh,
		UStaticMesh* AlarmMesh,
		UMaterialInterface* BodyMaterial,
		UMaterialInterface* DisplayOffMaterial);
	void BuildPosHousing(
		UStaticMesh* CubeMesh,
		UStaticMesh* PosMesh,
		UMaterialInterface* BodyMaterial,
		UMaterialInterface* DisplayOffMaterial,
		UMaterialInterface* ButtonMaterial);
	AIGTimeEntryButton* SpawnButton(
		EIGTimeEntryButtonAction Action,
		const FVector& RelativeLocation,
		UStaticMesh* CubeMesh,
		UMaterialInterface* Material,
		const FText& Prompt);
	void BuildDisplay(
		UStaticMesh* CubeMesh,
		UMaterialInterface* DisplayOffMaterial);
	void UpdateDisplay();
	void UpdateButtonPrompts();
	void CycleHour();
	void CycleMinute();
	AIGTimeEntryButton* FindButton(EIGTimeEntryButtonAction Action) const;

	UPROPERTY(VisibleAnywhere, Category = "Time Entry")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TArray<FTransform> DisplaySegmentTransforms;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> DisplayOnInstances;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> DisplayOffInstances;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> PosKeypadInstances;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> DisplayBacking;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> DisplayLens;

	/** 그림자와 원본 재질을 유지하는 P1/P2 고유 외형 부품이다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PresentationParts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AIGTimeEntryButton>> Buttons;

	UPROPERTY(Transient)
	TObjectPtr<AIGSecondMorningDirector> Director;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DisplayOffMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DisplayOnMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DisplayGlassMaterial;

	EIGTimeEntryPuzzleId PuzzleId = EIGTimeEntryPuzzleId::None;
	TArray<int32> HourCandidates;
	TArray<int32> MinuteCandidates;
	int32 TargetHour = 0;
	int32 TargetMinute = 0;
	int32 Hour = 4;
	int32 Minute = 44;
	int32 WrongAttempts = 0;
	int32 PartSerial = 0;
	bool bAvailable = false;
	bool bSolved = false;
	bool bUsesAuthoredHousing = false;
};
