param(
    [string]$SourceName = "All",
    [string]$Environment = "",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$UECmd = "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$ProjectPath = Resolve-Path "$PSScriptRoot\..\..\..\..\*.uproject" | Select-Object -First 1

if (-not $ProjectPath) {
    Write-Error "Could not find .uproject file"
    exit 1
}

$Args = @($ProjectPath, "-run=DataBridgeUpdate", "-unattended", "-nopause")

if ($SourceName -eq "All") {
    $Args += "-All"
} else {
    $Args += "-SourceName=$SourceName"
}

if ($Environment) {
    $Args += "-Environment=$Environment"
}

if ($DryRun) {
    $Args += "-DryRun"
}

Write-Host "Running DataBridge update: $SourceName"
& $UECmd @Args
exit $LASTEXITCODE
