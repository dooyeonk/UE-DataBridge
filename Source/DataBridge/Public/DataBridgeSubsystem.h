#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DataBridgeTypes.h"
#include "IDataBridgeParser.h"
#include "IDataBridgeHttpClient.h"
#include "DataBridgeSubsystem.generated.h"

class UDataTable;
class UCurveTable;

UCLASS()
class DATABRIDGE_API UDataBridgeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// === Direct API ===

	UFUNCTION(BlueprintCallable, Category = "DataBridge")
	void FetchTable(const FString& URL, UDataTable* TargetTable, EDataBridgeFormat Format = EDataBridgeFormat::Json);

	UFUNCTION(BlueprintCallable, Category = "DataBridge")
	void FetchCurveTable(const FString& URL, UCurveTable* TargetTable, EDataBridgeFormat Format = EDataBridgeFormat::Json);

	// === Source-based API ===

	UFUNCTION(BlueprintCallable, Category = "DataBridge")
	void FetchSource(FName SourceName);

	UFUNCTION(BlueprintCallable, Category = "DataBridge")
	void FetchAllSources();

	// === Cache ===

	UFUNCTION(BlueprintCallable, Category = "DataBridge")
	void InvalidateCache(FName SourceName);

	UFUNCTION(BlueprintCallable, Category = "DataBridge")
	void InvalidateAllCache();

	// === Environment ===

	UFUNCTION(BlueprintCallable, Category = "DataBridge")
	void SetEnvironment(EDataBridgeEnvironment NewEnvironment);

	UFUNCTION(BlueprintPure, Category = "DataBridge")
	EDataBridgeEnvironment GetCurrentEnvironment() const { return CurrentEnvironment; }

	// === Parser ===

	void RegisterParser(TSharedPtr<IDataBridgeParser> Parser);

	// === Delegates ===

	UPROPERTY(BlueprintAssignable, Category = "DataBridge")
	FOnDataBridgeFetchCompleted OnFetchCompleted;

	UPROPERTY(BlueprintAssignable, Category = "DataBridge")
	FOnDataBridgeAllSourcesCompleted OnAllSourcesCompleted;

private:
	void FetchTableInternal(FName SourceName, const FString& URL, UDataTable* TargetTable, EDataBridgeFormat Format);
	void FetchCurveTableInternal(FName SourceName, const FString& URL, UCurveTable* TargetTable, EDataBridgeFormat Format);

	FString ResolveURL(const FDataBridgeSource& Source) const;
	FName ResolveParserName(EDataBridgeFormat Format, const FString& URL, bool bCurveTable) const;

	TSharedPtr<IDataBridgeHttpClient> HttpClient;
	TMap<FName, TSharedPtr<IDataBridgeParser>> Parsers;

	struct FCacheEntry
	{
		double FetchTime = 0.0;
	};
	TMap<FString, FCacheEntry> Cache;

	EDataBridgeEnvironment CurrentEnvironment = EDataBridgeEnvironment::Local;

	// 6-C: 콘솔 명령 핸들 보관
	TArray<IConsoleCommand*> ConsoleCommands;
};
