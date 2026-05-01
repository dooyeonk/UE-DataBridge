#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DataBridgeTypes.h"
#include "Interfaces/IDataBridgeParser.h"
#include "Http/IDataBridgeHttpClient.h"
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
	void FetchCurveTable(const FString& URL, UCurveTable* TargetTable, EDataBridgeFormat Format = EDataBridgeFormat::Csv);

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
	void RegisterConsoleCommands();
	void FetchSourceWithCallback(FName SourceName, TFunction<void(bool)> OnComplete);
	void FetchTableInternal(FName SourceName, const FString& URL, UDataTable* TargetTable, EDataBridgeFormat Format, TFunction<void(bool)> OnComplete = nullptr);
	void FetchCurveTableInternal(FName SourceName, const FString& URL, UCurveTable* TargetTable, EDataBridgeFormat Format, TFunction<void(bool)> OnComplete = nullptr);

	FString ResolveURL(const FDataBridgeSource& Source) const;
	FName ResolveParserName(EDataBridgeFormat Format, const FString& URL, bool bCurveTable) const;

	FString MakeCacheKey(FName SourceName) const;
	bool IsCacheValid(FName SourceName, float TTLSeconds) const;
	void StoreCache(FName SourceName, const FString& Body, EDataBridgeFormat Format, bool bIsCurveTable);

	bool TryServeFromCache(FName SourceName, float TTLSeconds, UDataTable* TargetTable, UCurveTable* CurveTable);

	TSharedPtr<IDataBridgeHttpClient> HttpClient;
	TMap<FName, TSharedPtr<IDataBridgeParser>> Parsers;

	EDataBridgeEnvironment CurrentEnvironment = EDataBridgeEnvironment::Local;

	TArray<IConsoleCommand*> ConsoleCommands;
};
