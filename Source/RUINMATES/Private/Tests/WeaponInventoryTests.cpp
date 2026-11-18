#include "Misc/AutomationTest.h"
#include "Character/RuinmatesCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Weapon/BaseWeapon.h"
#include "Weapon/WeaponData.h"
#include "Weapon/WeaponInventoryComponent.h"
#include "Weapon/WeaponMountComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RuinmatesInventoryTest
{
	void DestroyWorld(UWorld* World, AActor* Character)
	{
		if (IsValid(Character))
		{
			Character->Destroy();
		}
		if (World)
		{
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}
	}

	UWorld* CreateTestWorld(const TCHAR* Name)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, Name);
		if (World)
		{
			World->AddToRoot();
		}
		return World;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesInventorySpawnAndEquipTest, "RuinMates.Inventory.SpawnAndEquip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesInventorySpawnAndEquipTest::RunTest(const FString& Parameters)
{
	using namespace RuinmatesInventoryTest;
	UWorld* World = CreateTestWorld(TEXT("RuinMatesInventorySpawnAndEquipTest"));
	if (!TestNotNull(TEXT("Created a transient world"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ARuinmatesCharacter* Character = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	if (!TestTrue(TEXT("Character spawned"), IsValid(Character)))
	{
		DestroyWorld(World, Character);
		return false;
	}

	UWeaponInventoryComponent* Inventory = Character->GetWeaponInventory();
	if (!TestTrue(TEXT("Weapon inventory component exists"), IsValid(Inventory)))
	{
		DestroyWorld(World, Character);
		return false;
	}

	if (USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")))
	{
		Character->GetMesh()->SetSkeletalMesh(Body);
	}

	TestEqual(TEXT("Starts with an empty inventory"), Inventory->GetInventory().Num(), 0);
	TestFalse(TEXT("Starts unequipped"), IsValid(Character->GetEquippedWeapon()));

	Inventory->DefaultWeaponClass.Add(ABaseWeapon::StaticClass());
	Inventory->DefaultWeaponClass.Add(ABaseWeapon::StaticClass());
	Inventory->SpawnInventory();

	TestEqual(TEXT("SpawnInventory creates one actor per default class"), Inventory->GetInventory().Num(), 2);
	ABaseWeapon* First = Inventory->GetInventory().Num() > 0 ? Inventory->GetInventory()[0].Get() : nullptr;
	ABaseWeapon* Second = Inventory->GetInventory().Num() > 1 ? Inventory->GetInventory()[1].Get() : nullptr;
	TestTrue(TEXT("First default weapon spawned"), IsValid(First));
	TestTrue(TEXT("Second default weapon spawned"), IsValid(Second));
	TestEqual(TEXT("First weapon is equipped"), Character->GetEquippedWeapon(), First);
	TestEqual(TEXT("Equipped weapon owner is the character"), First->GetOwner(), static_cast<AActor*>(Character));
	TestEqual(TEXT("Equipped weapon instigator is the character"), First->GetInstigator(), static_cast<APawn*>(Character));
	TestEqual(TEXT("Unequipped spare stays hidden"), Second->IsHidden(), true);
	TestEqual(TEXT("Unequipped spare status is Unequipped"), Second->GetWeaponStatus(), ERuinmatesWeaponStatus::Unequipped);
	TestTrue(TEXT("Bare ABaseWeapon has no WeaponData and still equipped safely"), First->GetWeaponData() == nullptr);

	Inventory->CycleNextWeapon();
	TestEqual(TEXT("CycleNextWeapon equips the next inventory entry through EquipWeapon"), Character->GetEquippedWeapon(), Second);

	// SpawnInventory is idempotent when weapons already exist.
	Inventory->SpawnInventory();
	TestEqual(TEXT("Second SpawnInventory does not duplicate weapons"), Inventory->GetInventory().Num(), 2);

	DestroyWorld(World, Character);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesInventorySocketFromDataTest, "RuinMates.Inventory.SocketFromWeaponData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesInventorySocketFromDataTest::RunTest(const FString& Parameters)
{
	using namespace RuinmatesInventoryTest;
	UWorld* World = CreateTestWorld(TEXT("RuinMatesInventorySocketFromDataTest"));
	if (!TestNotNull(TEXT("Created a transient world"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ARuinmatesCharacter* Character = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	UWeaponInventoryComponent* InventoryComp = IsValid(Character) ? Character->GetWeaponInventory() : nullptr;
	if (!TestTrue(TEXT("Character and inventory exist"), IsValid(Character) && IsValid(InventoryComp)))
	{
		DestroyWorld(World, Character);
		return false;
	}

	if (USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")))
	{
		Character->GetMesh()->SetSkeletalMesh(Body);
	}

	InventoryComp->DefaultWeaponClass.Add(ABaseWeapon::StaticClass());
	InventoryComp->SpawnInventory();
	ABaseWeapon* Weapon = InventoryComp->GetInventory().Num() > 0 ? InventoryComp->GetInventory()[0].Get() : nullptr;
	if (!TestTrue(TEXT("Default weapon spawned"), IsValid(Weapon)))
	{
		DestroyWorld(World, Character);
		return false;
	}

	UWeaponData* Data = NewObject<UWeaponData>(Weapon);
	Data->MeshAttachSocket = FName(TEXT("hand_l"));
	Data->LeftHandSocket = FName(TEXT("LeftHandSocket"));
	Weapon->SetWeaponData(Data);

	// Re-equip through the same EquipWeapon path SpawnInventory uses, now that data is present.
	Character->UnequipWeapon();
	Character->EquipWeapon(Weapon);

	UWeaponMountComponent* Mount = Weapon->GetMount();
	if (!TestTrue(TEXT("Mount exists"), IsValid(Mount)))
	{
		DestroyWorld(World, Character);
		return false;
	}
	TestEqual(TEXT("EquipWeapon copies MeshAttachSocket onto Mesh3PAttachSocket"), Mount->Mesh3PAttachSocket, FName(TEXT("hand_l")));
	TestEqual(TEXT("EquipWeapon copies MeshAttachSocket onto DefaultGripSocket"), Mount->DefaultGripSocket, FName(TEXT("hand_l")));

	DestroyWorld(World, Character);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesInventoryAuthorityAndPossessTest, "RuinMates.Inventory.AuthorityAndPossess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesInventoryAuthorityAndPossessTest::RunTest(const FString& Parameters)
{
	using namespace RuinmatesInventoryTest;
	UWorld* World = CreateTestWorld(TEXT("RuinMatesInventoryAuthorityAndPossessTest"));
	if (!TestNotNull(TEXT("Created a transient world"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ARuinmatesCharacter* ClientCharacter = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	if (!TestTrue(TEXT("Client-role character spawned"), IsValid(ClientCharacter)))
	{
		DestroyWorld(World, ClientCharacter);
		return false;
	}

	UWeaponInventoryComponent* ClientInventory = ClientCharacter->GetWeaponInventory();
	ClientInventory->DefaultWeaponClass.Add(ABaseWeapon::StaticClass());
	ClientCharacter->SetRole(ROLE_SimulatedProxy);
	ClientInventory->SpawnInventory();
	TestEqual(TEXT("Simulated proxy does not spawn inventory"), ClientInventory->GetInventory().Num(), 0);
	TestFalse(TEXT("Simulated proxy stays unequipped"), IsValid(ClientCharacter->GetEquippedWeapon()));

	ClientCharacter->Destroy();

	ARuinmatesCharacter* Character = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	if (!TestTrue(TEXT("Authority character spawned"), IsValid(Character)))
	{
		DestroyWorld(World, Character);
		return false;
	}

	UWeaponInventoryComponent* Inventory = Character->GetWeaponInventory();
	Inventory->DefaultWeaponClass.Add(ABaseWeapon::StaticClass());
	TestEqual(TEXT("Inventory is empty before possess"), Inventory->GetInventory().Num(), 0);

	APlayerController* PC = World->SpawnActor<APlayerController>(SpawnParams);
	if (!TestTrue(TEXT("Player controller spawned"), IsValid(PC)))
	{
		DestroyWorld(World, Character);
		return false;
	}

	Character->PossessedBy(PC);
	TestEqual(TEXT("PossessedBy spawns DefaultWeaponClass"), Inventory->GetInventory().Num(), 1);
	TestTrue(TEXT("PossessedBy equips the first weapon"), IsValid(Character->GetEquippedWeapon()));
	TestEqual(TEXT("PossessedBy equipped weapon is inventory[0]"), Character->GetEquippedWeapon(), Inventory->GetInventory()[0].Get());

	const int32 CountAfterFirstPossess = Inventory->GetInventory().Num();
	Character->PossessedBy(PC);
	TestEqual(TEXT("Re-possess does not duplicate inventory"), Inventory->GetInventory().Num(), CountAfterFirstPossess);

	Inventory->RebuildInventory();
	TestEqual(TEXT("RebuildInventory respawns the default list"), Inventory->GetInventory().Num(), 1);
	TestTrue(TEXT("RebuildInventory equips the first weapon"), IsValid(Character->GetEquippedWeapon()));

	PC->Destroy();
	DestroyWorld(World, Character);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
