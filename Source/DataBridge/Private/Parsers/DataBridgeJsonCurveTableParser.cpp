#include "DataBridgeJsonCurveTableParser.h"
#include "Utilities/DataBridgeLog.h"
#include "Engine/CurveTable.h"

bool FDataBridgeJsonCurveTableParser::ParseToDataTable(const FString& RawData, UDataTable* TargetTable, FString& OutError)
{
	OutError = TEXT("JsonCurveTableParser does not support DataTable");
	return false;
}

bool FDataBridgeJsonCurveTableParser::ParseToCurveTable(const FString& RawData, UCurveTable* TargetTable, FString& OutError)
{
	if (!ensureMsgf(TargetTable, TEXT("DataBridgeJsonCurveTableParser: TargetTable is null")))
	{
		OutError = TEXT("TargetTable is null");
		return false;
	}

	TArray<FString> Problems = TargetTable->CreateTableFromJSONString(RawData);
	if (Problems.Num() > 0)
	{
		OutError = FString::Join(Problems, TEXT(", "));
		UE_LOG(LogDataBridge, Warning, TEXT("JSON CurveTable parse problems: %s"), *OutError);
		return false;
	}

	return true;
}
