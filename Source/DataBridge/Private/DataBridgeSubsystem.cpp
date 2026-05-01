#include "DataBridgeSubsystem.h"
#include "DataBridgeLog.h"
#include "DataBridgeSettings.h"
#include "DataBridgeHttpClient.h"
#include "Parsers/DataBridgeJsonDataTableParser.h"
#include "Parsers/DataBridgeCsvDataTableParser.h"
#include "Parsers/DataBridgeJsonCurveTableParser.h"
#include "Engine/DataTable.h"
#include "Engine/CurveTable.h"
#include "HAL/IConsoleManager.h"

void UDataBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UDataBridgeSettings* Settings = GetDefault<UDataBridgeSettings>();
	CurrentEnvironment = Settings->CurrentEnvironment;

	HttpClient = MakeShared<FDataBridgeHttpClient>(
		Settings->RequestTimeoutSeconds,
		Settings->RetryCount,
		Settings->RetryDelaySeconds);

	RegisterParser(MakeShared<FDataBridgeJsonDataTableParser>());
	RegisterParser(MakeShared<FDataBridgeCsvDataTableParser>());
	RegisterParser(MakeShared<FDataBridgeJsonCurveTableParser>());

	RegisterConsoleCommands();

	UE_LOG(LogDataBridge, Log, TEXT("DataBridgeSubsystem initialized (env: %d)"), (int32)CurrentEnvironment);
}

void UDataBridgeSubsystem::Deinitialize()
{
	for (IConsoleCommand* Cmd : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}
	ConsoleCommands.Empty();

	HttpClient.Reset();
	Parsers.Empty();
	Cache.Empty();

	Super::Deinitialize();
}

void UDataBridgeSubsystem::RegisterParser(TSharedPtr<IDataBridgeParser> Parser)
{
	if (Parser.IsValid())
	{
		Parsers.Add(Parser->GetFormatName(), Parser);
	}
}

// ============================================================
// Direct API
// ============================================================

void UDataBridgeSubsystem::FetchTable(const FString& URL, UDataTable* TargetTable, EDataBridgeFormat Format)
{
	FetchTableInternal(NAME_None, URL, TargetTable, Format);
}

void UDataBridgeSubsystem::FetchCurveTable(const FString& URL, UCurveTable* TargetTable, EDataBridgeFormat Format)
{
	FetchCurveTableInternal(NAME_None, URL, TargetTable, Format);
}

// ============================================================
// Source-based API
// ============================================================

void UDataBridgeSubsystem::FetchSource(FName SourceName)
{
	FetchSourceWithCallback(SourceName, nullptr);
}

void UDataBridgeSubsystem::FetchAllSources()
{
	const UDataBridgeSettings* Settings = GetDefault<UDataBridgeSettings>();

	if (Settings->Sources.IsEmpty())
	{
		OnAllSourcesCompleted.Broadcast(true, 0);
		return;
	}

	const int32 Total = Settings->Sources.Num();
	TSharedPtr<int32> Remaining = MakeShared<int32>(Total);
	TSharedPtr<int32> FailCount = MakeShared<int32>(0);

	for (const FDataBridgeSource& Source : Settings->Sources)
	{
		FetchSourceWithCallback(Source.SourceName, [this, Remaining, FailCount](bool bSuccess)
		{
			if (!bSuccess)
			{
				(*FailCount)++;
			}
			if (--(*Remaining) == 0)
			{
				OnAllSourcesCompleted.Broadcast(*FailCount == 0, *FailCount);
			}
		});
	}
}

// ============================================================
// Cache
// ============================================================

void UDataBridgeSubsystem::InvalidateCache(FName SourceName)
{
	Cache.Remove(MakeCacheKey(SourceName));
	UE_LOG(LogDataBridge, Log, TEXT("Cache invalidated: %s"), *SourceName.ToString());
}

void UDataBridgeSubsystem::InvalidateAllCache()
{
	Cache.Empty();
	UE_LOG(LogDataBridge, Log, TEXT("All cache invalidated"));
}

// ============================================================
// Environment
// ============================================================

