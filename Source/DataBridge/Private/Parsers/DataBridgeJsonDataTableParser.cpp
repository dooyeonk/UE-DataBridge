#include "DataBridgeJsonDataTableParser.h"
#include "Utilities/DataBridgeLog.h"
#include "Engine/DataTable.h"

bool FDataBridgeJsonDataTableParser::ParseToDataTable(const FString& RawData, UDataTable* TargetTable, FString& OutError)
{
	if (!ensureMsgf(TargetTable, TEXT("DataBridgeJsonDataTableParser: TargetTable is null")))
	{
		OutError = TEXT("TargetTable is null");
		return false;
	}

	const UScriptStruct* RowStruct = TargetTable->GetRowStruct();
	if (!RowStruct)
	{
		OutError = TEXT("TargetTable has no RowStruct");
		return false;
	}

	UDataTable* TempTable = NewObject<UDataTable>(GetTransientPackage());
	TempTable->RowStruct = const_cast<UScriptStruct*>(RowStruct);

	TArray<FString> Problems = TempTable->CreateTableFromJSONString(RawData);
	const bool bHasRows = TempTable->GetRowMap().Num() > 0;

	if (!bHasRows && Problems.Num() > 0)
	{
		OutError = FString::Join(Problems, TEXT(", "));
		UE_LOG(LogDataBridge, Warning, TEXT("JSON parse failed (TargetTable preserved): %s"), *OutError);
		return false;
	}

	if (Problems.Num() > 0)
	{
		UE_LOG(LogDataBridge, Warning, TEXT("JSON parse warnings: %s"), *FString::Join(Problems, TEXT(", ")));
	}

	TargetTable->CreateTableFromJSONString(RawData);
	return true;
}
