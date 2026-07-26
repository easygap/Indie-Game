#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IGAlarmClock.h"
#include "Interaction/IGGetUpInteractable.h"
#include "Sequence/IGWakeUpDirector.h"
#include "IGPrologueWorldScene.generated.h"

class AIGCheckoutCounter;
class AIGDemoDirector;
class AIGElevator;
class AIGFridge;
class AIGInspectable;
class AIGApartmentStoryDressing;
class AIGMorningRoutineDirector;
class AIGNeighborhoodLifeDirector;
class AIGPickupItem;
class AIGReadableNote;
class AIGSecondMorningDirector;
class AIGSlidingDoor;
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
class UIGAlarmSoundWave;

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

	/** Arms the homecoming volume only after the second purchase is complete. */
	void SetChapterTwoReturnZoneArmed(bool bArmed);

	/** Kills/restores the two north-side sales-floor luminaires and diffusers. */
	void SetStoreNorthLightsLive(bool bLive);

	/** CH02 ending: relight 4F, sound the distant alarm, fade, and show CH03 card. */
	void FinishChapterTwo();

	int32 GetCorridorFixtureCount() const { return CorridorLights.Num(); }
	int32 GetLobbyFixtureCount() const { return LobbyLights.Num(); }

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
		USceneComponent* Parent = nullptr);
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
	 * Wraps a printed label band around a lathed bottle. BottleBase is the
	 * bottle's own base; BandBottomZ/BandHeight are measured from there.
	 */
	UStaticMeshComponent* CreateBottleLabel(
		const FVector& BottleBase,
		float Radius,
		float BandBottomZ,
		float BandHeight,
		const TCHAR* LabelMaterialName,
		float YawDegrees);

	/**
	 * Places one cup ramyeon: foam cup, printed sleeve, foil lid.
	 *
	 * Three props rather than one, because the cup mesh has a single material
	 * slot and the lid is unioned into it — texturing the mesh with the label
	 * smears the artwork across the foil and the base.
	 */
	void CreateCupRamyeon(const FVector& BaseLocation, float YawDegrees);

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

	// --- construction stages ---------------------------------------------
	void InitializePrologue();
	bool PositionPlayer();
	void BuildApartment();
	void BuildCorridor();
	void BuildChapterTwoOverlay();
	void BuildLobby();
	void BuildAlley();
	void BuildStore();
	void BuildSkyAndFog();
	void SpawnInteractables();
	void SpawnChapterTwoInteractables();
	void SpawnDirectors();
	void CreateAmbience();
	void SpawnDemoDirectorIfRequested();
	void SpawnReturnBoundary();
	void BeginChapterTwoTransition();
	void EnterChapterTwo();
	void EnterChapterThree();
	void StartChapterTwoCaptureSequence();
	void CaptureNextChapterTwoFrame();
	void FinishChapterTwoCaptureSequence();

	UFUNCTION()
	void HandleStoryStateChanged(FGameplayTag StateTag, bool bAdded);

	UFUNCTION()
	void HandleReturnBoundaryTriggered(AIGZoneTrigger* Zone);

	/** Grants the torch to the player pawn once the pickup is taken. */
	UFUNCTION()
	void HandleFlashlightPickedUp(AIGPickupItem* Item);

	UFUNCTION()
	void HandleFlickerZoneTriggered(AIGZoneTrigger* Zone);

	/** Aging-ballast shimmer on one corridor fluorescent. */
	void HandleCorridorFlicker();

	// --- components -------------------------------------------------------
	UPROPERTY(VisibleAnywhere, Category = "Prologue|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Prologue|Components")
	TArray<TObjectPtr<UStaticMeshComponent>> GeometryComponents;

	UPROPERTY(VisibleAnywhere, Category = "Prologue|Lighting")
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	UPROPERTY(VisibleAnywhere, Category = "Prologue|Lighting")
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> AmbientBeds;

	// --- assets -----------------------------------------------------------
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CylinderMesh;
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

	/** Everything on the 4th floor (unit 404 + its corridor) hangs off this. */
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
	UPROPERTY(Transient) TObjectPtr<AIGSlidingDoor> StoreDoor;
	UPROPERTY(Transient) TObjectPtr<AIGCheckoutCounter> Checkout;
	UPROPERTY(Transient) TObjectPtr<AIGMorningRoutineDirector> MorningDirector;
	UPROPERTY(Transient) TObjectPtr<AIGNeighborhoodLifeDirector> NeighborhoodLifeDirector;
	UPROPERTY(Transient) TObjectPtr<AIGSecondMorningDirector> SecondMorningDirector;
	UPROPERTY(Transient) TObjectPtr<AIGThirdMorningDirector> ThirdMorningDirector;
	UPROPERTY(Transient) TObjectPtr<AIGDemoDirector> DemoDirector;
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
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> MailboxBills;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> SaltMemo;
	UPROPERTY(Transient) TObjectPtr<AIGReadableNote> NightRoster;
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

	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> FlickerStreetlight;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> DegradedCorridorLight;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> StoreLights;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> JingleComponent;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> DistantAlarmComponent;

	/** When set, assembly helpers parent to this instead of SceneRoot. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ActiveParent;

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

	FTimerHandle ChapterTransitionHandle;
	FTimerHandle ChapterEndingHandle;
	FTimerHandle ChapterCaptureHandle;
	int32 ChapterCaptureIndex = 0;
};
