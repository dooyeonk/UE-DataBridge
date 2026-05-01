#include "DataBridgeToolbar.h"
#include "DataBridgeUpdateCommandlet.h"
#include "Core/DataBridgeSettings.h"
#include "DataBridgeEditorLog.h"
#include "ToolMenus.h"
#include "ISettingsModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "DataBridgeToolbar"

namespace
{
	void ShowNotification(const FString& Message, bool bSuccess)
	{
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = 4.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notif = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notif.IsValid())
		{
			Notif->SetCompletionState(bSuccess
				? SNotificationItem::CS_Success
				: SNotificationItem::CS_Fail);
		}
	}
}

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
			LOCTEXT("RefreshAllTooltip", "Fetch all registered sources and update .uasset files"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&FDataBridgeToolbar::RefreshAll))
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
				FDataBridgeToolbar::RefreshSource(SourceName);
			}))
		);
	}
}

void FDataBridgeToolbar::RefreshSource(FName SourceName)
{
	UE_LOG(LogDataBridgeEditor, Log, TEXT("Toolbar: refreshing %s"), *SourceName.ToString());
	auto Result = UDataBridgeUpdateCommandlet::ProcessSource(SourceName, NAME_None, /*bDryRun=*/false);

	const FString Msg = FString::Printf(TEXT("DataBridge: %s — %s"),
		*SourceName.ToString(), *Result.Message);
	ShowNotification(Msg, Result.bSuccess);
}

void FDataBridgeToolbar::RefreshAll()
{
	const UDataBridgeSettings* Settings = GetDefault<UDataBridgeSettings>();
	if (Settings->Sources.IsEmpty())
	{
		ShowNotification(TEXT("DataBridge: no sources registered"), false);
		return;
	}

	int32 Success = 0, Fail = 0;
	for (const FDataBridgeSource& Source : Settings->Sources)
	{
		auto Result = UDataBridgeUpdateCommandlet::ProcessSource(Source.SourceName, NAME_None, /*bDryRun=*/false);
		(Result.bSuccess ? Success : Fail)++;
	}

	const FString Msg = FString::Printf(TEXT("DataBridge: %d succeeded, %d failed"), Success, Fail);
	ShowNotification(Msg, Fail == 0);
}

#undef LOCTEXT_NAMESPACE
