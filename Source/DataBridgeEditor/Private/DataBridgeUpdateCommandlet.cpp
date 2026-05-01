#include "DataBridgeUpdateCommandlet.h"
#include "Core/DataBridgeSettings.h"
#include "Utilities/DataBridgeDiff.h"
#include "DataBridgeEditorLog.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Engine/DataTable.h"
#include "Engine/CurveTable.h"
#include "Containers/Ticker.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
	bool ParseEnvironment(const FString& Str, EDataBridgeEnvironment& OutEnv)
	{
		if (Str.IsEmpty()) return false;
		if (Str == TEXT("Local"))            { OutEnv = EDataBridgeEnvironment::Local;       return true; }
		if (Str == TEXT("Development"))      { OutEnv = EDataBridgeEnvironment::Development; return true; }
		if (Str == TEXT("Staging"))          { OutEnv = EDataBridgeEnvironment::Staging;     return true; }
		if (Str == TEXT("Production"))       { OutEnv = EDataBridgeEnvironment::Production;  return true; }
		return false;
	}
}

int32 UDataBridgeUpdateCommandlet::Main(const FString& Params)
{
	const bool bAll = FParse::Param(*Params, TEXT("All"));
	const bool bDryRun = FParse::Param(*Params, TEXT("DryRun"));

	FString SourceNameStr;
	FParse::Value(*Params, TEXT("SourceName="), SourceNameStr);

	FString EnvironmentStr;
	FParse::Value(*Params, TEXT("Environment="), EnvironmentStr);

	FString FilterStr;
	FParse::Value(*Params, TEXT("Filter="), FilterStr);

	TArray<FString> FilterNames;
	if (!FilterStr.IsEmpty())
	{
		FilterStr.ParseIntoArray(FilterNames, TEXT(","), true);
	}

	const UDataBridgeSettings* Settings = GetDefault<UDataBridgeSettings>();
	TArray<FName> SourcesToProcess;

	if (!SourceNameStr.IsEmpty())
	{
		SourcesToProcess.Add(FName(*SourceNameStr));
	}
	else if (bAll)
	{
		for (const FDataBridgeSource& Source : Settings->Sources)
		{
			if (FilterNames.IsEmpty() || FilterNames.Contains(Source.SourceName.ToString()))
			{
				SourcesToProcess.Add(Source.SourceName);
			}
		}
	}
	else
	{
		UE_LOG(LogDataBridgeEditor, Error, TEXT("Specify -SourceName=<name> or -All"));
		return 3;
	}

	if (SourcesToProcess.IsEmpty())
	{
		UE_LOG(LogDataBridgeEditor, Error, TEXT("No matching sources found in settings"));
		return 3;
	}

	const FName EnvOverride = EnvironmentStr.IsEmpty() ? NAME_None : FName(*EnvironmentStr);

	int32 FailCount = 0;
	for (FName SourceName : SourcesToProcess)
	{
		FSourceResult Result = ProcessSource(SourceName, EnvOverride, bDryRun);
		if (Result.bSuccess)
		{
			UE_CLOG(!bDryRun, LogDataBridgeEditor, Log, TEXT("SUCCESS: %s — %s"), *SourceName.ToString(), *Result.Message);
		}
		else
		{
			UE_LOG(LogDataBridgeEditor, Error, TEXT("FAILED:  %s — %s"), *SourceName.ToString(), *Result.Message);
			FailCount++;
		}
	}

	if (FailCount == 0) return 0;
	if (FailCount < SourcesToProcess.Num()) return 1;
	return 2;
}

