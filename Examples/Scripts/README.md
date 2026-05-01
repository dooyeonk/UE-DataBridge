# DataBridge — Wrapper Scripts

DataBridge Commandlet (`UnrealEditor-Cmd.exe -run=DataBridgeUpdate`)을 **VCS 환경에 맞게 호출**하는 예제 스크립트 모음.

플러그인 자체는 git/p4 같은 VCS 명령을 직접 실행하지 **않습니다**. 락 획득, 커밋, 푸시는 환경마다 다르기 때문에 wrapper가 책임집니다. 각 팀 환경에 맞게 수정해서 쓰세요.

---

## 처음 사용한다면 (3분 가이드)

### 1단계: UE 설치 경로 확인
스크립트 안의 `UECmd` 변수를 본인 환경 경로로 수정:

```powershell
# update_data.ps1 (Windows)
$UECmd = "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
```

```bash
# update_data.sh (macOS / Linux)
UE_CMD="/opt/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd"
# macOS: "/Users/Shared/Epic Games/UE_5.6/Engine/Binaries/Mac/UnrealEditor-Cmd"
```

### 2단계: 디렉토리 위치 확인
스크립트는 자기 위치(`Plugins/DataBridge/Examples/Scripts/`)에서 **상위 4단계** 올라가서 `.uproject` 파일을 찾습니다:

```
MyProject/
├── MyProject.uproject              ← 자동 탐색 대상
└── Plugins/
    └── DataBridge/
        └── Examples/
            └── Scripts/
                └── update_data.ps1 ← 스크립트 위치
```

다른 위치에서 실행하려면 스크립트 안의 `$ProjectPath` 부분을 직접 수정하세요.

### 3단계: 첫 실행 (DryRun으로 안전 테스트)
실제 `.uasset`을 건드리기 전에 변경사항만 미리 봅니다:

```powershell
.\update_data.ps1 -DryRun
```

```bash
./update_data.sh All "" --dryrun
```

성공 시 다음과 비슷한 출력이 나옵니다:

```
[DataBridge] Source: WeaponStats
  URL:    https://script.google.com/.../exec?sheet=Weapons
  Target: /Game/Data/DT_WeaponStats

  Changes detected:
    + Added   (1): Sword_Legendary
    ~ Modified(2):
        Sword_Common (ATK: 10 → 12)
        Wand_Basic (Range: 800 → 850)
  [DryRun] No changes saved.
```

문제 없어 보이면 `-DryRun` 빼고 실제 실행하세요.

---

## 스크립트 목록

| 파일 | 환경 | 용도 |
|---|---|---|
| **`update_data.ps1`** | Windows (PowerShell, 권장) | 가장 단순한 갱신 — VCS 통합 없음 |
| `update_data.bat` | Windows (CMD) | PowerShell 못 쓰는 환경 |
| `update_data.sh` | macOS / Linux | 동등한 셸 버전 |
| **`update_data_lfs.ps1`** | Windows + Git LFS | LFS 락 획득 → 갱신 → 자동 commit/push |
| `update_data_perforce.bat` | Windows + Perforce | `p4 edit` → 갱신 → `p4 submit` |

> **추천 시작점**: VCS 통합 없는 환경이면 `update_data.ps1`. Git LFS 쓰면 `update_data_lfs.ps1`.

---

## 사용법 — 시나리오별

### A) 단순 갱신 (VCS 자동화 없음)

가장 흔한 케이스. 디자이너가 시트 수정 → 엔지니어가 수동 실행 → 직접 commit:

```powershell
.\update_data.ps1
.\update_data.ps1 -SourceName WeaponStats          # 단일 소스만
.\update_data.ps1 -Environment Staging              # 환경 override
.\update_data.ps1 -DryRun                           # 미리보기
```

```bash
./update_data.sh                                    # 전체
./update_data.sh WeaponStats                        # 단일
./update_data.sh All Staging                        # 환경 지정
./update_data.sh All "" --dryrun                    # 미리보기
```

실행 후 결과:
- 변경 있음 → `.uasset` 갱신됨, exit 0
- 변경 없음 → 아무 일 없음, exit 0
- 일부 실패 → 성공한 소스만 갱신, exit 1
- 전체 실패 → 갱신 없음, exit 2
- 설정 오류 → 갱신 없음, exit 3

### B) Git LFS 자동화 (`update_data_lfs.ps1`)

LFS로 `.uasset`을 관리하면 락(lock) 충돌이 흔합니다. 이 스크립트는:

1. **DryRun으로 영향받을 `.uasset` 파악**
2. **각 파일에 `git lfs lock` 시도**
3. **락 성공한 파일만** Commandlet에 넘김 (`-Filter` 사용)
4. **갱신 → `git add` → `git commit` → `git push`**
5. **`finally` 블록에서 락 해제** (실패해도 무조건 풀림)
6. **락 못 딴 파일은 stderr로 보고** + exit 1

