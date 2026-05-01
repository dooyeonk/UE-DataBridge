#pragma once

#include "CoreMinimal.h"

class UDataTable;

struct FDataBridgeFieldChange
{
	FString PropertyName;
	FString OldValue;
	FString NewValue;
};

struct DATABRIDGE_API FDataTableDiff
{
	TArray<FName> AddedRows;
	TArray<FName> RemovedRows;
	TArray<FName> ModifiedRows;
	// 행 단위 필드 변경 내역 (ModifiedRows의 각 RowName에 대한 상세)
	TMap<FName, TArray<FDataBridgeFieldChange>> ModifiedRowFields;

	bool HasChanges() const
	{
		return AddedRows.Num() > 0 || RemovedRows.Num() > 0 || ModifiedRows.Num() > 0;
	}

	FString ToString(const FString& SourceName, const FString& URL, const FString& TablePath) const;
};

FDataTableDiff DATABRIDGE_API ComputeDataTableDiff(UDataTable* Existing, UDataTable* Incoming);
