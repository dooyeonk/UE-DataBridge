#include "DataBridgeToolbar.h"
#include "DataBridgeSubsystem.h"
#include "DataBridgeSettings.h"
#include "DataBridgeLog.h"
#include "ToolMenus.h"
#include "ISettingsModule.h"
#include "Engine/GameInstance.h"

#define LOCTEXT_NAMESPACE "DataBridgeToolbar"

void FDataBridgeToolbar::Register()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateStatic(&FDataBridgeToolbar::RegisterMenus));
}

void FDataBridgeToolbar::Unregister()
{
	UToolMenus::UnRegisterStartupCallback(&FDataBridgeToolbar::RegisterMenus);
	UToolMenus::Get()->RemoveMenu("LevelEditor.MainMenu.DataBridge");
}

void FDataBridgeToolbar::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(TEXT("DataBridge"));

	UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu");
	FToolMenuSection& Section = MainMenu->AddSection("DataBridge");

	Section.AddSubMenu(
		"DataBridgeMenu",
		LOCTEXT("MenuLabel", "DataBridge"),
		LOCTEXT("MenuTooltip", "DataBridge plugin actions"),
		FNewMenuDelegate::CreateStatic(&FDataBridgeToolbar::BuildDropdownMenu)
	);
}

void FDataBridgeToolbar::BuildDropdownMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("DataBridgeFetch", LOCTEXT("FetchSection", "Fetch"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("RefreshAll", "Refresh All Sources"),
			LOCTEXT("RefreshAllTooltip", "Fetch all registered sources (requires active PIE)"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				UDataBridgeSubsystem* Subsystem = FDataBridgeToolbar::FindPIESubsystem();
				if (Subsystem)
				{
					Subsystem->InvalidateAllCache();
					Subsystem->FetchAllSources();
				}
				else
				{
					UE_LOG(LogDataBridge, Warning, TEXT("Refresh All: no active PIE session"));
				}
			}))
		);

		MenuBuilder.AddSubMenu(
			LOCTEXT("RefreshSource", "Refresh Source"),
			LOCTEXT("RefreshSourceTooltip", "Fetch a specific registered source"),
			FNewMenuDelegate::CreateStatic(&FDataBridgeToolbar::BuildSourcesSubmenu)
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("DataBridgeSettings", LOCTEXT("SettingsSection", "Settings"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("OpenSettings", "Open Settings"),
			LOCTEXT("OpenSettingsTooltip", "Open DataBridge project settings"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FModuleManager::LoadModuleChecked<ISettingsModule>("Settings")
					.ShowViewer(FName("Project"), FName("DataBridge"), FName("DataBridge"));
			}))
		);
	}
	MenuBuilder.EndSection();
}

void FDataBridgeToolbar::BuildSourcesSubmenu(FMenuBuilder& MenuBuilder)
{
	const UDataBridgeSettings* Settings = GetDefault<UDataBridgeSettings>();
	if (Settings->Sources.IsEmpty())
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NoSources", "(No sources registered)"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction()
		);
		return;
	}

	for (const FDataBridgeSource& Source : Settings->Sources)
	{
		FName SourceName = Source.SourceName;
		MenuBuilder.AddMenuEntry(
			FText::FromName(SourceName),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([SourceName]()
			{
				UDataBridgeSubsystem* Subsystem = FDataBridgeToolbar::FindPIESubsystem();
				if (Subsystem)
				{
					Subsystem->InvalidateCache(SourceName);
					Subsystem->FetchSource(SourceName);
				}
				else
				{
					UE_LOG(LogDataBridge, Warning, TEXT("Refresh Source: no active PIE session"));
				}
			}))
		);
	}
}

UDataBridgeSubsystem* FDataBridgeToolbar::FindPIESubsystem()
{
	if (!GEngine) return nullptr;

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.OwningGameInstance)
		{
			return Context.OwningGameInstance->GetSubsystem<UDataBridgeSubsystem>();
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
