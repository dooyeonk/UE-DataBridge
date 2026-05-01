# DataBridge

> **Unreal Engine 5.6+** runtime data fetch plugin.
> 외부 소스(Google Sheets, REST API 등)에서 받은 JSON/CSV로 `UDataTable` / `UCurveTable`을 빌드/패치 없이 실시간 갱신.

---

## 왜 만들었나

UE 게임 개발에서 수치 데이터(무기 스탯, 강화 테이블, 가챠율 등)는 보통 `.uasset` DataTable에 박혀 있습니다.
- 기획자가 값 하나 바꾸려면 → 엔지니어가 빌드 → QA → 패치
- 라이브 서비스 중 밸런스 핫픽스가 어려움
- A/B 테스트 / 시즌별 이벤트 데이터 동적 적용이 까다로움

**DataBridge**는 시트 → 서버(JSON/CSV) → 게임 흐름을 1줄 호출로 묶어, **빌드 없이 다음 게임 실행 시 자동 반영**되도록 합니다.

---

## 핵심 기능

| | |
|---|---|
| **1줄 호출** | `Subsystem->FetchSource("WeaponStats")` 만으로 fetch + 파싱 + 적용 |
| **DataTable + CurveTable** | 둘 다 1차 지원 (JSON / CSV) |
| **환경 분기** | Local / Development / Staging / Production — Source마다 URL 따로 설정 |
| **폴백 우아함** | 네트워크/파싱 실패 시 기존 `.uasset` 값 보존 (게임 크래시 X) |
| **캐싱** | TTL 기반, PIE 재시작 후에도 유지 (개발 편의 옵션) |
| **Editor Commandlet** | 헤드리스 `.uasset` 갱신 — CI / 자동화 / 로컬 스크립트 |
| **Row-level diff** | `.uasset` 가짜 변경 회피, DryRun으로 PR 코멘트 자동화 |
| **확장 포인트** | `IDataBridgeParser` 인터페이스로 커스텀 포맷 추가 |
| **Blueprint 노출** | 모든 핵심 API + delegate BP 호출 가능 |
| **콘솔 명령** | `DataBridge.RefreshAll`, `DataBridge.SetEnvironment` 등 |

---

## 아키텍처

```
DataBridge/
├── DataBridge.uplugin
└── Source/
    ├── DataBridge/                    # Runtime 모듈
    │   ├── Public/
    │   │   ├── Core/
    │   │   │   ├── DataBridgeSubsystem.h        # 핵심 API (UGameInstanceSubsystem)
    │   │   │   ├── DataBridgeSettings.h         # UDeveloperSettings
    │   │   │   └── DataBridgeTypes.h            # 공용 enum/struct
    │   │   ├── Http/
    │   │   │   ├── IDataBridgeHttpClient.h      # HTTP 추상화 (mock 가능)
    │   │   │   └── DataBridgeHttpClient.h
    │   │   ├── Interfaces/
    │   │   │   └── IDataBridgeParser.h          # 파서 확장 포인트
    │   │   └── Utilities/
    │   │       ├── DataBridgeLibrary.h          # BP 정적 헬퍼
    │   │       └── DataBridgeDiff.h             # Row-level diff
    │   └── Private/
    │       └── Parsers/
    │           ├── DataBridgeJsonDataTableParser.cpp
    │           ├── DataBridgeCsvDataTableParser.cpp
    │           ├── DataBridgeJsonCurveTableParser.cpp
    │           └── DataBridgeCsvCurveTableParser.cpp
    │
    └── DataBridgeEditor/              # Editor 모듈
        ├── Public/
        │   ├── DataBridgeToolbar.h              # 툴바 메뉴
        │   └── DataBridgeUpdateCommandlet.h     # 헤드리스 갱신
        └── Private/...
```

### Module 의존성

```
DataBridge        → Core, CoreUObject, Engine, HTTP, Json, JsonUtilities, DeveloperSettings
DataBridgeEditor  → DataBridge + UnrealEd, Slate, ToolMenus, Settings, HTTP
```

---

