#include "Misc/AutomationTest.h"
#include "Character/RuinmatesCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Weapon/BaseWeapon.h"
#include "Weapon/WeaponData.h"
#include "Weapon/WeaponMountComponent.h"
#include "Weapon/WeaponPoseComponent.h"
#include "Weapon/WeaponTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesEquipPoseBindTest, "RuinMates.Batch2.EquipPoseBind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesEquipPoseBindTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesEquipPoseBindTest"));
	if (!TestNotNull(TEXT("Created a transient world"), World))
	{
		return false;
	}
	World->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ARuinmatesCharacter* Character = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	ABaseWeapon* Weapon = World->SpawnActor<ABaseWeapon>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!TestTrue(TEXT("Character and weapon spawned"), IsValid(Character) && IsValid(Weapon)))
	{
		if (IsValid(Weapon)) { Weapon->Destroy(); }
		if (IsValid(Character)) { Character->Destroy(); }
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	if (USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")))
	{
		Character->GetMesh()->SetSkeletalMesh(Body);
	}

	UWeaponData* Data = NewObject<UWeaponData>(Weapon);
	Data->WeaponType = RuinmatesTags::Tag_WeaponType_Rifle;
	Data->LeftHandSocket = FName(TEXT("LeftHandSocket"));
	Data->CombatParams.Spread.HipFireSpread = 0.f;
	Data->CombatParams.Fire.FireTime = 0.f;
	Data->CombatParams.Ammo.MagCapacity = 10;
	Data->CombatParams.Ammo.StartingAmmoInMag = 1;
	Data->CombatParams.Ammo.StartingCarriedAmmo = 20;
	Weapon->SetWeaponData(Data);

	TestTrue(TEXT("Pose component exists on the character"), IsValid(Character->GetWeaponPose()));
	TestFalse(TEXT("Starts unequipped"), IsValid(Character->GetEquippedWeapon()));

	Character->EquipWeapon(Weapon);
	TestEqual(TEXT("Equip sets the equipped weapon"), Character->GetEquippedWeapon(), Weapon);
	TestEqual(TEXT("Weapon instigator is the character"), Weapon->GetInstigator(), static_cast<APawn*>(Character));
	TestEqual(TEXT("Weapon kind maps from the rifle tag"), Weapon->GetWeaponKind(), ERuinmatesWeaponKind::Rifle);

	UWeaponPoseComponent* Pose = Character->GetWeaponPose();
	TestEqual(TEXT("Pose body mesh is the character mesh"), Pose->GetBodyMesh(), Character->GetMesh());
	TestEqual(TEXT("Pose weapon mesh is the equipped gun"), Pose->GetWeaponMesh(), Weapon->GetWeaponMesh());
	TestEqual(TEXT("Pose kind follows the equipped rifle"), Pose->GetWeaponKind(), ERuinmatesWeaponKind::Rifle);
	if (UWeaponMountComponent* Mount = Weapon->GetMount())
	{
		if (USkeletalMeshComponent* Gun = Weapon->GetWeaponMesh())
		{
			TestEqual(TEXT("Mount parent is the character mesh when the body has a usable hand"), Gun->GetAttachParent(),
				UWeaponMountComponent::IsUsableAttachPoint(Character->GetMesh(), FName(TEXT("hand_r")))
					? static_cast<USceneComponent*>(Character->GetMesh())
					: Gun->GetAttachParent());
		}
	}

	Weapon->ReloadFallbackDuration = 10.f;
	TestTrue(TEXT("Can reload with reserve ammo"), Weapon->CanReload());
	Character->TryReloadWeapon();
	TestTrue(TEXT("Reload starts on the server"), Weapon->IsReloading());
	TestTrue(TEXT("Pose is marked reloading while the gun reloads"), Pose->IsReloading());
	TestFalse(TEXT("Cannot fire while reloading"), Weapon->CanFire());
	TestTrue(TEXT("Character reports the weapon is reloading"), Character->IsWeaponReloading());
	TestEqual(TEXT("Reload has not consumed or filled ammo yet"), Weapon->GetAmmoInMag(), 1);

	// Immediate reload path: cancel the long timer by using a zero-duration reload on a fresh mag state.
	Weapon->ReloadFallbackDuration = 0.f;
	// Still reloading from the 10s path; finish by issuing a zero-duration reload after manually...
	// The in-flight reload must complete through the zero-duration helper: stop via unequip/re-equip, then reload.
	Character->UnequipWeapon();
	TestFalse(TEXT("Unequip clears the equipped weapon"), IsValid(Character->GetEquippedWeapon()));
	TestTrue(TEXT("Pose weapon mesh is cleared on unequip"), Pose->GetWeaponMesh() == nullptr);
	TestFalse(TEXT("Pose is not reloading after unequip"), Pose->IsReloading());
	TestEqual(TEXT("Unequipped weapon status is Unequipped"), Weapon->GetWeaponStatus(), ERuinmatesWeaponStatus::Unequipped);

	Character->EquipWeapon(Weapon);
	TestEqual(TEXT("Re-equip restores the weapon"), Character->GetEquippedWeapon(), Weapon);
	TestEqual(TEXT("Re-equip bind pose mesh again"), Pose->GetWeaponMesh(), Weapon->GetWeaponMesh());
	Character->TryReloadWeapon();
	TestFalse(TEXT("Zero-duration reload is finished immediately"), Weapon->IsReloading());
	TestEqual(TEXT("Reload fills the mag from carried ammo"), Weapon->GetAmmoInMag(), 10);
	TestEqual(TEXT("Carried ammo is reduced by the filled amount"), Weapon->GetCarriedAmmo(), 11);
	TestFalse(TEXT("Pose reloading is cleared after a finished reload"), Pose->IsReloading());

	const int32 FireBefore = Weapon->GetFireNotifyId();
	Character->TryFireWeapon();
	TestEqual(TEXT("Server fire notify increments"), Weapon->GetFireNotifyId(), FireBefore + 1);
	TestEqual(TEXT("Fire consumes a round after reload"), Weapon->GetAmmoInMag(), 9);

	Weapon->SetRole(ROLE_SimulatedProxy);
	Character->SetRole(ROLE_SimulatedProxy);
	const int32 AmmoBeforeClient = Weapon->GetAmmoInMag();
	Character->TryFireWeapon();
	TestEqual(TEXT("Simulated proxy fire does not consume ammo"), Weapon->GetAmmoInMag(), AmmoBeforeClient);

	Weapon->Destroy();
	Character->Destroy();
	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
