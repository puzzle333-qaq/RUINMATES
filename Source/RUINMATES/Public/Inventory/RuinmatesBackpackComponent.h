#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/RuinmatesItemTypes.h"
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
	int32 GetCapacity() const { return RuinmatesItems::GetBackpackCapacity(Tier); }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Backpack")
	int32 GetUsedSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Backpack")
	int32 GetLootValue() const;

	/** Monotonic server revision used to reject inventory commands built from stale UI state. */
	uint32 GetRevision() const { return Revision; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Backpack")
	const TArray<FRuinmatesItemStack>& GetSlots() const { return Slots; }

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
	 * Rejected when the revision, the source stack or the free-slot requirement fails.
	 */
	bool TrySplitStack(int32 SourceIndex, int32 Count, uint32 ExpectedRevision);

	UFUNCTION()
	void OnRep_Slots();

protected:
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Tier, Category = "RuinMates|Backpack")
	ERuinmatesBackpackTier Tier = ERuinmatesBackpackTier::Issue;

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FRuinmatesItemStack> Slots;

	UPROPERTY(Replicated)
	uint32 Revision = 0;

	UFUNCTION()
	void OnRep_Tier();

	void EnsureSlotCount();
	void NormalizeStoredSlots();
	void MarkChanged();
};
