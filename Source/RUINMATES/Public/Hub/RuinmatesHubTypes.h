#pragma once

#include "CoreMinimal.h"
#include "RuinmatesHubTypes.generated.h"

/** Stable facility identifiers; Actor labels never identify saved construction progress. */
namespace RuinmatesHub
{
	inline FName CoreId() { return TEXT("Hub.Core"); }
	inline FName MedicalId() { return TEXT("Hub.Medical"); }
	inline FName WorkshopId() { return TEXT("Hub.Workshop"); }
	inline FName ElevatorId() { return TEXT("Hub.Elevator"); }
	inline FName BlackMarketId() { return TEXT("Hub.BlackMarket"); }
	inline FName ExitId() { return TEXT("Hub.Exit"); }
}

/** Permanent construction facts. Power changes must never rewrite these fields. */
USTRUCT(BlueprintType)
struct RUINMATES_API FRuinmatesHubPermanentState
{
	GENERATED_BODY()

	/** Completed upgrade level; independent of clearing and unlocking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	int32 Level = 0;

	/** Whether the authored structural debris has been cleared permanently. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	bool bCleared = false;

	/** Whether the room has been permanently unlocked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	bool bUnlocked = false;

	/** Whether permanent structural obstructions still block the entrance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	bool bStructureBlocked = true;

	bool operator==(const FRuinmatesHubPermanentState& Other) const
	{
		return Level == Other.Level && bCleared == Other.bCleared
			&& bUnlocked == Other.bUnlocked && bStructureBlocked == Other.bStructureBlocked;
	}
};

/** Temporary operating facts, recomputed by the future authority-side power manager. */
USTRUCT(BlueprintType)
struct RUINMATES_API FRuinmatesHubRuntimeState
{
	GENERATED_BODY()

	/** Actual electrical supply; does not imply permanent room restoration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	bool bPowered = false;

	/** Whether the facility service may currently operate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	bool bOperational = false;

	/** Derived hardware visibility; independent of Core level or temporary electrical supply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	bool bGeneratorInstalled = false;

	bool operator==(const FRuinmatesHubRuntimeState& Other) const
	{
		return bPowered == Other.bPowered && bOperational == Other.bOperational && bGeneratorInstalled == Other.bGeneratorInstalled;
	}
};

/** Installed hardware and runtime fuel are distinct from room restoration. */
USTRUCT(BlueprintType)
struct RUINMATES_API FRuinmatesHubGeneratorState
{
	GENERATED_BODY()
	/** Installation is permanent until an explicit future removal action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	bool bInstalled = false;
	/** Player-requested running state; fuel exhaustion turns it off safely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	bool bStarted = false;
	/** Simulation fuel units, never deducted while the Hub world is not running. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	float Fuel = 0.f;
	bool IsPowered() const { return bInstalled && bStarted && Fuel > 0.f; }
};

/** One authoritative facility state; static definitions and recipes are added in Phase 5. */
USTRUCT(BlueprintType)
struct RUINMATES_API FRuinmatesHubRoomState
{
	GENERATED_BODY()

	/** Stable identifier shared by map bindings, future recipes and campaign persistence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FName RoomId = NAME_None;

	/** Durable building state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FRuinmatesHubPermanentState Permanent;

	/** Ephemeral service and power state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FRuinmatesHubRuntimeState Runtime;

	bool operator==(const FRuinmatesHubRoomState& Other) const
	{
		return RoomId == Other.RoomId && Permanent == Other.Permanent && Runtime == Other.Runtime;
	}
};

/** One replicated property prevents a room array arriving without its initialization gate. */
USTRUCT()
struct RUINMATES_API FRuinmatesHubSnapshot
{
	GENERATED_BODY()

	/** False until the server has supplied initial or restored construction state. */
	UPROPERTY()
	bool bInitialized = false;

	/** Authority-owned room snapshots. No map decoration state is stored here. */
	UPROPERTY()
	TArray<FRuinmatesHubRoomState> Rooms;

	/** Server revision protects UI requests against a changed construction snapshot. */
	UPROPERTY()
	int32 Revision = 0;

	/** Only first floor is playable; upper floors require independent future authorization. */
	UPROPERTY()
	TArray<int32> OpenFloors = { 1 };

	/** Hardware and fuel snapshot; actual room supply is derived. */
	UPROPERTY()
	FRuinmatesHubGeneratorState Generator;

	/** One-time initial materials must not be granted again on map re-entry. */
	UPROPERTY()
	bool bStarterKitClaimed = false;
};

/** Authoritative action evaluation rendered by the existing UI style. */
USTRUCT(BlueprintType)
struct RUINMATES_API FRuinmatesHubActionQuote
{
	GENERATED_BODY()
	/** Whether all permanent, electrical and material conditions currently pass. */
	UPROPERTY(BlueprintReadOnly, Category = "Hub")
	bool bCanExecute = false;
	/** Specific server-side rejection or available-action explanation. */
	UPROPERTY(BlueprintReadOnly, Category = "Hub")
	FText Reason;
	/** Required and held quantities use existing DT_Items identifiers. */
	UPROPERTY(BlueprintReadOnly, Category = "Hub")
	TMap<FName, int32> Required;
	/** Combined eligible resources; never supplied by a client. */
	UPROPERTY(BlueprintReadOnly, Category = "Hub")
	TMap<FName, int32> Available;
};

/** Optional boolean filter for independently authored permanent visual groups. */
UENUM(BlueprintType)
enum class ERuinmatesHubStateRequirement : uint8
{
	Any,
	MustBeFalse,
	MustBeTrue
};

/** Only runtime groups use electrical conditions; permanent groups survive outages. */
UENUM(BlueprintType)
enum class ERuinmatesHubVisualDomain : uint8
{
	Permanent,
	Runtime
};
