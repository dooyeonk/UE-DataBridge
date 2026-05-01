#pragma once

#include "CoreMinimal.h"

class UDataTable;

struct DATABRIDGE_API FDataTableDiff
{
	TArray<FName> AddedRows;
	TArray<FName> RemovedRows;
	TArray<FName> ModifiedRows;

	bool HasChanges() const
	{
		return AddedRows.Num() > 0 || RemovedRows.Num() > 0 || ModifiedRows.Num() > 0;
	}

	FString ToString(const FString& SourceName, const FString& URL, const FString& TablePath) const;
};

FDataTableDiff DATABRIDGE_API ComputeDataTableDiff(UDataTable* Existing, UDataTable* Incoming);
