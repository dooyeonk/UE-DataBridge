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

# 1. DryRun으로 영향받을 .uasset 경로 파악
$DryArgs = @($ProjectPath, "-run=DataBridgeUpdate", "-unattended", "-nopause", "-DryRun")
if ($SourceName -eq "All") { $DryArgs += "-All" } else { $DryArgs += "-SourceName=$SourceName" }
if ($Environment) { $DryArgs += "-Environment=$Environment" }

$DryOutput = & $UECmd @DryArgs 2>&1
$AffectedFiles = $DryOutput | Select-String "Target: (.+)" | ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }

if ($AffectedFiles.Count -eq 0) {
    Write-Host "No changes detected. Exiting."
    exit 0
}

if ($DryRun) {
    Write-Host $DryOutput
    exit 0
}

# 2. Git LFS 락 시도
$LockedFiles = @()
$FailedLocks = @()

foreach ($File in $AffectedFiles) {
    $Result = git lfs lock $File 2>&1
    if ($LASTEXITCODE -eq 0) {
        $LockedFiles += $File
    } else {
        $FailedLocks += $File
        Write-Warning "Failed to lock: $File"
    }
}

if ($LockedFiles.Count -eq 0) {
    Write-Error "No files could be locked. Aborting."
    exit 1
}

# 3. 실제 갱신
try {
    $UpdateArgs = @($ProjectPath, "-run=DataBridgeUpdate", "-unattended", "-nopause")
    if ($SourceName -eq "All") { $UpdateArgs += "-All" } else { $UpdateArgs += "-SourceName=$SourceName" }
    if ($Environment) { $UpdateArgs += "-Environment=$Environment" }

    & $UECmd @UpdateArgs
    if ($LASTEXITCODE -ne 0) { throw "Commandlet failed with exit code $LASTEXITCODE" }

    # 4. 커밋 & 푸시
    git add $LockedFiles
    git commit -m "Auto-update DataTable assets: $SourceName"
    git push
}
finally {
    # 5. 락 해제 (항상)
    foreach ($File in $LockedFiles) {
        git lfs unlock $File
    }
}

# 6. 부분 실패 보고
if ($FailedLocks.Count -gt 0) {
    Write-Host "`nSkipped (locked by others):"
    $FailedLocks | ForEach-Object { Write-Host "  - $_" }
    exit 1
}

exit 0
