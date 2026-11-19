#include "Inventory/RuinmatesBackpackComponent.h"

#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Net/UnrealNetwork.h"

URuinmatesBackpackComponent::URuinmatesBackpackComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void URuinmatesBackpackComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		EnsureSlotCount();
		NormalizeStoredSlots();
	}
}

void URuinmatesBackpackComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URuinmatesBackpackComponent, Tier);
	DOREPLIFETIME(URuinmatesBackpackComponent, Slots);
	DOREPLIFETIME(URuinmatesBackpackComponent, Revision);
}

int32 URuinmatesBackpackComponent::GetUsedSlotCount() const
{
	int32 Used = 0;
	for (const FRuinmatesItemStack& Slot : Slots)
	{
		if (!Slot.IsEmpty())
		{
			++Used;
		}
	}
	return Used;
}

int32 URuinmatesBackpackComponent::GetLootValue() const
{
	int32 Total = 0;
	for (const FRuinmatesItemStack& Slot : Slots)
	{
		Total += Slot.GetTotalValue();
	}
	return Total;
}

int32 URuinmatesBackpackComponent::TryAdd(const FRuinmatesItemStack& Incoming)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Incoming.IsEmpty())
	{
		return 0;
	}

	EnsureSlotCount();
	FRuinmatesItemStack CanonicalIncoming = Incoming;
	if (const URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this))
	{
		if (Database->IsLoaded())
		{
			if (!Database->NormalizeStack(CanonicalIncoming, true))
			{
				return 0;
			}
		}
	}
	int32 Remaining = CanonicalIncoming.Count;

	for (FRuinmatesItemStack& Slot : Slots)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (!Slot.CanStackWith(CanonicalIncoming))
		{
			continue;
		}
		const int32 Room = Slot.MaxStack - Slot.Count;
		const int32 Moved = FMath::Min(Room, Remaining);
		Slot.Count += Moved;
		Remaining -= Moved;
	}

	for (FRuinmatesItemStack& Slot : Slots)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (!Slot.IsEmpty())
		{
			continue;
		}
		const int32 Moved = FMath::Min(CanonicalIncoming.MaxStack, Remaining);
		Slot = CanonicalIncoming;
		Slot.Count = Moved;
		Slot.bRevealed = true;
		Remaining -= Moved;
	}

	const int32 Accepted = Incoming.Count - Remaining;
	if (Accepted > 0)
	{
		MarkChanged();
	}
	return Accepted;
}

bool URuinmatesBackpackComponent::RemoveItemAt(int32 SlotIndex, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Count <= 0 || !Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FRuinmatesItemStack& Slot = Slots[SlotIndex];
	if (Slot.IsEmpty())
	{
		return false;
	}

	Slot.Count -= FMath::Min(Count, Slot.Count);
	if (Slot.Count <= 0)
	{
		Slot = FRuinmatesItemStack();
	}
	MarkChanged();
	return true;
}

int32 URuinmatesBackpackComponent::ComputeAvailableCapacity(const TArray<FRuinmatesItemStack>& InSlots, int32 Capacity,
	FName MatchId, ERuinmatesItemQuality Quality, int32 StackMax)
{
	if (MatchId.IsNone())
	{
		return 0;
	}

	const int32 SafeStackMax = FMath::Max(1, StackMax);
	int32 EmptySlots = 0;
	int32 Room = 0;
	for (const FRuinmatesItemStack& Slot : InSlots)
	{
		if (!Slot.IsEmpty() && Slot.ItemId == MatchId && Slot.Quality == Quality)
		{
			Room += FMath::Max(0, SafeStackMax - Slot.Count);
		}
		else if (Slot.IsEmpty())
		{
			++EmptySlots;
		}
	}
	// Slots are resized in BeginPlay on the server; count any not-yet-created slots as free.
	EmptySlots += FMath::Max(0, Capacity - InSlots.Num());
	Room += EmptySlots * SafeStackMax;
	return Room;
}

int32 URuinmatesBackpackComponent::GetAvailableCapacityForItem(const FRuinmatesItemStack& Incoming) const
{
	if (Incoming.ItemId.IsNone())
	{
		return 0;
	}

	int32 StackMax = FMath::Max(1, Incoming.MaxStack);
	ERuinmatesItemQuality Quality = Incoming.Quality;
	// Stacks are stored with the resolved database id, so a legacy id/name must be resolved before
	// the merge room is measured or the room in matching stacks is lost.
	FName MatchId = Incoming.ItemId;
	if (const URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this))
	{
		if (Database->IsLoaded())
		{
			const FName ResolvedId = Database->ResolveItemId(Incoming.ItemId, Incoming.DisplayName);
			const FRuinmatesItemRow* Row = Database->FindItemDefinition(ResolvedId);
			if (!Row)
			{
				return 0;
			}
			MatchId = ResolvedId;
			StackMax = FMath::Max(1, Row->MaxStack);
			Quality = Row->Quality;
		}
	}
	return ComputeAvailableCapacity(Slots, GetCapacity(), MatchId, Quality, StackMax);
}

