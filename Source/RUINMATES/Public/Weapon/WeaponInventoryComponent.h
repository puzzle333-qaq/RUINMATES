#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponInventoryComponent.generated.h"

class ABaseWeapon;
class ARuinmatesCharacter;

// Authority-side default inventory spawn, extracted from the 9aa225f CombatComponent
// SpawnInventory / SpawnWeapon path. Not a port of CombatComponent. Attachments,
// first-person, pouches, throwables, and warehouse UI are not included.
UCLASS(ClassGroup = (RuinMates), meta = (BlueprintSpawnableComponent))
class RUINMATES_API UWeaponInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	ARuinmatesCharacter* GetOwningCharacter() const;

	const TArray<TObjectPtr<ABaseWeapon>>& GetInventory() const { return Inventory; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	int32 GetInventoryCount() const { return Inventory.Num(); }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	ABaseWeapon* GetInventoryWeapon(int32 Index) const
	{
		return Inventory.IsValidIndex(Index) ? Inventory[Index].Get() : nullptr;
	}

	// Authority only. Spawns DefaultWeaponClass and equips the first weapon via ARuinmatesCharacter::EquipWeapon.
	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void SpawnInventory();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void DestroyInventory();

	// Authority only. Destroys current weapons and spawns again from DefaultWeaponClass.
	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void RebuildInventory();

	ABaseWeapon* SpawnWeapon(TSubclassOf<ABaseWeapon> WeaponClass) const;

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void EquipWeaponAtIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void CycleNextWeapon();

	// Assigned on the character Blueprint Class Defaults, on this component.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RuinMates|Weapon")
	TArray<TSubclassOf<ABaseWeapon>> DefaultWeaponClass;

protected:
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "RuinMates|Weapon")
	TArray<TObjectPtr<ABaseWeapon>> Inventory;

	bool IsOwnerAuthoritative() const;
};
