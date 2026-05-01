#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/DataBridgeTypes.h"
#include "DataBridgeLibrary.generated.h"

class UDataTable;
class UCurveTable;

UCLASS()
class DATABRIDGE_API UDataBridgeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DataBridge", meta = (WorldContext = "WorldContextObject"))
	static void FetchTable(UObject* WorldContextObject, const FString& URL, UDataTable* TargetTable, EDataBridgeFormat Format = EDataBridgeFormat::Json);

	UFUNCTION(BlueprintCallable, Category = "DataBridge", meta = (WorldContext = "WorldContextObject"))
	static void FetchCurveTable(UObject* WorldContextObject, const FString& URL, UCurveTable* TargetTable, EDataBridgeFormat Format = EDataBridgeFormat::Csv);

	UFUNCTION(BlueprintCallable, Category = "DataBridge", meta = (WorldContext = "WorldContextObject"))
	static void FetchSource(UObject* WorldContextObject, FName SourceName);

	UFUNCTION(BlueprintCallable, Category = "DataBridge", meta = (WorldContext = "WorldContextObject"))
	static void FetchAllSources(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "DataBridge", meta = (WorldContext = "WorldContextObject"))
	static void SetEnvironment(UObject* WorldContextObject, EDataBridgeEnvironment NewEnvironment);

	UFUNCTION(BlueprintPure, Category = "DataBridge", meta = (WorldContext = "WorldContextObject"))
	static EDataBridgeEnvironment GetCurrentEnvironment(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "DataBridge", meta = (WorldContext = "WorldContextObject"))
	static void InvalidateCache(UObject* WorldContextObject, FName SourceName);

	UFUNCTION(BlueprintCallable, Category = "DataBridge", meta = (WorldContext = "WorldContextObject"))
	static void InvalidateAllCache(UObject* WorldContextObject);

private:
	static class UDataBridgeSubsystem* GetSubsystem(UObject* WorldContextObject);
};