bool URuinmatesBackpackComponent::CanAddExactCount(const FRuinmatesItemStack& Incoming, int32 Count) const
{
	return Count > 0 && !Incoming.ItemId.IsNone() && GetAvailableCapacityForItem(Incoming) >= Count;
}

bool URuinmatesBackpackComponent::TryAddExact(const FRuinmatesItemStack& Incoming, int32 Count)
{
	if (!CanAddExactCount(Incoming, Count))
	{
		return false;
	}

	FRuinmatesItemStack Request = Incoming;
	Request.Count = Count;
	const int32 Accepted = TryAdd(Request);
	ensureMsgf(Accepted == Count, TEXT("TryAddExact accepted %d of %d after a capacity check"), Accepted, Count);
	return Accepted == Count;
}

bool URuinmatesBackpackComponent::DiscardSlot(int32 SlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsEmpty())
	{
		return false;
	}
	Slots[SlotIndex] = FRuinmatesItemStack();
	MarkChanged();
	return true;
}

bool URuinmatesBackpackComponent::CanMoveSlotSnapshot(int32 SourceIndex, int32 TargetIndex, FName ExpectedSourceItemId,
	ERuinmatesItemQuality ExpectedSourceQuality, int32 ExpectedSourceCount,
	FName ExpectedTargetItemId, ERuinmatesItemQuality ExpectedTargetQuality,
	int32 ExpectedTargetCount, uint32 ExpectedRevision) const
{
	if (ExpectedRevision != Revision || SourceIndex == TargetIndex || !Slots.IsValidIndex(SourceIndex) || !Slots.IsValidIndex(TargetIndex))
	{
		return false;
	}

	const FRuinmatesItemStack& Source = Slots[SourceIndex];
	const FRuinmatesItemStack& Target = Slots[TargetIndex];
	const bool bSourceMatches = !Source.IsEmpty() && !ExpectedSourceItemId.IsNone()
		&& Source.ItemId == ExpectedSourceItemId && Source.Quality == ExpectedSourceQuality
		&& Source.Count == ExpectedSourceCount;
	const bool bTargetMatches = ExpectedTargetItemId.IsNone()
		? Target.IsEmpty() && ExpectedTargetCount == 0
		: !Target.IsEmpty() && Target.ItemId == ExpectedTargetItemId
			&& Target.Quality == ExpectedTargetQuality && Target.Count == ExpectedTargetCount;
	if (!bSourceMatches || !bTargetMatches)
	{
		return false;
	}

	return !(Source.ItemId == Target.ItemId && Source.Quality == Target.Quality
		&& Target.Count >= Target.MaxStack);
}

bool URuinmatesBackpackComponent::TryMoveSlot(int32 SourceIndex, int32 TargetIndex, FName ExpectedSourceItemId,
	ERuinmatesItemQuality ExpectedSourceQuality, int32 ExpectedSourceCount,
	FName ExpectedTargetItemId, ERuinmatesItemQuality ExpectedTargetQuality,
	int32 ExpectedTargetCount, uint32 ExpectedRevision)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()
		|| !CanMoveSlotSnapshot(SourceIndex, TargetIndex, ExpectedSourceItemId, ExpectedSourceQuality, ExpectedSourceCount,
			ExpectedTargetItemId, ExpectedTargetQuality, ExpectedTargetCount, ExpectedRevision))
	{
		return false;
	}

	FRuinmatesItemStack& Source = Slots[SourceIndex];
	FRuinmatesItemStack& Target = Slots[TargetIndex];
	if (Target.IsEmpty())
	{
		Target = Source;
		Source = FRuinmatesItemStack();
	}
	else if (Source.ItemId == Target.ItemId && Source.Quality == Target.Quality)
	{
		const int32 Moved = FMath::Min(Source.Count, FMath::Max(0, Target.MaxStack - Target.Count));
		if (Moved <= 0)
		{
			return false;
		}
		Target.Count += Moved;
		Source.Count -= Moved;
		if (Source.Count <= 0)
		{
			Source = FRuinmatesItemStack();
		}
	}
	else
	{
		Swap(Source, Target);
	}

	MarkChanged();
	return true;
}

void URuinmatesBackpackComponent::OnRep_Slots()
{
	OnChanged.Broadcast();
}

void URuinmatesBackpackComponent::OnRep_Tier()
{
	OnChanged.Broadcast();
}

void URuinmatesBackpackComponent::EnsureSlotCount()
{
	const int32 Capacity = GetCapacity();
	if (Slots.Num() == Capacity)
	{
		return;
	}
	Slots.SetNum(Capacity);
}

