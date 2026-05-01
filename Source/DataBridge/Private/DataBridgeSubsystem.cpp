#include "DataBridgeSubsystem.h"
#include "DataBridgeLog.h"
#include "DataBridgeSettings.h"
#include "DataBridgeHttpClient.h"
#include "Parsers/DataBridgeJsonDataTableParser.h"
#include "Parsers/DataBridgeCsvDataTableParser.h"
#include "Parsers/DataBridgeJsonCurveTableParser.h"
#include "Engine/DataTable.h"
#include "Engine/CurveTable.h"

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

	UE_LOG(LogDataBridge, Log, TEXT("DataBridgeSubsystem initialized (env: %d)"), (int32)CurrentEnvironment);
}

void UDataBridgeSubsystem::Deinitialize()
{
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
	const UDataBridgeSettings* Settings = GetDefault<UDataBridgeSettings>();
	const FDataBridgeSource* Source = Settings->FindSource(SourceName);

	if (!Source)
	{
		FString Error = FString::Printf(TEXT("Source not registered: %s"), *SourceName.ToString());
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
		return;
	}

	FString URL = ResolveURL(*Source);
	if (URL.IsEmpty())
	{
		FString Error = FString::Printf(TEXT("No URL for current environment: %s"), *SourceName.ToString());
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
		return;
	}

	UObject* TableObject = Source->TablePath.TryLoad();
	if (!TableObject)
	{
		FString Error = FString::Printf(TEXT("Failed to load table asset: %s"), *Source->TablePath.ToString());
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
		return;
	}

	if (UDataTable* DataTable = Cast<UDataTable>(TableObject))
	{
		FetchTableInternal(SourceName, URL, DataTable, Source->Format);
	}
	else if (UCurveTable* CurveTable = Cast<UCurveTable>(TableObject))
	{
		FetchCurveTableInternal(SourceName, URL, CurveTable, Source->Format);
	}
	else
	{
		FString Error = TEXT("TablePath is not a DataTable or CurveTable");
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
	}
}

void UDataBridgeSubsystem::FetchAllSources()
{
	// 6-B에서 구현
}

// ============================================================
// Cache
// ============================================================

void UDataBridgeSubsystem::InvalidateCache(FName SourceName)
{
	// 6-B에서 구현
}

void UDataBridgeSubsystem::InvalidateAllCache()
{
	// 6-B에서 구현
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

void UDataBridgeSubsystem::FetchTableInternal(FName SourceName, const FString& URL, UDataTable* TargetTable, EDataBridgeFormat Format)
{
	if (!ensureMsgf(TargetTable, TEXT("FetchTableInternal: TargetTable is null")))
	{
		OnFetchCompleted.Broadcast(SourceName, false, TEXT("TargetTable is null"));
		return;
	}

	FName ParserName = ResolveParserName(Format, URL, false);
	TSharedPtr<IDataBridgeParser>* ParserPtr = Parsers.Find(ParserName);
	if (!ParserPtr)
	{
		FString Error = FString::Printf(TEXT("No parser registered for format: %s"), *ParserName.ToString());
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
		return;
	}

	TSharedPtr<IDataBridgeParser> Parser = *ParserPtr;

	HttpClient->Get(URL, [this, SourceName, TargetTable, Parser](bool bSuccess, const FString& Body, int32 StatusCode)
	{
		if (!bSuccess)
		{
			FString Error = FString::Printf(TEXT("HTTP error: %d"), StatusCode);
			UE_LOG(LogDataBridge, Warning, TEXT("FetchTable failed — %s"), *Error);
			OnFetchCompleted.Broadcast(SourceName, false, Error);
			return;
		}

		if (Body.IsEmpty())
		{
			UE_LOG(LogDataBridge, Warning, TEXT("FetchTable: empty response"));
			OnFetchCompleted.Broadcast(SourceName, false, TEXT("Empty response"));
			return;
		}

		FString ParseError;
		if (!Parser->ParseToDataTable(Body, TargetTable, ParseError))
		{
			UE_LOG(LogDataBridge, Warning, TEXT("FetchTable parse error: %s"), *ParseError);
			OnFetchCompleted.Broadcast(SourceName, false, ParseError);
			return;
		}

		UE_LOG(LogDataBridge, Log, TEXT("FetchTable success: %s"), *SourceName.ToString());
		OnFetchCompleted.Broadcast(SourceName, true, TEXT(""));
	});
}

void UDataBridgeSubsystem::FetchCurveTableInternal(FName SourceName, const FString& URL, UCurveTable* TargetTable, EDataBridgeFormat Format)
{
	if (!ensureMsgf(TargetTable, TEXT("FetchCurveTableInternal: TargetTable is null")))
	{
		OnFetchCompleted.Broadcast(SourceName, false, TEXT("TargetTable is null"));
		return;
	}

	FName ParserName = ResolveParserName(Format, URL, true);
	TSharedPtr<IDataBridgeParser>* ParserPtr = Parsers.Find(ParserName);
	if (!ParserPtr)
	{
		FString Error = FString::Printf(TEXT("No parser registered for format: %s"), *ParserName.ToString());
		UE_LOG(LogDataBridge, Warning, TEXT("%s"), *Error);
		OnFetchCompleted.Broadcast(SourceName, false, Error);
		return;
	}

	TSharedPtr<IDataBridgeParser> Parser = *ParserPtr;

	HttpClient->Get(URL, [this, SourceName, TargetTable, Parser](bool bSuccess, const FString& Body, int32 StatusCode)
	{
		if (!bSuccess)
		{
			FString Error = FString::Printf(TEXT("HTTP error: %d"), StatusCode);
			UE_LOG(LogDataBridge, Warning, TEXT("FetchCurveTable failed — %s"), *Error);
			OnFetchCompleted.Broadcast(SourceName, false, Error);
			return;
		}

		if (Body.IsEmpty())
		{
			UE_LOG(LogDataBridge, Warning, TEXT("FetchCurveTable: empty response"));
			OnFetchCompleted.Broadcast(SourceName, false, TEXT("Empty response"));
			return;
		}

		FString ParseError;
		if (!Parser->ParseToCurveTable(Body, TargetTable, ParseError))
		{
			UE_LOG(LogDataBridge, Warning, TEXT("FetchCurveTable parse error: %s"), *ParseError);
			OnFetchCompleted.Broadcast(SourceName, false, ParseError);
			return;
		}

		UE_LOG(LogDataBridge, Log, TEXT("FetchCurveTable success: %s"), *SourceName.ToString());
		OnFetchCompleted.Broadcast(SourceName, true, TEXT(""));
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
