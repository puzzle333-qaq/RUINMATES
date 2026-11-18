#include "Inventory/RuinmatesBackpackComponent.h"

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
	}
}

void URuinmatesBackpackComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URuinmatesBackpackComponent, Tier);
	DOREPLIFETIME(URuinmatesBackpackComponent, Slots);
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
	int32 Remaining = Incoming.Count;

	for (FRuinmatesItemStack& Slot : Slots)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (!Slot.CanStackWith(Incoming))
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
		const int32 Moved = FMath::Min(Incoming.MaxStack, Remaining);
		Slot = Incoming;
		Slot.Count = Moved;
		Slot.bRevealed = true;
		Remaining -= Moved;
	}

	const int32 Accepted = Incoming.Count - Remaining;
	if (Accepted > 0)
	{
		OnChanged.Broadcast();
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
	OnChanged.Broadcast();
	return true;
}

int32 URuinmatesBackpackComponent::GetAvailableCapacityForItem(const FRuinmatesItemStack& Incoming) const
{
	if (Incoming.ItemId.IsNone())
	{
		return 0;
	}

	const int32 StackMax = FMath::Max(1, Incoming.MaxStack);
	int32 EmptySlots = 0;
	int32 Room = 0;
	for (const FRuinmatesItemStack& Slot : Slots)
	{
		if (!Slot.IsEmpty() && Slot.ItemId == Incoming.ItemId && Slot.Quality == Incoming.Quality)
		{
			Room += FMath::Max(0, Slot.MaxStack - Slot.Count);
		}
		else if (Slot.IsEmpty())
		{
			++EmptySlots;
		}
	}
	// Slots are resized in BeginPlay on the server; count any not-yet-created slots as free.
	EmptySlots += FMath::Max(0, GetCapacity() - Slots.Num());
	Room += EmptySlots * StackMax;
	return Room;
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
	OnChanged.Broadcast();
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
