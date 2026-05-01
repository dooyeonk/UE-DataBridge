#include "Utilities/DataBridgeDiff.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"

namespace
{
	TArray<FDataBridgeFieldChange> ComputeRowFieldChanges(
		const UScriptStruct* RowStruct, const uint8* OldData, const uint8* NewData)
	{
		TArray<FDataBridgeFieldChange> Changes;
		if (!RowStruct || !OldData || !NewData) return Changes;

		for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
		{
			FProperty* Prop = *It;
			const void* OldVal = Prop->ContainerPtrToValuePtr<void>(OldData);
			const void* NewVal = Prop->ContainerPtrToValuePtr<void>(NewData);

			if (Prop->Identical(OldVal, NewVal, PPF_None)) continue;

			FDataBridgeFieldChange Change;
			Change.PropertyName = Prop->GetName();
			Prop->ExportTextItem_Direct(Change.OldValue, OldVal, nullptr, nullptr, PPF_None);
			Prop->ExportTextItem_Direct(Change.NewValue, NewVal, nullptr, nullptr, PPF_None);
			Changes.Add(MoveTemp(Change));
		}
		return Changes;
	}
}

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
		Out += FString::Printf(TEXT("    ~ Modified(%d):\n"), ModifiedRows.Num());
		for (FName Name : ModifiedRows)
		{
			const TArray<FDataBridgeFieldChange>* Fields = ModifiedRowFields.Find(Name);
			if (Fields && Fields->Num() > 0)
			{
				TArray<FString> Pairs;
				for (const FDataBridgeFieldChange& Field : *Fields)
				{
					Pairs.Add(FString::Printf(TEXT("%s: %s → %s"),
						*Field.PropertyName, *Field.OldValue, *Field.NewValue));
				}
				Out += FString::Printf(TEXT("        %s (%s)\n"),
					*Name.ToString(), *FString::Join(Pairs, TEXT(", ")));
			}
			else
			{
				Out += FString::Printf(TEXT("        %s\n"), *Name.ToString());
			}
		}
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
			TArray<FDataBridgeFieldChange> Fields = ComputeRowFieldChanges(RowStruct, ExistingData, Pair.Value);
			if (Fields.Num() > 0) Diff.ModifiedRowFields.Add(Pair.Key, MoveTemp(Fields));
		}
	}

	for (auto& Pair : ExistingRows)
	{
		if (!IncomingRows.Contains(Pair.Key))
			Diff.RemovedRows.Add(Pair.Key);
	}

	return Diff;
}
