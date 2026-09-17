#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IGAlarmClock.h"
#include "Interaction/IGGetUpInteractable.h"
#include "Sequence/IGWakeUpDirector.h"
#include "IGPrologueWorldScene.generated.h"

class AIGCheckoutCounter;
class APawn;
class AIGDemoDirector;
class AIGElevator;
class AIGFridge;
class AIGInspectable;
class AIGApartmentStoryDressing;
class AIGChapterOneIncidentDirector;
class AIGChapterOneIncidentAction;
class AIGChapterTwoHumanGateDirector;
class AIGItemContinuityDressing;
class AIGMorningRoutineDirector;
class AIGNeighborhoodLifeDirector;
class AIGPickupItem;
class AIGReadableNote;
class AIGSecondMorningDirector;
class AIGSlidingDoor;
class AIGStairTransition;
class AIGSwingDoor;
class AIGThirdMorningDirector;
class AIGZoneTrigger;
class UAudioComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInterface;
class UPointLightComponent;
class UPostProcessComponent;
class USceneComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UIGSettledDustComponent;
class UInstancedStaticMeshComponent;
class UIGAlarmSoundWave;
enum class EIGRebirthPurchaseProfile : uint8;

/** Prototype-only alarm subclass that exposes protected presentation components safely. */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGPrologueAlarmClock final : public AIGAlarmClock
{
	GENERATED_BODY()

public:
	/**
	 * Builds the clock radio. InBodyMesh is the authored beveled shell (real
	 * centimeters); InPanelMesh is a unit cube used for the LED face plate.
	 */
	void ConfigurePrototype(
		UStaticMesh* InBodyMesh,
		UStaticMesh* InPanelMesh,
		UMaterialInterface* InBodyMaterial,
		UMaterialInterface* InDisplayMaterial,
		UMaterialInterface* InButtonMaterial,
		USoundBase* InAlarmSound);

private:
	UStaticMeshComponent* AddPart(
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& RelativeLocation,
		const FVector& SizeCentimeters);

	int32 PartCounter = 0;
};

/** Prototype-only director adapter; production content will configure the base class in a Blueprint. */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGPrologueWakeDirector final : public AIGWakeUpDirector
{
	GENERATED_BODY()

public:
	void ConfigurePrototype(AIGAlarmClock* InAlarmClock);
};

/** Traceable pillow target used for the contextual Get Up interaction. */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGPrologueGetUpTarget final : public AIGGetUpInteractable
{
	GENERATED_BODY()

public:
	AIGPrologueGetUpTarget();
	void ConfigurePrototype(
		AIGWakeUpDirector* InWakeUpDirector,
		UStaticMesh* InTargetMesh,
		UMaterialInterface* InTargetMaterial);

private:
	UPROPERTY(VisibleAnywhere, Category = "Prototype")
	TObjectPtr<UStaticMeshComponent> TargetMesh;
};

