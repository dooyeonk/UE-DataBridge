#include "Core/DataBridgeSubsystem.h"
#include "Utilities/DataBridgeLog.h"
#include "Core/DataBridgeSettings.h"
#include "Http/DataBridgeHttpClient.h"
#include "Parsers/DataBridgeJsonDataTableParser.h"
#include "Parsers/DataBridgeCsvDataTableParser.h"
#include "Parsers/DataBridgeJsonCurveTableParser.h"
#include "Parsers/DataBridgeCsvCurveTableParser.h"
#include "Engine/DataTable.h"
#include "Engine/CurveTable.h"
#include "HAL/IConsoleManager.h"

namespace DataBridgeCachePrivate
{
	struct FCacheEntry
	{
		double FetchTime = 0.0;
		FString Body;
		EDataBridgeFormat Format = EDataBridgeFormat::Json;
		bool bIsCurveTable = false;
	};

	// Process-level cache: persists across PIE restarts when bEnablePIECache=true.
	// Cleared from Deinitialize when bEnablePIECache=false.
	static TMap<FString, FCacheEntry> GCache;
}

using namespace DataBridgeCachePrivate;

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
	RegisterParser(MakeShared<FDataBridgeCsvCurveTableParser>());

	RegisterConsoleCommands();

	UE_LOG(LogDataBridge, Log, TEXT("DataBridgeSubsystem initialized (env: %d)"), (int32)CurrentEnvironment);
}

void UDataBridgeSubsystem::Deinitialize()
{
	if (!IsEngineExitRequested())
	{
		// 엔진 종료 시 IConsoleManager destruction order 이슈로 unregister가 crash. 종료 중엔 OS가 회수
		for (IConsoleCommand* Cmd : ConsoleCommands)
		{
			IConsoleManager::Get().UnregisterConsoleObject(Cmd);
		}
	}
	ConsoleCommands.Empty();

	HttpClient.Reset();
	Parsers.Empty();

	const UDataBridgeSettings* Settings = GetDefault<UDataBridgeSettings>();
	if (!Settings || !Settings->bEnablePIECache)
	{
		GCache.Empty();
	}

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
			if (!bSuccess) (*FailCount)++;
			if (--(*Remaining) == 0)
				OnAllSourcesCompleted.Broadcast(*FailCount == 0, *FailCount);
		});
	}
}

// ============================================================
// Cache
// ============================================================

void UDataBridgeSubsystem::InvalidateCache(FName SourceName)
{
	GCache.Remove(MakeCacheKey(SourceName));
	UE_LOG(LogDataBridge, Log, TEXT("Cache invalidated: %s"), *SourceName.ToString());
}

void UDataBridgeSubsystem::InvalidateAllCache()
{
	GCache.Empty();
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
			if (Args.IsEmpty()) InvalidateAllCache();
			else InvalidateCache(FName(*Args[0]));
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

	UDataTable* DataTable = Cast<UDataTable>(TableObject);
	UCurveTable* CurveTable = Cast<UCurveTable>(TableObject);

	// Cache hit — re-apply cached body to TargetTable, no HTTP
	if (Source->CacheTTLSeconds > 0.0f && IsCacheValid(SourceName, Source->CacheTTLSeconds))
	{
		if (TryServeFromCache(SourceName, Source->CacheTTLSeconds, DataTable, CurveTable))
		{
			UE_LOG(LogDataBridge, Log, TEXT("Cache hit: %s"), *SourceName.ToString());
			OnFetchCompleted.Broadcast(SourceName, true, TEXT(""));
			if (OnComplete) OnComplete(true);
			return;
		}
	}

	EDataBridgeFormat Format = Source->Format;

	if (DataTable)
		FetchTableInternal(SourceName, URL, DataTable, Format, MoveTemp(OnComplete));
	else if (CurveTable)
		FetchCurveTableInternal(SourceName, URL, CurveTable, Format, MoveTemp(OnComplete));
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
	const FCacheEntry* Entry = GCache.Find(MakeCacheKey(SourceName));
	if (!Entry) return false;
	return (FPlatformTime::Seconds() - Entry->FetchTime) < TTLSeconds;
}

