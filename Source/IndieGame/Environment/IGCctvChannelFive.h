#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IGCctvChannelFive.generated.h"

class AIGPrologueWorldScene;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneCaptureComponent2D;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/** Where the one press has got to. */
UENUM()
enum class EIGCctvChannelState : uint8
{
	/** The four-way split. Nothing of channel 5 is allocated or rendering. */
	Idle,
	/** Snow, while a tube that has never carried this input finds sync. */
	Acquiring,
	/** The corridor that should not exist. */
	Live,
	/** Snow again, tearing. The channel is dying and will not come back. */
	Collapsing,
	/** Spent. Back to the split, and the button does nothing for the rest of the game. */
	Spent
};

/**
 * §14 CCTV 채널 5 — 「관측 호러는 이 한 번뿐」.
 *
 * The desk monitor in the management booth is a four-way split, and the channel
 * selector has a fifth button. Pressing it shows a live picture of a corridor
 * that is not on any drawing: stalled material, plastic sheeting, and a low
 * shape crossing the edge of the frame. Then the channel tears and the split
 * comes back, once and for all (§8 비트 2-2).
 *
 * This is the first *visual* proof the fifth floor exists, and the reason the
 * player climbs in night 3. §19.3 is explicit that lifting the observation-horror
 * genre wholesale would make this a clone, so the whole feature is one cut: no
 * channel cycling, no rewind, no camera mode.
 *
 * §14 requires the render budget to be protected — 상시 렌더 금지. Nothing here
 * exists until Play() is called: the render target is allocated on the press, the
 * capture renders about twelve frames a second for the seconds it is live, and
 * the target, the capture and the crawling shape are all released the moment the
 * channel dies. Between beats this actor holds one hidden plane.
 *
 * It also has to agree with §5.5. Channel 5 is wired straight into a spare BNC
 * input on the monitor with nothing looped through to the NVR, so it can be
 * watched and never recovered. That is why the world carries an `AUX 5 /
 * MONITOR ONLY` label on the monitor's own case and a coax that leaves the annex
 * camera and never reaches a recorder — §19.9 위험 8 is precisely the failure
 * where the screen and the rule contradict each other.
 */
UCLASS(NotBlueprintable, Transient)
class INDIEGAME_API AIGCctvChannelFive : public AActor
{
	GENERATED_BODY()

public:
	AIGCctvChannelFive();

	/**
	 * Builds the dark screen face over the booth monitor and the AUX label on its
	 * case. Loads the monitor material now, at night start, so the press itself
	 * never waits on a synchronous load in the middle of the beat.
	 */
	bool Configure(AIGPrologueWorldScene* InScene);

	/**
	 * The one press. Allocates the capture and starts the sequence; returns false
	 * if the channel has already been spent, which is what makes the beat
	 * unrepeatable at the source instead of relying on a narrative flag alone.
	 */
	bool Play();

	EIGCctvChannelState GetState() const { return State; }

	/** True once the channel has been used, whether or not it is still on screen. */
	bool IsSpent() const { return bUsed && State == EIGCctvChannelState::Spent; }

	/** True while the picture or its snow is on the monitor. */
	bool IsOnScreen() const;

	// -- receipts for the probe and the contracts ---------------------------
	/** Scene-capture renders issued so far. Zero unless the channel was played. */
	int32 GetCaptureCount() const { return CaptureCount; }
	/** True while a render target exists; must be false before and after. */
	bool HasFeed() const { return Feed != nullptr; }
	/** The feed's resolution, or zero when nothing is allocated. */
	FIntPoint GetFeedResolution() const;
	/** Current value driven into the material's `Static` parameter. */
	float GetStaticMix() const { return StaticMix; }
	/** True while the low shape is on its crossing. */
	bool IsShapeCrossing() const;
	/**
	 * Harness hook: the live target, so a probe can read the picture instead of
	 * inferring it. Null outside the beat, which is the point of §14's budget.
	 */
	UTextureRenderTarget2D* GetFeedForTesting() const { return Feed; }
	/** Seconds of picture, not counting the snow either side. */
	static constexpr float LiveSeconds = 5.60f;

private:
	void EnterState(EIGCctvChannelState NextState);
	void ReleaseChannel();
	void UpdateShape(float LiveProgress01);
	void ApplyMaterialParameters();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(Transient)
	TWeakObjectPtr<AIGPrologueWorldScene> Scene;

	/** The channel-5 face, sitting a few millimetres in front of the split. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ScreenFace;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ScreenMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScreenInstance;

	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> Feed;

	/**
	 * 낮은 형체 — 회백색, and built from lit masses rather than from the entity's
	 * sprite cards. Those cards are authored for one head-on angle under a torch;
	 * seen from a ceiling corner under an IR lamp they resolve to a bright
	 * artifact, which on 288 lines reads as a rendering bug instead of a body.
	 * Exists only while the channel is live.
	 */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> LowShapePivot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LowShapeParts;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	EIGCctvChannelState State = EIGCctvChannelState::Idle;
	float StateSeconds = 0.0f;
	float SinceCaptureSeconds = 0.0f;
	float StaticMix = 1.0f;
	int32 CaptureCount = 0;
	bool bUsed = false;
};
