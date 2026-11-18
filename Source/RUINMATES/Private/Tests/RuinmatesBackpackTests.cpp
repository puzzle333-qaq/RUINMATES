// Backpack stacking and hidden loot cannot be taken until the server reveals it.
#include "Misc/AutomationTest.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/World.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesLootContainer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBackpackStackTest, "RuinMates.Inventory.BackpackStackAndSearch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBackpackStackTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesBackpackStackTest"));
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}
	World->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ARuinmatesCharacter* Character = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	ARuinmatesLootContainer* Container = World->SpawnActor<ARuinmatesLootContainer>(SpawnParams);
	URuinmatesBackpackComponent* Backpack = Character ? Character->GetBackpack() : nullptr;
	if (!TestTrue(TEXT("Character, backpack, and container exist"), IsValid(Character) && IsValid(Backpack) && IsValid(Container)))
	{
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	FRuinmatesItemStack Bandage;
	Bandage.ItemId = TEXT("Bandage");
	Bandage.DisplayName = NSLOCTEXT("RuinMatesTest", "Bandage", "绷带");
	Bandage.Count = 4;
	Bandage.MaxStack = 5;
	Bandage.UnitValue = 100;
	TestEqual(TEXT("First add takes the whole stack"), Backpack->TryAdd(Bandage), 4);

	Bandage.Count = 3;
	TestEqual(TEXT("Overflow fills the stack then a new slot"), Backpack->TryAdd(Bandage), 3);
	TestEqual(TEXT("Two slots are occupied"), Backpack->GetUsedSlotCount(), 2);
	TestEqual(TEXT("Loot value counts both stacks"), Backpack->GetLootValue(), 700);

	TestEqual(TEXT("Container starts with six hidden items"), Container->GetContents().Num(), 6);
	TestEqual(TEXT("Nothing is revealed before the timer"), Container->GetRevealedCount(), 0);
	TestEqual(TEXT("Hidden loot cannot be taken"), Container->GiveTo(Character, 0), 0);

	TestTrue(TEXT("Discard frees a slot"), Backpack->DiscardSlot(0));
	TestEqual(TEXT("One stack remains"), Backpack->GetUsedSlotCount(), 1);

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

#endif
