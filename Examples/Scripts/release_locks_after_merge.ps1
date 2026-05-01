# =====================================================================
# release_locks_after_merge.ps1 — Unlock files after PR merge
#
# update_data_lfs_ci.ps1로 만든 PR이 머지된 뒤, 그 PR이 건드린 .uasset
# 파일들의 LFS 락을 해제. 머지 안 됐으면 unlock 안 함 (안전 가드).
#
# 사용:
#   .\release_locks_after_merge.ps1 -Branch databridge/auto-20260502-103045
#   .\release_locks_after_merge.ps1 -PRNumber 123
# =====================================================================
param(
    [string]$Branch = "",
    [int]$PRNumber = 0,
    [string]$BaseBranch = "main"
)

$ErrorActionPreference = "Stop"

$ProjectPath = Resolve-Path "$PSScriptRoot\..\..\..\..\*.uproject" | Select-Object -First 1
if (-not $ProjectPath) { Write-Error "Could not find .uproject file"; exit 1 }
$ProjectRoot = Split-Path $ProjectPath -Parent

if (-not $Branch -and $PRNumber -eq 0) {
    Write-Error "Specify -Branch <name> or -PRNumber <n>"
    exit 1
}

Push-Location $ProjectRoot
try {
    # -----------------------------------------------------------------
    # 1. PRNumber로 호출됐으면 gh CLI로 branch 이름 조회
    # -----------------------------------------------------------------
    if ($PRNumber -gt 0) {
        & gh --version > $null 2>&1
        if ($LASTEXITCODE -ne 0) { Write-Error "gh CLI required for -PRNumber mode"; exit 1 }

        $PRJson = gh pr view $PRNumber --json headRefName,merged,mergeCommit | ConvertFrom-Json
        if (-not $PRJson.merged) {
            Write-Error "PR #$PRNumber is not merged yet. Aborting."
            exit 1
        }
        $Branch = $PRJson.headRefName
    }

    # -----------------------------------------------------------------
    # 2. 머지 검증 — branch가 BaseBranch에 실제로 들어갔는지
    # -----------------------------------------------------------------
    git fetch origin $BaseBranch 2>&1 | Out-Null
    git fetch origin $Branch 2>&1 | Out-Null

    $MergeBase = git merge-base "origin/$BaseBranch" "origin/$Branch" 2>$null
    $BranchTip = git rev-parse "origin/$Branch" 2>$null

    if (-not $MergeBase -or -not $BranchTip) {
        Write-Error "Could not resolve branch refs (origin/$Branch, origin/$BaseBranch)"
        exit 1
    }

    # branch tip이 BaseBranch에 도달 가능한지 확인
    $InBase = git branch -r --contains $BranchTip | Select-String "origin/$BaseBranch"
    if (-not $InBase) {
        Write-Error "Branch '$Branch' has not been merged into '$BaseBranch'. Aborting."
        Write-Host "  → 머지 후 다시 실행하거나, 강제 해제는 git lfs unlock --force 사용"
        exit 1
    }

    # -----------------------------------------------------------------
    # 3. branch가 건드린 .uasset 파일 목록 추출
    # -----------------------------------------------------------------
    $Files = git diff --name-only "$MergeBase".."$BranchTip" |
             Where-Object { $_ -like "*.uasset" }

    if (-not $Files -or $Files.Count -eq 0) {
        Write-Host "No .uasset files modified by this branch. Nothing to unlock."
        exit 0
    }

    # -----------------------------------------------------------------
    # 4. unlock
    # -----------------------------------------------------------------
    $Released = @()
    $Failed = @()
    foreach ($File in $Files) {
        git lfs unlock $File 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            $Released += $File
        } else {
            $Failed += $File
        }
    }

    Write-Host ""
    Write-Host "Unlocked $($Released.Count) file(s):" -ForegroundColor Green
    $Released | ForEach-Object { Write-Host "  $_" }

    if ($Failed.Count -gt 0) {
        Write-Host ""
        Write-Host "Failed to unlock $($Failed.Count) file(s):" -ForegroundColor Yellow
        $Failed | ForEach-Object { Write-Host "  - $_" }
        Write-Host "  → 다른 사람의 락이거나 이미 해제됨. 강제 해제: git lfs unlock --force <file>"
        exit 1
    }
}
finally {
    Pop-Location
}

exit 0
