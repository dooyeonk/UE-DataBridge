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

	TArray<FString> Problems = TargetTable->CreateTableFromJSONString(RawData);
	if (Problems.Num() > 0)
	{
		OutError = FString::Join(Problems, TEXT(", "));
		UE_LOG(LogDataBridge, Warning, TEXT("JSON parse problems: %s"), *OutError);
		return false;
	}

	return true;
}
