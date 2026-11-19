#include "Inventory/RuinmatesItemDatabaseSubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Inventory/RuinmatesItemDatabaseSettings.h"

void URuinmatesItemDatabaseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsureLoaded();
}

void URuinmatesItemDatabaseSubsystem::EnsureLoaded()
{
	if (IsValid(LoadedTable) || bLoadAttempted)
	{
		return;
	}
	bLoadAttempted = true;

	const URuinmatesItemDatabaseSettings* Settings = GetDefault<URuinmatesItemDatabaseSettings>();
	if (!Settings)
	{
		UE_LOG(LogTemp, Warning, TEXT("RuinMates item database settings are missing"));
		return;
	}
	if (Settings->ItemTable.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("RuinMates item database asset is not configured (Project Settings > Game > RuinMates Items)"));
		return;
	}

	LoadedTable = Settings->ItemTable.LoadSynchronous();
	if (!IsValid(LoadedTable))
	{
		UE_LOG(LogTemp, Error, TEXT("RuinMates item database could not load %s"), *Settings->ItemTable.ToString());
		return;
	}
	if (LoadedTable->GetRowStruct() != FRuinmatesItemRow::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("RuinMates item database %s does not use FRuinmatesItemRow"), *LoadedTable->GetName());
		LoadedTable = nullptr;
	}
}

void URuinmatesItemDatabaseSubsystem::SetTableOverride(UDataTable* Table)
{
	LoadedTable = Table;
	bLoadAttempted = true;
}

URuinmatesItemDatabaseSubsystem* URuinmatesItemDatabaseSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<URuinmatesItemDatabaseSubsystem>() : nullptr;
}

const FRuinmatesItemRow* URuinmatesItemDatabaseSubsystem::FindRowInTable(const UDataTable* Table, FName ItemId)
{
	if (!IsValid(Table) || ItemId.IsNone() || Table->GetRowStruct() != FRuinmatesItemRow::StaticStruct())
	{
		return nullptr;
	}
	static const FString Context(TEXT("RuinMatesItemDatabase"));
	return Table->FindRow<FRuinmatesItemRow>(ItemId, Context, false);
}

const FRuinmatesItemRow* URuinmatesItemDatabaseSubsystem::FindItemDefinition(FName ItemId) const
{
	if (ItemId.IsNone())
	{
		return nullptr;
	}
	if (!IsValid(LoadedTable))
	{
		const_cast<URuinmatesItemDatabaseSubsystem*>(this)->EnsureLoaded();
	}
	return FindRowInTable(LoadedTable, ItemId);
}

FName URuinmatesItemDatabaseSubsystem::ResolveItemId(FName ItemId, const FText& LegacyDisplayName) const
{
	if (FindItemDefinition(ItemId))
	{
		return ItemId;
	}
	if (!IsValid(LoadedTable) || LegacyDisplayName.IsEmpty())
	{
		return NAME_None;
	}

	FName ResolvedId = NAME_None;
	const FString WantedName = LegacyDisplayName.ToString();
	LoadedTable->ForeachRow<FRuinmatesItemRow>(TEXT("RuinMatesLegacyItemId"),
		[&ResolvedId, &WantedName](const FName& RowName, const FRuinmatesItemRow& Row)
		{
			if (ResolvedId.IsNone() && Row.DisplayName.ToString() == WantedName)
			{
				ResolvedId = RowName;
			}
		});
	return ResolvedId;
}

bool URuinmatesItemDatabaseSubsystem::NormalizeStack(FRuinmatesItemStack& Stack, bool bPreserveCount) const
{
	const int32 OriginalCount = Stack.Count;
	const FName ResolvedId = ResolveItemId(Stack.ItemId, Stack.DisplayName);
	const FRuinmatesItemRow* Row = FindItemDefinition(ResolvedId);
	if (!Row || OriginalCount <= 0)
	{
		return false;
	}

	const bool bWasRevealed = Stack.bRevealed;
	Stack = CreateStackFromRow(*Row, ResolvedId, bPreserveCount ? 1 : FMath::Min(OriginalCount, FMath::Max(1, Row->MaxStack)));
	Stack.Count = bPreserveCount ? OriginalCount : FMath::Min(OriginalCount, FMath::Max(1, Row->MaxStack));
	Stack.bRevealed = bWasRevealed;
	return true;
}

