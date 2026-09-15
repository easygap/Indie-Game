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
		throw "M1_CAPTURE_CONTRACT FAIL: missing file: $RelativePath"
	}
	return Get-Content -Raw -Encoding UTF8 -LiteralPath $path
}

function Assert-True {
	param(
		[Parameter(Mandatory)][bool]$Condition,
		[Parameter(Mandatory)][string]$Message
	)

	if (-not $Condition) {
		throw "M1_CAPTURE_CONTRACT FAIL: $Message"
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

$nightHeader = Read-ProjectText 'Source/IndieGame/Entity/IGNightLoopDirector.h'
$night = Read-ProjectText 'Source/IndieGame/Entity/IGNightLoopDirector.cpp'
$playerHeader = Read-ProjectText 'Source/IndieGame/Player/IGPlayerCharacter.h'
$player = Read-ProjectText 'Source/IndieGame/Player/IGPlayerCharacter.cpp'
$hudHeader = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.h'
$hud = Read-ProjectText 'Source/IndieGame/Player/IGHorrorHUD.cpp'
$listener = Read-ProjectText 'Source/IndieGame/Entity/IGListenerEntity.cpp'
$greybox = Read-ProjectText 'Source/IndieGame/Entity/IGListenerGreyboxDirector.cpp'
$toneSequence = Read-ProjectText 'Source/IndieGame/Audio/IGToneSequenceSoundWave.cpp'
$prepareArt = Read-ProjectText 'Scripts/Prepare-AIArt.ps1'
$surfaceTextures = Read-ProjectText 'Scripts/generate_surface_textures.py'
$artBuild = Read-ProjectText 'Scripts/Build-ArtAssets.ps1'
$assetPolicy = Read-ProjectText 'Docs/ASSET_POLICY.md'
$imageGenRecord = Read-ProjectText 'Docs/IMAGEGEN_PROMPTS_2026-08-11.md'
$story = Read-ProjectText 'Docs/STORY_BIBLE_MISSING_FLOOR.md'
$readme = Read-ProjectText 'README.md'

Assert-ContainsAll $nightHeader @(
	'float FadeOutSeconds = 2.15f;',
	'int32 GetCaptureHandprintCount() const',
	'TArray<TObjectPtr<UStaticMeshComponent>> CaptureHandprints;',
	'float GetWakeFadeInSeconds() const;'
) 'night-loop declarations'
Assert-ContainsAll $night @(
	'Character->PlayCaptureFeedback(FadeOutSeconds);',
	'Character->DisableInput(Controller);',
	'FMath::Max(FadeOutSeconds, 0.05f)',
	'SpawnCaptureHandprint(Character);',
	'WallProbeCount = 8',
	'WallProbeDistance = 260.0f',
	'FMath::Abs(Hit.ImpactNormal.Z) > 0.35f',
	'/Game/Prototype/Materials/M_MissingFloorHandprints.',
	'MaximumCaptureHandprints = 12',
	'MissingFloor.CaptureHandprint',
	'return 3.0f;',
	'return 2.2f;',
	'return 1.4f;',
	'return 0.4f;'
) 'capture reset and residue'
$captureBlock = Get-Block $night `
	'void AIGNightLoopDirector::HandlePlayerCaptured(APawn* Player)' `
	'void AIGNightLoopDirector::FinishReset()'
Assert-True (
	$captureBlock.IndexOf('Character->PlayCaptureFeedback(FadeOutSeconds);') -lt
	$captureBlock.IndexOf('Character->DisableInput(Controller);')) `
	'capture feedback must start before input is disabled'
Assert-True (-not $captureBlock.Contains('+ 0.4f')) `
	'blackout reset must finish at the authored 2.15-second boundary'

Assert-ContainsAll $playerHeader @(
	'void PlayCaptureFeedback(float DurationSeconds = 1.2f);',
	'void UpdateCaptureFeedback(float DeltaSeconds);',
	'float CaptureFeedbackRemainingSeconds = 0.0f;',
	'uint64 CaptureForceFeedbackHandle = 0;'
) 'player capture declarations'
Assert-ContainsAll $player @(
	'CaptureCameraKickDegrees = 3.2f',
	'CaptureHapticIntensity = 0.70f',
	'void AIGPlayerCharacter::PlayCaptureFeedback(',
	'HorrorHUD->PlayCaptureEmbrace(CaptureFeedbackDurationSeconds);',
	'EDynamicForceFeedbackAction::Start',
	'EDynamicForceFeedbackAction::Update',
	'EDynamicForceFeedbackAction::Stop',
	'AccessibilitySubsystem->AreHapticsEnabled()',
	'IGPlayerNoise::CaptureHapticIntensity * RemainingAlpha',
	'IGPlayerNoise::CaptureCameraKickDegrees',
	'if (!bReducedMotion && CaptureFeedbackRemainingSeconds > 0.0f)'
) 'camera and haptic feedback'

Assert-ContainsAll $hudHeader @(
	'void PlayCaptureEmbrace(float DurationSeconds = 1.2f);',
	'bool DrawCaptureEmbrace(double CurrentTime);',
	'TArray<TObjectPtr<UTexture2D>> CaptureEmbraceFrames;',
	'double CaptureEmbraceEndTime = -1.0;',
	'bool bCaptureEmbracePreview = false;'
) 'capture HUD declarations'
Assert-ContainsAll $hud @(
	'CaptureEmbraceDurationSeconds = 1.2',
	'CaptureEmbraceFrameCount = 4',
	'T_FPCaptureEmbrace0_D.T_FPCaptureEmbrace0_D',
	'T_FPCaptureEmbrace3_D.T_FPCaptureEmbrace3_D',
	'TEXT("IGM1CapturePreview")',
	'if (DrawCaptureEmbrace(CurrentTime))',
	'const float SpriteSize = FMath::Max(Canvas->ClipX, Canvas->ClipY);',
	'FrameTile.BlendMode = SE_BLEND_Translucent;',
	'Accessibility->IsReducedCameraMotionEnabled()',
	# 정지 화면으로 세우는 칸은 손이 있는 마지막 포즈여야 한다. 시트 3번은
	# 손이 잘려 나간 팔뚝 두 개라 무엇이 닿았는지를 말해 주지 못한다.
	'DrawFrame(0, Visibility * 0.94f);',
	'ClosingAnimationEnd = 0.72f',
	# 시트 순서대로 틀면 팔이 바깥으로 벌어져 화면을 빠져나간다. 포옹은 안으로
	# 닫히는 동작이므로 뒤에서부터 튼다. v2 원화를 반입하면 이 뒤집기를
	# 되돌려야 한다 — v2는 좌상부터가 첫 접촉이다.
	'IGHorrorHUD::CaptureEmbraceFrameCount - 1 - SheetIndex',
	'DrawFrame(FrameIndex, Visibility);'
) 'capture HUD presentation'

Assert-ContainsAll $listener @(
	'UIGToneSequenceSoundWave::CreateCaptureStruggle(this)',
	'OnPlayerCaptured.Broadcast(Player);'
) 'close capture sound'
Assert-ContainsAll $greybox @(
	'NightLoop->GetCaptureHandprintCount() >= 1',
	'!NightLoop->IsCaptureResetInFlight()',
	'PlayerCharacter->InputEnabled()',
	'PHYSICAL_CAPTURE_CONTACT',
	'bTexturesReady &= Texture->IsFullyStreamedIn();',
	'reset incomplete (atBed=%d tier=%d handprints=%d recovery=%d input=%d)'
) 'capture runtime probe'
$replyBlock = Get-Block $toneSequence `
	'UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateWallKnockReply(UObject* Outer)' `
	'UIGToneSequenceSoundWave* UIGToneSequenceSoundWave::CreateAnswerKnockPattern('
Assert-ContainsAll $replyBlock @(
	'KnockIndex < 2',
	'const float Start = 0.42f * KnockIndex;'
) 'calm close double knock'

Assert-ContainsAll $prepareArt @(
	"Source = 'SheetListenerCaptureEmbracePhases_v1_RGBA'; Target = 'T_FPCaptureEmbrace0_D.png'",
	"Source = 'SheetListenerCaptureEmbracePhases_v1_RGBA'; Target = 'T_FPCaptureEmbrace3_D.png'",
	"Mode = 'PreserveAlphaGreenDespill'"
) 'capture sprite extraction'

# 1024 시트는 여럿이라 크기만으로는 이 스프라이트를 못 짚는다.
# 네 장이 각각 1024로 뽑히는지 대상 이름과 짝지어 본다.
foreach ($phase in 0..3) {
	$sizePattern = "T_FPCaptureEmbrace$($phase)_D.png'\s*\r?\n\s*" +
		"Crop = [^\r\n]*Size = @\(1024, 1024\)"
	Assert-True ($prepareArt -match $sizePattern) `
		"capture embrace phase $phase must extract at 1024"
}
Assert-ContainsAll $surfaceTextures @(
	'"T_FPCaptureEmbrace0_D"',
	'"T_FPCaptureEmbrace3_D"',
	'"mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS',
	'texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)',
	'texture.set_editor_property("never_stream", True)'
) 'capture texture import settings'
Assert-ContainsAll $artBuild @(
	"'SheetListenerCaptureEmbracePhases_v1_RGBA'",
	"'\[IndieGame\] Imported 11 textures'",
	"'Content\Prototype\Textures\T_FPCaptureEmbrace0_D.uasset'",
	"'Content\Prototype\Textures\T_FPCaptureEmbrace3_D.uasset'"
) 'targeted capture asset build'
Assert-ContainsAll $assetPolicy @(
	'SheetListenerCaptureEmbracePhases_v1.png',
	'C27CAEBAE413E574AB3BA48AA95979718233C87893492420E8F5DBB9E11C999A',
	'T_FPCaptureEmbrace0_D',
	'UI-space'
) 'capture asset provenance'
Assert-ContainsAll $imageGenRecord @(
	'Asset type: production 2x2 first-person capture-embrace animation sprite sheet',
	'perfectly flat solid #00ff00 chroma-key background',
	'C27CAEBAE413E574AB3BA48AA95979718233C87893492420E8F5DBB9E11C999A',
	'SheetListenerCaptureEmbracePhases_v1_RGBA.png'
) 'capture ImageGen prompt record'
Assert-ContainsAll $story @(
	'## 28. v2.7',
	# 세 값을 맨 숫자로 찾으면 문서 아무 데나 있어도 통과한다.
	'붙잡힌 접촉은 2.15초간 이어지고 침대에서 풀린다',
	'카메라 피치 최대 3.2°와 전 모터 진동 0.70을 0까지 감쇠한다',
	'3.0초 → 2.2초 → 1.4초 → 0.4초'
) 'v2.7 capture design supplement'
Assert-ContainsAll $readme @(
	# README는 Docs/Media/readme/의 표시용 파생본을 건다. 원본은 아래 자산
	# 목록에서 따로 확인한다 — 파생본이 있다고 원본이 있는 것은 아니다.
	# 진동 감쇠 초 수 같은 구현 수치는 플레이어 문서에 싣지 않는다. 문서가
	# 약속해야 하는 것은 포획이 벌이 아니라 되감기라는 사실이다.
	'Docs/Media/readme/m1-capture-embrace.gif',
	'포획은 게임 오버가 아닙니다.',
	'읽은 기록과 알아낸 것은 그대로 남고'
) 'player-facing capture README'

$sourceAsset = Join-Path $projectRoot `
	'Content/SourceArt/AI/SheetListenerCaptureEmbracePhases_v1.png'
$sourceHash = (Get-FileHash -LiteralPath $sourceAsset -Algorithm SHA256).Hash
Assert-True ($sourceHash -eq `
	'C27CAEBAE413E574AB3BA48AA95979718233C87893492420E8F5DBB9E11C999A') `
	'capture source hash does not match the approved ImageGen output'

foreach ($relativeAsset in @(
	'Content/SourceArt/AI/SheetListenerCaptureEmbracePhases_v1.png',
	'Content/SourceArt/AI/SheetListenerCaptureEmbracePhases_v1_RGBA.png',
	'Content/SourceArt/T_FPCaptureEmbrace0_D.png',
	'Content/SourceArt/T_FPCaptureEmbrace1_D.png',
	'Content/SourceArt/T_FPCaptureEmbrace2_D.png',
	'Content/SourceArt/T_FPCaptureEmbrace3_D.png',
	'Content/Prototype/Textures/T_FPCaptureEmbrace0_D.uasset',
	'Content/Prototype/Textures/T_FPCaptureEmbrace1_D.uasset',
	'Content/Prototype/Textures/T_FPCaptureEmbrace2_D.uasset',
	'Content/Prototype/Textures/T_FPCaptureEmbrace3_D.uasset',
	'Docs/Media/m1-capture-embrace.png',
	'Docs/Media/m1-capture-embrace.gif'
)) {
	Assert-True (Test-Path -LiteralPath (Join-Path $projectRoot $relativeAsset) -PathType Leaf) `
		"missing capture asset: $relativeAsset"
}

Write-Host (
	'M1 capture contract passed ({0} assertions).' -f $assertionCount) `
	-ForegroundColor Green
