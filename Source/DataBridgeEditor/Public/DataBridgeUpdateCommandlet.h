#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DataBridgeUpdateCommandlet.generated.h"

UCLASS()
class DATABRIDGEEDITOR_API UDataBridgeUpdateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;

private:
	struct FSourceResult
	{
		FName SourceName;
		bool bSuccess = false;
		FString Message;
	};

	FSourceResult ProcessSource(FName SourceName, const FString& Environment, bool bDryRun);
	bool FetchSync(const FString& URL, FString& OutBody);
	bool SaveTable(UDataTable* Table);
};