FRuinmatesItemStack URuinmatesItemDatabaseSubsystem::CreateStackFromRow(const FRuinmatesItemRow& Row, FName ItemId, int32 Count)
{
	FRuinmatesItemStack Stack;
	Stack.ItemId = ItemId;
	Stack.DisplayName = Row.DisplayName;
	Stack.Description = Row.Description;
	Stack.Quality = Row.Quality == ERuinmatesItemQuality::Advanced ? ERuinmatesItemQuality::Fine : Row.Quality;
	Stack.MaxStack = FMath::Max(1, Row.MaxStack);
	Stack.UnitValue = FMath::Max(0, Row.SellPrice);
	Stack.bUpgradeMaterial = Row.bUpgradeMaterial;
	Stack.Count = FMath::Clamp(Count, 0, Stack.MaxStack);
	return Stack;
}

bool URuinmatesItemDatabaseSubsystem::IsValidItemId(FName ItemId) const
{
	return FindItemDefinition(ItemId) != nullptr;
}

int32 URuinmatesItemDatabaseSubsystem::GetItemBuyPrice(FName ItemId) const
{
	const FRuinmatesItemRow* Row = FindItemDefinition(ItemId);
	return Row && Row->bCanBuy ? FMath::Max(0, Row->BuyPrice) : 0;
}

int32 URuinmatesItemDatabaseSubsystem::GetItemSellPrice(FName ItemId) const
{
	const FRuinmatesItemRow* Row = FindItemDefinition(ItemId);
	return Row && Row->bCanSell ? FMath::Max(0, Row->SellPrice) : 0;
}

int32 URuinmatesItemDatabaseSubsystem::GetMaxStack(FName ItemId) const
{
	const FRuinmatesItemRow* Row = FindItemDefinition(ItemId);
	return Row ? FMath::Max(1, Row->MaxStack) : 1;
}

TArray<ERuinmatesItemCategory> URuinmatesItemDatabaseSubsystem::GetAvailableCategories() const
{
	TArray<ERuinmatesItemCategory> Categories;
	if (!IsValid(LoadedTable))
	{
		const_cast<URuinmatesItemDatabaseSubsystem*>(this)->EnsureLoaded();
	}
	if (!IsValid(LoadedTable))
	{
		return Categories;
	}
	LoadedTable->ForeachRow<FRuinmatesItemRow>(TEXT("RuinMatesItemCategories"),
		[&Categories](const FName&, const FRuinmatesItemRow& Row)
		{
			Categories.AddUnique(Row.Category);
		});
	Categories.Sort([](ERuinmatesItemCategory A, ERuinmatesItemCategory B)
	{
		return static_cast<uint8>(A) < static_cast<uint8>(B);
	});
	return Categories;
}

FRuinmatesItemStack URuinmatesItemDatabaseSubsystem::CreateItemStack(FName ItemId, int32 Count) const
{
	const FRuinmatesItemRow* Row = FindItemDefinition(ItemId);
	return Row ? CreateStackFromRow(*Row, ItemId, Count) : FRuinmatesItemStack();
}

TArray<FName> URuinmatesItemDatabaseSubsystem::GetAllItemIds() const
{
	if (!IsValid(LoadedTable))
	{
		const_cast<URuinmatesItemDatabaseSubsystem*>(this)->EnsureLoaded();
	}
	if (!IsValid(LoadedTable))
	{
		return TArray<FName>();
	}
	return LoadedTable->GetRowNames();
}
