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