void UDataBridgeSubsystem::SetEnvironment(EDataBridgeEnvironment NewEnvironment)
{
	CurrentEnvironment = NewEnvironment;
	UE_LOG(LogDataBridge, Log, TEXT("Environment changed to %d"), (int32)NewEnvironment);
}

// ============================================================
// Internal
// ============================================================

void UDataBridgeSubsystem::RegisterConsoleCommands()
{
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("DataBridge.RefreshAll"),
		TEXT("Fetch all registered sources, ignoring cache"),
		FConsoleCommandDelegate::CreateLambda([this]()
		{
			InvalidateAllCache();
			FetchAllSources();
		}),
		ECVF_Default
	));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("DataBridge.Refresh"),
		TEXT("Fetch a single source by name, ignoring cache. Usage: DataBridge.Refresh <SourceName>"),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
		{
			if (Args.IsEmpty())
			{
				UE_LOG(LogDataBridge, Warning, TEXT("DataBridge.Refresh: SourceName argument required"));
				return;
			}
			FName SourceName = FName(*Args[0]);
			InvalidateCache(SourceName);
			FetchSource(SourceName);
		}),
		ECVF_Default
	));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("DataBridge.SetEnvironment"),
		TEXT("Switch environment at runtime. Usage: DataBridge.SetEnvironment <Local|Development|Staging|Production>"),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
		{
			if (Args.IsEmpty())
			{
				UE_LOG(LogDataBridge, Warning, TEXT("DataBridge.SetEnvironment: environment argument required"));
				return;
			}
			const FString& EnvStr = Args[0];
			EDataBridgeEnvironment NewEnv = EDataBridgeEnvironment::Local;
			if (EnvStr == TEXT("Development"))       NewEnv = EDataBridgeEnvironment::Development;
			else if (EnvStr == TEXT("Staging"))      NewEnv = EDataBridgeEnvironment::Staging;
			else if (EnvStr == TEXT("Production"))   NewEnv = EDataBridgeEnvironment::Production;
			SetEnvironment(NewEnv);
			UE_LOG(LogDataBridge, Log, TEXT("Environment set to: %s"), *EnvStr);
		}),
		ECVF_Default
	));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("DataBridge.PrintSources"),
		TEXT("Print all registered sources with current environment and cache status"),
		FConsoleCommandDelegate::CreateLambda([this]()
		{
			const UDataBridgeSettings* Settings = GetDefault<UDataBridgeSettings>();
			UE_LOG(LogDataBridge, Log, TEXT("=== DataBridge Sources (env: %d) ==="), (int32)CurrentEnvironment);
			for (const FDataBridgeSource& Source : Settings->Sources)
			{
				const bool bCached = IsCacheValid(Source.SourceName, Source.CacheTTLSeconds);
				UE_LOG(LogDataBridge, Log, TEXT("  [%s] table=%s cached=%s"),
					*Source.SourceName.ToString(),
					*Source.TablePath.ToString(),
					bCached ? TEXT("YES") : TEXT("NO"));
			}
		}),
		ECVF_Default
	));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("DataBridge.InvalidateCache"),
		TEXT("Invalidate cache. Usage: DataBridge.InvalidateCache [SourceName] (omit for all)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
		{
			if (Args.IsEmpty())
			{
				InvalidateAllCache();
			}
			else
			{
				InvalidateCache(FName(*Args[0]));
			}
		}),
		ECVF_Default
	));
}

