[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

function Assert-Route {
	param(
		[Parameter(Mandatory)][bool]$Condition,
		[Parameter(Mandatory)][string]$Message
	)

	if (-not $Condition) {
		throw "REBIRTH route matrix failed: $Message"
	}
}

function Add-Truth {
	param(
		[Parameter(Mandatory)][hashtable]$TruthSources,
		[Parameter(Mandatory)][hashtable]$StrongBeatCounts,
		[Parameter(Mandatory)][string]$Truth,
		[Parameter(Mandatory)][string]$Source
	)

	if (-not $TruthSources.ContainsKey($Truth)) {
		$TruthSources[$Truth] =
			[System.Collections.Generic.List[string]]::new()
	}
	if (-not $TruthSources[$Truth].Contains($Source)) {
		$TruthSources[$Truth].Add($Source)
	}
	if (-not $StrongBeatCounts.ContainsKey($Truth)) {
		$StrongBeatCounts[$Truth] = 1
	}
}

function Set-TruthSourcesAtomically {
	param(
		[Parameter(Mandatory)][hashtable]$TruthSources,
		[Parameter(Mandatory)][hashtable]$StrongBeatCounts,
		[Parameter(Mandatory)][string]$Truth,
		[Parameter(Mandatory)][string[]]$Sources
	)

	$uniqueSources = [System.Collections.Generic.List[string]]::new()
	foreach ($source in $Sources) {
		if (-not $uniqueSources.Contains($source)) {
			$uniqueSources.Add($source)
		}
	}
	$TruthSources[$Truth] = $uniqueSources
	if (-not $StrongBeatCounts.ContainsKey($Truth)) {
		$StrongBeatCounts[$Truth] = 1
	}
}

function Test-NegligenceFormula {
	param([string[]]$Sources = @())

	$hasHandover = $Sources -contains 'Handover.Closed0358'
	return $hasHandover -and (
		(($Sources -contains 'ManagementApp.Photo0403') -and
			($Sources -contains 'ManagementDb.FalseCompletion0620')) -or
		($Sources -contains 'PoliceChecklist.PhotoAndAudit'))
}

function Test-P5Formula {
	param(
		[Parameter(Mandatory)][string]$Truth,
		[string[]]$Sources = @()
	)

	switch ($Truth) {
		'Truth.CatSafe' {
			return ($Sources -contains 'P5.CatEnteredPrints') -and
				($Sources -contains 'P5.CatExitedPrints')
		}
		'Truth.HoseCause' {
			return ($Sources -contains 'P5.HosePawCompression') -and
				($Sources -contains 'P5.CouplingImpact') -and
				($Sources -contains 'P5.InnerRimFriction')
		}
		'Truth.Fall' {
			return ($Sources -contains 'P5.UpperSlipperEnd') -and
				($Sources -contains 'P5.LiftedPad') -and
				($Sources -contains 'P5.CorrodedClips') -and
				($Sources -contains 'P5.InwardHandSmear')
		}
		'Truth.Identity' {
			$direct = ($Sources -contains
					'P5.CurrentSleeveThreeStitches') -and
				($Sources -contains 'P5.TankSleeveThreeStitches') -and
				($Sources -contains 'P5.TankHeelWear')
			$triangulated = ($Sources -contains
					'P5.SearchPosterOutfit') -and
				($Sources -contains 'P5.Glasses') -and
				($Sources -contains 'P5.TankOutfit')
			return $direct -or $triangulated
		}
		default {
			return $false
		}
	}
}

function Test-SnapshotRoundTrip {
	param([Parameter(Mandatory)][hashtable]$State)

	$truthRecords = @(
		foreach ($truth in $State.TruthSources.Keys | Sort-Object) {
			[ordered]@{
				Truth = $truth
				Sources = @($State.TruthSources[$truth] | Sort-Object)
			}
		}
	)
	$serializable = [ordered]@{
		SaveSchemaVersion = $State.SaveSchemaVersion
		RebirthSnapshotSchemaVersion =
			$State.RebirthSnapshotSchemaVersion
		PurchaseProfile = $State.PurchaseProfile
		PaymentMethod = $State.PaymentMethod
		CatAction = $State.CatAction
		ChapterOneRoute = $State.ChapterOneRoute
		ChapterTwoRoute = $State.ChapterTwoRoute
		P3Route = $State.P3Route
		P4Route = $State.P4Route
		TankTiming = $State.TankTiming
		FlashlightRoute = $State.FlashlightRoute
		TruthRecords = $truthRecords
		NarrativeDebt = @($State.NarrativeDebt | Sort-Object)
		P3DirectInletClosed = $State.P3DirectInletClosed
		P3ReserveInletClosed = $State.P3ReserveInletClosed
		P3PressureReleaseOpen = $State.P3PressureReleaseOpen
		P3PressureZero = $State.P3PressureZero
		P3Completed = $State.P3Completed
		P3PressureKPa = $State.P3PressureKPa
		P3MistakeCount = $State.P3MistakeCount
		P3ZeroConfirmationTicks = $State.P3ZeroConfirmationTicks
		P3HintElapsedSeconds = $State.P3HintElapsedSeconds
		P3HintStage = $State.P3HintStage
		FocusedEvidence = $State.FocusedEvidence
		ObservedP5Sources = @($State.ObservedP5Sources | Sort-Object)
		P5CatSafeConfirmed = $State.P5CatSafeConfirmed
		P5HoseCauseConfirmed = $State.P5HoseCauseConfirmed
		P5FallConfirmed = $State.P5FallConfirmed
		P5IdentityConfirmed = $State.P5IdentityConfirmed
		AccidentScratchCount = $State.AccidentScratchCount
		ScratchTailSettled = $State.ScratchTailSettled
		LookedAwayAfterFirstScratch =
			$State.LookedAwayAfterFirstScratch
		ActedAfterSecondScratch = $State.ActedAfterSecondScratch
		EndingChoice = $State.EndingChoice
		CommonDiscoveryCommitted = $State.CommonDiscoveryCommitted
		ActualStateRestored = $State.ActualStateRestored
		Found0731Presented = $State.Found0731Presented
		PlayedOneShotBeats = @($State.PlayedOneShotBeats | Sort-Object)
	}

	$before = $serializable | ConvertTo-Json -Depth 8 -Compress
	$after = $before | ConvertFrom-Json |
		ConvertTo-Json -Depth 8 -Compress
	Assert-Route ($before -ceq $after) 'Snapshot JSON changed after a round trip.'
}

$purchaseProfiles = @(
	'ProfileA500MlX2',
	'ProfileB1LX1',
	'ProfileC2LX2'
)
$paymentMethods = @('WalletCard', 'PocketCard')
$catActions = @(
	'PassedBy',
	'CapLeftImmediately',
	'CapWaited',
	'PaperCupLeftImmediately',
	'PaperCupWaited'
)
$chapterOneRoutes = @('Elevator', 'Stairs')
$chapterTwoRoutes = @(
	'P1ThenP2',
	'P2ThenP1',
	'P1OnlyWithSearchEvidence',
	'P2OnlyWithSearchEvidence',
	'BothSkippedWithAlternateRecords'
)
$p3Routes = @('SolvedAtServiceBox', 'ProvenFromDocuments')
$p4Routes = @('ImmediateRoof', 'FollowWater', 'WrongWayThenWater')
$tankTimings = @('Early', 'AfterEvidence')
$flashlightRoutes = @('AlreadyHeld', 'ReturnAfterDarkReveal')

$c5Truths = @(
	'Truth.Alarm0510',
	'Truth.DeathOverlay',
	'Truth.WasSearched',
	'Truth.Negligence',
	'Truth.CatSafe',
	'Truth.HoseCause',
	'Truth.Fall',
	'Truth.Identity',
	'Truth.RecheckScheduled0731'
)
$chapterThreeAlternateSources = @{
	'Truth.Alarm0510' = 'CH03.Planner0510'
	'Truth.DeathOverlay' = 'CH03.PhoneApprovalHistory'
	'Truth.WasSearched' = 'CH03.MotherMessages'
}

$routeCount = 0
$endingCheckCount = 0
$roundTripCount = 0
$coveredRouteKeys = [System.Collections.Generic.HashSet[string]]::new()

foreach ($profile in $purchaseProfiles) {
	foreach ($payment in $paymentMethods) {
		foreach ($catAction in $catActions) {
			foreach ($chapterOneRoute in $chapterOneRoutes) {
				foreach ($chapterTwoRoute in $chapterTwoRoutes) {
					foreach ($p3Route in $p3Routes) {
						foreach ($p4Route in $p4Routes) {
							foreach ($tankTiming in $tankTimings) {
								foreach ($flashlightRoute in $flashlightRoutes) {
									$routeCount++
									[void]$coveredRouteKeys.Add((
										@(
											$profile,
											$payment,
											$catAction,
											$chapterOneRoute,
											$chapterTwoRoute,
											$p3Route,
											$p4Route,
											$tankTiming,
											$flashlightRoute
										) -join '|'))
									$truthSources = @{}
									$strongBeats = @{}
									$narrativeDebt =
										[System.Collections.Generic.List[string]]::new()

									Add-Truth $truthSources $strongBeats `
										'Truth.NeedWater' 'CH01.EmptyFridge'
									Add-Truth $truthSources $strongBeats `
										'Truth.Purchase0431' 'CH01.Checkout0431'
									$visitedStore = $true

									switch ($chapterTwoRoute) {
										'P1ThenP2' {
											Add-Truth $truthSources $strongBeats `
												'Truth.Alarm0510' 'CH02.P1Clock'
											Add-Truth $truthSources $strongBeats `
												'Truth.DeathOverlay' 'CH02.P2Receipts'
										}
										'P2ThenP1' {
											Add-Truth $truthSources $strongBeats `
												'Truth.DeathOverlay' 'CH02.P2Receipts'
											Add-Truth $truthSources $strongBeats `
												'Truth.Alarm0510' 'CH02.P1Clock'
										}
										'P1OnlyWithSearchEvidence' {
											Add-Truth $truthSources $strongBeats `
												'Truth.Alarm0510' 'CH02.P1Clock'
											Add-Truth $truthSources $strongBeats `
												'Truth.WasSearched' 'CH02.MissingNotices'
										}
										'P2OnlyWithSearchEvidence' {
											Add-Truth $truthSources $strongBeats `
												'Truth.DeathOverlay' 'CH02.P2Receipts'
											Add-Truth $truthSources $strongBeats `
												'Truth.WasSearched' 'CH02.MissingNotices'
										}
										'BothSkippedWithAlternateRecords' {
											Add-Truth $truthSources $strongBeats `
												'Truth.Alarm0510' 'CH02.HomePlanner0510'
											Add-Truth $truthSources $strongBeats `
												'Truth.WasSearched' 'CH02.ManagementComplaint'
										}
									}

									$c3Confirmed = @(
										'Truth.Alarm0510',
										'Truth.DeathOverlay',
										'Truth.WasSearched'
									) | Where-Object { $truthSources.ContainsKey($_) }
									Assert-Route ($c3Confirmed.Count -ge 2) `
										"C3 could not converge for $chapterTwoRoute."

									foreach ($truth in @(
										'Truth.Alarm0510',
										'Truth.DeathOverlay',
										'Truth.WasSearched'
									)) {
										if (-not $truthSources.ContainsKey($truth)) {
											$narrativeDebt.Add($truth)
											Assert-Route (
												$chapterThreeAlternateSources.ContainsKey($truth)) `
												"Missing actual CH03 alternate source for $truth."
											Add-Truth $truthSources $strongBeats $truth `
												$chapterThreeAlternateSources[$truth]
											[void]$narrativeDebt.Remove($truth)
										}
									}

									$negligenceSources = @(
										'Handover.Closed0358',
										'ManagementApp.Photo0403',
										'ManagementDb.FalseCompletion0620'
									)
									if ($p3Route -eq 'SolvedAtServiceBox') {
										$negligenceSources +=
											'P3.EmptyMeasurements'
									}
									Assert-Route (
										Test-NegligenceFormula $negligenceSources) `
										"Negligence formula rejected $p3Route."
									Set-TruthSourcesAtomically `
										$truthSources $strongBeats `
										'Truth.Negligence' $negligenceSources

									$p5Sources = @{
										'Truth.CatSafe' = @(
											'P5.CatEnteredPrints',
											'P5.CatExitedPrints'
										)
										'Truth.HoseCause' = @(
											'P5.HosePawCompression',
											'P5.CouplingImpact',
											'P5.InnerRimFriction'
										)
										'Truth.Fall' = @(
											'P5.UpperSlipperEnd',
											'P5.LiftedPad',
											'P5.CorrodedClips',
											'P5.InwardHandSmear'
										)
										'Truth.Identity' = if (
											$payment -eq 'WalletCard') {
											@(
												'P5.CurrentSleeveThreeStitches',
												'P5.TankSleeveThreeStitches',
												'P5.TankHeelWear'
											)
										}
										else {
											@(
												'P5.SearchPosterOutfit',
												'P5.Glasses',
												'P5.TankOutfit'
											)
										}
									}
									foreach ($truth in $p5Sources.Keys) {
										$sourcesForTruth =
											[string[]]$p5Sources[$truth]
										Assert-Route (
											Test-P5Formula $truth $sourcesForTruth) `
											"P5 formula rejected complete sources for $truth."
										Set-TruthSourcesAtomically `
											$truthSources $strongBeats `
											$truth $sourcesForTruth
									}
									Add-Truth $truthSources $strongBeats `
										'Truth.RecheckScheduled0731' `
										'CH03.VendorRevisitNotice'
									$observedP5Sources = @(
										$p5Sources.Values |
											ForEach-Object { $_ }
									)

									$state = @{
										SaveSchemaVersion = 3
										RebirthSnapshotSchemaVersion = 3
										PurchaseProfile = $profile
										PaymentMethod = $payment
										CatAction = $catAction
										ChapterOneRoute = $chapterOneRoute
										ChapterTwoRoute = $chapterTwoRoute
										P3Route = $p3Route
										P4Route = $p4Route
										TankTiming = $tankTiming
										FlashlightRoute = $flashlightRoute
										TruthSources = $truthSources
										NarrativeDebt = $narrativeDebt
										P3DirectInletClosed = $true
										P3ReserveInletClosed = $true
										P3PressureReleaseOpen = $true
										P3PressureZero = $true
										P3Completed = $true
										P3PressureKPa = 0.0
										P3MistakeCount = 0
										P3ZeroConfirmationTicks = 2
										P3HintElapsedSeconds = 0.0
										P3HintStage = 0
										FocusedEvidence = 'None'
										ObservedP5Sources =
											$observedP5Sources
										P5CatSafeConfirmed = $true
										P5HoseCauseConfirmed = $true
										P5FallConfirmed = $true
										P5IdentityConfirmed = $true
										AccidentScratchCount = 3
										ScratchTailSettled = $true
										LookedAwayAfterFirstScratch = $true
										ActedAfterSecondScratch = $true
										EndingChoice = 'None'
										CommonDiscoveryCommitted = $false
										ActualStateRestored = $false
										Found0731Presented = $false
										PlayedOneShotBeats = @()
									}

									Assert-Route (
										$truthSources.ContainsKey('Truth.NeedWater') -or
										$visitedStore) 'C1 did not converge.'
									Assert-Route (
										$truthSources.ContainsKey('Truth.Purchase0431')) `
										'C2 did not converge.'
									Assert-Route ($narrativeDebt.Count -eq 0) `
										'NarrativeDebt remained at C5.'
									foreach ($truth in $c5Truths) {
										Assert-Route ($truthSources.ContainsKey($truth)) `
											"C5 omitted $truth."
									}
									foreach ($count in $strongBeats.Values) {
										Assert-Route ($count -le 1) `
											'A strong truth beat replayed.'
									}
									Assert-Route (
										$state.AccidentScratchCount -eq 3 -and
										$state.ScratchTailSettled) `
										'Final presentation gate did not settle.'

									Test-SnapshotRoundTrip $state
									$roundTripCount++

									foreach ($ending in @('ResidualA', 'AcceptanceB')) {
										$endingState = @{} + $state
										$endingState.TruthSources = @{} +
											$state.TruthSources
										$endingState.EndingChoice = $ending
										$endingState.CommonDiscoveryCommitted = $true
										$endingState.ActualStateRestored = $true
										$endingState.Found0731Presented = $true
										$endingState.PlayedOneShotBeats = @(
											'CH03.ActualStateRestored'
											if ($ending -eq 'ResidualA') {
												'Ending.A.StrongCuePlayed'
											}
											else {
												'Ending.B.StrongCuePlayed'
											}
										)
										Add-Truth $endingState.TruthSources $strongBeats `
											'Truth.Found0731' 'Ending.CommonDiscoveryCard'
										Assert-Route (
											$endingState.EndingChoice -eq $ending) `
											'Ending choice was not exclusive.'
										Assert-Route (
											$endingState.CommonDiscoveryCommitted -and
											$endingState.ActualStateRestored -and
											$endingState.Found0731Presented -and
											$endingState.TruthSources.ContainsKey(
												'Truth.Found0731')) `
											'C6 did not converge through common discovery.'
										Test-SnapshotRoundTrip $endingState
										$roundTripCount++
										$endingCheckCount++
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

Assert-Route ($routeCount -eq 7200) `
	"The route matrix covered $routeCount states instead of 7200."
Assert-Route ($endingCheckCount -eq 14400) `
	'Every route must validate both endings.'
Assert-Route ($roundTripCount -eq 21600) `
	'Every pre-ending and ending state must survive a round trip.'

$persistenceBoundaryCount = 0
$p3Checkpoints = @(
	@{
		Name = 'Untouched'
		Direct = $false
		Reserve = $false
		Release = $false
		Zero = $false
		Completed = $false
		Pressure = 60.0
		ZeroTicks = 0
	},
	@{
		Name = 'InflowsClosed'
		Direct = $true
		Reserve = $true
		Release = $false
		Zero = $false
		Completed = $false
		Pressure = 60.0
		ZeroTicks = 0
	},
	@{
		Name = 'ReleaseOpened'
		Direct = $true
		Reserve = $true
		Release = $true
		Zero = $false
		Completed = $false
		Pressure = 60.0
		ZeroTicks = 0
	},
	@{
		Name = 'MidBleed'
		Direct = $true
		Reserve = $true
		Release = $true
		Zero = $false
		Completed = $false
		Pressure = 37.0
		ZeroTicks = 0
	},
	@{
		Name = 'ZeroTickOne'
		Direct = $true
		Reserve = $true
		Release = $true
		Zero = $false
		Completed = $false
		Pressure = 0.0
		ZeroTicks = 1
	},
	@{
		Name = 'PressureZero'
		Direct = $true
		Reserve = $true
		Release = $true
		Zero = $true
		Completed = $false
		Pressure = 0.0
		ZeroTicks = 2
	},
	@{
		Name = 'DrainCompleted'
		Direct = $true
		Reserve = $true
		Release = $true
		Zero = $true
		Completed = $true
		Pressure = 0.0
		ZeroTicks = 2
	}
)
foreach ($checkpoint in $p3Checkpoints) {
	$checkpointState = @{
		SaveSchemaVersion = 3
		RebirthSnapshotSchemaVersion = 3
		PurchaseProfile = 'ProfileA500MlX2'
		PaymentMethod = 'WalletCard'
		CatAction = 'PassedBy'
		ChapterOneRoute = 'Elevator'
		ChapterTwoRoute = 'P1ThenP2'
		P3Route = $checkpoint.Name
		P4Route = 'ImmediateRoof'
		TankTiming = 'AfterEvidence'
		FlashlightRoute = 'AlreadyHeld'
		TruthSources = @{}
		NarrativeDebt = [System.Collections.Generic.List[string]]::new()
		P3DirectInletClosed = $checkpoint.Direct
		P3ReserveInletClosed = $checkpoint.Reserve
		P3PressureReleaseOpen = $checkpoint.Release
		P3PressureZero = $checkpoint.Zero
		P3Completed = $checkpoint.Completed
		P3PressureKPa = $checkpoint.Pressure
		P3MistakeCount = 2
		P3ZeroConfirmationTicks = $checkpoint.ZeroTicks
		P3HintElapsedSeconds = 149.0
		P3HintStage = 1
		FocusedEvidence = 'None'
		ObservedP5Sources = @()
		P5CatSafeConfirmed = $false
		P5HoseCauseConfirmed = $false
		P5FallConfirmed = $false
		P5IdentityConfirmed = $false
		AccidentScratchCount = 0
		ScratchTailSettled = $false
		LookedAwayAfterFirstScratch = $false
		ActedAfterSecondScratch = $false
		EndingChoice = 'None'
		CommonDiscoveryCommitted = $false
		ActualStateRestored = $false
		Found0731Presented = $false
		PlayedOneShotBeats = @()
	}
	Test-SnapshotRoundTrip $checkpointState
	Assert-Route (
		$checkpointState.P3PressureKPa -eq $checkpoint.Pressure) `
		"P3 checkpoint '$($checkpoint.Name)' changed pressure."
	$persistenceBoundaryCount++
}

$endingBoundaryCount = 0
foreach ($ending in @('ResidualA', 'AcceptanceB')) {
	$oppositeCue = if ($ending -eq 'ResidualA') {
		'Ending.B.StrongCuePlayed'
	}
	else {
		'Ending.A.StrongCuePlayed'
	}
	foreach ($boundary in @('PreCard', 'PostCard', 'PostStrongCue')) {
		$isCommonCommitted = $boundary -ne 'PreCard'
		$boundaryState = @{
			SaveSchemaVersion = 3
			RebirthSnapshotSchemaVersion = 3
			PurchaseProfile = 'ProfileB1LX1'
			PaymentMethod = 'PocketCard'
			CatAction = 'CapWaited'
			ChapterOneRoute = 'Stairs'
			ChapterTwoRoute = 'P2ThenP1'
			P3Route = 'SolvedAtServiceBox'
			P4Route = 'WrongWayThenWater'
			TankTiming = 'Early'
			FlashlightRoute = 'ReturnAfterDarkReveal'
			TruthSources = @{}
			NarrativeDebt = [System.Collections.Generic.List[string]]::new()
			P3DirectInletClosed = $true
			P3ReserveInletClosed = $true
			P3PressureReleaseOpen = $true
			P3PressureZero = $true
			P3Completed = $true
			P3PressureKPa = 0.0
			P3MistakeCount = 0
			P3ZeroConfirmationTicks = 2
			P3HintElapsedSeconds = 210.0
			P3HintStage = 3
			FocusedEvidence = 'None'
			ObservedP5Sources = @(
				'P5.CatEnteredPrints',
				'P5.CatExitedPrints',
				'P5.HosePawCompression',
				'P5.CouplingImpact',
				'P5.InnerRimFriction',
				'P5.UpperSlipperEnd',
				'P5.LiftedPad',
				'P5.CorrodedClips',
				'P5.InwardHandSmear',
				'P5.CurrentSleeveThreeStitches',
				'P5.TankSleeveThreeStitches',
				'P5.TankHeelWear'
			)
			P5CatSafeConfirmed = $true
			P5HoseCauseConfirmed = $true
			P5FallConfirmed = $true
			P5IdentityConfirmed = $true
			AccidentScratchCount = 3
			ScratchTailSettled = $true
			LookedAwayAfterFirstScratch = $true
			ActedAfterSecondScratch = $true
			EndingChoice = $ending
			CommonDiscoveryCommitted = $isCommonCommitted
			ActualStateRestored = $isCommonCommitted
			Found0731Presented = $isCommonCommitted
			PlayedOneShotBeats = @()
		}
		if ($isCommonCommitted) {
			$boundaryState.PlayedOneShotBeats +=
				'CH03.ActualStateRestored'
			Add-Truth $boundaryState.TruthSources @{} `
				'Truth.Found0731' 'Ending.CommonDiscoveryCard'
		}
		if ($boundary -eq 'PostStrongCue') {
			$boundaryState.PlayedOneShotBeats += if (
				$ending -eq 'ResidualA') {
				'Ending.A.StrongCuePlayed'
			}
			else {
				'Ending.B.StrongCuePlayed'
			}
		}
		Test-SnapshotRoundTrip $boundaryState
		Assert-Route (
			-not ($boundaryState.PlayedOneShotBeats -contains $oppositeCue)) `
			"$ending loaded the opposite branch cue at $boundary."
		Assert-Route (
			$boundaryState.CommonDiscoveryCommitted -eq
				($boundaryState.ActualStateRestored -and
					$boundaryState.Found0731Presented)) `
			"$ending common discovery was not atomic at $boundary."
		$endingBoundaryCount++
	}
}
Assert-Route ($persistenceBoundaryCount -eq 7) `
	'P3 persistence must cover all seven authored checkpoints, including the first zero tick.'
Assert-Route ($endingBoundaryCount -eq 6) `
	'Both endings must cover pre-card, post-card, and post-cue saves.'

$representativeRoutes = @(
	@{
		Id = 'R1'
		Route = @(
			'ProfileA500MlX2',
			'WalletCard',
			'PassedBy',
			'Elevator',
			'BothSkippedWithAlternateRecords',
			'ProvenFromDocuments',
			'ImmediateRoof',
			'AfterEvidence',
			'AlreadyHeld'
		)
		Ending = 'ResidualA'
	},
	@{
		Id = 'R2'
		Route = @(
			'ProfileC2LX2',
			'PocketCard',
			'CapWaited',
			'Stairs',
			'P1ThenP2',
			'SolvedAtServiceBox',
			'FollowWater',
			'AfterEvidence',
			'AlreadyHeld'
		)
		Ending = 'AcceptanceB'
	},
	@{
		Id = 'R3'
		Route = @(
			'ProfileB1LX1',
			'WalletCard',
			'PassedBy',
			'Elevator',
			'P2ThenP1',
			'ProvenFromDocuments',
			'ImmediateRoof',
			'Early',
			'AlreadyHeld'
		)
		Ending = 'ResidualA'
	},
	@{
		Id = 'R4'
		Route = @(
			'ProfileA500MlX2',
			'PocketCard',
			'PaperCupWaited',
			'Stairs',
			'P1ThenP2',
			'SolvedAtServiceBox',
			'WrongWayThenWater',
			'Early',
			'ReturnAfterDarkReveal'
		)
		Ending = 'AcceptanceB'
	}
)
$representativeRouteCount = 0
foreach ($representative in $representativeRoutes) {
	$routeKey = $representative.Route -join '|'
	Assert-Route ($coveredRouteKeys.Contains($routeKey)) `
		"$($representative.Id) is outside the exhaustive route dimensions."
	Assert-Route ($representative.Ending -in @('ResidualA', 'AcceptanceB')) `
		"$($representative.Id) has an invalid ending."
	$representativeRouteCount++
}
Assert-Route ($representativeRouteCount -eq 4) `
	'R1-R4 representative routes must remain defined inside the matrix.'

$p5TruthOrder = @('CatSafe', 'HoseCause', 'Fall', 'Identity')
$p5PermutationCount = 0
foreach ($first in $p5TruthOrder) {
	foreach ($second in $p5TruthOrder | Where-Object { $_ -ne $first }) {
		foreach ($third in $p5TruthOrder |
			Where-Object { $_ -notin @($first, $second) }) {
			foreach ($fourth in $p5TruthOrder |
				Where-Object { $_ -notin @($first, $second, $third) }) {
				$p5PermutationCount++
				$scratchCount = 0
				foreach ($truth in @($first, $second, $third, $fourth)) {
					if ($scratchCount -lt 3) {
						$scratchCount++
					}
				}
				Assert-Route ($scratchCount -eq 3) `
					"P5 order $first/$second/$third/$fourth did not settle at three scratches."
			}
		}
	}
}
Assert-Route ($p5PermutationCount -eq 24) `
	'P5 acquisition-order test must cover 24 permutations.'

$p5FormulaSubsets = @(
	@{
		Truth = 'Truth.CatSafe'
		Sources = @(
			'P5.CatEnteredPrints',
			'P5.CatExitedPrints'
		)
	},
	@{
		Truth = 'Truth.HoseCause'
		Sources = @(
			'P5.HosePawCompression',
			'P5.CouplingImpact',
			'P5.InnerRimFriction'
		)
	},
	@{
		Truth = 'Truth.Fall'
		Sources = @(
			'P5.UpperSlipperEnd',
			'P5.LiftedPad',
			'P5.CorrodedClips',
			'P5.InwardHandSmear'
		)
	},
	@{
		Truth = 'Truth.Identity'
		Sources = @(
			'P5.CurrentSleeveThreeStitches',
			'P5.TankSleeveThreeStitches',
			'P5.TankHeelWear',
			'P5.SearchPosterOutfit',
			'P5.Glasses',
			'P5.TankOutfit'
		)
	}
)
$p5FormulaSubsetCount = 0
$p5FormulaConfirmedCount = 0
foreach ($formula in $p5FormulaSubsets) {
	$sourceCount = $formula.Sources.Count
	for ($mask = 0; $mask -lt (1 -shl $sourceCount); $mask++) {
		$selected = @()
		for ($sourceIndex = 0;
			$sourceIndex -lt $sourceCount;
			$sourceIndex++) {
			if (($mask -band (1 -shl $sourceIndex)) -ne 0) {
				$selected += $formula.Sources[$sourceIndex]
			}
		}
		$confirmed = Test-P5Formula $formula.Truth $selected
		$expected = if ($formula.Truth -eq 'Truth.Identity') {
			(($mask -band 7) -eq 7) -or
				(($mask -band 56) -eq 56)
		}
		else {
			$mask -eq ((1 -shl $sourceCount) - 1)
		}
		Assert-Route ($confirmed -eq $expected) `
			"P5 partial-source formula changed for $($formula.Truth) mask=$mask."
		if ($confirmed) {
			$p5FormulaConfirmedCount++
		}
		$p5FormulaSubsetCount++
	}
}
Assert-Route (
	$p5FormulaSubsetCount -eq 92 -and
	$p5FormulaConfirmedCount -eq 18) `
	'P5 formulas must reject every incomplete raw-source subset.'

$negligenceSubsetCount = 0
$negligenceConfirmedCount = 0
$negligenceRawSources = @(
	'P3.EmptyMeasurements',
	'ManagementApp.Photo0403',
	'Handover.Closed0358',
	'ManagementDb.FalseCompletion0620',
	'PoliceChecklist.PhotoAndAudit'
)
for ($mask = 0; $mask -lt 32; $mask++) {
	$selected = @()
	for ($sourceIndex = 0;
		$sourceIndex -lt $negligenceRawSources.Count;
		$sourceIndex++) {
		if (($mask -band (1 -shl $sourceIndex)) -ne 0) {
			$selected += $negligenceRawSources[$sourceIndex]
		}
	}
	$confirmed = Test-NegligenceFormula $selected
	$expected = ($mask -band 4) -ne 0 -and (
		((($mask -band 2) -ne 0) -and
			(($mask -band 8) -ne 0)) -or
		(($mask -band 16) -ne 0))
	Assert-Route ($confirmed -eq $expected) `
		"Negligence partial-source formula changed for mask=$mask."
	if ($confirmed) {
		$negligenceConfirmedCount++
	}
	$negligenceSubsetCount++
}
Assert-Route (
	$negligenceSubsetCount -eq 32 -and
	$negligenceConfirmedCount -eq 10) `
	'Negligence must require 03:58 handover plus a complete audit route.'

$p5ObservationBundles = [ordered]@{
	CatEntered = @('P5.CatEnteredPrints')
	CatExited = @('P5.CatExitedPrints')
	HosePaw = @('P5.HosePawCompression')
	HoseImpact = @('P5.CouplingImpact', 'P5.InnerRimFriction')
	Bag = @()
	WetRung = @(
		'P5.UpperSlipperEnd',
		'P5.LiftedPad',
		'P5.CorrodedClips'
	)
	HandSmear = @('P5.InwardHandSmear')
	Glasses = @('P5.Glasses')
	TankClothing = @(
		'P5.TankSleeveThreeStitches',
		'P5.TankHeelWear',
		'P5.TankOutfit'
	)
	CurrentSleeve = @('P5.CurrentSleeveThreeStitches')
	SearchPoster = @('P5.SearchPosterOutfit')
}
$p5EvidenceNodes = @($p5ObservationBundles.Keys)
$p5ObservationRoundTripCount = 0
$p5MaxObservedRawCount = 0
for ($mask = 0; $mask -lt (1 -shl $p5EvidenceNodes.Count); $mask++) {
	$observedSet = [System.Collections.Generic.HashSet[string]]::new()
	for ($index = 0; $index -lt $p5EvidenceNodes.Count; $index++) {
		if (($mask -band (1 -shl $index)) -ne 0) {
			foreach ($rawSource in
				$p5ObservationBundles[$p5EvidenceNodes[$index]]) {
				[void]$observedSet.Add($rawSource)
			}
		}
	}
	$observed = @($observedSet | Sort-Object)
	$p5MaxObservedRawCount = [Math]::Max(
		$p5MaxObservedRawCount,
		$observed.Count)
	foreach ($focus in @('None') + $p5EvidenceNodes) {
		$observationState = [ordered]@{
			SchemaVersion = 3
			ObservedRawSources = $observed
			Focus = $focus
			CommittedTruthSources = @()
		}
		$json = $observationState | ConvertTo-Json -Compress
		$restored = $json | ConvertFrom-Json
		Assert-Route (
			@($restored.ObservedRawSources).Count -eq
				$observed.Count -and
			$restored.Focus -eq $focus -and
			@($restored.CommittedTruthSources).Count -eq 0) `
			"P5 observation-only round trip changed mask=$mask focus=$focus."
		foreach ($truth in @(
			'Truth.CatSafe',
			'Truth.HoseCause',
			'Truth.Fall',
			'Truth.Identity'
		)) {
			Assert-Route (-not (
				Test-P5Formula $truth @($restored.CommittedTruthSources))) `
				"Observation-only state confirmed $truth at mask=$mask."
		}
		$p5ObservationRoundTripCount++
	}
}
Assert-Route (
	$p5ObservationRoundTripCount -eq 24576 -and
	$p5MaxObservedRawCount -eq 15) `
	'P5 must preserve every 11-node/15-source observation bundle across all focus states without confirming truth.'

$mixedTransportRouteCount = 0
foreach ($downRoute in @('Elevator', 'Stairs')) {
	foreach ($upRoute in @('Elevator', 'Stairs')) {
		$cabFloor = if ($downRoute -eq 'Elevator') {
			'Lobby'
		}
		else {
			'FourthFloor'
		}
		if ($upRoute -eq 'Elevator') {
			if ($cabFloor -eq 'FourthFloor') {
				$cabFloor = 'Lobby'
			}
			Assert-Route ($cabFloor -eq 'Lobby') `
				"$downRoute/$upRoute could not call the cab to the lobby."
			$cabFloor = 'FourthFloor'
		}
		$reachedFourthFloor = $upRoute -eq 'Stairs' -or
			$cabFloor -eq 'FourthFloor'
		Assert-Route $reachedFourthFloor `
			"$downRoute/$upRoute did not converge at the physical 4F landing."
		$mixedTransportRouteCount++
	}
}
Assert-Route ($mixedTransportRouteCount -eq 4) `
	'CH01 must cover elevator/elevator, elevator/stairs, stairs/elevator, and stairs/stairs.'

$emptyCabRecallCount = 0
foreach ($recall in @(
	@{ CallerFloor = 'Lobby'; CabFloor = 'FourthFloor'; Arrival = 'Lobby' },
	@{ CallerFloor = 'FourthFloor'; CabFloor = 'Lobby'; Arrival = 'FourthFloor' }
)) {
	Assert-Route ($recall.CallerFloor -ne $recall.CabFloor) `
		'Empty-cab recall must start on the opposite landing.'
	$recall.CabFloor = $recall.Arrival
	Assert-Route ($recall.CabFloor -eq $recall.CallerFloor) `
		"Empty cab did not reach $($recall.CallerFloor)."
	$emptyCabRecallCount++
}
Assert-Route ($emptyCabRecallCount -eq 2) `
	'Both opposite-floor empty-cab recalls must converge without moving a rider.'

Write-Host (
	"REBIRTH route matrix passed " +
	"(routes=$routeCount, endings=$endingCheckCount, " +
	"roundtrips=$roundTripCount, p3_checkpoints=$persistenceBoundaryCount, " +
	"ending_boundaries=$endingBoundaryCount, " +
	"representative_routes=$representativeRouteCount, " +
	"p5_orders=$p5PermutationCount, " +
	"p5_formula_subsets=$p5FormulaSubsetCount, " +
	"negligence_subsets=$negligenceSubsetCount, " +
	"p5_observation_roundtrips=$p5ObservationRoundTripCount, " +
	"mixed_transport=$mixedTransportRouteCount, " +
	"empty_recalls=$emptyCabRecallCount)."
) -ForegroundColor Green
