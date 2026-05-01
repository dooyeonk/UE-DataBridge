#pragma once

#include "CoreMinimal.h"

class UDataTable;
class UCurveTable;

class DATABRIDGE_API IDataBridgeParser
{
public:
	virtual ~IDataBridgeParser() = default;

	virtual bool ParseToDataTable(const FString& RawData, UDataTable* TargetTable, FString& OutError) = 0;

	virtual bool ParseToCurveTable(const FString& RawData, UCurveTable* TargetTable, FString& OutError)
	{
		OutError = TEXT("CurveTable parsing not supported by this parser");
		return false;
	}

	virtual FName GetFormatName() const = 0;
};
