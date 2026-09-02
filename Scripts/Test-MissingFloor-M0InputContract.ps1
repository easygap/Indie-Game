[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$assertionCount = 0

function Read-ProjectText {
	param([Parameter(Mandatory)][string]$RelativePath)

	$path = Join-Path $projectRoot $RelativePath
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
		throw "M0_INPUT_CONTRACT FAIL: missing file: $RelativePath"
	}
	return Get-Content -Raw -Encoding UTF8 -LiteralPath $path
}

function Assert-True {
	param(
		[Parameter(Mandatory)][bool]$Condition,
		[Parameter(Mandatory)][string]$Message
	)

	if (-not $Condition) {
		throw "M0_INPUT_CONTRACT FAIL: $Message"
	}
	$script:assertionCount++
}

function Assert-ContainsAll {
	param(
		[Parameter(Mandatory)][string]$Text,
		[Parameter(Mandatory)][string[]]$Needles,
		[Parameter(Mandatory)][string]$Context
	)

	foreach ($needle in $Needles) {
		Assert-True $Text.Contains($needle) "$Context missing token: $needle"
	}
}

function Get-Block {
	param(
		[Parameter(Mandatory)][string]$Text,
		[Parameter(Mandatory)][string]$Start,
		[Parameter(Mandatory)][string]$End
	)

	$startIndex = $Text.IndexOf($Start)
	$endIndex = $Text.IndexOf($End, $startIndex + $Start.Length)
	Assert-True ($startIndex -ge 0 -and $endIndex -gt $startIndex) `
		"could not isolate block: $Start"
	return $Text.Substring($startIndex, $endIndex - $startIndex)
}

$inputConfig = Read-ProjectText 'Config/DefaultInput.ini'
$characterHeader = Read-ProjectText 'Source/IndieGame/Player/IGPlayerCharacter.h'
$character = Read-ProjectText 'Source/IndieGame/Player/IGPlayerCharacter.cpp'
$interactionHeader = Read-ProjectText 'Source/IndieGame/Player/IGInteractionComponent.h'
$interaction = Read-ProjectText 'Source/IndieGame/Player/IGInteractionComponent.cpp'
$hudHeader = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.h'
$hud = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.cpp'
$accessibilityHeader = Read-ProjectText `
	'Source/IndieGame/Accessibility/IGAccessibilitySubsystem.h'
$accessibility = Read-ProjectText `
	'Source/IndieGame/Accessibility/IGAccessibilitySubsystem.cpp'
$controller = Read-ProjectText 'Source/IndieGame/Player/IGPlayerController.cpp'
$swingDoor = Read-ProjectText 'Source/IndieGame/Interaction/IGSwingDoor.h'
$toneSequence = Read-ProjectText 'Source/IndieGame/Audio/IGToneSequenceSoundWave.cpp'
$nightThree = Read-ProjectText `
	'Source/IndieGame/Entity/IGMissingFloorNightThreeDirector.cpp'
$story = Read-ProjectText 'Docs/STORY_BIBLE_MISSING_FLOOR.md'
$prepareArt = Read-ProjectText 'Scripts/Prepare-AIArt.ps1'
$surfaceTextures = Read-ProjectText 'Scripts/generate_surface_textures.py'
$artBuild = Read-ProjectText 'Scripts/Build-ArtAssets.ps1'
$assetPolicy = Read-ProjectText 'Docs/ASSET_POLICY.md'
$imageGenRecord = Read-ProjectText 'Docs/IMAGEGEN_PROMPTS_2026-08-11.md'

Assert-ContainsAll $inputConfig @(
	'DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput',
	'DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent',
	'ActionName="Interact",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=E',
	'ActionName="Interact",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=Gamepad_FaceButton_Bottom',
	'ActionName="Sprint",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=LeftShift',
	'ActionName="Sprint",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=Gamepad_LeftThumbstick',
	'ActionName="Crouch",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=C',
	'ActionName="Crouch",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=Gamepad_RightThumbstick',
	'ActionName="Knock",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=Q',
	'ActionName="Knock",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=Gamepad_FaceButton_Right',
	'ActionName="Listen",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=Gamepad_RightTrigger',
	'ActionName="HoldBreath",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=LeftControl',
	'ActionName="HoldBreath",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=Gamepad_LeftTrigger'
) 'input map'
Assert-True (-not $inputConfig.Contains(
	'ActionName="Interact",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=Q')) `
	'Q must never alias Interact'

Assert-ContainsAll $character @(
	'ReferenceWalkSpeed = 300.0f',
	'SprintSpeed = 460.0f',
	'CrouchSpeed = 160.0f',
	'ListenSpeed = 80.0f',
	'WalkAcceleration = 1200.0f',
	'CrouchAcceleration = 900.0f',
	'CrouchBraking = 1500.0f',
	'SprintAcceleration = 1400.0f',
	'SprintBraking = 900.0f',
	'CrouchTransitionSeconds = 0.35f',
	'CrouchTransitionSpeedScale = 0.5f',
	'KnockInputLockSeconds = 0.9f',
	'KnockCameraKickDegrees = 0.4f',
	'TEXT("Crouch"), IE_Released, this, &ThisClass::EndCrouchInput',
	'AccessibilitySubsystem->UsesToggleCrouch()',
	'PlayHapticFeedback(0.35f, 0.06f)',
	'PlayHapticFeedback(0.12f, 0.04f)',
	'AccessibilitySubsystem->AreHapticsEnabled()',
	'FName(TEXT("Interaction.Door"))',
	'HorrorHUD->PlayFirstPersonKnock()'
) 'movement and verb runtime'
Assert-ContainsAll $characterHeader @(
	'void BeginCrouchInput();',
	'void EndCrouchInput();',
	'void OnStartCrouch(',
	'void OnEndCrouch(',
	'double KnockInputLockedUntil = -1.0;'
) 'character declarations'

$knockBlock = Get-Block $character `
	'void AIGPlayerCharacter::Knock()' `
	'void AIGPlayerCharacter::ApplyPlayerKnockFeedback()'
Assert-True (-not $knockBlock.Contains('SetTimer(')) `
	'player knock path must not add a game-thread delay timer'

Assert-ContainsAll $interactionHeader @(
	'float TraceDistance = 220.0f;',
	'float FocusSweepRadius = 12.0f;',
	'float InputBufferSeconds = 0.12f;',
	'float HoldRewindSpeedScale = 0.6f;',
	'bool bInteractionPressBuffered = false;',
	'void TickComponent('
) 'interaction declarations'
Assert-ContainsAll $interaction @(
	'PrimaryComponentTick.bCanEverTick = true;',
	'SetComponentTickEnabled(true);',
	'void UIGInteractionComponent::TryConsumeBufferedPress()',
	'World->GetTimeSeconds() + InputBufferSeconds',
	'Elapsed * HoldRewindSpeedScale / RewindSourceHoldDuration',
	'BeginHoldProgressRewind(HoldProgress, CompletedHoldDuration)'
) 'interaction state machine'

Assert-ContainsAll $hud @(
	'FocusAcquireDelaySeconds = 0.09f',
	'FocusAcquireRevealSeconds = 0.09f',
	'const float HoldProgress = Interaction ? Interaction->GetHoldProgress() : 0.0f;',
	'const FVector2D DrawMin = FMath::Lerp(',
	'const FVector2D DrawMax = FMath::Lerp(',
	'TEXT("/Game/Prototype/Textures/T_FPHandKnock0_D.T_FPHandKnock0_D")',
	'TEXT("/Game/Prototype/Textures/T_FPHandKnock3_D.T_FPHandKnock3_D")',
	'void AIGHorrorHUD::PlayFirstPersonKnock()',
	'void AIGHorrorHUD::DrawFirstPersonKnock(const double CurrentTime)',
	'DrawFrame(2, Visibility * (1.0f - Blend));',
	'DrawFrame(3, Visibility * Blend);',
	'DrawFrame(0, Visibility * Blend);',
	'FrameTile.BlendMode = SE_BLEND_Translucent;',
	'Accessibility->IsReducedCameraMotionEnabled()'
) 'focus bracket presentation'
Assert-ContainsAll $hudHeader @(
	'void PlayFirstPersonKnock();',
	'TArray<TObjectPtr<UTexture2D>> FirstPersonKnockFrames;',
	'double FirstPersonKnockStartTime = -1.0;'
) 'first-person knock presentation declarations'
Assert-True (-not $hud.Contains('DrawHoldProgress')) `
	'standalone hold bar must not return'
Assert-True (-not $hudHeader.Contains('DrawHoldProgress')) `
	'standalone hold bar declaration must not return'

Assert-ContainsAll $accessibilityHeader @(
	'bool bToggleCrouch = true;',
	'bool bHapticsEnabled = true;',
	'bool UsesToggleCrouch() const',
	'bool AreHapticsEnabled() const'
) 'accessibility settings'
Assert-ContainsAll $accessibility @(
	'TEXT("ToggleCrouch")',
	'TEXT("HapticsEnabled")',
	'TEXT("IGHoldCrouch")',
	'TEXT("IGNoHaptics")'
) 'accessibility persistence and QA overrides'
Assert-ContainsAll $controller @(
	'RowCount = IGSettingsMenuLayout::AccessibilityRowCount',
	'Settings.bToggleCrouch = !Settings.bToggleCrouch;',
	'Settings.bHapticsEnabled = !Settings.bHapticsEnabled;'
) 'accessibility menu input'

Assert-True $swingDoor.Contains('float QuietOpenHoldSeconds = 1.4f;') `
	'quiet door hold must be 1.4 seconds'
Assert-True $nightThree.Contains('constexpr float KnockLoudness = 0.30f;') `
	'P4 knock must use the authored 0.30 noise cost'

$singleKnockBlock = Get-Block $toneSequence `
	'UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateWallKnockSingle(' `
	'UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateWallKnockReply('
Assert-ContainsAll $singleKnockBlock @(
	'KnockNotes.Add({0.0f, 0.110f',
	'KnockNotes.Add({0.0f, 0.060f',
	'KnockNotes.Add({0.0f, 0.030f'
) 'zero-lead knock waveform'

Assert-ContainsAll $story @(
	'## 27. v2.6',
	'### 27.1',
	'### 27.2',
	'### 27.3',
	'### 27.4',
	'### 27.5',
	'### 27.6',
	# 맨 숫자로 두면 문서 어디에 있어도 통과한다. 값이 무엇을
	# 정하는지까지 적힌 줄을 본다.
	'120ms 버퍼, 홀드 취소 0.6배 되감기, 앉기 0.35초·이동 ×0.5, 세 번째 노크 뒤 0.9초 잠금',
	'최댓값 40ms 이하를 기록한다.'
) 'v2.6 design supplement'

Assert-ContainsAll $prepareArt @(
	"Source = 'SheetFirstPersonKnockPhases_v2_RGBA'; Target = 'T_FPHandKnock0_D.png'",
	"Source = 'SheetFirstPersonKnockPhases_v2_RGBA'; Target = 'T_FPHandKnock3_D.png'",
	"Mode = 'PreserveAlphaGreenDespill'"
) 'first-person sprite extraction'

# 0.63은 이 파일에 네 번 나온다 — 노크 4단이 다 같은 값을 쓰기 때문이다.
# 값만 찾으면 한 장이 다른 배율로 바뀌어도 통과한다. 장마다 짝지어 본다.
foreach ($phase in 0..3) {
	$knockPattern = "T_FPHandKnock$($phase)_D.png'\s*\r?\n\s*Crop = " +
		"[^\r\n]*Size = @\(768, 768\)\s*\r?\n\s*ContentScale = 0\.63;"
	Assert-True ($prepareArt -match $knockPattern) `
		"first-person knock phase $phase must extract at 768 with 0.63 scale"
}
Assert-ContainsAll $surfaceTextures @(
	'"T_FPHandKnock0_D"',
	'"T_FPHandKnock3_D"',
	'"mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS',
	'texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)',
	'texture.set_editor_property("never_stream", True)'
) 'first-person sprite import settings'
Assert-ContainsAll $artBuild @(
	'-OnlySource @(',
	"'SheetFirstPersonKnockPhases_v2_RGBA'",
	"'\[IndieGame\] Imported 11 textures'",
	"'Content\Prototype\Textures\T_FPHandKnock0_D.uasset'",
	"'Content\Prototype\Textures\T_FPHandKnock3_D.uasset'"
) 'first-person sprite targeted build'
Assert-ContainsAll $assetPolicy @(
	'SheetFirstPersonKnockPhases_v1.png',
	'36C3E5BA1829C36D0B8CE967DFEF5719768E3AC02ABD444F088437F448DD58E1',
	'SheetFirstPersonKnockPhases_v2.png',
	'412D9173E7EE8AFD2270CF45008E3904AC0530DF2D0CDFCFEBCBC3A6DD9241E7',
	'UI-space 전용'
) 'first-person sprite provenance'
Assert-ContainsAll $imageGenRecord @(
	'Asset type: production game animation sprite sheet',
	'perfectly flat solid #00ff00 chroma-key background',
	'36C3E5BA1829C36D0B8CE967DFEF5719768E3AC02ABD444F088437F448DD58E1',
	'412D9173E7EE8AFD2270CF45008E3904AC0530DF2D0CDFCFEBCBC3A6DD9241E7'
) 'first-person ImageGen prompt record'

foreach ($relativeAsset in @(
	'Content/SourceArt/AI/SheetFirstPersonKnockPhases_v1.png',
	'Content/SourceArt/AI/SheetFirstPersonKnockPhases_v1_RGBA.png',
	'Content/SourceArt/AI/SheetFirstPersonKnockPhases_v2.png',
	'Content/SourceArt/AI/SheetFirstPersonKnockPhases_v2_RGBA.png',
	'Content/SourceArt/T_FPHandKnock0_D.png',
	'Content/SourceArt/T_FPHandKnock1_D.png',
	'Content/SourceArt/T_FPHandKnock2_D.png',
	'Content/SourceArt/T_FPHandKnock3_D.png',
	'Content/Prototype/Textures/T_FPHandKnock0_D.uasset',
	'Content/Prototype/Textures/T_FPHandKnock1_D.uasset',
	'Content/Prototype/Textures/T_FPHandKnock2_D.uasset',
	'Content/Prototype/Textures/T_FPHandKnock3_D.uasset'
)) {
	Assert-True (Test-Path -LiteralPath (Join-Path $projectRoot $relativeAsset) -PathType Leaf) `
		"missing first-person knock asset: $relativeAsset"
}

# --- 응답 노크가 존재에게 닿는 경로 (§7 P4, §8 비트 3-7) --------------------
# P4는 지정된 벽에서 둘-쉬고-하나를 가르치고, 비트 3-7은 복도에서 그것으로
# 지나가라고 한다. 그런데 이 게임에는 대답이 존재에게 닿는 경로가 아예 없었다:
# Waiting 상태와 NotifyAnswerKnock은 구현돼 있었지만 부르는 사람이 없었다.
Assert-ContainsAll $character @(
	'if (OfferAnswerKnock(GetActorLocation()))',
	'bool AIGPlayerCharacter::OfferAnswerKnock(const FVector& Where)',
	'It->TryAnswerKnock(Where)',
	'OfferAnswerKnock(FocusedActor->GetActorLocation());'
) 'answer knock reaches the listener'
# 인식과 연출을 나눈다. 부른 쪽이 소리를 소유하므로 한 번의 탭이 두 번
# 들리지 않는다 — 문을 두드리는 경로가 이미 소리를 내고 있다.
$offerStart = $character.IndexOf(
	'bool AIGPlayerCharacter::OfferAnswerKnock(const FVector& Where)')
Assert-True ($offerStart -ge 0) 'the answer recogniser is missing'
$offerEnd = $character.IndexOf("`r`n}", $offerStart)
if ($offerEnd -lt 0) { $offerEnd = $character.IndexOf("`n}", $offerStart) }
Assert-True ($offerEnd -gt $offerStart) 'the answer recogniser has no end brace'
$offerBlock = $character.Substring($offerStart, $offerEnd - $offerStart)
Assert-True (-not $offerBlock.Contains('SpawnOneShotAt')) `
	'the answer recogniser must not play the knock itself'
Assert-True (-not $offerBlock.Contains('ApplyPlayerKnockFeedback')) `
	'the answer recogniser must not apply feedback itself'

$listenerHeader = Read-ProjectText 'Source/IndieGame/Entity/IGListenerEntity.h'
$listener = Read-ProjectText 'Source/IndieGame/Entity/IGListenerEntity.cpp'
Assert-ContainsAll $listenerHeader @(
	'bool TryAnswerKnock(const FVector& KnockLocation);',
	'static constexpr double AnswerPairMinSeconds = 0.18;',
	'static constexpr double AnswerPairMaxSeconds = 0.65;',
	'static constexpr double AnswerRestMinSeconds = 0.68;',
	'static constexpr double AnswerRestMaxSeconds = 1.80;',
	'static constexpr double AnswerSequenceResetSeconds = 3.0;'
) 'answer cadence has one definition'
# 귀에 닿지 않는 박자는 그냥 벽을 두드리는 것이다. 탭을 기록하기 전에 검사해야
# 두 층 위에서 시작한 시퀀스를 여기서 완성할 수 없다.
Assert-ContainsAll $listener @(
	'bool AIGListenerEntity::TryAnswerKnock(const FVector& KnockLocation)',
	'if (!CanHear(Probe))',
	'NotifyAnswerKnock(KnockLocation);',
	'EIGListenerState::CaptureHold',
	'EIGListenerState::FinaleLured'
) 'answer knock guards'
# 밤3 디렉터는 같은 창을 참조한다. 두 벌로 두면 언젠가 어긋난다.
$nightThree = Read-ProjectText 'Source/IndieGame/Entity/IGMissingFloorNightThreeDirector.cpp'
Assert-ContainsAll $nightThree @(
	'AIGListenerEntity::AnswerPairMinSeconds;',
	'AIGListenerEntity::AnswerPairMaxSeconds;',
	'AIGListenerEntity::AnswerRestMinSeconds;',
	'AIGListenerEntity::AnswerRestMaxSeconds;'
) 'P4 shares the one cadence definition'
$greybox = Read-ProjectText 'Source/IndieGame/Entity/IGListenerGreyboxDirector.cpp'
Assert-ContainsAll $greybox @(
	'case EProbeStep::AnswerReachContract:',
	'MISSINGFLOOR_ANSWERREACH PASS',
	'an answer from two floors up reached him'
) 'answer reach probe'

Write-Host (
	'M0 input contract passed ({0} assertions).' -f $assertionCount) `
	-ForegroundColor Green