사용:
```powershell
.\update_data_lfs.ps1 -SourceName WeaponStats
.\update_data_lfs.ps1                              # 전체
```

### C) Perforce (`update_data_perforce.bat`)

`p4 edit`로 체크아웃 → 갱신 → `p4 submit`. Perforce 클라이언트 워크스페이스 안에서 실행.

---

## Exit Code

Commandlet이 반환하는 값. Wrapper가 이걸 보고 retry / 알림 등을 결정:

| Code | 의미 | Wrapper 권장 동작 |
|---|---|---|
| `0` | 전체 성공 또는 변경 없음 | 정상 종료 |
| `1` | **일부** 소스 실패 | 성공한 부분은 commit, 실패 목록 알림 |
| `2` | **전체** 실패 (네트워크 등) | retry 또는 즉시 알림 |
| `3` | 설정 오류 (소스 미등록, ini 잘못됨 등) | 코드 수정 필요 — retry 무의미 |

---

## 흔한 문제

### "Could not find .uproject file"
→ 스크립트 위치가 `Plugins/DataBridge/Examples/Scripts/` 아닐 때 발생. `$ProjectPath` 변수를 직접 수정하거나 스크립트를 표준 위치로 이동.

### "Source not registered: XXX"
→ `Config/DefaultGame.ini`에 등록 안 됨. Project Settings의 DataBridge → Sources에서 추가. 이름 대소문자 구분.

### "HTTP fetch failed"
→ URL 접근 불가 / 타임아웃. 브라우저로 같은 URL을 직접 열어 확인. 응답이 1MB 넘으면 `RequestTimeoutSeconds` 늘리기.

### "Failed to load table"
→ `TablePath` 잘못 설정됨. 형식: `/Game/Data/DT_Foo.DT_Foo` (앞뒤 같은 이름). Content Browser에서 우클릭 → "Copy Reference"로 가져와 `_C` 접미사만 제거.

### Git LFS: "Failed to lock"
→ 다른 사람이 같은 파일에 락을 걸어놓은 상태. `git lfs locks`로 확인:
```bash
git lfs locks                          # 현재 락 목록
git lfs unlock --force <file>          # 자기 락 강제 해제
git lfs unlock --id=<id> --force       # 타인 락 강제 해제 (관리자 권한)
```

### 스크립트 중간에 Ctrl+C로 끊어버린 경우 (LFS 락이 남음)
→ 위와 동일한 `git lfs unlock --force` 명령으로 해제. 스크립트의 `finally` 블록은 정상 종료/실패엔 동작하지만 강제 종료엔 호출 안 됨.

---

## 커스터마이징 가이드

### Slack 알림 추가 예시 (PowerShell)

```powershell
# update_data.ps1 마지막에 추가
$Body = if ($LASTEXITCODE -eq 0) { "DataBridge update OK" } else { "DataBridge update FAILED ($LASTEXITCODE)" }
Invoke-RestMethod -Uri $env:SLACK_WEBHOOK_URL -Method Post -Body (@{text=$Body} | ConvertTo-Json) -ContentType "application/json"
```

### CI에서 PR 코멘트 자동화

`update_data.ps1 -DryRun`의 stdout을 캡처해 그대로 PR 코멘트로 붙이면 됩니다 (Diff 출력이 이미 사람이 읽기 좋은 포맷).

```yaml
# GitHub Actions 예시
- name: DataBridge DryRun
  id: dryrun
  run: |
    OUTPUT=$(./update_data.sh All "" --dryrun)
    echo "diff<<EOF" >> $GITHUB_OUTPUT
    echo "$OUTPUT" >> $GITHUB_OUTPUT
    echo "EOF" >> $GITHUB_OUTPUT

- name: Comment PR
  uses: actions/github-script@v7
  with:
    script: |
      github.rest.issues.createComment({
        issue_number: context.issue.number,
        owner: context.repo.owner,
        repo: context.repo.repo,
        body: "```\n${{ steps.dryrun.outputs.diff }}\n```"
      })
```

### 다른 VCS (SVN, Plastic SCM)

`update_data_lfs.ps1` 구조를 그대로 따라가면 됩니다:
1. 락 획득 단계 → SVN `svn lock` 또는 Plastic `cm lock`
2. Commandlet 실행
3. 커밋 단계 → 각 VCS 명령
4. `finally`에서 락 해제

---

## 참고

플러그인 메인 문서: [../../README.md](../../README.md)
Commandlet 파라미터 전체: 메인 README의 **Editor Integration → Commandlet** 섹션