void URuinmatesBackpackComponent::NormalizeStoredSlots()
{
	const URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this);
	if (!Database || !Database->IsLoaded())
	{
		return;
	}

	bool bChanged = false;
	for (FRuinmatesItemStack& Stack : Slots)
	{
		if (Stack.IsEmpty())
		{
			continue;
		}
	const FName OriginalId = Stack.ItemId;
		const FText OriginalName = Stack.DisplayName;
		const ERuinmatesItemQuality OriginalQuality = Stack.Quality;
		const int32 OriginalValue = Stack.UnitValue;
		const int32 OriginalMaxStack = Stack.MaxStack;
		const FText OriginalDescription = Stack.Description;
		const bool bOriginalUpgradeMaterial = Stack.bUpgradeMaterial;
		if (!Database->NormalizeStack(Stack, true))
		{
			UE_LOG(LogTemp, Warning, TEXT("RuinMates backpack contains an item missing from DT_Items: %s"), *OriginalId.ToString());
			continue;
		}
		bChanged |= OriginalId != Stack.ItemId || !OriginalName.EqualTo(Stack.DisplayName)
			|| OriginalQuality != Stack.Quality || OriginalValue != Stack.UnitValue
			|| OriginalMaxStack != Stack.MaxStack || !OriginalDescription.EqualTo(Stack.Description)
			|| bOriginalUpgradeMaterial != Stack.bUpgradeMaterial;
	}
	if (bChanged)
	{
		MarkChanged();
	}
}

void URuinmatesBackpackComponent::MarkChanged()
{
	++Revision;
	OnChanged.Broadcast();
}

bool URuinmatesBackpackComponent::TrySort(uint32 ExpectedRevision)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ExpectedRevision != Revision)
	{
		return false;
	}

	// Merge identical stacks (same id, quality and max stack) before ordering.
	TArray<FRuinmatesItemStack> Merged;
	int32 TotalBefore = 0;
	for (const FRuinmatesItemStack& Slot : Slots)
	{
		if (Slot.IsEmpty())
		{
			continue;
		}
		TotalBefore += Slot.Count;
		FRuinmatesItemStack* Existing = Merged.FindByPredicate([&Slot](const FRuinmatesItemStack& Other)
		{
			return Other.ItemId == Slot.ItemId && Other.Quality == Slot.Quality && Other.MaxStack == Slot.MaxStack;
		});
		if (Existing)
		{
			Existing->Count += Slot.Count;
		}
		else
		{
			Merged.Add(Slot);
		}
	}

	// Split merged totals back into legal stacks; a single pass only needs one write below.
	TArray<FRuinmatesItemStack> Result;
	for (const FRuinmatesItemStack& Item : Merged)
	{
		const int32 StackMax = FMath::Max(1, Item.MaxStack);
		for (int32 Remaining = Item.Count; Remaining > 0;)
		{
			FRuinmatesItemStack Stack = Item;
			Stack.Count = FMath::Min(Remaining, StackMax);
			Remaining -= Stack.Count;
			Result.Add(Stack);
		}
	}

	// Category, then quality (high first), then ItemId so equal items keep a stable order.
	const URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this);
	auto CategoryOf = [Database](const FRuinmatesItemStack& Stack)
	{
		if (Database && Database->IsLoaded())
		{
			if (const FRuinmatesItemRow* Row = Database->FindItemDefinition(Stack.ItemId))
			{
				return static_cast<int32>(Row->Category);
			}
		}
		return Stack.bUpgradeMaterial ? static_cast<int32>(ERuinmatesItemCategory::Material)
			: static_cast<int32>(ERuinmatesItemCategory::Consumable);
	};
	Result.Sort([&CategoryOf](const FRuinmatesItemStack& A, const FRuinmatesItemStack& B)
	{
		const int32 CategoryA = CategoryOf(A);
		const int32 CategoryB = CategoryOf(B);
		if (CategoryA != CategoryB)
		{
			return CategoryA < CategoryB;
		}
		if (A.Quality != B.Quality)
		{
			return static_cast<int32>(A.Quality) > static_cast<int32>(B.Quality);
		}
		return A.ItemId.LexicalLess(B.ItemId);
	});

	// Conservation and capacity guards: a sort never creates or destroys items or slots.
	int32 TotalAfter = 0;
	for (const FRuinmatesItemStack& Stack : Result)
	{
		TotalAfter += Stack.Count;
	}
	if (TotalAfter != TotalBefore || Result.Num() > Slots.Num())
	{
		ensureMsgf(false, TEXT("Sort changed the item total (%d -> %d) or overflowed the slot array"), TotalBefore, TotalAfter);
		return false;
	}

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		Slots[Index] = Result.IsValidIndex(Index) ? Result[Index] : FRuinmatesItemStack();
	}
	MarkChanged();
	return true;
}

bool URuinmatesBackpackComponent::TrySplitStack(int32 SourceIndex, int32 Count, uint32 ExpectedRevision)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ExpectedRevision != Revision || !Slots.IsValidIndex(SourceIndex))
	{
		return false;
	}

	const FRuinmatesItemStack& Source = Slots[SourceIndex];
	if (Source.IsEmpty() || Count <= 0 || Count >= Source.Count)
	{
		return false;
	}

	int32 EmptyIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].IsEmpty())
		{
			EmptyIndex = Index;
			break;
		}
	}
	if (EmptyIndex == INDEX_NONE)
	{
		return false;
	}

	// Copy every field (id, quality, description, value, flags) so the split keeps all properties.
	FRuinmatesItemStack Split = Source;
	Split.Count = Count;
	Slots[SourceIndex].Count -= Count;
	Slots[EmptyIndex] = Split;
	MarkChanged();
	return true;
}