UDataBridgeUpdateCommandlet::FSourceResult UDataBridgeUpdateCommandlet::ProcessSource(
	FName SourceName, FName EnvironmentOverride, bool bDryRun)
{
	FSourceResult Result;
	Result.SourceName = SourceName;

	const UDataBridgeSettings* Settings = GetDefault<UDataBridgeSettings>();
	const FDataBridgeSource* Source = Settings->FindSource(SourceName);
	if (!Source)
	{
		Result.Message = FString::Printf(TEXT("Source not registered: %s"), *SourceName.ToString());
		return Result;
	}

	EDataBridgeEnvironment Env = Settings->CurrentEnvironment;
	if (!EnvironmentOverride.IsNone())
	{
		ParseEnvironment(EnvironmentOverride.ToString(), Env);
	}

	const FString* URLPtr = Source->URLs.Find(Env);
	if (!URLPtr) URLPtr = Source->URLs.Find(EDataBridgeEnvironment::Local);
	if (!URLPtr || URLPtr->IsEmpty())
	{
		Result.Message = TEXT("No URL for environment");
		return Result;
	}

	UObject* Loaded = Source->TablePath.TryLoad();
	if (!Loaded)
	{
		Result.Message = FString::Printf(TEXT("Failed to load table: %s"), *Source->TablePath.ToString());
		return Result;
	}

	if (UDataTable* DataTable = Cast<UDataTable>(Loaded))
		return ProcessDataTable(*Source, DataTable, *URLPtr, bDryRun);

	if (UCurveTable* CurveTable = Cast<UCurveTable>(Loaded))
		return ProcessCurveTable(*Source, CurveTable, *URLPtr, bDryRun);

	Result.Message = TEXT("TablePath is not a DataTable or CurveTable");
	return Result;
}

UDataBridgeUpdateCommandlet::FSourceResult UDataBridgeUpdateCommandlet::ProcessDataTable(
	const FDataBridgeSource& Source, UDataTable* ExistingTable, const FString& URL, bool bDryRun)
{
	FSourceResult Result;
	Result.SourceName = Source.SourceName;

	FString Body;
	if (!FetchSync(URL, Body))
	{
		Result.Message = TEXT("HTTP fetch failed");
		return Result;
	}

	UDataTable* TempTable = NewObject<UDataTable>(GetTransientPackage());
	TempTable->RowStruct = const_cast<UScriptStruct*>(ExistingTable->GetRowStruct());

	TArray<FString> Problems = (Source.Format == EDataBridgeFormat::Csv)
		? TempTable->CreateTableFromCSVString(Body)
		: TempTable->CreateTableFromJSONString(Body);

	if (TempTable->GetRowMap().Num() == 0 && Problems.Num() > 0)
	{
		Result.Message = FString::Printf(TEXT("Parse error: %s"), *FString::Join(Problems, TEXT(", ")));
		return Result;
	}

	FDataTableDiff Diff = ComputeDataTableDiff(ExistingTable, TempTable);
	UE_LOG(LogDataBridgeEditor, Log, TEXT("\n%s"),
		*Diff.ToString(Source.SourceName.ToString(), URL, Source.TablePath.ToString()));

	if (!Diff.HasChanges())
	{
		Result.bSuccess = true;
		Result.Message = TEXT("No changes");
		return Result;
	}

	if (bDryRun)
	{
		UE_LOG(LogDataBridgeEditor, Log, TEXT("  [DryRun] No changes saved."));
		Result.bSuccess = true;
		Result.Message = TEXT("DryRun — changes not saved");
		return Result;
	}

	if (Source.Format == EDataBridgeFormat::Csv)
		ExistingTable->CreateTableFromCSVString(Body);
	else
		ExistingTable->CreateTableFromJSONString(Body);

	if (!SaveAsset(ExistingTable))
	{
		Result.Message = TEXT("Failed to save package");
		return Result;
	}

	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("+%d ~%d -%d rows"),
		Diff.AddedRows.Num(), Diff.ModifiedRows.Num(), Diff.RemovedRows.Num());
	return Result;
}

