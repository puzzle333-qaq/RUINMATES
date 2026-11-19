#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Inventory/RuinmatesItemTypes.h"
#include "RuinmatesItemDatabaseSubsystem.generated.h"

class UDataTable;

// Single read path for static item data on both server and clients.
// Everything (loot, backpack display, merchant, buy/sell) resolves ItemId through here; the server
// never trusts a client-provided name, quality, stack size or price.
UCLASS()
class RUINMATES_API URuinmatesItemDatabaseSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Returns the subsystem for a world context, or null outside a game instance (e.g. bare test worlds).
	static URuinmatesItemDatabaseSubsystem* Get(const UObject* WorldContextObject);

	// Loads the configured table once. Safe to call repeatedly.
	void EnsureLoaded();

	bool IsValidItemId(FName ItemId) const;
	const FRuinmatesItemRow* FindItemDefinition(FName ItemId) const;
	FName ResolveItemId(FName ItemId, const FText& LegacyDisplayName = FText::GetEmpty()) const;
	bool NormalizeStack(FRuinmatesItemStack& Stack, bool bPreserveCount = true) const;

	// Table-level helpers so tooling and tests can use the same parsing rules without a subsystem.
	static const FRuinmatesItemRow* FindRowInTable(const UDataTable* Table, FName ItemId);
	static FRuinmatesItemStack CreateStackFromRow(const FRuinmatesItemRow& Row, FName ItemId, int32 Count);

	// Table-level legacy id resolution (unknown id -> matching DisplayName), used by tooling/tests.
	static FName ResolveItemIdInTable(const UDataTable* Table, FName ItemId, const FText& LegacyDisplayName);

	int32 GetItemBuyPrice(FName ItemId) const;
	int32 GetItemSellPrice(FName ItemId) const;
	int32 GetMaxStack(FName ItemId) const;
	TArray<ERuinmatesItemCategory> GetAvailableCategories() const;

	// Builds a runtime stack carrying the static fields copied from the database row.
	FRuinmatesItemStack CreateItemStack(FName ItemId, int32 Count) const;

	TArray<FName> GetAllItemIds() const;

	bool IsLoaded() const { return IsValid(LoadedTable); }
	UDataTable* GetTable() const { return LoadedTable; }

	// Test / tools hook: use a specific table instead of the configured asset.
	void SetTableOverride(UDataTable* Table);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedTable;

	bool bLoadAttempted = false;
};
