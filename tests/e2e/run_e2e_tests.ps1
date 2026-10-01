<#
.SYNOPSIS
    Runs the PPF-42 DYNAMICS Automated Headless E2E Test Suite.
.DESCRIPTION
    Executes all 345 opaque-box E2E test cases across Tiers 1-4.
.EXAMPLE
    .\run_e2e_tests.ps1
.EXAMPLE
    .\run_e2e_tests.ps1 -Tier 1
.EXAMPLE
    .\run_e2e_tests.ps1 -Verbose
#>

[CmdletBinding()]
param (
    [Parameter(Mandatory = $false)]
    [ValidateSet(1, 2, 3, 4)]
    [int]$Tier,

    [Parameter(Mandatory = $false)]
    [string]$Feature,

    [Parameter(Mandatory = $false)]
    [string]$JsonOut,

    [switch]$VerboseOutput
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RunnerScript = Join-Path $ScriptDir "run_e2e_tests.py"

$ArgsList = @($RunnerScript)

if ($PSBoundParameters.ContainsKey('Tier')) {
    $ArgsList += "--tier"
    $ArgsList += $Tier
}

if ($PSBoundParameters.ContainsKey('Feature')) {
    $ArgsList += "--feature"
    $ArgsList += $Feature
}

if ($PSBoundParameters.ContainsKey('JsonOut')) {
    $ArgsList += "--json-out"
    $ArgsList += $JsonOut
}

if ($VerboseOutput) {
    $ArgsList += "--verbose"
}

python @ArgsList
exit $LASTEXITCODE