UDataBridgeUpdateCommandlet::FSourceResult UDataBridgeUpdateCommandlet::ProcessCurveTable(
	const FDataBridgeSource& Source, UCurveTable* ExistingTable, const FString& URL, bool bDryRun)
{
	FSourceResult Result;
	Result.SourceName = Source.SourceName;

	FString Body;
	if (!FetchSync(URL, Body))
	{
		Result.Message = TEXT("HTTP fetch failed");
		return Result;
	}

	// CurveTable diff은 v1.0 범위 외 (스펙 Open Question). Row 추가/제거만 감지.
	UCurveTable* TempTable = NewObject<UCurveTable>(GetTransientPackage());
	TArray<FString> Problems = (Source.Format == EDataBridgeFormat::Csv)
		? TempTable->CreateTableFromCSVString(Body)
		: TempTable->CreateTableFromJSONString(Body);

	if (TempTable->GetRowMap().Num() == 0 && Problems.Num() > 0)
	{
		Result.Message = FString::Printf(TEXT("Parse error: %s"), *FString::Join(Problems, TEXT(", ")));
		return Result;
	}

	const auto& OldRows = ExistingTable->GetRowMap();
	const auto& NewRows = TempTable->GetRowMap();
	TArray<FName> Added, Removed;
	for (auto& Pair : NewRows) if (!OldRows.Contains(Pair.Key)) Added.Add(Pair.Key);
	for (auto& Pair : OldRows) if (!NewRows.Contains(Pair.Key)) Removed.Add(Pair.Key);

	UE_LOG(LogDataBridgeEditor, Log, TEXT("[DataBridge] Source: %s (CurveTable)\n  URL:    %s\n  Target: %s\n  Rows: +%d -%d (curve values not diffed)"),
		*Source.SourceName.ToString(), *URL, *Source.TablePath.ToString(), Added.Num(), Removed.Num());

	if (bDryRun)
	{
		UE_LOG(LogDataBridgeEditor, Log, TEXT("  [DryRun] No changes saved."));
		Result.bSuccess = true;
		Result.Message = TEXT("DryRun — changes not saved");
		return Result;
	}

	if (Source.Format == EDataBridgeFormat::Csv)
		ExistingTable->CreateTableFromCSVString(Body);
	else
		ExistingTable->CreateTableFromJSONString(Body);

	if (!SaveAsset(ExistingTable))
	{
		Result.Message = TEXT("Failed to save package");
		return Result;
	}

	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("CurveTable saved (+%d -%d rows)"), Added.Num(), Removed.Num());
	return Result;
}

bool UDataBridgeUpdateCommandlet::FetchSync(const FString& URL, FString& OutBody)
{
	const UDataBridgeSettings* Settings = GetDefault<UDataBridgeSettings>();
	const float TimeoutSeconds = (Settings && Settings->RequestTimeoutSeconds > 0.0f)
		? Settings->RequestTimeoutSeconds : 30.0f;

	struct FResult { bool bDone = false; bool bSuccess = false; FString Body; };
	TSharedPtr<FResult> Result = MakeShared<FResult>();

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb(TEXT("GET"));
	Request->SetTimeout(TimeoutSeconds);
	Request->OnProcessRequestComplete().BindLambda(
		[Result](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			Result->bSuccess = bConnected && Response.IsValid() && Response->GetResponseCode() == 200;
			if (Result->bSuccess) Result->Body = Response->GetContentAsString();
			Result->bDone = true;
		});
	Request->ProcessRequest();

	const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds + 5.0;

	while (!Result->bDone)
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			UE_LOG(LogDataBridgeEditor, Warning,
				TEXT("FetchSync: timeout after %.1fs (URL: %s)"), TimeoutSeconds + 5.0, *URL);
			Request->CancelRequest();
			OutBody.Reset();
			return false;
		}

		FPlatformProcess::Sleep(0.01f);
		FHttpModule::Get().GetHttpManager().Tick(0.01f);
		FTSTicker::GetCoreTicker().Tick(0.01f);
	}

	OutBody = Result->Body;
	return Result->bSuccess;
}

bool UDataBridgeUpdateCommandlet::SaveAsset(UObject* Asset)
{
	UPackage* Package = Asset->GetOutermost();
	Package->MarkPackageDirty();

	FString FilePath = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;

	return UPackage::SavePackage(Package, Asset, *FilePath, Args);
}
