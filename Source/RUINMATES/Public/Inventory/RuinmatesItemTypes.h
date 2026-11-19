#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RuinmatesItemTypes.generated.h"

class UTexture2D;

// Loot rarity. Colors stay muted except for the top two tiers.
UENUM(BlueprintType)
enum class ERuinmatesItemQuality : uint8
{
	Common UMETA(DisplayName = "普通"),
	Fine UMETA(DisplayName = "精良"),
	Advanced UMETA(DisplayName = "高级"),
	Rare UMETA(DisplayName = "稀有"),
	Precious UMETA(DisplayName = "珍贵")
};

// Static inventory category. Used by the item database, filters and merchant lists.
UENUM(BlueprintType)
enum class ERuinmatesItemCategory : uint8
{
	Consumable UMETA(DisplayName = "消耗品"),
	Ammo UMETA(DisplayName = "弹药"),
	Material UMETA(DisplayName = "材料"),
	Valuable UMETA(DisplayName = "贵重物品"),
	Weapon UMETA(DisplayName = "武器"),
	Equipment UMETA(DisplayName = "装备"),
	Quest UMETA(DisplayName = "任务物品")
};

// One row of the single authoritative item database (DT_Items). Static data only:
// it never carries a held count or a search reveal state.
USTRUCT(BlueprintType)
struct RUINMATES_API FRuinmatesItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Item")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Item")
	ERuinmatesItemCategory Category = ERuinmatesItemCategory::Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Item")
	ERuinmatesItemQuality Quality = ERuinmatesItemQuality::Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Item")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Item", meta = (ClampMin = "1"))
	int32 MaxStack = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Item", meta = (ClampMin = "0"))
	int32 BuyPrice = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Item", meta = (ClampMin = "0"))
	int32 SellPrice = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Item")
	bool bCanBuy = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Item")
	bool bCanSell = true;

	// Upgrade parts are not only vendor trash. The base-upgrade use comes later.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Item")
	bool bUpgradeMaterial = false;
};

// Permanent backpack growth. Slot counts are a starting pass, not a final economy.
UENUM(BlueprintType)
enum class ERuinmatesBackpackTier : uint8
{
	Issue UMETA(DisplayName = "免费制式背包"),
	Explore UMETA(DisplayName = "普通探索背包"),
	Tactical UMETA(DisplayName = "高级战术背包"),
	Heavy UMETA(DisplayName = "重型回收背包")
};

// One stack in a uniform grid. Same ItemId and quality stack together.
USTRUCT(BlueprintType)
struct RUINMATES_API FRuinmatesItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Item")
	ERuinmatesItemQuality Quality = ERuinmatesItemQuality::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Item", meta = (ClampMin = "0"))
	int32 UnitValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Item", meta = (ClampMin = "0"))
	int32 Count = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Item", meta = (ClampMin = "1"))
	int32 MaxStack = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Item")
	FText Description;

	// Upgrade parts are not only vendor trash. The base-upgrade use comes later.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Item")
	bool bUpgradeMaterial = false;

	// Containers hide this until the shared search timer reveals it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Item")
	bool bRevealed = true;

	bool IsEmpty() const { return ItemId.IsNone() || Count <= 0; }

	int32 GetTotalValue() const { return FMath::Max(0, UnitValue) * FMath::Max(0, Count); }

	bool CanStackWith(const FRuinmatesItemStack& Other) const
	{
		return !IsEmpty() && !Other.IsEmpty() && ItemId == Other.ItemId && Quality == Other.Quality && Count < MaxStack;
	}
};

USTRUCT(BlueprintType)
struct RUINMATES_API FRuinmatesLootEntry
{
	GENERATED_BODY()

	// Preferred source: an ItemId resolved through the item database.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Loot")
	FName ItemId;

	// Legacy template kept so already-authored LootPool entries keep working. Only used when
	// ItemId is empty or unknown to the database.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Loot")
	FRuinmatesItemStack Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 Weight = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MaxCount = 1;
};

namespace RuinmatesItems
{
	inline int32 GetBackpackCapacity(ERuinmatesBackpackTier Tier)
	{
		switch (Tier)
		{
		case ERuinmatesBackpackTier::Explore: return 15;
		case ERuinmatesBackpackTier::Tactical: return 20;
		case ERuinmatesBackpackTier::Heavy: return 30;
		case ERuinmatesBackpackTier::Issue:
		default: return 10;
		}
	}

	inline FText GetQualityName(ERuinmatesItemQuality Quality)
	{
		switch (Quality)
		{
		case ERuinmatesItemQuality::Fine: return NSLOCTEXT("RuinMates", "QualityFine", "精良");
		case ERuinmatesItemQuality::Advanced: return NSLOCTEXT("RuinMates", "QualityFineLegacy", "精良");
		case ERuinmatesItemQuality::Rare: return NSLOCTEXT("RuinMates", "QualityRare", "稀有");
		case ERuinmatesItemQuality::Precious: return NSLOCTEXT("RuinMates", "QualityPrecious", "珍贵");
		case ERuinmatesItemQuality::Common:
		default: return NSLOCTEXT("RuinMates", "QualityCommon", "普通");
		}
	}

	inline FText GetBackpackTierName(ERuinmatesBackpackTier Tier)
	{
		switch (Tier)
		{
		case ERuinmatesBackpackTier::Explore: return NSLOCTEXT("RuinMates", "PackExplore", "普通探索背包");
		case ERuinmatesBackpackTier::Tactical: return NSLOCTEXT("RuinMates", "PackTactical", "高级战术背包");
		case ERuinmatesBackpackTier::Heavy: return NSLOCTEXT("RuinMates", "PackHeavy", "重型回收背包");
		case ERuinmatesBackpackTier::Issue:
		default: return NSLOCTEXT("RuinMates", "PackIssue", "免费制式背包");
		}
	}

	inline FLinearColor GetQualityColor(ERuinmatesItemQuality Quality)
	{
		switch (Quality)
		{
		case ERuinmatesItemQuality::Fine: return FLinearColor(0.55f, 0.66f, 0.48f);
		case ERuinmatesItemQuality::Advanced: return FLinearColor(0.55f, 0.66f, 0.48f);
		case ERuinmatesItemQuality::Rare: return FLinearColor(0.62f, 0.45f, 0.82f);
		case ERuinmatesItemQuality::Precious: return FLinearColor(0.93f, 0.76f, 0.32f);
		case ERuinmatesItemQuality::Common:
		default: return FLinearColor(0.62f, 0.64f, 0.60f);
		}
	}
}
