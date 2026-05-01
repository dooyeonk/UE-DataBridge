#pragma once

#include "CoreMinimal.h"

class FDataBridgeToolbar
{
public:
	static void Register();
	static void Unregister();

private:
	static void RegisterMenus();
	static void BuildDropdownMenu(FMenuBuilder& MenuBuilder);
	static void BuildSourcesSubmenu(FMenuBuilder& MenuBuilder);

	static void RefreshSource(FName SourceName);
	static void RefreshAll();
};