/**
 * Self-contained greybox world for the prologue morning:
 * a 5-pyeong basement studio, the pre-dawn alley outside and the glowing
 * convenience store that terminates it. Geometry is assembled at BeginPlay
 * from Engine basic shapes; nothing here Ticks after construction settles.
 *
 * Runtime flow: alarm wake-up -> empty fridge -> wallet -> alley ->
 * store -> water bottle -> self checkout, coordinated by the wake and
 * morning-routine directors this scene spawns and wires together.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGPrologueWorldScene : public AActor
{
	GENERATED_BODY()

public:
	/**
	 * 403호가 앉은 4층 슬래브의 월드 Z. 도로에서 세 층 위다.
	 *
	 * 밤 디렉터들이 이 숫자를 각자 다시 적고 있었다. 씬의 좌표 이름공간이
	 * .cpp 안에 있어 밖에서 볼 수 없었기 때문인데, 그 상태로 슬래브를 옮기면
	 * 문·노크·순찰 높이가 옛 층에 남는다. 건물의 사실은 건물이 하나만 든다.
	 */
	static constexpr float FourthFloorZ = 900.0f;

	/**
	 * 403호 현관문이 선 자리(cm). 남쪽 벽이 Y=-225라 집 안은 Y가
	 * 0에 가까운 쪽, 복도는 그 반대쪽이다.
	 *
	 * 성분으로 내는 것은 FVector의 세 인자 생성자가 constexpr이
	 * 아니어서다. 밤 2가 이 문에서 노크·문구멍·인물 자리를 재므로
	 * 정적 초기화 순서에 걸리지 않는 값이어야 한다.
	 */
	static constexpr float HomeDoorX = 78.0f;
	static constexpr float HomeDoorY = -225.0f;
	static constexpr float WideDoorLeafWidth = 106.0f;
	static constexpr float WideDoorClearWidth = 108.0f;

	/** 실제 배치된 벽과 문에 플레이어 캡슐을 통과시킨다. 시작 구간 검사에서 사용한다. */
	bool AuditPlayerClearance(APawn* Pawn, AIGSwingDoor* BoothDoor);

	/** 종이는 책상 메시의 실제 상판 높이에 놓는다. */
	float GetDeskSurfaceWorldZ() const { return DeskSurfaceWorldZ; }

	/**
	 * 복도 동쪽 끝 설비 벽장. §5.1의 세 번째 험 존이 여기 붙는다.
	 *
	 * 보일러실이라는 방은 이 게임에 없다 — §6의 공간 표에 행이 없고 배관
	 * 샤프트 설명에 한 번 나올 뿐이다. 배전반과 같은 방식으로 벽에 붙인
	 * 벽장이며, 문도 창도 없는 X 475~690 구간에 선다.
	 */
	static FVector GetBoilerCupboardLocation();

	/**
	 * 5층 베이 위를 지나는 공용 라이저.
	 *
	 * 밤 4의 망치가 「움직이는 라이저가 먼저 자기를 부른다」고
	 * 소리를 내는 자리이자, §32 자비 디렉터의 배관 울음이 나는
	 * 자리다. 둘이 같은 배관이어야 그 울음이 「이 건물에 물이 있고
	 * 움직인다」는 같은 말을 한다.
	 */
	static constexpr float SharedRiserX = 310.0f;
	static constexpr float SharedRiserY = 700.0f;
	static constexpr float SharedRiserZ = 1300.0f;
	static FVector GetSharedRiserLocation()
	{
		return FVector(SharedRiserX, SharedRiserY, SharedRiserZ);
	}

	/**
	 * 플레이어가 처음 눈뜨는 자리. 캡처 투어가 여기로 보내는데, 좌표를
	 * 투어 쪽에 적어 두면 침대를 옮겼을 때 투어만 옛 방을 찍는다.
	 */
	static FVector GetPlayerStartLocation();

	/**
	 * 401호 라디오의 월드 자리.
	 *
	 * 한동안 챕터 2 게이트가 지역 좌표로 들고 자기 액터 트랜스폼을
	 * 걸었고, 챕터 1 프레즌스 컴포넌트는 같은 숫자를 월드로 바로
	 * 썼다. 둘 다 원점에 있어 결과가 같았을 뿐이다. 같은 라디오는
	 * 한 좌표계로 적는다.
	 */
	static FVector GetRadio401Location();

	/**
	 * 로비 CCTV 모니터 화면의 실치수(cm).
	 *
	 * 케이스와 4분할 발광면은 씬이 세우고, 채널 5의 렌더 면은
	 * AIGCctvChannelFive가 세운다. 같은 화면이다. 치수를 각자 적어
	 * 두면 모니터를 키웠을 때 채널 5만 옛 크기로 남아, 화면 안에
	 * 화면이 뜬다.
	 */
	static constexpr float CctvScreenWidth = 34.0f;
	static constexpr float CctvScreenHeight = 25.5f;
	static constexpr float CctvScreenCenterZ = 105.5f;

	AIGPrologueWorldScene();

protected:
	virtual void BeginPlay() override;

