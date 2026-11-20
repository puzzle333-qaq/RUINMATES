#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RuinmatesHubDefinition.generated.h"

class UWorld;

/** One cost in the existing DT_Items identifier space; never a separate Hub currency. */
USTRUCT(BlueprintType)
struct RUINMATES_API FHubMaterialCost
{
	GENERATED_BODY()

	/** Item row identifier resolved and validated by the authority against the existing database. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Cost")
	FName ItemId = NAME_None;

	/** Exact positive unit count; the construction transaction must reject invalid or partial costs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Cost", meta = (ClampMin = "1"))
	int32 Count = 1;
};

/** Required permanent progress of another facility; temporary power is evaluated separately. */
USTRUCT(BlueprintType)
struct RUINMATES_API FHubPrerequisite
{
	GENERATED_BODY()

	/** Stable room identifier accepted by the authoritative Hub state source. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Prerequisite")
	FName RoomId = NAME_None;

	/** Minimum permanent level of the prerequisite facility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Prerequisite", meta = (ClampMin = "0", ClampMax = "2"))
	int32 MinimumLevel = 0;

	/** Also require the prerequisite room to have been permanently unlocked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Prerequisite")
	bool bRequiresUnlocked = true;
};

/** Construction intent, interpreted by the authority rather than supplied as a client result. */
UENUM(BlueprintType)
enum class ERuinmatesHubActionType : uint8
{
	Clear,
	Unlock,
	Upgrade,
	RestoreExit
};

/** One configurable construction recipe; visual Actors never inspect or consume its resources. */
USTRUCT(BlueprintType)
struct RUINMATES_API FHubActionDefinition
{
	GENERATED_BODY()

	/** Unique stable action identifier sent as an intent and resolved from this server-side asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Action")
	FName ActionId = NAME_None;

	/** Stable target facility identifier; level limits remain owned by the Hub state source. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Action")
	FName RoomId = NAME_None;

	/** Permanent construction operation performed after the entire transaction validates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Action")
	ERuinmatesHubActionType Type = ERuinmatesHubActionType::Clear;

	/** Exact current level required by this recipe; prevents replaying an earlier upgrade. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Action", meta = (ClampMin = "0", ClampMax = "2"))
	int32 RequiredLevel = 0;

	/** Resulting level, still checked against the fixed room scope by the authoritative state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Action", meta = (ClampMin = "0", ClampMax = "2"))
	int32 TargetLevel = 0;

	/** Complete atomic material bill, using existing item IDs. Duplicate IDs must be aggregated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Action")
	TArray<FHubMaterialCost> Costs;

	/** Permanent facility requirements, all of which must pass before resources are consumed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Action")
	TArray<FHubPrerequisite> Prerequisites;

	/** Require actual current power while performing this operation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Action")
	bool bRequiresPower = false;
};

/** Configurable construction resource sources; the warehouse is always considered first. */
UENUM(BlueprintType)
enum class ERuinmatesHubResourcePolicy : uint8
{
	WarehouseOnly,
	WarehouseThenBackpack
};

/**
 * Central Hub recipes and operating configuration. Contains no player inventory or mutable progress.
 * Constructor defaults are explicitly test economy data; all concrete assets remain editor-configured.
 */
UCLASS(BlueprintType)
class RUINMATES_API URuinmatesHubDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Initializes pure configuration only; never loads content or references a live world. */
	URuinmatesHubDefinition();

	/** Replaces pure configuration with the documented test recipes; never grants or saves resources. */
	static void FillTestDefaults(URuinmatesHubDefinition* Definition);

	/** Authoritative construction recipes; generator install/refuel/start use dedicated service intents. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Construction")
	TArray<FHubActionDefinition> Actions;

	/** Allowed inventory sources for construction, installation and refuelling transactions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Resources")
	ERuinmatesHubResourcePolicy ResourcePolicy = ERuinmatesHubResourcePolicy::WarehouseOnly;

	/** Number of item slots in the shared warehouse, using the existing item stack representation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Resources", meta = (ClampMin = "1"))
	int32 WarehouseCapacity = 96;

	/** Existing item database ID consumed once by the dedicated generator installation transaction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Generator")
	FName GeneratorItemId = TEXT("Hub_Generator");

	/** Existing item database ID consumed by the dedicated fuel transaction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Generator")
	FName FuelItemId = TEXT("Hub_Fuel");

	/** Maximum stored fuel units; runtime code also validates finite positive configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Generator", meta = (ClampMin = "0.01"))
	float FuelCapacity = 100.f;

	/** Fuel units supplied by one fuel item; no partial item consumption is permitted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Generator", meta = (ClampMin = "0.01"))
	float UnitsPerFuel = 20.f;

	/** Fuel units consumed each second while the installed, started generator actually simulates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Generator", meta = (ClampMin = "0"))
	float BurnPerSecond = 0.02f;

	/** Authority simulation timer interval in seconds; offline or mission time is never deducted here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Generator", meta = (ClampMin = "0.1"))
	float SimulationInterval = 1.f;

	/** One-time campaign starter grant; the campaign save must own its claimed flag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Resources")
	TArray<FHubMaterialCost> StarterKit;

	/** Per-facility operating requirement; missing entries fail safe by requiring electricity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hub|Generator")
	TMap<FName,bool> FacilityRequiresPower;

	/** Optional real exploration destination. Null explicitly disables departure rather than faking a mission. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Travel")
	TSoftObjectPtr<UWorld> DestinationMap;

	/** True means these resource quantities are provisional development values, not balanced release data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Construction")
	bool bTestEconomy = true;

	/** Optional future workshop service hook; None means no manufacturing/repair economy is configured. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Services")
	FName WorkshopServiceId = NAME_None;

	/** Basic server-authoritative medical service amount; dead characters cannot use the terminal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub|Services", meta=(ClampMin="0"))
	float MedicalHealAmount = 25.f;
};