## Quick Start

### 1. 플러그인 활성화
`MyProject.uproject`에 추가:
```json
"Plugins": [
    { "Name": "DataBridge", "Enabled": true }
]
```

### 2. Source 등록 (`Config/DefaultGame.ini`)
```ini
[/Script/DataBridge.DataBridgeSettings]
CurrentEnvironment=Local
RequestTimeoutSeconds=10.0
RetryCount=2

+Sources=(SourceName="WeaponStats", \
    URLs=((Local,"http://localhost:3000/weapons"), \
          (Production,"https://api.example.com/weapons")), \
    TablePath="/Game/Data/DT_WeaponStats.DT_WeaponStats", \
    Format=Json, \
    CacheTTLSeconds=300.0)
```

또는 Project Settings → DataBridge UI에서 등록.

### 3. 호출 (C++)
```cpp
void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();

    UDataBridgeSubsystem* DB = GetGameInstance()->GetSubsystem<UDataBridgeSubsystem>();
    DB->OnFetchCompleted.AddDynamic(this, &AMyGameMode::HandleFetchDone);
    DB->FetchSource(FName("WeaponStats"));
}

void AMyGameMode::HandleFetchDone(FName Src, bool bSuccess, const FString& Err)
{
    UE_LOG(LogTemp, Log, TEXT("Fetch %s: %s"), *Src.ToString(),
        bSuccess ? TEXT("OK") : *Err);
}
```

### 3'. 호출 (Blueprint)
```
[Get Data Bridge Subsystem] → [Fetch Source: "WeaponStats"]
[Bind Event to On Fetch Completed]
```

---

## Configuration

`UDataBridgeSettings` (Project Settings → DataBridge):

| 카테고리 | 설정 | 기본값 | 설명 |
|---|---|---|---|
| Environment | `CurrentEnvironment` | Local | URL 분기 기준 |
| Sources | `Sources[]` | — | (SourceName, URL맵, TablePath, Format, CacheTTL) |
| Network | `RequestTimeoutSeconds` | 10.0 | HTTP 타임아웃 |
| Network | `RetryCount` | 2 | 재시도 횟수 (5xx + 네트워크 실패 시) |
| Network | `RetryDelaySeconds` | 1.0 | 재시도 간격 |
| Development | `bEnablePIECache` | true | PIE 재시작 후에도 캐시 유지 |

---

## Console Commands

```
DataBridge.RefreshAll                              # 전체 재fetch (캐시 무시)
DataBridge.Refresh <SourceName>                    # 단일 소스
DataBridge.SetEnvironment <Local|Staging|...>      # 런타임 환경 전환
DataBridge.PrintSources                            # 등록 목록 + 캐시 상태
DataBridge.InvalidateCache [SourceName]            # 캐시 무효화 (인자 없으면 전체)
```

---

## Editor Integration

### 툴바 메뉴
Level Editor 메뉴바 → **DataBridge** 드롭다운:
- `Refresh All Sources` — 모든 소스 fetch + `.uasset` 갱신 (PIE 불필요)
- `Refresh Source ▶` — 등록된 소스별 서브메뉴
- `Open Settings` — Project Settings의 DataBridge 섹션 열기

결과는 Slate notification으로 표시.

### Commandlet (CI / 자동화)
```bash
UnrealEditor-Cmd.exe MyProject.uproject -run=DataBridgeUpdate -All -Environment=Staging
```

| Parameter | Description |
|---|---|
| `-SourceName=<n>` | 단일 소스 갱신 |
| `-All` | 전체 갱신 |
| `-Environment=<env>` | 환경 override |
| `-DryRun` | 변경사항만 출력, 저장 X |
| `-Filter=<a,b>` | `-All`과 결합해 일부만 처리 |

**Exit code**: 0 success / 1 partial / 2 total fail / 3 config error

