// Backpack stacking and hidden loot cannot be taken until the server reveals it.
#include "Misc/AutomationTest.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/World.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesLootContainer.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Engine/DataTable.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBackpackMoveSnapshotTest, "RuinMates.Inventory.BackpackMoveSnapshots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBackpackMoveSnapshotTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesBackpackMoveSnapshotTest"));
	if (!TestNotNull(TEXT("World"), World)) return false;
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
	Bandage.Quality = ERuinmatesItemQuality::Common;
	Bandage.MaxStack = 10;
	Bandage.Count = 8;
	TestEqual(TEXT("Initial target stack is added"), Backpack->TryAdd(Bandage), 8);
	Bandage.Count = 5;
	TestEqual(TEXT("Source stack is split into the next slot"), Backpack->TryAdd(Bandage), 5);
	// TryAdd filled the first slot to MaxStack, so free two units to give the merge somewhere to go.
	TestTrue(TEXT("Reopen room in the target stack"), Backpack->RemoveItemAt(0, 2));
	const uint32 MergeRevision = Backpack->GetRevision();
	TestTrue(TEXT("Partial merge validates matching source and target snapshots"),
		Backpack->CanMoveSlotSnapshot(1, 0, TEXT("Bandage"), ERuinmatesItemQuality::Common, 3,
			TEXT("Bandage"), ERuinmatesItemQuality::Common, 8, MergeRevision));
	TestTrue(TEXT("Partial merge succeeds"),
		Backpack->TryMoveSlot(1, 0, TEXT("Bandage"), ERuinmatesItemQuality::Common, 3,
			TEXT("Bandage"), ERuinmatesItemQuality::Common, 8, MergeRevision));
	TestEqual(TEXT("Target is filled to MaxStack"), Backpack->GetSlots()[0].Count, 10);
	TestEqual(TEXT("Overflow remains in the source slot"), Backpack->GetSlots()[1].Count, 1);
	TestFalse(TEXT("Old revision cannot replay the same move"),
		Backpack->TryMoveSlot(1, 0, TEXT("Bandage"), ERuinmatesItemQuality::Common, 3,
			TEXT("Bandage"), ERuinmatesItemQuality::Common, 8, MergeRevision));

	const uint32 MoveRevision = Backpack->GetRevision();
	TestTrue(TEXT("Move to an empty slot succeeds"),
		Backpack->TryMoveSlot(1, 2, TEXT("Bandage"), ERuinmatesItemQuality::Common, 1,
			NAME_None, ERuinmatesItemQuality::Common, 0, MoveRevision));
	TestTrue(TEXT("Source slot is empty after moving"), Backpack->GetSlots()[1].IsEmpty());
	TestEqual(TEXT("Destination preserves the stack"), Backpack->GetSlots()[2].Count, 1);

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

// Legacy ids and names must be resolved before the capacity math compares stored stacks.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBackpackLegacyCapacityTest, "RuinMates.Inventory.BackpackLegacyCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBackpackLegacyCapacityTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/RUINMATES/Data/Items/DT_Items.DT_Items"));
	if (!TestNotNull(TEXT("DT_Items exists"), Table))
	{
		return false;
	}

	// Pre-database saves and loot templates carried a numeric id plus the localized name.
	const FText LegacyName = FText::FromString(TEXT("绷带"));
	const FName Resolved = URuinmatesItemDatabaseSubsystem::ResolveItemIdInTable(Table, FName(TEXT("1")), LegacyName);
	TestEqual(TEXT("Legacy numeric id resolves through the display name"), Resolved, FName(TEXT("Bandage")));
	TestEqual(TEXT("A known id resolves to itself"),
		URuinmatesItemDatabaseSubsystem::ResolveItemIdInTable(Table, FName(TEXT("Cans")), FText::GetEmpty()), FName(TEXT("Cans")));
	TestTrue(TEXT("An unknown id without a name stays unresolved"),
		URuinmatesItemDatabaseSubsystem::ResolveItemIdInTable(Table, FName(TEXT("nope")), FText::GetEmpty()).IsNone());

	// Stored slots already use the resolved id, so merging must be measured with it.
	TArray<FRuinmatesItemStack> Slots;
	FRuinmatesItemStack Stored;
	Stored.ItemId = TEXT("Bandage");
	Stored.Quality = ERuinmatesItemQuality::Common;
	Stored.MaxStack = 10;
	Stored.Count = 3;
	Slots.Add(Stored);
	Slots.SetNum(10);

	const int32 EmptyRoom = 9 * 10;
	TestEqual(TEXT("A raw legacy id misses the existing stack"),
		URuinmatesBackpackComponent::ComputeAvailableCapacity(Slots, 10, FName(TEXT("1")), ERuinmatesItemQuality::Common, 10),
		EmptyRoom);
	TestEqual(TEXT("The resolved id includes the room left in the existing stack"),
		URuinmatesBackpackComponent::ComputeAvailableCapacity(Slots, 10, Resolved, ERuinmatesItemQuality::Common, 10),
		EmptyRoom + 7);
	return true;
}

#

// Legacy stacks are normalized through DT_Items before they merge into stored canonical stacks.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBackpackTryAddLegacyTest, "RuinMates.Inventory.BackpackTryAddLegacyId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBackpackTryAddLegacyTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/RUINMATES/Data/Items/DT_Items.DT_Items"));
	if (!TestNotNull(TEXT("DT_Items exists"), Table))
	{
		return false;
	}

	// A pre-database stack: numeric id plus the localized name.
	FRuinmatesItemStack Legacy;
	Legacy.ItemId = TEXT("1");
	Legacy.DisplayName = FText::FromString(TEXT("绷带"));
	Legacy.Count = 4;
	TestTrue(TEXT("Legacy stack normalizes against the table"),
		URuinmatesItemDatabaseSubsystem::NormalizeStackInTable(Table, Legacy, true));
	TestEqual(TEXT("Normalized stack takes the row id"), Legacy.ItemId, FName(TEXT("Bandage")));
	TestEqual(TEXT("Normalized stack keeps its count"), Legacy.Count, 4);

	const FRuinmatesItemRow* Row = URuinmatesItemDatabaseSubsystem::FindRowInTable(Table, TEXT("Bandage"));
	if (TestNotNull(TEXT("Bandage row exists"), Row))
	{
		TestEqual(TEXT("Normalized quality comes from the row"),
			static_cast<int32>(Legacy.Quality), static_cast<int32>(Row->Quality));
		TestEqual(TEXT("Normalized max stack comes from the row"), Legacy.MaxStack, FMath::Max(1, Row->MaxStack));
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesBackpackTryAddLegacyTest"));
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

	// Bare test worlds have no game instance, so TryAdd sees a canonical stack here exactly like the
	// normalized loot and purchase paths do in game.
	FRuinmatesItemStack First = Legacy;
	First.Count = 3;
	TestEqual(TEXT("Canonical stack is added"), Backpack->TryAdd(First), 3);
	FRuinmatesItemStack Second = Legacy;
	Second.Count = 4;
	TestEqual(TEXT("Second canonical stack is accepted"), Backpack->TryAdd(Second), 4);
	TestEqual(TEXT("Both adds share one slot"), Backpack->GetUsedSlotCount(), 1);
	if (Backpack->GetSlots().IsValidIndex(0))
	{
		TestEqual(TEXT("Merged count keeps the sum"), Backpack->GetSlots()[0].Count, 7);
		TestEqual(TEXT("Merged slot keeps the canonical id"), Backpack->GetSlots()[0].ItemId, FName(TEXT("Bandage")));
	}

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}
#endif