#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/RuinmatesItemTypes.h"
class UDataTable;
#include "RuinmatesBackpackComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FRuinmatesBackpackChanged);

// Personal loot grid. Team money is separate and is not stored here.
UCLASS(ClassGroup = (RuinMates), meta = (BlueprintSpawnableComponent))
class RUINMATES_API URuinmatesBackpackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URuinmatesBackpackComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FRuinmatesBackpackChanged OnChanged;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Backpack")
	ERuinmatesBackpackTier GetTier() const { return Tier; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Backpack")
	int32 GetCapacity() const { return CapacityOverride > 0 ? CapacityOverride : RuinmatesItems::GetBackpackCapacity(Tier); }

	/** Authority only. Configures 1..512 slots; refuses a shrink that would discard any held stack. */
	bool SetCapacityOverride(int32 NewCapacity);

	/** Authority-only complete saved inventory restore; validates before publishing tier/grid together. */
	bool RestoreSavedInventory(ERuinmatesBackpackTier SavedTier, int32 SavedOverride, const TArray<FRuinmatesItemStack>& SavedSlots);

	/** Zero means the existing tier determines capacity; positive values configure a shared grid. */
	int32 GetCapacityOverride() const { return CapacityOverride; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Backpack")
	int32 GetUsedSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Backpack")
	int32 GetLootValue() const;

	/** Monotonic server revision used to reject inventory commands built from stale UI state. */
	uint32 GetRevision() const { return Revision; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Backpack")
	const TArray<FRuinmatesItemStack>& GetSlots() const { return Slots; }

	/** Counts all positive units with this stable ID, saturating at INT32_MAX rather than overflowing. */
	int32 CountItem(FName ItemId) const;

	/**
	 * Authority only. Validates revision, capacity and every item against the loaded item database.
	 * Returns canonical static fields and capacity-sized slots without changing live storage.
	 * Unknown IDs, malformed empty slots and oversized stacks fail; no legacy-name fallback is used.
	 * Output remains unchanged on failure, and may safely alias InSlots.
	 */
	bool PrepareSlotsForTransaction(const TArray<FRuinmatesItemStack>& InSlots, uint32 ExpectedRevision,
		TArray<FRuinmatesItemStack>& OutCanonical) const;

	/**
	 * Authority-only native transaction primitive; never exposed as a client inventory replacement RPC.
	 * Validates before replacing. With bNotify=false the revision and events stay unchanged, so a
	 * synchronous manager can roll back at the same revision. Never yield between staging/publishing.
	 */
	bool ReplaceSlotsForTransaction(const TArray<FRuinmatesItemStack>& InSlots, uint32 ExpectedRevision, bool bNotify = true);

	/** Publishes a staged replacement exactly once: advances revision and emits one OnChanged. */
	void PublishTransactionChange();

	/**
	 * Pure all-or-nothing recipe consumption across matching stacks, independent of quality.
	 * Costs must already be aggregated by ItemId (TMap keys are unique); empty costs are a free action.
	 * Invalid IDs/counts, malformed stacks or insufficient totals leave InOutSlots untouched.
	 */
	static bool ConsumeItemsFromSlots(TArray<FRuinmatesItemStack>& InOutSlots, const TMap<FName, int32>& Costs);

	/** Server only. Adds as many as will fit. Returns how many were accepted. */
	int32 TryAdd(const FRuinmatesItemStack& Incoming);

	/** Server only. Free units for this stack: partial room in matching stacks plus empty slots. */
	int32 GetAvailableCapacityForItem(const FRuinmatesItemStack& Incoming) const;

	/** Pure capacity math, split out so the resolved-id matching can be unit tested. */
	static int32 ComputeAvailableCapacity(const TArray<FRuinmatesItemStack>& InSlots, int32 Capacity,
		FName MatchId, ERuinmatesItemQuality Quality, int32 StackMax);

	/** Server only. True when the whole count would fit, used for all-or-nothing purchases. */
	bool CanAddExactCount(const FRuinmatesItemStack& Incoming, int32 Count) const;

	/** Server only. Adds exactly Count or nothing, so money and items can be committed together. */
	bool TryAddExact(const FRuinmatesItemStack& Incoming, int32 Count);

	/** Server only. Removes Count from one slot; clears the slot when it reaches zero. */
	bool RemoveItemAt(int32 SlotIndex, int32 Count);

	/** Server only. Clears one stack so the player can make room. */
	bool DiscardSlot(int32 SlotIndex);

	/** Server only. Validates both slot snapshots before moving, merging, or swapping stacks. */
	bool TryMoveSlot(int32 SourceIndex, int32 TargetIndex, FName ExpectedSourceItemId,
		ERuinmatesItemQuality ExpectedSourceQuality, int32 ExpectedSourceCount,
		FName ExpectedTargetItemId, ERuinmatesItemQuality ExpectedTargetQuality,
		int32 ExpectedTargetCount, uint32 ExpectedRevision);
	bool CanMoveSlotSnapshot(int32 SourceIndex, int32 TargetIndex, FName ExpectedSourceItemId,
		ERuinmatesItemQuality ExpectedSourceQuality, int32 ExpectedSourceCount,
		FName ExpectedTargetItemId, ERuinmatesItemQuality ExpectedTargetQuality,
		int32 ExpectedTargetCount, uint32 ExpectedRevision) const;

	/**
	 * Server only. Merges identical stacks, orders them and pushes the gaps to the end.
	 * Rejected when the caller's revision is stale. Emits one change for the whole pass.
	 */
	bool TrySort(uint32 ExpectedRevision);

	/**
	 * Server only. Splits Count units off SourceIndex into the first empty slot.
	 * The caller must describe the exact stack it rendered; a mismatch or a missing free slot
	 * changes nothing.
	 */
	bool TrySplitStack(int32 SourceIndex, int32 Count, FName ExpectedItemId,
		ERuinmatesItemQuality ExpectedQuality, int32 ExpectedSourceCount, uint32 ExpectedRevision);

	/** Pure merge/order pass shared by TrySort and the automation tests. */
	static void SortStacks(const UDataTable* Table, TArray<FRuinmatesItemStack>& InOut);

#if WITH_DEV_AUTOMATION_TESTS
	/** Test-only: installs exact slot contents so merges can be tested with separate stacks. */
	void SetSlotsForTest(const TArray<FRuinmatesItemStack>& InSlots) { Slots = InSlots; }
#endif

	UFUNCTION()
	void OnRep_Slots();

protected:
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Tier, Category = "RuinMates|Backpack")
	ERuinmatesBackpackTier Tier = ERuinmatesBackpackTier::Issue;

	/** Shared storage reuses this same grid; zero preserves every existing personal tier capacity. */
	UPROPERTY(ReplicatedUsing = OnRep_Tier)
	int32 CapacityOverride = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FRuinmatesItemStack> Slots;

	UPROPERTY(Replicated)
	uint32 Revision = 0;

	/** A synchronous manager stages one or more replacements before one final publication. */
	bool bTransactionChangePending = false;

	UFUNCTION()
	void OnRep_Tier();

	void EnsureSlotCount();
	void NormalizeStoredSlots();
	void MarkChanged();
};
