#include "DataBridgeUpdateCommandlet.h"
#include "Core/DataBridgeSettings.h"
#include "Utilities/DataBridgeDiff.h"
#include "DataBridgeEditorLog.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Engine/DataTable.h"
#include "Containers/Ticker.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

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

	int32 FailCount = 0;
	for (FName SourceName : SourcesToProcess)
	{
		FSourceResult Result = ProcessSource(SourceName, EnvironmentStr, bDryRun);
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
	FName SourceName, const FString& EnvironmentStr, bool bDryRun)
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

	// 환경 결정
	EDataBridgeEnvironment Env = Settings->CurrentEnvironment;
	if (!EnvironmentStr.IsEmpty())
	{
		if (EnvironmentStr == TEXT("Local"))            Env = EDataBridgeEnvironment::Local;
		else if (EnvironmentStr == TEXT("Development")) Env = EDataBridgeEnvironment::Development;
		else if (EnvironmentStr == TEXT("Staging"))     Env = EDataBridgeEnvironment::Staging;
		else if (EnvironmentStr == TEXT("Production"))  Env = EDataBridgeEnvironment::Production;
	}

	const FString* URLPtr = Source->URLs.Find(Env);
	if (!URLPtr) URLPtr = Source->URLs.Find(EDataBridgeEnvironment::Local);
	if (!URLPtr || URLPtr->IsEmpty())
	{
		Result.Message = TEXT("No URL for environment");
		return Result;
	}

	// 기존 테이블 로드
	UDataTable* ExistingTable = Cast<UDataTable>(Source->TablePath.TryLoad());
	if (!ExistingTable)
	{
		Result.Message = FString::Printf(TEXT("Failed to load table: %s"), *Source->TablePath.ToString());
		return Result;
	}

	// 동기 fetch
	FString Body;
	if (!FetchSync(*URLPtr, Body))
	{
		Result.Message = TEXT("HTTP fetch failed");
		return Result;
	}

	// 임시 테이블에 파싱
	UDataTable* TempTable = NewObject<UDataTable>(GetTransientPackage());
	TempTable->RowStruct = const_cast<UScriptStruct*>(ExistingTable->GetRowStruct());

	TArray<FString> Problems;
	if (Source->Format == EDataBridgeFormat::Csv)
		Problems = TempTable->CreateTableFromCSVString(Body);
	else
		Problems = TempTable->CreateTableFromJSONString(Body);

	if (Problems.Num() > 0)
	{
		Result.Message = FString::Printf(TEXT("Parse error: %s"), *FString::Join(Problems, TEXT(", ")));
		return Result;
	}

	// Diff 계산
	FDataTableDiff Diff = ComputeDataTableDiff(ExistingTable, TempTable);
	FString DiffLog = Diff.ToString(SourceName.ToString(), *URLPtr, Source->TablePath.ToString());
	UE_LOG(LogDataBridgeEditor, Log, TEXT("\n%s"), *DiffLog);

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

	// 변경 적용 및 저장
	if (Source->Format == EDataBridgeFormat::Csv)
		ExistingTable->CreateTableFromCSVString(Body);
	else
		ExistingTable->CreateTableFromJSONString(Body);

	if (!SaveTable(ExistingTable))
	{
		Result.Message = TEXT("Failed to save package");
		return Result;
	}

	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("+%d ~%d -%d rows"),
		Diff.AddedRows.Num(), Diff.ModifiedRows.Num(), Diff.RemovedRows.Num());
	return Result;
}

bool UDataBridgeUpdateCommandlet::FetchSync(const FString& URL, FString& OutBody)
{
	struct FResult { bool bDone = false; bool bSuccess = false; FString Body; };
	TSharedPtr<FResult> Result = MakeShared<FResult>();

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda(
		[Result](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			Result->bSuccess = bConnected && Response.IsValid() && Response->GetResponseCode() == 200;
			if (Result->bSuccess) Result->Body = Response->GetContentAsString();
			Result->bDone = true;
		});
	Request->ProcessRequest();

	// 완료까지 엔진 틱 펌핑
	while (!Result->bDone)
	{
		FPlatformProcess::Sleep(0.01f);
		FHttpModule::Get().GetHttpManager().Tick(0.01f);
		FTSTicker::GetCoreTicker().Tick(0.01f);
	}

	OutBody = Result->Body;
	return Result->bSuccess;
}

bool UDataBridgeUpdateCommandlet::SaveTable(UDataTable* Table)
{
	UPackage* Package = Table->GetOutermost();
	Package->MarkPackageDirty();

	FString FilePath = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;

	return UPackage::SavePackage(Package, Table, *FilePath, Args);
}
