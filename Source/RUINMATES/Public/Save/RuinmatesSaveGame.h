#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Hub/RuinmatesHubTypes.h"
#include "Inventory/RuinmatesItemTypes.h"
#include "RuinmatesSaveGame.generated.h"

USTRUCT(BlueprintType)
struct RUINMATES_API FRuinmatesMemberSave
{
	GENERATED_BODY()

	// Stable id that survives map travel and a process restart (unique net id or a generated guid).
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	FString StableId;

	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	int32 Level = 1;

	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	TArray<FName> TalentIds;

	// Soft identifier for the member's equipped loadout / weapon data. May be empty.
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	FString EquippedLoadoutId;

	/** Distinguishes a deliberately empty backpack from a legacy member with no inventory data. */
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	bool bHasBackpack = false;

	/** Personal grid tier, using the existing backpack representation. */
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	ERuinmatesBackpackTier BackpackTier = ERuinmatesBackpackTier::Issue;

	/** Zero preserves tier capacity; an explicit override is checked before any restore. */
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	int32 BackpackCapacityOverride = 0;

	/** Canonical item identities/counts; static fields are normalized from DT_Items when loaded. */
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	TArray<FRuinmatesItemStack> BackpackStacks;
};

UCLASS()
class RUINMATES_API URuinmatesSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Current campaign schema. Default field value stays zero to identify pre-versioned slots. */
	static constexpr int32 CurrentSaveVersion = 1;

	/** Zero identifies a legacy campaign; unsupported future versions are rejected, never reset. */
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	int32 SaveVersion = 0;

	/** Monotonic complete-snapshot generation used to choose the latest valid A/B slot. */
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	int64 SaveGeneration = 0;

	/** Stable offline host identity within this profile, independent of session PlayerId/GUID. */
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	FString LocalHostProfileId = TEXT("LocalHost");

	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	int32 SharedFunds = 0;

	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	TArray<FRuinmatesMemberSave> Members;

	/** False in legacy campaigns; loading must not pretend that a default Hub has been saved. */
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	bool bHasHubState = false;

	/** Durable room/generator/floor data only; runtime room fields and initialization are cleared. */
	UPROPERTY()
	FRuinmatesHubSnapshot HubSnapshot;

	/** Real shared inventory capacity, reusing the existing backpack grid implementation. */
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	int32 WarehouseCapacity = 96;

	/** Canonical shared inventory; never a second set of mutable resource totals. */
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	TArray<FRuinmatesItemStack> WarehouseStacks;

	FRuinmatesMemberSave* FindMember(const FString& StableId);
	FRuinmatesMemberSave& FindOrAddMember(const FString& StableId);
};
