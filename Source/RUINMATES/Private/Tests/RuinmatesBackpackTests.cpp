// Backpack stacking and hidden loot cannot be taken until the server reveals it.
#include "Misc/AutomationTest.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/World.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesLootContainer.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "UI/RuinmatesTerminalTypes.h"
#include "Stats/StatsComponent.h"
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
#

// ---- Sort / split (Phase 1D) -------------------------------------------------

namespace
{
	FRuinmatesItemStack MakeItemStack(UDataTable* Table, const TCHAR* ItemId, int32 Count)
	{
		const FRuinmatesItemRow* Row = URuinmatesItemDatabaseSubsystem::FindRowInTable(Table, ItemId);
		return Row ? URuinmatesItemDatabaseSubsystem::CreateStackFromRow(*Row, ItemId, Count) : FRuinmatesItemStack();
	}

	int32 TotalUnits(URuinmatesBackpackComponent* Backpack)
	{
		int32 Total = 0;
		for (const FRuinmatesItemStack& Stack : Backpack->GetSlots())
		{
			Total += Stack.Count;
		}
		return Total;
	}

	UWorld* MakeBackpackWorld(ARuinmatesCharacter*& OutCharacter, URuinmatesBackpackComponent*& OutBackpack)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesBackpackOpsTest"));
		World->AddToRoot();
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		OutCharacter = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
		OutBackpack = OutCharacter ? OutCharacter->GetBackpack() : nullptr;
		return World;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBackpackSortTest, "RuinMates.Inventory.BackpackSort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBackpackSortTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/RUINMATES/Data/Items/DT_Items.DT_Items"));
	if (!TestNotNull(TEXT("DT_Items exists"), Table))
	{
		return false;
	}

	ARuinmatesCharacter* Character = nullptr;
	URuinmatesBackpackComponent* Backpack = nullptr;
	UWorld* World = MakeBackpackWorld(Character, Backpack);
	if (!TestTrue(TEXT("Character and backpack exist"), IsValid(Character) && IsValid(Backpack)))
	{
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	// Two genuinely separate Bandage stacks (installed directly, so TryAdd cannot merge them first).
	TArray<FRuinmatesItemStack> Seeded;
	Seeded.SetNum(Backpack->GetCapacity());
	Seeded[0] = MakeItemStack(Table, TEXT("Bandage"), 3);
	Seeded[1] = MakeItemStack(Table, TEXT("Cans"), 2);
	Seeded[2] = MakeItemStack(Table, TEXT("AmmoBox"), 5);
	Seeded[3] = MakeItemStack(Table, TEXT("Bandage"), 4);
	Backpack->SetSlotsForTest(Seeded);
	TestEqual(TEXT("Seeded stacks stay separate before the sort"), Backpack->GetUsedSlotCount(), 4);
	const int32 TotalBefore = TotalUnits(Backpack);
	const int32 UsedBefore = Backpack->GetUsedSlotCount();
	const uint32 RevisionBefore = Backpack->GetRevision();
	TestTrue(TEXT("Sort is accepted with the current revision"), Backpack->TrySort(RevisionBefore));
	TestEqual(TEXT("One sort emits exactly one revision"), Backpack->GetRevision(), RevisionBefore + 1);
	TestEqual(TEXT("Sort conserves the total unit count"), TotalUnits(Backpack), TotalBefore);
	TestEqual(TEXT("Sort merges the two Bandage stacks"), Backpack->GetUsedSlotCount(), 3);
	TestTrue(TEXT("Merging never increases slot usage"), Backpack->GetUsedSlotCount() <= UsedBefore);

	const TArray<FRuinmatesItemStack>& Slots = Backpack->GetSlots();
	// No game instance in the test world, so TrySort uses the quality/id fallback; the real
	// DT_Items category order is covered by BackpackSortCategories.
	TestTrue(TEXT("Higher quality leads the fallback order"), Slots.IsValidIndex(0) && Slots[0].ItemId == TEXT("AmmoBox") && Slots[0].Count == 5);
	TestTrue(TEXT("The merged Bandage stack follows"), Slots.IsValidIndex(1) && Slots[1].ItemId == TEXT("Bandage") && Slots[1].Count == 7);
	TestTrue(TEXT("Equal quality order is stable by ItemId"), Slots.IsValidIndex(2) && Slots[2].ItemId == TEXT("Cans"));
	for (int32 Index = Backpack->GetUsedSlotCount(); Index < Slots.Num(); ++Index)
	{
		TestTrue(TEXT("Every gap is pushed to the end"), Slots[Index].IsEmpty());
	}

	// A second pass on an already sorted backpack must not reorder anything.
	TArray<FName> FirstOrder;
	for (const FRuinmatesItemStack& Stack : Slots)
	{
		FirstOrder.Add(Stack.ItemId);
	}
	TestTrue(TEXT("Sorting twice is accepted"), Backpack->TrySort(Backpack->GetRevision()));
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		TestEqual(TEXT("A sorted backpack keeps its order"), Slots[Index].ItemId, FirstOrder[Index]);
	}
	TestFalse(TEXT("A stale revision is rejected"), Backpack->TrySort(RevisionBefore));

	// Same id, different quality must stay separate.
	FRuinmatesItemStack FineBandage = MakeItemStack(Table, TEXT("Bandage"), 2);
	FineBandage.Quality = ERuinmatesItemQuality::Fine;
	Backpack->TryAdd(FineBandage);
	const int32 BeforeQualitySort = Backpack->GetUsedSlotCount();
	TestTrue(TEXT("Sort with mixed qualities succeeds"), Backpack->TrySort(Backpack->GetRevision()));
	TestEqual(TEXT("Different qualities never merge"), Backpack->GetUsedSlotCount(), BeforeQualitySort);

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBackpackSortEdgesTest, "RuinMates.Inventory.BackpackSortEdges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBackpackSortEdgesTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/RUINMATES/Data/Items/DT_Items.DT_Items"));
	if (!TestNotNull(TEXT("DT_Items exists"), Table))
	{
		return false;
	}

	ARuinmatesCharacter* Character = nullptr;
	URuinmatesBackpackComponent* Backpack = nullptr;
	UWorld* World = MakeBackpackWorld(Character, Backpack);
	if (!TestTrue(TEXT("Character and backpack exist"), IsValid(Character) && IsValid(Backpack)))
	{
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	// Empty backpack: the pass succeeds, changes nothing and still bumps the revision once.
	const uint32 EmptyRevision = Backpack->GetRevision();
	TestTrue(TEXT("An empty backpack can be sorted"), Backpack->TrySort(EmptyRevision));
	TestEqual(TEXT("Empty sort still emits one revision"), Backpack->GetRevision(), EmptyRevision + 1);
	TestEqual(TEXT("Empty sort keeps zero used slots"), Backpack->GetUsedSlotCount(), 0);

	// Full backpack of non-stackable items: every slot is occupied, order must stay stable.
	const int32 Capacity = Backpack->GetCapacity();
	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		FRuinmatesItemStack Core = MakeItemStack(Table, TEXT("Core"), 1);
		TestEqual(TEXT("Core is accepted"), Backpack->TryAdd(Core), 1);
	}
	const int32 FullTotal = TotalUnits(Backpack);
	TestEqual(TEXT("The backpack is full"), Backpack->GetUsedSlotCount(), Capacity);
	TestTrue(TEXT("A full backpack can be sorted"), Backpack->TrySort(Backpack->GetRevision()));
	TestEqual(TEXT("Full sort conserves the unit total"), TotalUnits(Backpack), FullTotal);
	TestEqual(TEXT("Full sort keeps every slot filled"), Backpack->GetUsedSlotCount(), Capacity);

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBackpackSplitTest, "RuinMates.Inventory.BackpackSplit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBackpackSplitTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/RUINMATES/Data/Items/DT_Items.DT_Items"));
	if (!TestNotNull(TEXT("DT_Items exists"), Table))
	{
		return false;
	}

	ARuinmatesCharacter* Character = nullptr;
	URuinmatesBackpackComponent* Backpack = nullptr;
	UWorld* World = MakeBackpackWorld(Character, Backpack);
	if (!TestTrue(TEXT("Character and backpack exist"), IsValid(Character) && IsValid(Backpack)))
	{
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	Backpack->TryAdd(MakeItemStack(Table, TEXT("Bandage"), 7));
	const uint32 Revision = Backpack->GetRevision();
	const FName BandageId = TEXT("Bandage");
	const ERuinmatesItemQuality BandageQuality = Backpack->GetSlots()[0].Quality;
	TestFalse(TEXT("Splitting zero is rejected"), Backpack->TrySplitStack(0, 0, BandageId, BandageQuality, 7, Revision));
	TestFalse(TEXT("Splitting the whole stack is rejected"), Backpack->TrySplitStack(0, 7, BandageId, BandageQuality, 7, Revision));
	TestFalse(TEXT("Splitting more than the stack is rejected"), Backpack->TrySplitStack(0, 8, BandageId, BandageQuality, 7, Revision));
	TestFalse(TEXT("A mismatched item snapshot is rejected"), Backpack->TrySplitStack(0, 3, FName(TEXT("Cans")), BandageQuality, 7, Revision));
	TestFalse(TEXT("A mismatched count snapshot is rejected"), Backpack->TrySplitStack(0, 3, BandageId, BandageQuality, 6, Revision));

	TestTrue(TEXT("Splitting three units is accepted"), Backpack->TrySplitStack(0, 3, BandageId, BandageQuality, 7, Revision));
	TestEqual(TEXT("The source keeps the remainder"), Backpack->GetSlots()[0].Count, 4);
	TestEqual(TEXT("The split lands in the next free slot"), Backpack->GetSlots()[1].Count, 3);
	TestEqual(TEXT("Split conserves the total"), TotalUnits(Backpack), 7);
	TestEqual(TEXT("Split keeps the item identity"), Backpack->GetSlots()[1].ItemId, FName(TEXT("Bandage")));
	TestEqual(TEXT("Split keeps the quality"), static_cast<int32>(Backpack->GetSlots()[1].Quality),
		static_cast<int32>(Backpack->GetSlots()[0].Quality));
	const uint32 SplitRevision = Backpack->GetRevision();
	TestFalse(TEXT("A stale revision is rejected"), Backpack->TrySplitStack(0, 2, BandageId, Backpack->GetSlots()[0].Quality, Backpack->GetSlots()[0].Count, Revision));
	TestFalse(TEXT("A stale revision is also rejected after the split"), Backpack->TrySplitStack(0, 1, BandageId, Backpack->GetSlots()[0].Quality, Backpack->GetSlots()[0].Count, SplitRevision - 1));

	// Fill every remaining slot so no free slot exists.
	while (Backpack->GetUsedSlotCount() < Backpack->GetCapacity())
	{
		Backpack->TryAdd(MakeItemStack(Table, TEXT("Core"), 1));
	}
	const int32 FullTotal = TotalUnits(Backpack);
	const uint32 FullRevision = Backpack->GetRevision();
	TestFalse(TEXT("Splitting without a free slot fails"), Backpack->TrySplitStack(0, 1, Backpack->GetSlots()[0].ItemId, Backpack->GetSlots()[0].Quality, Backpack->GetSlots()[0].Count, FullRevision));
	TestEqual(TEXT("A failed split changes nothing"), TotalUnits(Backpack), FullTotal);
	TestEqual(TEXT("A failed split keeps the revision"), Backpack->GetRevision(), FullRevision);

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesMarketTradeStateTest, "RuinMates.UI.V4.MarketTradeState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesMarketTradeStateTest::RunTest(const FString& Parameters)
{
	FRuinmatesMarketTradeState State;
	TestFalse(TEXT("No request is pending by default"), State.IsPending());
	TestTrue(TEXT("The first request starts"), State.BeginRequest());
	TestTrue(TEXT("A second request is blocked while pending"), State.IsPending());
	TestFalse(TEXT("A second BeginRequest is refused"), State.BeginRequest());

	State.SellItemId = TEXT("Bandage");
	State.SellQuality = ERuinmatesItemQuality::Fine;
	State.SellSlot = 3;
	State.CompleteRequest();
	TestFalse(TEXT("The server answer clears pending"), State.IsPending());
	TestEqual(TEXT("The selection survives a completed request"), State.SellItemId, FName(TEXT("Bandage")));

	State.BeginRequest();
	State.Reset();
	TestFalse(TEXT("Reset clears pending"), State.IsPending());
	TestTrue(TEXT("Reset clears the selected item"), State.SellItemId.IsNone());
	TestEqual(TEXT("Reset clears the selected slot"), State.SellSlot, INDEX_NONE);
	TestTrue(TEXT("A request can start after a reset"), State.BeginRequest());
	return true;
}
#

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBackpackSortCategoryTest, "RuinMates.Inventory.BackpackSortCategories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBackpackSortCategoryTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/RUINMATES/Data/Items/DT_Items.DT_Items"));
	if (!TestNotNull(TEXT("DT_Items exists"), Table))
	{
		return false;
	}

	// One row per real category; the pure sorter must order by the table category, not the fallback.
	TArray<FRuinmatesItemStack> Stacks;
	Stacks.Add(MakeItemStack(Table, TEXT("Circuit"), 1));   // Material
	Stacks.Add(MakeItemStack(Table, TEXT("Cans"), 1));      // Consumable
	Stacks.Add(MakeItemStack(Table, TEXT("AmmoBox"), 1));   // Ammo
	Stacks.Add(MakeItemStack(Table, TEXT("Bandage"), 1));   // Consumable
	URuinmatesBackpackComponent::SortStacks(Table, Stacks);
	TestEqual(TEXT("Category sort keeps every stack"), Stacks.Num(), 4);
	TestEqual(TEXT("Consumables come first"), static_cast<int32>(Stacks[0].Quality) >= 0 ? Stacks[0].ItemId : NAME_None, FName(TEXT("Bandage")));
	TestEqual(TEXT("Second consumable keeps ItemId order"), Stacks[1].ItemId, FName(TEXT("Cans")));
	TestEqual(TEXT("Ammo follows consumables"), Stacks[2].ItemId, FName(TEXT("AmmoBox")));
	TestEqual(TEXT("Material comes last"), Stacks[3].ItemId, FName(TEXT("Circuit")));

	// Quality wins inside one category.
	TArray<FRuinmatesItemStack> Mixed;
	Mixed.Add(MakeItemStack(Table, TEXT("Bandage"), 1));
	FRuinmatesItemStack FineBandage = MakeItemStack(Table, TEXT("Bandage"), 1);
	FineBandage.Quality = ERuinmatesItemQuality::Rare;
	Mixed.Add(FineBandage);
	URuinmatesBackpackComponent::SortStacks(Table, Mixed);
	TestEqual(TEXT("Higher quality sorts first inside a category"), static_cast<int32>(Mixed[0].Quality),
		static_cast<int32>(ERuinmatesItemQuality::Rare));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesItemUseTest, "RuinMates.Inventory.ItemUse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesItemUseTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/RUINMATES/Data/Items/DT_Items.DT_Items"));
	if (!TestNotNull(TEXT("DT_Items exists"), Table))
	{
		return false;
	}

	ARuinmatesCharacter* Character = nullptr;
	URuinmatesBackpackComponent* Backpack = nullptr;
	UWorld* World = MakeBackpackWorld(Character, Backpack);
	if (!TestTrue(TEXT("Character and backpack exist"), IsValid(Character) && IsValid(Backpack)))
	{
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	UStatsComponent* Stats = Character->GetStats();
	if (!TestNotNull(TEXT("Character has stats"), Stats))
	{
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	// Heal success: damaged character, one bandage used, exactly one consumed.
	Stats->ChangeHealthByAmount(-40.f, nullptr);
	const float BeforeHeal = Stats->GetHealth();
	Backpack->TryAdd(MakeItemStack(Table, TEXT("Bandage"), 2));
	const FName BandageId = TEXT("Bandage");
	const ERuinmatesItemQuality BandageQuality = Backpack->GetSlots()[0].Quality;
	TestTrue(TEXT("Using a bandage succeeds"),
		Character->TryUseBackpackItem(0, BandageId, BandageQuality, 2, Backpack->GetRevision()));
	TestTrue(TEXT("Healing raises health"), Stats->GetHealth() > BeforeHeal);
	TestEqual(TEXT("A successful use consumes exactly one unit"), Backpack->GetSlots()[0].Count, 1);

	// Full health rejects the use and keeps the item.
	Stats->ChangeHealthByAmount(1000.f, nullptr);
	TestTrue(TEXT("Health is full"), Stats->GetHealth() >= Stats->GetMaxHealth());
	TestFalse(TEXT("Using a bandage at full health fails"),
		Character->TryUseBackpackItem(0, BandageId, BandageQuality, 1, Backpack->GetRevision()));
	TestEqual(TEXT("A failed use consumes nothing"), Backpack->GetSlots()[0].Count, 1);

	// Stale revision, wrong snapshot, illegal slot and empty slot all fail without consuming.
	TestFalse(TEXT("A stale revision is rejected"),
		Character->TryUseBackpackItem(0, BandageId, BandageQuality, 1, Backpack->GetRevision() - 1));
	TestFalse(TEXT("A mismatched count snapshot is rejected"),
		Character->TryUseBackpackItem(0, BandageId, BandageQuality, 9, Backpack->GetRevision()));
	TestFalse(TEXT("A negative slot is rejected"),
		Character->TryUseBackpackItem(-1, BandageId, BandageQuality, 1, Backpack->GetRevision()));
	TestFalse(TEXT("An out-of-range slot is rejected"),
		Character->TryUseBackpackItem(Backpack->GetCapacity() + 3, BandageId, BandageQuality, 1, Backpack->GetRevision()));
	TestEqual(TEXT("Rejected uses leave the stack untouched"), Backpack->GetSlots()[0].Count, 1);

	// Ammo box without a weapon fails and keeps the item.
	Backpack->TryAdd(MakeItemStack(Table, TEXT("AmmoBox"), 1));
	const int32 AmmoSlot = 1;
	const int32 AmmoCount = Backpack->GetSlots()[AmmoSlot].Count;
	TestFalse(TEXT("An ammo box without a weapon fails"),
		Character->TryUseBackpackItem(AmmoSlot, FName(TEXT("AmmoBox")), Backpack->GetSlots()[AmmoSlot].Quality, AmmoCount, Backpack->GetRevision()));
	TestEqual(TEXT("A failed ammo use consumes nothing"), Backpack->GetSlots()[AmmoSlot].Count, AmmoCount);

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}
#endif