void UDataBridgeSubsystem::FetchSourceWithCallback(FName SourceName, TFunction<void(bool)> OnComplete)
{
	const UDataBridgeSettings* Settings = GetDefault<UDataBridgeSettings>();
	const FDataBridgeSource* Source = Settings->FindSource(SourceName);

	if (!Source)
	{
		FString Error = FString::Printf(TEXT("Source not registered: %s"), *SourceName.ToString());
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
		if (OnComplete) OnComplete(false);
		return;
	}

	// 캐시 hit 확인
	if (Source->CacheTTLSeconds > 0.0f && IsCacheValid(SourceName, Source->CacheTTLSeconds))
	{
		UE_LOG(LogDataBridge, Log, TEXT("Cache hit: %s"), *SourceName.ToString());
		OnFetchCompleted.Broadcast(SourceName, true, TEXT(""));
		if (OnComplete) OnComplete(true);
		return;
	}

	FString URL = ResolveURL(*Source);
	if (URL.IsEmpty())
	{
		FString Error = FString::Printf(TEXT("No URL for current environment: %s"), *SourceName.ToString());
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
		if (OnComplete) OnComplete(false);
		return;
	}

	UObject* TableObject = Source->TablePath.TryLoad();
	if (!TableObject)
	{
		FString Error = FString::Printf(TEXT("Failed to load table asset: %s"), *Source->TablePath.ToString());
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
		if (OnComplete) OnComplete(false);
		return;
	}

	EDataBridgeFormat Format = Source->Format;

	if (UDataTable* DataTable = Cast<UDataTable>(TableObject))
	{
		FetchTableInternal(SourceName, URL, DataTable, Format, MoveTemp(OnComplete));
	}
	else if (UCurveTable* CurveTable = Cast<UCurveTable>(TableObject))
	{
		FetchCurveTableInternal(SourceName, URL, CurveTable, Format, MoveTemp(OnComplete));
	}
	else
	{
		FString Error = TEXT("TablePath is not a DataTable or CurveTable");
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
		if (OnComplete) OnComplete(false);
	}
}

FString UDataBridgeSubsystem::MakeCacheKey(FName SourceName) const
{
	return FString::Printf(TEXT("%s_%d"), *SourceName.ToString(), (int32)CurrentEnvironment);
}

bool UDataBridgeSubsystem::IsCacheValid(FName SourceName, float TTLSeconds) const
{
	const FCacheEntry* Entry = Cache.Find(MakeCacheKey(SourceName));
	if (!Entry) return false;
	return (FPlatformTime::Seconds() - Entry->FetchTime) < TTLSeconds;
}

void UDataBridgeSubsystem::UpdateCache(FName SourceName)
{
	Cache.FindOrAdd(MakeCacheKey(SourceName)).FetchTime = FPlatformTime::Seconds();
}

void UDataBridgeSubsystem::FetchTableInternal(FName SourceName, const FString& URL, UDataTable* TargetTable, EDataBridgeFormat Format, TFunction<void(bool)> OnComplete)
{
	if (!ensureMsgf(TargetTable, TEXT("FetchTableInternal: TargetTable is null")))
	{
		OnFetchCompleted.Broadcast(SourceName, false, TEXT("TargetTable is null"));
		if (OnComplete) OnComplete(false);
		return;
	}

	FName ParserName = ResolveParserName(Format, URL, false);
	TSharedPtr<IDataBridgeParser>* ParserPtr = Parsers.Find(ParserName);
	if (!ParserPtr)
	{
		FString Error = FString::Printf(TEXT("No parser registered for format: %s"), *ParserName.ToString());
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
		if (OnComplete) OnComplete(false);
		return;
	}

	TSharedPtr<IDataBridgeParser> Parser = *ParserPtr;

	HttpClient->Get(URL, [this, SourceName, TargetTable, Parser, OnComplete = MoveTemp(OnComplete)](bool bSuccess, const FString& Body, int32 StatusCode) mutable
	{
		if (!bSuccess)
		{
			FString Error = FString::Printf(TEXT("HTTP error: %d"), StatusCode);
			UE_LOG(LogDataBridge, Warning, TEXT("FetchTable failed — %s"), *Error);
			OnFetchCompleted.Broadcast(SourceName, false, Error);
			if (OnComplete) OnComplete(false);
			return;
		}

		if (Body.IsEmpty())
		{
			UE_LOG(LogDataBridge, Warning, TEXT("FetchTable: empty response"));
			OnFetchCompleted.Broadcast(SourceName, false, TEXT("Empty response"));
			if (OnComplete) OnComplete(false);
			return;
		}

		FString ParseError;
		if (!Parser->ParseToDataTable(Body, TargetTable, ParseError))
		{
			UE_LOG(LogDataBridge, Warning, TEXT("FetchTable parse error: %s"), *ParseError);
			OnFetchCompleted.Broadcast(SourceName, false, ParseError);
			if (OnComplete) OnComplete(false);
			return;
		}

		if (SourceName != NAME_None)
		{
			UpdateCache(SourceName);
		}

		UE_LOG(LogDataBridge, Log, TEXT("FetchTable success: %s"), *SourceName.ToString());
		OnFetchCompleted.Broadcast(SourceName, true, TEXT(""));
		if (OnComplete) OnComplete(true);
	});
}

