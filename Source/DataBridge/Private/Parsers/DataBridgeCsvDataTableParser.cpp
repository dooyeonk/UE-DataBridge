#include "DataBridgeCsvDataTableParser.h"
#include "DataBridgeLog.h"
#include "Engine/DataTable.h"

bool FDataBridgeCsvDataTableParser::ParseToDataTable(const FString& RawData, UDataTable* TargetTable, FString& OutError)
{
	if (!ensureMsgf(TargetTable, TEXT("DataBridgeCsvDataTableParser: TargetTable is null")))
	{
		OutError = TEXT("TargetTable is null");
		return false;
	}

	TArray<FString> Problems = TargetTable->CreateTableFromCSVString(RawData);
	if (Problems.Num() > 0)
	{
		OutError = FString::Join(Problems, TEXT(", "));
		UE_LOG(LogDataBridge, Warning, TEXT("CSV parse problems: %s"), *OutError);
		return false;
	}

	return true;
}
