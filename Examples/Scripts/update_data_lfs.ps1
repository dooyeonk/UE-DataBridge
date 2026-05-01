# =====================================================================
# update_data_lfs.ps1 — Manual flow (lock + fetch + save)
#
# Lock 획득 → DataTable 갱신 → 사람이 직접 commit / push / unlock.
# 자동 커밋 안 함. 검토 후 의미 있는 메시지로 commit 하기 위함.
#
# CI/스케줄러는 update_data_lfs_ci.ps1 사용 (PR까지 자동).
# =====================================================================
param(
    [string]$SourceName = "All",
    [string]$Environment = "",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$UECmd = "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$ProjectPath = Resolve-Path "$PSScriptRoot\..\..\..\..\*.uproject" | Select-Object -First 1
if (-not $ProjectPath) { Write-Error "Could not find .uproject file"; exit 1 }

$ProjectRoot = Split-Path $ProjectPath -Parent

# ---------------------------------------------------------------------
# 1. DryRun → 영향받을 .uasset 파악
# ---------------------------------------------------------------------
$DryArgs = @($ProjectPath, "-run=DataBridgeUpdate", "-unattended", "-nopause", "-DryRun")
if ($SourceName -eq "All") { $DryArgs += "-All" } else { $DryArgs += "-SourceName=$SourceName" }
if ($Environment)          { $DryArgs += "-Environment=$Environment" }

$DryOutput = & $UECmd @DryArgs 2>&1
$AffectedFiles = $DryOutput | Select-String "Target: (.+)" | ForEach-Object {
    # /Game/Data/DT_Foo.DT_Foo  →  Content/Data/DT_Foo.uasset
    $Path = $_.Matches[0].Groups[1].Value.Trim()
    $Path = $Path -replace "^/Game/", "Content/"
    $Path = $Path -replace "\.[^./]+$", ".uasset"
    $Path
}

if ($AffectedFiles.Count -eq 0) {
    Write-Host "No changes detected. Nothing to lock."
    exit 0
}

if ($DryRun) {
    Write-Host $DryOutput
    exit 0
}

Push-Location $ProjectRoot
try {
    # -----------------------------------------------------------------
    # 2. LFS lock (변경될 파일에만)
    # -----------------------------------------------------------------
    $LockedFiles = @()
    $FailedLocks = @()
    foreach ($File in $AffectedFiles) {
        $Result = git lfs lock $File 2>&1
        if ($LASTEXITCODE -eq 0) {
            $LockedFiles += $File
        } else {
            $FailedLocks += $File
            Write-Warning "Failed to lock: $File ($Result)"
        }
    }

    if ($LockedFiles.Count -eq 0) {
        Write-Error "No files could be locked. Aborting."
        exit 1
    }

    # -----------------------------------------------------------------
    # 3. 실제 갱신
    # -----------------------------------------------------------------
    $UpdateArgs = @($ProjectPath, "-run=DataBridgeUpdate", "-unattended", "-nopause")
    if ($SourceName -eq "All") { $UpdateArgs += "-All" } else { $UpdateArgs += "-SourceName=$SourceName" }
    if ($Environment)          { $UpdateArgs += "-Environment=$Environment" }

    & $UECmd @UpdateArgs
    $UpdateExit = $LASTEXITCODE

    # 전체 실패(2) 또는 설정 오류(3)면 락 즉시 해제 (잡고 있을 가치 없음)
    if ($UpdateExit -ge 2) {
        Write-Error "Commandlet failed with exit code $UpdateExit — releasing locks"
        foreach ($File in $LockedFiles) { git lfs unlock $File 2>&1 | Out-Null }
        exit $UpdateExit
    }

    # -----------------------------------------------------------------
    # 4. 정지. 사람이 commit + unlock.
    # -----------------------------------------------------------------
    Write-Host ""
    Write-Host "==========================================================" -ForegroundColor Green
    Write-Host "  DataBridge update applied. Locks held until you commit." -ForegroundColor Green
    Write-Host "==========================================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Locked files:"
    $LockedFiles | ForEach-Object { Write-Host "  $_" }
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Yellow
    Write-Host "  1. Review changes in editor (Source Control panel)"
    Write-Host "  2. git add <files>"
    Write-Host "  3. git commit -m 'Update data: <meaningful message>'"
    Write-Host "  4. git push"
    Write-Host "  5. git lfs unlock <files>   (or: .\release_locks_after_merge.ps1)"
    Write-Host ""

    if ($FailedLocks.Count -gt 0) {
        Write-Host "Skipped (locked by others):" -ForegroundColor Yellow
        $FailedLocks | ForEach-Object { Write-Host "  - $_" }
        exit 1
    }
}
finally {
    Pop-Location
}

exit 0
