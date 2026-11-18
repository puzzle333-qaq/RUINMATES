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
	// Test worlds are created without UWorld::BeginPlay, so loot generation (which now lives in
	// BeginPlay) never runs unless the actors are started explicitly.
	if (Container && !Container->HasActorBegunPlay())
	{
		Container->DispatchBeginPlay();
	}
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

// Stacking rules at the edges: fill existing stacks first, split on MaxStack, keep quality apart,
// and never accept more than the bag can hold.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBackpackStackBoundaryTest, "RuinMates.Inventory.BackpackStackBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBackpackStackBoundaryTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesBackpackStackBoundaryTest"));
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}
	World->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ARuinmatesCharacter* Character = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	URuinmatesBackpackComponent* Backpack = Character ? Character->GetBackpack() : nullptr;
	if (!TestTrue(TEXT("Character and backpack exist"), IsValid(Character) && IsValid(Backpack)))
	{
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	FRuinmatesItemStack Bandage;
	Bandage.ItemId = TEXT("Bandage");
	Bandage.DisplayName = NSLOCTEXT("RuinMatesTest", "StackBandage", "绷带");
	Bandage.Quality = ERuinmatesItemQuality::Common;
	Bandage.UnitValue = 100;
	Bandage.MaxStack = 5;

	Bandage.Count = 3;
	TestEqual(TEXT("Three bandages occupy one slot"), Backpack->TryAdd(Bandage), 3);
	TestEqual(TEXT("One used slot after the first add"), Backpack->GetUsedSlotCount(), 1);

	// 3 + 4 must fill the first stack to 5 and spill 2 into the next slot.
	Bandage.Count = 4;
	TestEqual(TEXT("Four more are all accepted"), Backpack->TryAdd(Bandage), 4);
	TestEqual(TEXT("Spill creates a second slot"), Backpack->GetUsedSlotCount(), 2);
	const TArray<FRuinmatesItemStack>& AfterSpill = Backpack->GetSlots();
	if (TestTrue(TEXT("Two slots exist"), AfterSpill.Num() >= 2))
	{
		TestEqual(TEXT("First stack is filled to MaxStack"), AfterSpill[0].Count, 5);
		TestEqual(TEXT("Remainder goes to the second slot"), AfterSpill[1].Count, 2);
	}
	TestEqual(TEXT("Loot value counts stacks with their real counts"), Backpack->GetLootValue(), 700);

	// Same ItemId, different quality must not merge.
	Bandage.Count = 1;
	Bandage.Quality = ERuinmatesItemQuality::Rare;
	TestEqual(TEXT("Rare bandage is accepted"), Backpack->TryAdd(Bandage), 1);
	TestEqual(TEXT("Different quality opens a new slot"), Backpack->GetUsedSlotCount(), 3);

	// Different ItemId must not merge.
	FRuinmatesItemStack Scrap;
	Scrap.ItemId = TEXT("Scrap");
	Scrap.DisplayName = NSLOCTEXT("RuinMatesTest", "StackScrap", "废料");
	Scrap.Quality = ERuinmatesItemQuality::Common;
	Scrap.UnitValue = 10;
	Scrap.MaxStack = 1;
	Scrap.Count = 1;
	TestEqual(TEXT("Scrap is accepted"), Backpack->TryAdd(Scrap), 1);
	TestEqual(TEXT("Different ItemId opens a new slot"), Backpack->GetUsedSlotCount(), 4);

	// Fill the bag completely with single-slot scrap.
	const int32 Capacity = Backpack->GetCapacity();
	while (Backpack->GetUsedSlotCount() < Capacity)
	{
		if (Backpack->TryAdd(Scrap) <= 0)
		{
			break;
		}
	}
	TestEqual(TEXT("Bag is full"), Backpack->GetUsedSlotCount(), Capacity);

	// A different ItemId cannot ride an existing stack, so a full bag must reject it entirely.
	const int32 ValueBeforeFullBag = Backpack->GetLootValue();
	FRuinmatesItemStack Rations;
	Rations.ItemId = TEXT("Rations");
	Rations.DisplayName = NSLOCTEXT("RuinMatesTest", "StackRations", "口粮");
	Rations.Quality = ERuinmatesItemQuality::Common;
	Rations.UnitValue = 50;
	Rations.MaxStack = 5;
	Rations.Count = 2;
	TestEqual(TEXT("A full bag accepts nothing"), Backpack->TryAdd(Rations), 0);
	TestEqual(TEXT("Nothing is lost from the bag"), Backpack->GetUsedSlotCount(), Capacity);
	TestEqual(TEXT("Rejected items do not change value"), Backpack->GetLootValue(), ValueBeforeFullBag);

	// Discarding frees a slot and the next add can use it again.
	const int32 ValueBeforeDiscard = Backpack->GetLootValue();
	TestTrue(TEXT("Discard succeeds on an occupied slot"), Backpack->DiscardSlot(0));
	TestEqual(TEXT("Discard frees exactly one slot"), Backpack->GetUsedSlotCount(), Capacity - 1);
	TestEqual(TEXT("Discard removes the stack value"), Backpack->GetLootValue(), ValueBeforeDiscard - 500);
	TestEqual(TEXT("Discarding the same slot twice fails"), Backpack->DiscardSlot(0), false);
	TestEqual(TEXT("Add after discard uses the freed slot"), Backpack->TryAdd(Rations), 2);
	TestEqual(TEXT("Bag is full again"), Backpack->GetUsedSlotCount(), Capacity);

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

#endif
