#pragma once

#include "Interfaces/IDataBridgeParser.h"

class FDataBridgeCsvCurveTableParser : public IDataBridgeParser
{
public:
	virtual bool ParseToDataTable(const FString& RawData, UDataTable* TargetTable, FString& OutError) override;
	virtual bool ParseToCurveTable(const FString& RawData, UCurveTable* TargetTable, FString& OutError) override;
	virtual FName GetFormatName() const override { return FName("CsvCurve"); }
};
