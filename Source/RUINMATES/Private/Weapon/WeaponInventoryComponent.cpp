#include "Weapon/WeaponInventoryComponent.h"

#include "Character/RuinmatesCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/BaseWeapon.h"

UWeaponInventoryComponent::UWeaponInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UWeaponInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWeaponInventoryComponent, Inventory);
}

ARuinmatesCharacter* UWeaponInventoryComponent::GetOwningCharacter() const
{
	return Cast<ARuinmatesCharacter>(GetOwner());
}

bool UWeaponInventoryComponent::IsOwnerAuthoritative() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->GetLocalRole() >= ROLE_Authority;
}

void UWeaponInventoryComponent::SpawnInventory()
{
	if (!IsOwnerAuthoritative())
	{
		return;
	}

	// Already spawned. RebuildInventory is the explicit respawn path (PossessedBy can run more than once).
	if (Inventory.Num() > 0)
	{
		return;
	}

	// Batch 3 campaign save only stores FRuinmatesMemberSave::EquippedLoadoutId, a string stub.
	// There is no submitted weapon-class list equivalent to the old GetSubmittedLoadout / FLoadoutPreset,
	// so this uses DefaultWeaponClass only. Warehouse UI and attachment presets are not migrated.
	for (const TSubclassOf<ABaseWeapon>& WeaponClass : DefaultWeaponClass)
	{
		ABaseWeapon* Weapon = SpawnWeapon(WeaponClass);
		if (IsValid(Weapon))
		{
			Inventory.AddUnique(Weapon);
		}
	}

	if (Inventory.Num() > 0)
	{
		if (ARuinmatesCharacter* Character = GetOwningCharacter())
		{
			Character->EquipWeapon(Inventory[0]);
		}
	}
}

void UWeaponInventoryComponent::DestroyInventory()
{
	if (!IsOwnerAuthoritative())
	{
		return;
	}

	if (ARuinmatesCharacter* Character = GetOwningCharacter())
	{
		ABaseWeapon* Equipped = Character->GetEquippedWeapon();
		if (IsValid(Equipped) && Inventory.Contains(Equipped))
		{
			Character->UnequipWeapon();
		}
	}

	for (ABaseWeapon* Weapon : Inventory)
	{
		if (IsValid(Weapon))
		{
			Weapon->Destroy();
		}
	}
	Inventory.Reset();
}

void UWeaponInventoryComponent::RebuildInventory()
{
	if (!IsOwnerAuthoritative())
	{
		return;
	}

	DestroyInventory();
	SpawnInventory();
}

ABaseWeapon* UWeaponInventoryComponent::SpawnWeapon(TSubclassOf<ABaseWeapon> WeaponClass) const
{
	AActor* OwningActor = GetOwner();
	if (!IsValid(OwningActor) || !IsOwnerAuthoritative() || !GetWorld())
	{
		return nullptr;
	}
	if (!WeaponClass || WeaponClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Cast<APawn>(OwningActor);
	SpawnInfo.Owner = OwningActor;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABaseWeapon* Weapon = GetWorld()->SpawnActor<ABaseWeapon>(WeaponClass, SpawnInfo);
	if (IsValid(Weapon))
	{
		Weapon->NotifyUnequipped();
	}
	return Weapon;
}

void UWeaponInventoryComponent::EquipWeaponAtIndex(int32 Index)
{
	if (!Inventory.IsValidIndex(Index) || !IsValid(Inventory[Index]))
	{
		return;
	}
	if (ARuinmatesCharacter* Character = GetOwningCharacter())
	{
		Character->EquipWeapon(Inventory[Index]);
	}
}

void UWeaponInventoryComponent::CycleNextWeapon()
{
	if (Inventory.Num() <= 1)
	{
		return;
	}

	ARuinmatesCharacter* Character = GetOwningCharacter();
	if (!IsValid(Character))
	{
		return;
	}

	const ABaseWeapon* Current = Character->GetEquippedWeapon();
	int32 CurrentIndex = Inventory.IndexOfByKey(Current);
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}

	for (int32 Step = 1; Step <= Inventory.Num(); ++Step)
	{
		const int32 NextIndex = (CurrentIndex + Step) % Inventory.Num();
		if (IsValid(Inventory[NextIndex]) && Inventory[NextIndex] != Current)
		{
			Character->EquipWeapon(Inventory[NextIndex]);
			return;
		}
	}
}
