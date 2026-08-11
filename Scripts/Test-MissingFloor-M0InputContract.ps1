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
	'RowCount = 17',
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
	'120ms',
	'0.6',
	'0.9',
	'40ms'
) 'v2.6 design supplement'

Assert-ContainsAll $prepareArt @(
	"Source = 'SheetFirstPersonKnockPhases_v2_RGBA'; Target = 'T_FPHandKnock0_D.png'",
	"Source = 'SheetFirstPersonKnockPhases_v2_RGBA'; Target = 'T_FPHandKnock3_D.png'",
	'ContentScale = 0.63',
	"Mode = 'PreserveAlphaGreenDespill'"
) 'first-person sprite extraction'
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
	"'\[IndieGame\] Imported 10 textures'",
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

Write-Host (
	'M0 input contract passed ({0} assertions).' -f $assertionCount) `
	-ForegroundColor Green
