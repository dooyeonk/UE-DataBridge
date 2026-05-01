#pragma once

#include "IDataBridgeParser.h"

class FDataBridgeJsonDataTableParser : public IDataBridgeParser
{
public:
	virtual bool ParseToDataTable(const FString& RawData, UDataTable* TargetTable, FString& OutError) override;
	virtual FName GetFormatName() const override { return FName("Json"); }
};
