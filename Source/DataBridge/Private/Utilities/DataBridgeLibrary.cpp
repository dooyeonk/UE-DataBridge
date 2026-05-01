#include "Utilities/DataBridgeLibrary.h"
#include "Core/DataBridgeSubsystem.h"
#include "Utilities/DataBridgeLog.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

UDataBridgeSubsystem* UDataBridgeLibrary::GetSubsystem(UObject* WorldContextObject)
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject);
	if (!GI)
	{
		UE_LOG(LogDataBridge, Warning, TEXT("DataBridgeLibrary: GameInstance not found"));
		return nullptr;
	}

	UDataBridgeSubsystem* Subsystem = GI->GetSubsystem<UDataBridgeSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogDataBridge, Warning, TEXT("DataBridgeLibrary: DataBridgeSubsystem not found"));
	}
	return Subsystem;
}

void UDataBridgeLibrary::FetchTable(UObject* WorldContextObject, const FString& URL, UDataTable* TargetTable, EDataBridgeFormat Format)
{
	if (UDataBridgeSubsystem* S = GetSubsystem(WorldContextObject))
		S->FetchTable(URL, TargetTable, Format);
}

void UDataBridgeLibrary::FetchCurveTable(UObject* WorldContextObject, const FString& URL, UCurveTable* TargetTable, EDataBridgeFormat Format)
{
	if (UDataBridgeSubsystem* S = GetSubsystem(WorldContextObject))
		S->FetchCurveTable(URL, TargetTable, Format);
}

void UDataBridgeLibrary::FetchSource(UObject* WorldContextObject, FName SourceName)
{
	if (UDataBridgeSubsystem* S = GetSubsystem(WorldContextObject))
		S->FetchSource(SourceName);
}

void UDataBridgeLibrary::FetchAllSources(UObject* WorldContextObject)
{
	if (UDataBridgeSubsystem* S = GetSubsystem(WorldContextObject))
		S->FetchAllSources();
}

void UDataBridgeLibrary::SetEnvironment(UObject* WorldContextObject, EDataBridgeEnvironment NewEnvironment)
{
	if (UDataBridgeSubsystem* S = GetSubsystem(WorldContextObject))
		S->SetEnvironment(NewEnvironment);
}

EDataBridgeEnvironment UDataBridgeLibrary::GetCurrentEnvironment(UObject* WorldContextObject)
{
	if (UDataBridgeSubsystem* S = GetSubsystem(WorldContextObject))
		return S->GetCurrentEnvironment();
	return EDataBridgeEnvironment::Local;
}

void UDataBridgeLibrary::InvalidateCache(UObject* WorldContextObject, FName SourceName)
{
	if (UDataBridgeSubsystem* S = GetSubsystem(WorldContextObject))
		S->InvalidateCache(SourceName);
}

void UDataBridgeLibrary::InvalidateAllCache(UObject* WorldContextObject)
{
	if (UDataBridgeSubsystem* S = GetSubsystem(WorldContextObject))
		S->InvalidateAllCache();
}
