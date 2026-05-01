#include "DataBridgeEditor.h"
#include "DataBridgeToolbar.h"

#define LOCTEXT_NAMESPACE "FDataBridgeEditorModule"

void FDataBridgeEditorModule::StartupModule()
{
	FDataBridgeToolbar::Register();
}

void FDataBridgeEditorModule::ShutdownModule()
{
	FDataBridgeToolbar::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDataBridgeEditorModule, DataBridgeEditor)
