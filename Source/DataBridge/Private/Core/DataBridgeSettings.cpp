#include "Core/DataBridgeSettings.h"

UDataBridgeSettings::UDataBridgeSettings()
{
	CategoryName = FName("DataBridge");
}

const FDataBridgeSource* UDataBridgeSettings::FindSource(FName SourceName) const
{
	return Sources.FindByPredicate([&](const FDataBridgeSource& Source)
	{
		return Source.SourceName == SourceName;
	});
}
