[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

function Read-ProjectText([string]$RelativePath) {
	return [IO.File]::ReadAllText((Join-Path $projectRoot $RelativePath))
}

$controller = Read-ProjectText 'Source/IndieGame/Player/IGPlayerController.cpp'
$gameMode = Read-ProjectText 'Source/IndieGame/Core/IGPrologueGameMode.cpp'
$save = Read-ProjectText 'Source/IndieGame/Save/IGSaveSubsystem.cpp'
$world = Read-ProjectText 'Source/IndieGame/Core/IGPrologueWorldScene.cpp'
$director = Read-ProjectText 'Source/IndieGame/Entity/IGListenerGreyboxDirector.cpp'
$phase = Read-ProjectText 'Source/IndieGame/Entity/IGNightPhaseDirector.cpp'
$player = Read-ProjectText 'Source/IndieGame/Player/IGPlayerCharacter.cpp'
$evidence = Read-ProjectText 'Source/IndieGame/Entity/IGMissingFloorEvidence.cpp'
$input = Read-ProjectText 'Config/DefaultInput.ini'
$tags = Read-ProjectText 'Config/DefaultGameplayTags.ini'
$probe = Read-ProjectText 'Scripts/Run-MissingFloor-ArrivalProbe.ps1'

foreach ($token in @(
	'IGMissingFloor=1?IGIgnoreDirectStart=1?IGNewGame=1',
	'MissingFloorState->ResetNarrative()'
)) {
	if (-not $controller.Contains($token)) {
		throw "Production new-game route is missing: $token"
	}
}
foreach ($token in @(
	'cardboard_box_01_1k.cardboard_box_01_1k',
	'PresentationMesh->bDisallowNanite = true',
	'const FVector Scale = SizeCentimeters / MeshSize',
	'PresentationMesh->SetMaterial(Slot, Material)'
)) {
	if (-not ($director + $evidence).Contains($token)) {
		throw "Arrival scanned-prop material safety is missing: $token"
	}
}
foreach ($token in @(
	'IGArrivalProbe',
	'MISSINGFLOOR_ARRIVAL PASS',
	'M_MovingBoxCardboardUV',
	'RunArrivalProbe()'
)) {
	if (-not $director.Contains($token)) {
		throw "Arrival runtime receipt is missing: $token"
	}
}
foreach ($token in @(
	"'-RenderOffScreen'",
	"'-nullrhi'",
	"'-IGMissingFloor'",
	"'-IGArrivalProbe'",
	'MISSINGFLOOR_ARRIVAL_PROBE PASS'
)) {
	if (-not $probe.Contains($token)) {
		throw "Arrival runtime harness is missing: $token"
	}
}
foreach ($token in @(
	'World->URL.HasOption(TEXT("IGMissingFloor"))',
	'AIGListenerGreyboxDirector::StaticClass()'
)) {
	if (-not $gameMode.Contains($token)) {
		throw "Production game-mode route is missing: $token"
	}
}
foreach ($token in @(
	'bIsMissingFloorSave',
	'MissingFloorSnapshot.Night.CompletedBeats.Num() > 0',
	'IGMissingFloor=1?IGIgnoreDirectStart=1?IGResumeSave=1'
)) {
	if (-not $save.Contains($token)) {
		throw "Missing Floor save-resume route is missing: $token"
	}
}
foreach ($token in @(
	'bMissingFloorRuntime',
	'if (!bMissingFloorRuntime)',
	'M_MovingBoxCardboardUV'
)) {
	if (-not $world.Contains($token)) {
		throw "Shared-world ownership/material contract is missing: $token"
	}
}
foreach ($token in @(
	'InitializeArrivalSequence()',
	'SpawnArrivalInteractables(CubeMesh)',
	'Arrival.Contract',
	'Arrival.Box.Parcel',
	'Arrival.Box.Notebook',
	'Arrival.Box.Voicemail',
	'Arrival.Store',
	'Arrival.Unit401',
	'Arrival.Unit402',
	'Arrival.RoofDoor',
	'Arrival.Complete',
	'NightPhase->ResumeTheHour(',
	'Checkpoint.MissingFloor.Arrival'
)) {
	if (-not $director.Contains($token)) {
		throw "Arrival story contract is missing: $token"
	}
}
foreach ($token in @(
	'void AIGNightPhaseDirector::ResumeTheHour(',
	'Checkpoint.MissingFloor.Night',
	'Checkpoint.MissingFloor.Day'
)) {
	if (-not $phase.Contains($token)) {
		throw "Night persistence contract is missing: $token"
	}
}
foreach ($token in @(
	'MovementComponent->JumpZVelocity = 330.0f',
	'MovementComponent->AirControl = 0.08f',
	'void AIGPlayerCharacter::Landed(',
	'Noise->ReportNoise(GetActorLocation(), LandingLoudness, this)'
)) {
	if (-not $player.Contains($token)) {
		throw "Grounded jump/landing contract is missing: $token"
	}
}
foreach ($token in @(
	'ActionName="Jump"',
	'Key=SpaceBar',
	'Key=Gamepad_LeftShoulder'
)) {
	if (-not $input.Contains($token)) {
		throw "Jump input binding is missing: $token"
	}
}
foreach ($token in @(
	'Chapter.MissingFloor',
	'Checkpoint.MissingFloor.Arrival',
	'Checkpoint.MissingFloor.Night',
	'Checkpoint.MissingFloor.Day'
)) {
	if (-not $tags.Contains($token)) {
		throw "Missing Floor gameplay tag is missing: $token"
	}
}

Write-Host 'MISSINGFLOOR_PRODUCTION_ENTRY_CONTRACT PASS arrival_beats=8 save_resume=true jump_landing=true'
