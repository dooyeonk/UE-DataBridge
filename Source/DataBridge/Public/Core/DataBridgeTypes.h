#pragma once

#include "CoreMinimal.h"
#include "DataBridgeTypes.generated.h"

UENUM(BlueprintType)
enum class EDataBridgeFormat : uint8
{
	Json,
	Csv,
	Auto,
};

UENUM(BlueprintType)
enum class EDataBridgeEnvironment : uint8
{
	Local,
	Development,
	Staging,
	Production,
};

USTRUCT(BlueprintType)
struct FDataBridgeSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly)
	FName SourceName;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly)
	TMap<EDataBridgeEnvironment, FString> URLs;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, meta = (AllowedClasses = "/Script/Engine.DataTable,/Script/Engine.CurveTable"))
	FSoftObjectPath TablePath;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly)
	EDataBridgeFormat Format = EDataBridgeFormat::Json;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly)
	float CacheTTLSeconds = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDataBridgeFetchCompleted,
	FName, SourceName,
	bool, bSuccess,
	const FString&, ErrorMessage);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDataBridgeAllSourcesCompleted,
	bool, bAllSuccess,
	int32, FailedCount);