public:
	// --- chapter control surface -------------------------------------------
	// The scene owns every piece of geometry and lighting in the prologue, and
	// nothing outside it should reach into that. Chapter directors drive the
	// world through this small, explicit set of verbs instead.

	/**
	 * Hands the west corridor fixture to a chapter director.
	 *
	 * The prologue runs a 10 Hz flicker timer on that fixture for the whole
	 * session; while suspended it leaves the light alone.
	 */
	void SuspendCorridorFlicker(bool bSuspend);

	/**
	 * Lights or kills one ceiling fixture, disc included.
	 *
	 * Corridor fixtures are indexed west to east (four of them); lobby
	 * fixtures are mailboxes-then-lift (two). Out-of-range indices are
	 * ignored rather than fatal — callers are chapter scripts, not code that
	 * should have to know how many lamps a corridor has.
	 */
	void SetFixtureLive(int32 Index, bool bLive, bool bCorridor);
	FVector GetCorridorFixtureLocation(int32 Index) const;

	/** Reveals or parks the pre-built second-morning set without reallocating it. */
	void SetChapterTwoOverlayVisible(bool bVisible);

	/** Wet prints appear only at the end of the interrupted 2F lift stop. */
	void RevealElevatorFootprints();

	/** Drops the duplicate 04:44 receipt after the second checkout. */
	void RevealSecondReceipt();

	/** Arms the 4F approach-to-404 volume after C3 has two independent truths. */
	void SetChapterTwoReturnZoneArmed(bool bArmed);

	/** Kills/restores the two north-side sales-floor luminaires and diffusers. */
	void SetStoreNorthLightsLive(bool bLive);

	/** CH02 ending: relight 4F, sound the distant alarm, fade, and show CH03 card. */
	void FinishChapterTwo();

	/** Rebuilds the persisted CH01 cat-water trace for CH02 and direct save loads. */
	void RefreshChapterTwoCatWaterAftermath();
	bool ValidateChapterTwoCatWaterAftermath() const;

	int32 GetCorridorFixtureCount() const { return CorridorLights.Num(); }
	int32 GetLobbyFixtureCount() const { return LobbyLights.Num(); }

	/**
	 * 없는 층 '그 시간' (§1): seals or releases the building envelope.
	 *
	 * Two seals are needed, not one. The common entrance is the obvious door,
	 * but the ground-floor stair mouth opens into the open pilotis car park, so
	 * a player who takes the stairs down walks out to the alley without ever
	 * touching the entrance. The connector gate closes that hole.
	 *
	 * Sealing is expressed physically — a shut leaf, a dead lift button, a
	 * shutter across the passage — never as a HUD notice.
	 */
	void SetTheHourSealed(bool bSealed);

	/**
	 * §11 V2 403호 3단계 노화. Stage 0 is the prologue, when the flat is simply
	 * a flat; stage 2 is night four, with the ceiling corner cracked and the
	 * damp down the east wall. Nothing here is interactive or story-gated — it
	 * is the building getting worse while she lives in it, and the only player
	 * who ever notices is the one who looks up twice.
	 */
	UFUNCTION(BlueprintCallable, Category = "Missing Floor")
	void SetUnit403AgeStage(int32 Stage);

	UFUNCTION(BlueprintPure, Category = "Missing Floor")
	int32 GetUnit403AgeStage() const { return Unit403AgeStage; }

	/** Planes actually revealed at the current stage. Contract and diagnostics. */
	int32 GetUnit403AgingPlaneCount() const;

	UFUNCTION(BlueprintPure, Category = "Story|Night")
	bool IsTheHourSealed() const { return bTheHourSealed; }

	/** True once BuildLobby has produced the connector gate the seal needs. */
	bool HasNightSealGeometry() const { return StairCoreNightGate != nullptr; }

	/** P1 fixtures, so the puzzle's director can dress them without rebuilding. */
	UStaticMeshComponent* GetFifthMeterDisc() const { return FifthMeterDisc; }
	void AdvanceUtilityMeters(float Degrees, bool bUnnamedPowered, bool bCommonPowered);
	float GetUtilityMeterUpdateInterval() const;
	UStaticMeshComponent* GetUnnamedBreakerToggle() const { return UnnamedBreakerToggle; }
	UStaticMeshComponent* GetCommonBreakerToggle() const { return CommonBreakerToggle; }
	void SetCommonInspectionLightsEnabled(bool bEnabled);
	bool AreCommonInspectionLightsOff() const;

	/**
	 * 없는 층 밤1: slides the stair teleport west so the 3.5F half-landing
	 * becomes a walkable viewing pocket instead of being swallowed by the
	 * portal. Off restores the legacy trigger position byte-for-byte, so the
	 * chapters that never meet the night keep their exact traversal.
	 */
	void SetNightStairPocketEnabled(bool bEnabled);

	/**
	 * Release topology for 없는 층 night 3. The path is built from real
	 * collision surfaces: fourteen upper treads, a roof threshold, and a
	 * 6.4 m L-shaped maintenance lane. This deliberately validates structure,
	 * not a teleport trigger hidden behind a door.
	 */
	bool ValidateMissingFloorRooftopRoute(
		float& OutCenterlineLengthCentimeters,
		int32& OutUpperStepCount) const;

	/** Night 4 breaker cut; keeps geometry and flashlight independent. */
	void SetMissingFloorAnnexPower(bool bPowered);

	/**
	 * §14 CCTV 채널 5's vantage, owned by the world rather than by the beat.
	 *
	 * The camera housing is permanent annex dressing because §17 asks the player
	 * to stand here in 밤3 and recognise the frame they were shown in 밤2. The
	 * scene capture borrows these three values so the live channel and the prop
	 * can never disagree about where the shot is taken from.
	 */
	UStaticMeshComponent* GetMissingFloorCctvCamera() const
	{
		return MissingFloorCctvCamera;
	}
	FVector GetMissingFloorCctvCameraLocation() const;
	FRotator GetMissingFloorCctvCameraRotation() const;
	float GetMissingFloorCctvFieldOfView() const;

	/** Removes only the cavity-facing gypsum panel, never the structural studs. */
	bool OpenMissingFloorCavity();
	/** 밤 4 재시도 때 탈착 패널의 외형과 충돌을 함께 복구한다. */
	bool ResetMissingFloorCavity();
	bool IsMissingFloorCavityOpen() const { return bMissingFloorCavityOpen; }

	/**
	 * Knocks the corridor extinguisher off its bracket. The prop is kinematic
	 * its whole life until this call — zero simulation cost at rest — and it
	 * freezes again once settled. Returns false when already dropped.
	 */
	bool DropCorridorExtinguisher();
	bool IsCorridorExtinguisherDropped() const { return bCorridorExtinguisherDropped; }
	FVector GetCorridorExtinguisherLocation() const;

	/**
	 * 냉장고가 실제로 서 있는 자리. §5.1의 험 존이 여기 붙는다.
	 * 좌표를 다른 파일에 한 번 더 적으면 냉장고만 옮겨지고 험은
	 * 옛 자리에서 계속 운다.
	 */
	FVector GetFridgeLocation() const;

