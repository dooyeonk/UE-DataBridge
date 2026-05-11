#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/DataBridgeTypes.h"
#include "DataBridgeSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "DataBridge"))
class DATABRIDGE_API UDataBridgeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDataBridgeSettings();

	virtual FName GetCategoryName() const override { return FName("DataBridge"); }

	UPROPERTY(EditAnywhere, Config, Category = "Environment")
	EDataBridgeEnvironment CurrentEnvironment = EDataBridgeEnvironment::Local;

	UPROPERTY(EditAnywhere, Config, Category = "Sources", meta = (TitleProperty = "SourceName"))
	TArray<FDataBridgeSource> Sources;

	UPROPERTY(EditAnywhere, Config, Category = "Development")
	bool bEnablePIECache = true;

	UPROPERTY(EditAnywhere, Config, Category = "Development")
	float DefaultPIECacheTTLSeconds = 300.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Development", meta = (DisplayName = "Skip Fetch in PIE", ToolTip = "PIE 실행 시 자동 fetch 비활성화. 콘솔 명령 및 에디터 메뉴는 영향 없음"))
	bool bSkipFetchInPIE = true;

	UPROPERTY(EditAnywhere, Config, Category = "Network")
	float RequestTimeoutSeconds = 10.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Network")
	int32 RetryCount = 2;

	UPROPERTY(EditAnywhere, Config, Category = "Network")
	float RetryDelaySeconds = 1.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Logging")
	bool bVerboseLogging = false;

	const FDataBridgeSource* FindSource(FName SourceName) const;
};
