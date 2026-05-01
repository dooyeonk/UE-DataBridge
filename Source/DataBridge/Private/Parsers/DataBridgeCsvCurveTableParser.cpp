#include "DataBridgeCsvCurveTableParser.h"
#include "Utilities/DataBridgeLog.h"
#include "Engine/CurveTable.h"

bool FDataBridgeCsvCurveTableParser::ParseToDataTable(const FString& RawData, UDataTable* TargetTable, FString& OutError)
{
	OutError = TEXT("CsvCurveTableParser does not support DataTable");
	return false;
}

bool FDataBridgeCsvCurveTableParser::ParseToCurveTable(const FString& RawData, UCurveTable* TargetTable, FString& OutError)
{
	if (!ensureMsgf(TargetTable, TEXT("DataBridgeCsvCurveTableParser: TargetTable is null")))
	{
		OutError = TEXT("TargetTable is null");
		return false;
	}

	UCurveTable* TempTable = NewObject<UCurveTable>(GetTransientPackage());

	TArray<FString> Problems = TempTable->CreateTableFromCSVString(RawData);
	const bool bHasRows = TempTable->GetRowMap().Num() > 0;

	if (!bHasRows && Problems.Num() > 0)
	{
		OutError = FString::Join(Problems, TEXT(", "));
		UE_LOG(LogDataBridge, Warning, TEXT("CSV CurveTable parse failed (TargetTable preserved): %s"), *OutError);
		return false;
	}

	if (Problems.Num() > 0)
	{
		UE_LOG(LogDataBridge, Warning, TEXT("CSV CurveTable parse warnings: %s"), *FString::Join(Problems, TEXT(", ")));
	}

	TargetTable->CreateTableFromCSVString(RawData);
	return true;
}