private:
	// --- assembly helpers -------------------------------------------------
	UStaticMeshComponent* CreateBlock(
		const FVector& Center,
		const FVector& SizeCentimeters,
		UMaterialInterface* Material,
		bool bEnableCollision = true,
		UStaticMesh* MeshOverride = nullptr,
		const FRotator& Rotation = FRotator::ZeroRotator,
		USceneComponent* Parent = nullptr);
	/**
	 * 인쇄면을 몸통에서 떼어 낸 블록. 몸통을 돌려준다.
	 *
	 * 엔진 큐브는 여섯 면이 같은 UV를 쓴다. 그래서 두꺼운 몸통에 인쇄 재질을
	 * 그대로 주면 정면뿐 아니라 옆면·윗면·뒷면에도 같은 글자가 눌려 찍힌다.
	 * 소화전함 9 cm 마구리에 「소화전」이 한 번 더 나오고, 계량기함 6 cm
	 * 옆면에 「분전반」이 세로로 눌려 있던 것이 그것이다.
	 *
	 * PrintFacing은 월드 단위축이어야 한다. bPrintBothFaces는 돌출 간판처럼
	 * 양쪽에서 읽는 것에 쓴다.
	 */
	UStaticMeshComponent* CreatePrintedBlock(
		const FVector& Center,
		const FVector& SizeCentimeters,
		UMaterialInterface* BodyMaterial,
		UMaterialInterface* PrintMaterial,
		const FVector& PrintFacing,
		bool bEnableCollision = false,
		bool bPrintBothFaces = false);
	UStaticMeshComponent* CreatePhysicsProp(
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& Scale,
		const FVector& Location,
		const FRotator& Rotation,
		float MassKg);
	UPointLightComponent* CreateLight(
		const FVector& Location,
		float Intensity,
		float Radius,
		const FLinearColor& Color,
		bool bCastShadows,
		float SourceRadius = 0.0f,
		USceneComponent* Parent = nullptr,
		bool bDownlight = false);
	UAudioComponent* CreateAmbientBed(
		USoundBase* Sound,
		const FVector& Location,
		float Volume,
		float InnerRadius,
		float FalloffDistance);

	/** Non-colliding dressing mesh parented to an existing component. */
	UStaticMeshComponent* CreateDecoOnComponent(
		USceneComponent* Parent,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& RelativeLocation,
		const FRotator& RelativeRotation,
		const FVector& RelativeScale);

	/** Loads the generated textured materials; missing entries fall back to flats. */
	void LoadTexturedMaterials();
	UMaterialInterface* TexMat(FName MaterialName, UMaterialInterface* Fallback) const;

	/** Resolves an imported photogrammetry prop's static mesh, or nullptr. */
	UStaticMesh* FindPhotoPropMesh(const TCHAR* AssetId) const;

	/**
	 * Returns a hand-authored prop mesh from /Game/Meshes (built by
	 * Scripts/generate_meshes.py), or Fallback when it has not been generated.
	 */
	UStaticMesh* PropMesh(const TCHAR* MeshName, UStaticMesh* Fallback = nullptr) const;

	/**
	 * Places an authored prop mesh at real scale. These meshes are modelled in
	 * centimeters with their base at the origin, so BaseLocation is where the
	 * object rests. Returns nullptr when the mesh has not been generated.
	 */
	UStaticMeshComponent* CreateProp(
		const TCHAR* MeshName,
		const FVector& BaseLocation,
		UMaterialInterface* Material,
		float YawDegrees = 0.0f,
		float UniformScale = 1.0f,
		bool bEnableCollision = false);

	/**
	 * Adds one non-interactive store item to a mesh/material batch. Store stock
	 * has no gameplay collision and may fade once it is too small to read.
	 */
	bool AddStoreStockInstance(
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FTransform& RelativeTransform,
		bool bCastShadow);

	/** Adds an authored store prop at real scale; returns false if it is absent. */
	bool AddStoreStockProp(
		const TCHAR* MeshName,
		const FVector& BaseLocation,
		UMaterialInterface* Material,
		float YawDegrees = 0.0f,
		float UniformScale = 1.0f,
		bool bCastShadow = true);

	/** Adds a cube fallback to the same store-stock batching path. */
	void AddStoreStockBlock(
		const FVector& Center,
		const FVector& SizeCentimeters,
		UMaterialInterface* Material,
		bool bCastShadow,
		const FRotator& Rotation = FRotator::ZeroRotator);

	/** Wraps a printed, non-shadowing label around one batched bottle. */
	void AddStoreStockBottleLabel(
		const FVector& BottleBase,
		float Radius,
		float BandBottomZ,
		float BandHeight,
		const TCHAR* LabelMaterialName,
		float YawDegrees);

	/**
	 * Adds one batched cup ramyeon: foam cup, printed sleeve, foil lid.
	 *
	 * Three instances rather than one, because the cup mesh has a single material
	 * slot and the lid is unioned into it — texturing the mesh with the label
	 * smears the artwork across the foil and the base.
	 */
	void AddStoreStockCup(const FVector& BaseLocation, float YawDegrees);

	/** Registers completed batches once, after all instances have been added. */
	void FinalizeStoreStockBatches();

	/** Runtime release contract for draw-call batching and per-instance culling. */
	bool ValidateStoreStockBatches(
		int32& OutBatchCount,
		int32& OutInstanceCount) const;

	/**
	 * Places a scanned prop uniformly scaled to fit TargetSize, resting on
	 * FloorCenter.Z. Returns nullptr (leaving the greybox fallback to the
	 * caller) when the prop was not imported.
	 */
	UStaticMeshComponent* PlacePhotoProp(
		const TCHAR* AssetId,
		const FVector& FloorCenter,
		const FVector& TargetSize,
		float YawDegrees,
		bool bEnableCollision = true);

	/**
	 * Places furniture at audited real-world dimensions on all three axes.
	 *
	 * Photo scans such as the two-metre office desk cannot be uniformly shrunk
	 * to fit a compact room without also becoming coffee-table height. Use this
	 * only where support height and seated ergonomics are the gameplay contract;
	 * decorative scans continue through the aspect-preserving helper above.
	 */
	UStaticMeshComponent* PlacePhotoPropExactSize(
		const TCHAR* AssetId,
		const FVector& FloorCenter,
		const FVector& TargetSize,
		float YawDegrees,
		bool bEnableCollision = true);

	UStaticMeshComponent* PlacePhotoPropInternal(
		const TCHAR* AssetId,
		const FVector& FloorCenter,
		const FVector& TargetSize,
		float YawDegrees,
		bool bEnableCollision,
		bool bPreserveAspectRatio);

	// --- construction stages ---------------------------------------------
	void InitializePrologue();
	bool PositionPlayer();
	void BuildApartment();

	/** Shows the aging planes the current stage has reached, hides the rest. */
	void ApplyUnit403AgeStage();
	void BuildCorridor();
	void BuildChapterTwoOverlay();
	void BuildLobby();
	/**
	 * 없는 층 밤3: extends the real 4F stair to the roof, builds the 6.4 m
	 * tank-side lane, and places the half-finished annex on the same slab.
	 */
	void BuildFifthFloorAnnex();
	void BuildAlley();
	void BuildStore();
	void BuildSkyAndFog();
	void SpawnInteractables();
	void SpawnStairTransition();
	void SpawnChapterTwoInteractables();
	void SpawnChapterTwoItemContinuityDressing();
	void AddStaticPurchaseBagProxy(
		AIGPickupItem* WaterBottle,
		EIGRebirthPurchaseProfile PurchaseProfile);
	void RefreshPurchaseProfilePresentation();
	void SpawnDirectors();
	void CreateAmbience();
	void SpawnDemoDirectorIfRequested();
	void SpawnChapterOneIncident();
	void SpawnReturnBoundary();
	void ReconcileLoadedCheckpoint();
	void BeginChapterTwoTransition();
	void EnterChapterTwo();
	void EnterChapterThree();
	void StartRebirthEndToEndValidation();
	void ContinueRebirthEndToEndChapterTwo();
	void FailRebirthEndToEndValidation(const TCHAR* Reason) const;
	void StartChapterTwoCaptureSequence();
	void CaptureNextChapterTwoFrame();
	void FinishChapterTwoCaptureSequence();

	UFUNCTION()
	void HandleStoryStateChanged(FGameplayTag StateTag, bool bAdded);

	UFUNCTION()
	void HandleReturnBoundaryTriggered(AIGZoneTrigger* Zone);

	UFUNCTION()
	void HandleChapterOneMemoryBoundaryCompleted();

	UFUNCTION()
	void HandleElevatorReturnedToFourthFloor(AIGElevator* ReturnedElevator);

	UFUNCTION()
	void HandleStairTransitionCompleted(bool bGoingDown);

	/** Grants the torch to the player pawn once the pickup is taken. */
	UFUNCTION()
	void HandleFlashlightPickedUp(AIGPickupItem* Item);

	/** Refreshes price, receipt, and bag presentation after a profile swap. */
	UFUNCTION()
	void HandlePurchaseSelectionChanged(AIGPickupItem* Item);

	UFUNCTION()
	void HandleFlickerZoneTriggered(AIGZoneTrigger* Zone);

	/** Aging-ballast shimmer on one corridor fluorescent. */
	void HandleCorridorFlicker();

	// --- components -------------------------------------------------------
	UPROPERTY(VisibleAnywhere, Category = "Prologue|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Prologue|Components")
	TArray<TObjectPtr<UStaticMeshComponent>> GeometryComponents;

	/**
	 * Dense, non-interactive convenience-store stock. A material/mesh pair owns
	 * one component instead of one UObject and draw submission per item.
	 */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> StoreStockBatches;

	UPROPERTY(VisibleAnywhere, Category = "Prologue|Lighting")
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	UPROPERTY(VisibleAnywhere, Category = "Prologue|Lighting")
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> AmbientBeds;

	// --- assets -----------------------------------------------------------
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> PlaneMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> ConeMesh;

	/** Textured PBR materials generated by Scripts/create_textured_materials.py. */
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInterface>> TexturedMaterials;

	/** Lathed/beveled prop meshes generated by Scripts/generate_meshes.py. */
	UPROPERTY(Transient) mutable TMap<FName, TObjectPtr<UStaticMesh>> PropMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Prologue|Atmosphere")
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	UPROPERTY(VisibleAnywhere, Category = "Prologue|Atmosphere")
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Prologue|Atmosphere")
	TObjectPtr<USkyLightComponent> SkyAmbient;

	UPROPERTY(VisibleAnywhere, Category = "Prologue|Atmosphere")
	TObjectPtr<UDirectionalLightComponent> PreDawnSun;

	UPROPERTY(VisibleAnywhere, Category = "Prologue|Atmosphere")
	TObjectPtr<UDirectionalLightComponent> MoonLight;

	/** Everything on the 4th floor (unit 403 + its corridor) hangs off this. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> UpperFloorRoot;

	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WallMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FloorMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WoodMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BeddingMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> DoorMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AlarmMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AsphaltMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ConcreteMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ConcreteDarkMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> StoreFloorMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> LightPanelMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SignMintMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SignWhiteMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> GlassMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MetalFrameMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FridgeBodyMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FridgeInteriorMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> PlasticDarkMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> TrashBagMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CardboardMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WaterBlueMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BottleGreenMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BottleBrownMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SnackRedMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SnackYellowMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SnackBlueMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CupNoodleMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WindowGlowMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WindowDarkMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> NightSkyMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> StreetLampGlowMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WalletBrownMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CoolerBodyMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CounterTopMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ScreenGlowMaterial;

	// --- spawned actors ---------------------------------------------------
	UPROPERTY(Transient) TObjectPtr<UIGAlarmSoundWave> AlarmSound;
	UPROPERTY(Transient) TObjectPtr<AIGPrologueAlarmClock> AlarmClock;
	UPROPERTY(Transient) TObjectPtr<AIGPrologueWakeDirector> WakeDirector;
	UPROPERTY(Transient) TObjectPtr<AIGPrologueGetUpTarget> GetUpTarget;
	UPROPERTY(Transient) TObjectPtr<AIGFridge> Fridge;
	UPROPERTY(Transient) TObjectPtr<AIGApartmentStoryDressing> ApartmentStoryDressing;
	UPROPERTY(Transient) TObjectPtr<AIGSwingDoor> HomeDoor;
	UPROPERTY(Transient) TObjectPtr<AIGSwingDoor> BuildingDoor;
	UPROPERTY(Transient) TObjectPtr<AIGElevator> Elevator;
	UPROPERTY(Transient) TObjectPtr<AIGStairTransition> StairTransition;
	UPROPERTY(Transient) TObjectPtr<AIGSlidingDoor> StoreDoor;
	UPROPERTY(Transient) TObjectPtr<AIGCheckoutCounter> Checkout;
	/** CH01 checkout scan; CH02 replaces it with the interactive P2 terminal. */
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> StoreCashRegisterVisual;
	UPROPERTY(Transient) TObjectPtr<AIGChapterOneIncidentDirector> ChapterOneIncidentDirector;
	UPROPERTY(Transient) TObjectPtr<AIGMorningRoutineDirector> MorningDirector;
	UPROPERTY(Transient) TObjectPtr<AIGNeighborhoodLifeDirector> NeighborhoodLifeDirector;
	UPROPERTY(Transient) TObjectPtr<AIGSecondMorningDirector> SecondMorningDirector;
	UPROPERTY(Transient) TObjectPtr<AIGChapterTwoHumanGateDirector> ChapterTwoHumanGateDirector;
	UPROPERTY(Transient) TObjectPtr<AIGItemContinuityDressing> ChapterTwoItemContinuityDressing;
	UPROPERTY(Transient) TObjectPtr<AIGChapterOneIncidentAction> ChapterTwoCatWaterAftermath;
	UPROPERTY(Transient) TObjectPtr<AIGThirdMorningDirector> ThirdMorningDirector;
	UPROPERTY(Transient) TObjectPtr<AIGDemoDirector> DemoDirector;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> ChapterOneApartmentExitZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> LeftHomeZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> FlickerZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> StoreEntryZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> ReturnBoundaryZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> ChapterTwoLeftHomeZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> ChapterTwoOutdoorZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> MirrorSightZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> MirrorEntryZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> ChapterTwoStoreEntryZone;
	UPROPERTY(Transient) TObjectPtr<AIGZoneTrigger> ChapterTwoReturnZone;
	UPROPERTY(Transient) TArray<TObjectPtr<AIGPickupItem>> WaterBottles;
	UPROPERTY(Transient) TObjectPtr<AIGPickupItem> Wallet;
	UPROPERTY(Transient) TArray<TObjectPtr<AIGPickupItem>> ChapterTwoWaterBottles;
	UPROPERTY(Transient) TObjectPtr<AIGPickupItem> ChapterTwoWallet;

	// Ceiling fixtures, kept so a later chapter can put them out one at a
	// time. Each fixture is TWO things: the point light and the emissive disc
	// under it. Killing only the light leaves a bright ring hanging on a
	// black ceiling, which reads as a rendering bug rather than a power cut.
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> CorridorLights;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> CorridorLightDiscs;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> LobbyLights;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> LobbyLightDiscs;
	UPROPERTY(Transient) TObjectPtr<AIGPickupItem> Flashlight;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> ManagementNotice;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> ChapterOneReceipt;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> ExistingReceipt;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> DuplicateReceipt;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> MirrorAlarmMemo;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> MailboxBills;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> OfferingNote;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> NightRoster;
	/**
	 * 없는 층: the roller shutter across the lobby-to-stair-core connector.
	 * Built hidden and non-colliding; SetTheHourSealed is the only thing that
	 * ever shows it, so the legacy chapters never meet it.
	 */
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> StairCoreNightGate;
	/** P1: the fifth meter's dial, which never turns, and its dead breaker. */
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FifthMeterDisc;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> UtilityMeterDiscs;
	TArray<float> UtilityMeterPhases;

	/** §14 CCTV 채널 5's housing, high in the annex's south-west corner. */
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> MissingFloorCctvCamera;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> UnnamedBreakerToggle;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> CommonBreakerToggle;
	/** 밤1: the corridor extinguisher, kinematic until its scripted fall. */
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> CorridorExtinguisher;
	bool bCorridorExtinguisherDropped = false;
	FTimerHandle ExtinguisherSettleTimer;
	UPROPERTY(Transient) TObjectPtr<AIGSwingDoor> MirrorRoomDoor;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> MirrorRoomLamp;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> MirrorRoomBounce;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> OfferingLight;

	/** CH01 wall plug masks the future 403 doorway until the first loop ends. */
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> ChapterOneMaskComponents;

	/** Hidden-at-start CH02 dressing. Colliders are restored from a separate list. */
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> ChapterTwoOverlayComponents;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> ChapterTwoCollisionComponents;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> ElevatorFootprintComponents;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> StoreLightDiscs;

	/** Set while a chapter owns the west corridor fixture; see the flicker handler. */
	bool bCorridorFlickerSuspended = false;
	bool bCommonInspectionLightsEnabled = true;
	bool bTheHourSealed = false;

	/**
	 * 밤의 등 배율. 그 시간에는 복도 등 넷 중 서쪽 하나만 떨리며 남고 로비는
	 * 우편함 위 하나만 남는다. SetFixtureLive가 다시 켤 때도 이 배율을 곱하므로
	 * 연출이 밤에 등을 되살려도 낮의 밝기로 돌아오지 않는다.
	 */
	float NightFixtureScale(int32 Index, bool bCorridor) const;
	/** 등·안개·카메라 룩을 그 시간에 맞추거나 새벽으로 되돌린다. */
	void ApplyNightAtmosphere(bool bSealed);

	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> FlickerStreetlight;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> DegradedCorridorLight;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> StoreLights;
	UPROPERTY(Transient) TObjectPtr<class AIGStoreClerk> StoreClerk;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> JingleComponent;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> DistantAlarmComponent;

	/** When set, assembly helpers parent to this instead of SceneRoot. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ActiveParent;

	/** Collision receipts for the portal-free missing-floor route. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> MissingFloorRooftopRouteFloors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> MissingFloorUpperStairSteps;

	/** Night 4: the three practical lights on the upper stair/roof/annex circuit. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> MissingFloorAnnexLights;

	/** §11 V2: the fifth-floor dust that holds footprints and drag marks. */
	UPROPERTY(Transient)
	TObjectPtr<UIGSettledDustComponent> SettledDust;

	/** §11 V2: 403호 3단계 노화. Both stages are built hidden and revealed. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Unit403AgeStageOne;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Unit403AgeStageTwo;

	int32 Unit403AgeStage = 0;

	/** The middle bay's removable gypsum face; studs and evidence remain. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MissingFloorCavityWallPanel;

	/** Front-face hand residue follows the gypsum panel when it is removed. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MissingFloorCavityWallResidue;
	bool bMissingFloorCavityOpen = false;

	/**
	 * Exact world-space contact transform derived from the registered bedside
	 * table bounds. Imported scans are uniformly fitted and may finish shorter
	 * than their target box, so a hard-coded 60 cm surface can visibly float the
	 * clock above an otherwise correct table.
	 */
	FVector AlarmWorldLocation = FVector::ZeroVector;

	/** Measured desk support plane and its gravity-seated wallet transform. */
	float DeskSurfaceWorldZ = 0.0f;
	FVector WalletWorldLocation = FVector::ZeroVector;

	FTimerHandle CorridorFlickerHandle;
	float DegradedLightBaseIntensity = 850.0f;
	uint32 FlickerHashCounter = 0;
	int32 PlayerPositionAttempts = 0;
	int32 BlockCounter = 0;
	bool bPrologueInitialized = false;
	bool bChapterTwoTransitionPending = false;
	bool bChapterTwoActive = false;
	bool bChapterTwoFinished = false;
	bool bChapterThreeActive = false;
	bool bRebirthEndToEndValidation = false;

	FTimerHandle ChapterTransitionHandle;
	FTimerHandle ChapterEndingHandle;
	FTimerHandle ChapterCaptureHandle;
	FTimerHandle RebirthEndToEndHandle;
	int32 ChapterCaptureIndex = 0;
};