### DryRun Diff 출력 예시
```
[DataBridge] Source: WeaponStats
  URL:    https://script.google.com/.../exec?sheet=Weapons
  Target: /Game/Data/DT_WeaponStats

  Changes detected:
    + Added   (2): Sword_Legendary, Wand_Mythic
    ~ Modified (3):
        Sword_Common (ATK: 10 → 12)
        Wand_Basic (ATK: 5 → 6, Range: 800 → 850)
        Greatsword_Heavy (ATK: 25 → 30)
    - Removed (1): Sword_Deprecated
```

CI에서 `stdout` 캡처해 PR 코멘트로 그대로 붙일 수 있음.

---

## 설계 결정 (Design Decisions)

### 1. **Subsystem-based, GameInstance scope**
fetch 상태/캐시는 게임 세션 단위. `UGameInstanceSubsystem`이 라이프사이클 깔끔. PIE 재시작 시에도 자연스럽게 재초기화.

### 2. **HTTP 추상화 (`IDataBridgeHttpClient`)**
실제 HTTP 호출은 인터페이스 뒤에. 테스트에서 `FMockHttpClient`로 갈아끼울 수 있음 — 외부 네트워크 의존 없는 unit test 가능.

### 3. **파서 확장 포인트 (`IDataBridgeParser`)**
JSON/CSV 외 포맷(MessagePack, YAML 등) 지원이 필요해도 플러그인 코드 수정 없이 `RegisterParser()`로 추가. Format name으로 라우팅.

### 4. **TempTable 기반 fallback**
파싱 실패 시 기존 `.uasset` 값 보존. UE의 `CreateTableFromJSONString`은 RowMap을 먼저 비우고 파싱하므로, **TempTable로 검증 → 성공 시에만 TargetTable에 적용**하는 패턴 사용. 잘못된 시트가 와도 게임은 진행됨.

### 5. **Row-level diff로 가짜 변경 제거**
`.uasset`은 직렬화 시 미세한 바이너리 노이즈가 발생 — 매번 재저장하면 VCS가 폭발. `UScriptStruct::CompareScriptStruct(PPF_None)`로 USTRUCT 멤버 단위 비교, **변경된 row가 있을 때만 저장**.

### 6. **Process-level 캐시 (PIE persistence)**
`bEnablePIECache=true`이면 캐시를 process-level static에 보관. PIE 재시작해도 살아 있어서 같은 시트를 매번 다시 받지 않음. 개발 중 빠른 이터레이션용.

### 7. **환경 분기는 Source 단위 URL 맵**
`URLs: { Local: "...", Staging: "...", Production: "..." }` — 환경 전환은 URL 선택만 바꿔주면 됨. CurrentEnvironment 값 하나로 전체 fetch 동작이 바뀜.

---

## 테스트 가능성

`IDataBridgeHttpClient`로 HTTP 추상화 → 외부 네트워크 없이 unit test 가능:

```cpp
class FMockHttpClient : public IDataBridgeHttpClient
{
    TMap<FString, FString> Responses;
    virtual void Get(const FString& URL, FOnHttpResponse Callback) override
    {
        Callback(true, Responses.FindRef(URL), 200);
    }
};
```

`UDataBridgeSubsystem`에 `SetHttpClient()` 주입 후 fetch → DataTable 검증.

---

## 명시적으로 안 만든 것 (Non-goals)

스펙을 좁게 잡는 것도 설계의 일부. 다음은 의도적 제외:

- **POST/PUT (역방향 push)** — Source of Truth 모델이 깨짐. 외부 도구로 처리.
- **Asset Reference 동적 로드** (Mesh/Class) — UE Asset 시스템과 충돌. Asset Manager 영역.
- **Localization (FText)** — String Table과 결합. 별도 워크플로우.
- **인증 시스템** (OAuth 등) — 사용자가 외부에서 처리. 플러그인은 메커니즘만.
- **DB 직결** (MongoDB/PostgreSQL) — HTTP 추상화 유지. DB는 사용자가 API 서버 구축.

---

## 요구 사항

- **Unreal Engine 5.6+**
- C++ 프로젝트 (C++ 모듈 포함 시 BP 전용 프로젝트도 가능)
- HTTPS 사용 시 OS 기본 인증서

