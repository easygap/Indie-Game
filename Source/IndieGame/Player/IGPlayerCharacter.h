#pragma once

#include "CoreMinimal.h"
#include "Audio/IGToneSequenceSoundWave.h"
#include "GameFramework/Character.h"
#include "IGPlayerCharacter.generated.h"

namespace Audio
{
	class FAudioCaptureSynth;
}

class UCameraComponent;
class UIGAccessibilitySubsystem;
class UIGFlashlightComponent;
class UIGInteractionComponent;
class UIGStressComponent;
class UInputAction;
class UStaticMeshComponent;
struct FInputActionValue;

/** First-person player pawn with asset-driven Enhanced Input bindings. */
UCLASS()
class INDIEGAME_API AIGPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AIGPlayerCharacter();
	virtual ~AIGPlayerCharacter() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Player|Components")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category = "Player|Components")
	UIGInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	UFUNCTION(BlueprintPure, Category = "Player|Components")
	UIGFlashlightComponent* GetFlashlight() const { return Flashlight; }

	UFUNCTION(BlueprintPure, Category = "Player|Components")
	UIGStressComponent* GetStress() const { return StressComponent; }

	/**
	 * 둘-쉬고-하나를 들을 수 있는 존재에게 이 탭을 건넨다. 인식만 하며, 소리와
	 * 소음 보고와 피드백은 부른 쪽이 소유한다 — 한 번의 탭이 두 번 들리지
	 * 않도록. 아무도 받지 않으면 false.
	 */
	bool OfferAnswerKnock(const FVector& Where);

	/**
	 * Enables the procedural head-bob/breath sway and footstep cadence.
	 * Kept off while a director owns the camera (lying in bed, getting up).
	 */
	UFUNCTION(BlueprintCallable, Category = "Player|Camera")
	void SetCameraMotionEnabled(bool bEnabled);

	/** 비폭력 포획 포옹과 카메라 킥, 감쇠 진동을 시작한다. */
	UFUNCTION(BlueprintCallable, Category = "Player|Camera")
	void PlayCaptureFeedback(float DurationSeconds = 1.2f);

	/** Parents an item to the camera at the given relative pose (held item). */
	UFUNCTION(BlueprintCallable, Category = "Player|Carry")
	bool CarryActor(AActor* Item, const FVector& RelativeOffset, const FRotator& RelativeRotation);

	/** Clears the hand only when it still owns ExpectedItem. */
	UFUNCTION(BlueprintCallable, Category = "Player|Carry")
	bool ReleaseCarriedActor(AActor* ExpectedItem);

	UFUNCTION(BlueprintPure, Category = "Player|Carry")
	AActor* GetCarriedActor() const { return CarriedActor.Get(); }

	/**
	 * Profile-C static proxy for scripted beats (the 04:33 drink): sets the
	 * carried bag down for Seconds, then re-grips automatically. No input is
	 * taken away. Returns false unless the committed purchase actually needs
	 * both hands or the bag is already down.
	 */
	UFUNCTION(BlueprintCallable, Category = "Player|Carry")
	bool BeginScriptedHeavyBagRest(float Seconds);

	/**
	 * Renders the persistent REBIRTH outfit fact with a static first-person
	 * sleeve proxy. Restore callers leave bPlayPresentation false; the
	 * canonical first-exit commit requests the non-blocking 1.2 second reveal.
	 */
	UFUNCTION(BlueprintCallable, Category = "Player|Outfit")
	void SetRebirthOutfitEquipped(
		bool bEquipped,
		bool bPlayPresentation = false);

	UFUNCTION(BlueprintPure, Category = "Player|Outfit")
	bool IsRebirthOutfitEquipped() const { return bRebirthOutfitEquipped; }

	/** Runtime release probe for the static sleeve and its three repair stitches. */
	bool ValidateRebirthOutfitProxy(int32& OutStitchCount) const;

	/** Applies the persisted/command-line microphone mode immediately. */
	void RefreshMicrophoneCaptureMode();

	UFUNCTION(BlueprintPure, Category = "Player|Audio")
	bool IsMicrophoneCaptureRunning() const { return bMicrophoneCaptureRunning; }

	UFUNCTION(BlueprintPure, Category = "Player|Audio")
	EIGFootstepSurface GetLastFootstepSurface() const { return LastFootstepSurface; }

	UFUNCTION(BlueprintPure, Category = "Player|Audio")
	float GetLastFootstepNoiseLoudness() const { return LastFootstepNoiseLoudness; }

	/**
	 * Harness hook for §24's 즉시 차단 19. F9 must not restore inside a sealed
	 * hour; the probe asks for the restore and then reads which refusal the
	 * game gave, because silence is also what a broken binding looks like.
	 */
	void LoadLatestAutosaveForTesting() { LoadLatestAutosave(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UFUNCTION()
	void HandleFocusChanged(AActor* PreviousActor, AActor* NewActor);

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void BeginInteraction();
	void EndInteraction();
	void BeginSprint();
	void EndSprint();
	void BeginCrouchInput();
	void EndCrouchInput();
	void ToggleCrouch();
	void Knock();
	void BeginListen();
	void EndListen();
	void BeginHoldBreath();
	void EndHoldBreath();
	void ToggleFlashlight();
	void LoadLatestAutosave();
	void ApplyContextMovementSpeed();
	void RefreshSprintState();
	void UpdateCrouchTransition(float DeltaSeconds);
	void UpdateContextualActions(float DeltaSeconds);
	void FinishHoldBreath(bool bForcedRelease);
	void ApplyPlayerKnockFeedback();
	void RegisterKnockSequenceTap();
	void PlayHapticFeedback(float Intensity, float DurationSeconds) const;
	/** Samples how dark it is where the player stands, for the stress model. */
	float SampleAmbientDarkness() const;
	void TryRequestGetUpFallback();
	void EndScriptedHeavyBagRest();
	/** Footstep cadence and its noise report; runs whether or not the camera bobs. */
	void UpdateFootsteps(float DeltaSeconds);
	void UpdateCaptureFeedback(float DeltaSeconds);
	void UpdateCameraMotion(float DeltaSeconds);
	void UpdateCarriedItem(float DeltaSeconds);
	void UpdateOutfitPresentation(float DeltaSeconds);
	void PlayFootstep(float SpeedScale);
	EIGFootstepSurface ResolveFootstepSurface() const;
	float GetSurfaceMovementScale(EIGFootstepSurface Surface) const;
	float ResolveFootstepNoiseLoudness(EIGFootstepSurface Surface) const;
	void UpdateMicrophoneNoise(float DeltaSeconds);
	void StopMicrophoneCapture();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UIGInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UIGFlashlightComponent> Flashlight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UIGStressComponent> StressComponent;

	UPROPERTY(Transient)
	TObjectPtr<UIGAccessibilitySubsystem> AccessibilitySubsystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Outfit", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> OutfitSleeveProxy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Outfit", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UStaticMeshComponent>> OutfitStitchProxies;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InteractInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> FlashlightInputAction;

	/** Vertical bob amplitude at full walk speed, in centimeters. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float BobAmplitude = 2.1f;

	/** Distance covered by one footstep, in centimeters. */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "10.0", Units = "cm"))
	float StepDistance = 74.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float FootstepVolume = 0.34f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CarriedActor;

	FVector CameraBaseLocation = FVector(0.0f, 0.0f, 64.0f);
	float TraveledDistanceAccum = 0.0f;
	float BreathTime = 0.0f;
	float SprintActiveSeconds = 0.0f;
	float SprintRecoverySeconds = 0.0f;
	float ListenHeldSeconds = 0.0f;
	float BreathHeldSeconds = 0.0f;
	float CrouchTransitionRemaining = 0.0f;
	float CrouchCameraCompensation = 0.0f;
	float CrouchCameraCompensationStart = 0.0f;
	float AppliedCrouchCameraCompensation = 0.0f;
	float KnockCameraKick = 0.0f;
	float CaptureFeedbackDurationSeconds = 0.0f;
	float CaptureFeedbackRemainingSeconds = 0.0f;
	uint64 CaptureForceFeedbackHandle = 0;
	double LastKnockInputSeconds = -1.0;
	double KnockInputLockedUntil = -1.0;
	int32 LastStepIndex = 0;
	int32 KnockSequenceTapCount = 0;
	bool bCameraMotionEnabled = false;
	bool bSprintInputHeld = false;
	bool bSprinting = false;
	bool bCrouchInputHeld = false;
	bool bListening = false;
	bool bListenTriggered = false;
	bool bHoldingBreath = false;
	bool bInteractionRedirectedToListen = false;
	bool bInteractionRedirectedToInterludeListen = false;
	bool bRebirthOutfitEquipped = false;
	bool bOutfitPresentationActive = false;
	float OutfitPresentationElapsed = 0.0f;
	bool bHeavyBagInteractionProxyActive = false;
	FVector HeavyBagRestLocation = FVector::ZeroVector;
	FTimerHandle HeavyBagRestTimer;

	/** Direct capture path: samples are reduced to an envelope, never retained. */
	Audio::FAudioCaptureSynth* MicrophoneCaptureSynth = nullptr;
	TArray<float> MicrophoneScratchSamples;
	float MicrophonePollAccumulator = 0.0f;
	float MicrophoneNoiseFloor = 0.012f;
	float MicrophoneCalibrationRemaining = 0.0f;
	float MicrophoneReportCooldown = 0.0f;
	float LastFootstepNoiseLoudness = 0.0f;
	EIGFootstepSurface LastFootstepSurface = EIGFootstepSurface::Concrete;
	bool bMicrophoneCaptureRunning = false;
	bool bMicrophoneOpenAttempted = false;

	/** Decaying kick applied when an interaction is pressed. */
	float InteractPunch = 0.0f;
	/** Throttles the darkness probe: it traces, so it does not run per frame. */
	float DarknessSampleTimer = 0.0f;
	float CachedDarkness = 0.0f;
	/** Held-item inertia: lag offset and the previous view rotation driving it. */
	FRotator CarrySwayOffset = FRotator::ZeroRotator;
	FRotator PreviousControlRotation = FRotator::ZeroRotator;
	FRotator CarriedBaseRotation = FRotator::ZeroRotator;
	FVector CarriedBaseLocation = FVector::ZeroVector;
};