void UDataBridgeSubsystem::FetchCurveTableInternal(FName SourceName, const FString& URL, UCurveTable* TargetTable, EDataBridgeFormat Format, TFunction<void(bool)> OnComplete)
{
	if (!ensureMsgf(TargetTable, TEXT("FetchCurveTableInternal: TargetTable is null")))
	{
		OnFetchCompleted.Broadcast(SourceName, false, TEXT("TargetTable is null"));
		if (OnComplete) OnComplete(false);
		return;
	}

	FName ParserName = ResolveParserName(Format, URL, true);
	TSharedPtr<IDataBridgeParser>* ParserPtr = Parsers.Find(ParserName);
	if (!ParserPtr)
	{
		FString Error = FString::Printf(TEXT("No parser registered for format: %s"), *ParserName.ToString());
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
		if (OnComplete) OnComplete(false);
		return;
	}

	TSharedPtr<IDataBridgeParser> Parser = *ParserPtr;

	HttpClient->Get(URL, [this, SourceName, TargetTable, Parser, OnComplete = MoveTemp(OnComplete)](bool bSuccess, const FString& Body, int32 StatusCode) mutable
	{
		if (!bSuccess)
		{
			FString Error = FString::Printf(TEXT("HTTP error: %d"), StatusCode);
			UE_LOG(LogDataBridge, Warning, TEXT("FetchCurveTable failed — %s"), *Error);
			OnFetchCompleted.Broadcast(SourceName, false, Error);
			if (OnComplete) OnComplete(false);
			return;
		}

		if (Body.IsEmpty())
		{
			UE_LOG(LogDataBridge, Warning, TEXT("FetchCurveTable: empty response"));
			OnFetchCompleted.Broadcast(SourceName, false, TEXT("Empty response"));
			if (OnComplete) OnComplete(false);
			return;
		}

		FString ParseError;
		if (!Parser->ParseToCurveTable(Body, TargetTable, ParseError))
		{
			UE_LOG(LogDataBridge, Warning, TEXT("FetchCurveTable parse error: %s"), *ParseError);
			OnFetchCompleted.Broadcast(SourceName, false, ParseError);
			if (OnComplete) OnComplete(false);
			return;
		}

		if (SourceName != NAME_None)
		{
			UpdateCache(SourceName);
		}

		UE_LOG(LogDataBridge, Log, TEXT("FetchCurveTable success: %s"), *SourceName.ToString());
		OnFetchCompleted.Broadcast(SourceName, true, TEXT(""));
		if (OnComplete) OnComplete(true);
	});
}

FString UDataBridgeSubsystem::ResolveURL(const FDataBridgeSource& Source) const
{
	if (const FString* URL = Source.URLs.Find(CurrentEnvironment))
	{
		return *URL;
	}
	// 현재 환경 URL 없으면 Local로 폴백
	if (const FString* FallbackURL = Source.URLs.Find(EDataBridgeEnvironment::Local))
	{
		return *FallbackURL;
	}
	return FString();
}

FName UDataBridgeSubsystem::ResolveParserName(EDataBridgeFormat Format, const FString& URL, bool bCurveTable) const
{
	EDataBridgeFormat Resolved = Format;
	if (Format == EDataBridgeFormat::Auto)
	{
		Resolved = URL.EndsWith(TEXT(".csv")) ? EDataBridgeFormat::Csv : EDataBridgeFormat::Json;
	}

	if (bCurveTable)
	{
		return FName("JsonCurve"); // Csv CurveTable은 미지원 (v1.0)
	}

	return Resolved == EDataBridgeFormat::Csv ? FName("Csv") : FName("Json");
}
