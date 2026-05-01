# DataBridge — Wrapper Scripts

DataBridge Commandlet을 VCS 환경에 맞게 호출하는 예제 스크립트 모음.
플러그인은 VCS 명령을 직접 호출하지 않으므로, 각 팀 환경에 맞게 수정해서 사용.

---

## 스크립트 목록

| 파일 | 환경 | 설명 |
|---|---|---|
| `update_data.ps1` | Windows (PowerShell) | 기본 갱신 스크립트 |
| `update_data.bat` | Windows (CMD) | 기본 갱신 스크립트 |
| `update_data.sh` | macOS / Linux | 기본 갱신 스크립트 |
| `update_data_lfs.ps1` | Windows + Git LFS | LFS 락 획득 후 갱신, 자동 커밋/푸시 |
| `update_data_perforce.bat` | Windows + Perforce | p4 checkout 후 갱신, submit |

---

## 공통 사전 설정

`UE_CMD` 경로를 실제 UE 설치 경로에 맞게 수정:

```
# PowerShell
$UECmd = "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

# Bash
UE_CMD="/opt/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd"
```

스크립트는 자신의 위치(`Plugins/DataBridge/Examples/Scripts/`)를 기준으로
상위 4단계에서 `.uproject` 파일을 자동 탐색합니다.

---

## 사용법

### 전체 소스 갱신

```powershell
.\update_data.ps1
.\update_data.ps1 -Environment Staging
```

```bash
./update_data.sh
./update_data.sh All Staging
```

### 단일 소스 갱신

```powershell
.\update_data.ps1 -SourceName WeaponStats
```

```bash
./update_data.sh WeaponStats
```

### 변경사항 미리보기 (DryRun)

```powershell
.\update_data.ps1 -DryRun
.\update_data_lfs.ps1 -SourceName WeaponStats -DryRun
```

---

## Git LFS 워크플로우 (`update_data_lfs.ps1`)

1. DryRun으로 영향받을 `.uasset` 파악
2. `git lfs lock` 으로 락 획득 시도
3. 락 성공한 파일만 Commandlet으로 갱신
4. `git commit` + `git push`
5. `finally` 블록에서 락 해제 보장

**락이 점유된 채 남은 경우 복구:**

```bash
git lfs locks                          # 현재 락 목록
git lfs unlock --force <file>          # 내 락 강제 해제
git lfs unlock --id=<id> --force       # 타인 락 강제 해제 (관리자)
```

---

## Exit Codes

Commandlet이 반환하는 종료 코드:

| Code | 의미 |
|---|---|
| `0` | 전체 성공 또는 변경 없음 |
| `1` | 일부 소스 실패 |
| `2` | 전체 실패 |
| `3` | 설정 오류 (소스 미등록 등) |