void UDataBridgeSubsystem::StoreCache(FName SourceName, const FString& Body, EDataBridgeFormat Format, bool bIsCurveTable)
{
	if (SourceName == NAME_None) return;

	FCacheEntry& Entry = GCache.FindOrAdd(MakeCacheKey(SourceName));
	Entry.FetchTime = FPlatformTime::Seconds();
	Entry.Body = Body;
	Entry.Format = Format;
	Entry.bIsCurveTable = bIsCurveTable;
}

bool UDataBridgeSubsystem::TryServeFromCache(FName SourceName, float TTLSeconds, UDataTable* TargetTable, UCurveTable* CurveTable)
{
	const FCacheEntry* Entry = GCache.Find(MakeCacheKey(SourceName));
	if (!Entry) return false;

	FName ParserName = ResolveParserName(Entry->Format, FString(), Entry->bIsCurveTable);
	TSharedPtr<IDataBridgeParser>* ParserPtr = Parsers.Find(ParserName);
	if (!ParserPtr) return false;

	FString ParseError;
	if (Entry->bIsCurveTable && CurveTable)
		return (*ParserPtr)->ParseToCurveTable(Entry->Body, CurveTable, ParseError);
	if (!Entry->bIsCurveTable && TargetTable)
		return (*ParserPtr)->ParseToDataTable(Entry->Body, TargetTable, ParseError);

	return false;
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
	const EDataBridgeFormat ResolvedFormat =
		(Format == EDataBridgeFormat::Auto)
			? (URL.EndsWith(TEXT(".csv")) ? EDataBridgeFormat::Csv : EDataBridgeFormat::Json)
			: Format;

	HttpClient->Get(URL, [this, SourceName, TargetTable, Parser, ResolvedFormat, OnComplete = MoveTemp(OnComplete)](bool bSuccess, const FString& Body, int32 StatusCode) mutable
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

		StoreCache(SourceName, Body, ResolvedFormat, /*bIsCurveTable=*/false);

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
	const EDataBridgeFormat ResolvedFormat =
		(Format == EDataBridgeFormat::Auto)
			? (URL.EndsWith(TEXT(".csv")) ? EDataBridgeFormat::Csv : EDataBridgeFormat::Json)
			: Format;

	HttpClient->Get(URL, [this, SourceName, TargetTable, Parser, ResolvedFormat, OnComplete = MoveTemp(OnComplete)](bool bSuccess, const FString& Body, int32 StatusCode) mutable
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

		StoreCache(SourceName, Body, ResolvedFormat, /*bIsCurveTable=*/true);

		UE_LOG(LogDataBridge, Log, TEXT("FetchCurveTable success: %s"), *SourceName.ToString());
		OnFetchCompleted.Broadcast(SourceName, true, TEXT(""));
		if (OnComplete) OnComplete(true);
	});
}

FString UDataBridgeSubsystem::ResolveURL(const FDataBridgeSource& Source) const
{
	if (const FString* URL = Source.URLs.Find(CurrentEnvironment))
		return *URL;
	if (const FString* FallbackURL = Source.URLs.Find(EDataBridgeEnvironment::Local))
		return *FallbackURL;
	return FString();
}

FName UDataBridgeSubsystem::ResolveParserName(EDataBridgeFormat Format, const FString& URL, bool bCurveTable) const
{
	EDataBridgeFormat Resolved = Format;
	if (Format == EDataBridgeFormat::Auto)
		Resolved = URL.EndsWith(TEXT(".csv")) ? EDataBridgeFormat::Csv : EDataBridgeFormat::Json;

	if (bCurveTable)
		return Resolved == EDataBridgeFormat::Csv ? FName("CsvCurve") : FName("JsonCurve");

	return Resolved == EDataBridgeFormat::Csv ? FName("Csv") : FName("Json");
}
