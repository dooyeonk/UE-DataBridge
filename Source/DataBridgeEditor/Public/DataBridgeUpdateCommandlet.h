#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "Core/DataBridgeTypes.h"
#include "DataBridgeUpdateCommandlet.generated.h"

UCLASS()
class DATABRIDGEEDITOR_API UDataBridgeUpdateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;

	struct FSourceResult
	{
		FName SourceName;
		bool bSuccess = false;
		FString Message;
	};

	// Toolbar / 외부 호출 용도. EnvironmentOverride가 NAME_None이면 Settings 값 사용.
	static FSourceResult ProcessSource(FName SourceName, FName EnvironmentOverride, bool bDryRun);

private:
	static bool FetchSync(const FString& URL, FString& OutBody);
	static bool SaveAsset(UObject* Asset);
	static FSourceResult ProcessDataTable(const FDataBridgeSource& Source, UDataTable* ExistingTable, const FString& URL, bool bDryRun);
	static FSourceResult ProcessCurveTable(const FDataBridgeSource& Source, UCurveTable* ExistingTable, const FString& URL, bool bDryRun);
};
