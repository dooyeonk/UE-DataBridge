#include "DataBridgeDiff.h"
#include "Engine/DataTable.h"

FString FDataTableDiff::ToString(const FString& SourceName, const FString& URL, const FString& TablePath) const
{
	FString Out;
	Out += FString::Printf(TEXT("[DataBridge] Source: %s\n"), *SourceName);
	Out += FString::Printf(TEXT("  URL:    %s\n"), *URL);
	Out += FString::Printf(TEXT("  Target: %s\n"), *TablePath);

	if (!HasChanges())
	{
		Out += TEXT("  No changes detected.\n");
		return Out;
	}

	Out += TEXT("\n  Changes detected:\n");

	if (AddedRows.Num() > 0)
	{
		TArray<FString> Names;
		for (FName Name : AddedRows) Names.Add(Name.ToString());
		Out += FString::Printf(TEXT("    + Added   (%d): %s\n"), AddedRows.Num(), *FString::Join(Names, TEXT(", ")));
	}

	if (ModifiedRows.Num() > 0)
	{
		TArray<FString> Names;
		for (FName Name : ModifiedRows) Names.Add(Name.ToString());
		Out += FString::Printf(TEXT("    ~ Modified(%d): %s\n"), ModifiedRows.Num(), *FString::Join(Names, TEXT(", ")));
	}

	if (RemovedRows.Num() > 0)
	{
		TArray<FString> Names;
		for (FName Name : RemovedRows) Names.Add(Name.ToString());
		Out += FString::Printf(TEXT("    - Removed (%d): %s\n"), RemovedRows.Num(), *FString::Join(Names, TEXT(", ")));
	}

	return Out;
}

FDataTableDiff ComputeDataTableDiff(UDataTable* Existing, UDataTable* Incoming)
{
	FDataTableDiff Diff;
	if (!Existing || !Incoming) return Diff;

	const UScriptStruct* RowStruct = Existing->GetRowStruct();
	if (!RowStruct) return Diff;

	const TMap<FName, uint8*>& ExistingRows = Existing->GetRowMap();
	const TMap<FName, uint8*>& IncomingRows = Incoming->GetRowMap();

	// Added / Modified
	for (auto& Pair : IncomingRows)
	{
		const uint8* ExistingData = ExistingRows.FindRef(Pair.Key);
		if (!ExistingData)
		{
			Diff.AddedRows.Add(Pair.Key);
		}
		else if (!RowStruct->CompareScriptStruct(ExistingData, Pair.Value, PPF_None))
		{
			Diff.ModifiedRows.Add(Pair.Key);
		}
	}

	// Removed
	for (auto& Pair : ExistingRows)
	{
		if (!IncomingRows.Contains(Pair.Key))
		{
			Diff.RemovedRows.Add(Pair.Key);
		}
	}

	return Diff;
}
