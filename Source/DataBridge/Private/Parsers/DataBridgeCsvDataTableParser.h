#pragma once

#include "Interfaces/IDataBridgeParser.h"

class FDataBridgeCsvDataTableParser : public IDataBridgeParser
{
public:
	virtual bool ParseToDataTable(const FString& RawData, UDataTable* TargetTable, FString& OutError) override;
	virtual FName GetFormatName() const override { return FName("Csv"); }
};
