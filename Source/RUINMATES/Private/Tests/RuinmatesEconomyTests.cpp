// Item database, shared wallet and the backpack primitives the trade flow relies on.
#include "Misc/AutomationTest.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Game/RuinmatesGameState.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Inventory/RuinmatesItemTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesItemDatabaseTest, "RuinMates.Economy.ItemDatabase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesItemDatabaseTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/RUINMATES/Data/Items/DT_Items.DT_Items"));
	if (!TestNotNull(TEXT("DT_Items exists"), Table))
	{
		return false;
	}

	// The subsystem is normally created by the game instance; exercise the same parsing rules
	// through the static table helpers so the test does not need a game instance.
	auto FindRow = [Table](FName ItemId) { return URuinmatesItemDatabaseSubsystem::FindRowInTable(Table, ItemId); };
	auto CreateStack = [FindRow](FName ItemId, int32 Count)
	{
		const FRuinmatesItemRow* Row = FindRow(ItemId);
		return Row ? URuinmatesItemDatabaseSubsystem::CreateStackFromRow(*Row, ItemId, Count) : FRuinmatesItemStack();
	};

	TestTrue(TEXT("Bandage is a known item"), FindRow(TEXT("Bandage")) != nullptr);
	TestEqual(TEXT("Bandage buy price comes from the table"), (FindRow(TEXT("Bandage")) ? FindRow(TEXT("Bandage"))->BuyPrice : 0), 200);
	TestEqual(TEXT("Bandage sell price comes from the table"), (FindRow(TEXT("Bandage")) ? FindRow(TEXT("Bandage"))->SellPrice : 0), 120);
	TestEqual(TEXT("Bandage max stack comes from the table"), (FindRow(TEXT("Bandage")) ? FindRow(TEXT("Bandage"))->MaxStack : 1), 10);

	TestFalse(TEXT("Unknown ids are rejected"), FindRow(TEXT("Nope")) != nullptr);
	TestEqual(TEXT("Unknown ids have no buy price"), (FindRow(TEXT("Nope")) ? FindRow(TEXT("Nope"))->BuyPrice : 0), 0);
	TestNull(TEXT("Unknown ids resolve to no row"), FindRow(TEXT("Nope")));

	TestEqual(TEXT("Gland cannot be bought"), (FindRow(TEXT("Gland")) ? FindRow(TEXT("Gland"))->BuyPrice : 0), 0);
	TestEqual(TEXT("Gland can be sold"), (FindRow(TEXT("Gland")) ? FindRow(TEXT("Gland"))->SellPrice : 0), 6400);
	TestEqual(TEXT("Core stacks to one"), (FindRow(TEXT("Core")) ? FindRow(TEXT("Core"))->MaxStack : 1), 1);

	const FRuinmatesItemStack Stack = CreateStack(TEXT("Bandage"), 999);
	TestEqual(TEXT("Created stack clamps to the row max stack"), Stack.Count, 10);
	TestEqual(TEXT("Created stack copies the display name"), Stack.DisplayName.ToString(), FString(TEXT("绷带")));
	TestEqual(TEXT("Created stack copies sell value into unit value"), Stack.UnitValue, 120);
	TestEqual(TEXT("Created stack keeps the item id"), Stack.ItemId, FName(TEXT("Bandage")));

	TestTrue(TEXT("The table exposes at least the six seed items"), Table->GetRowMap().Num() >= 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesGameStateFundsTest, "RuinMates.Economy.SharedFunds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesGameStateFundsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesSharedFundsTest"));
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}
	World->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ARuinmatesGameState* GameState = World->SpawnActor<ARuinmatesGameState>(SpawnParams);
	if (!TestNotNull(TEXT("Game state spawned"), GameState))
	{
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	TestEqual(TEXT("Wallet starts empty"), GameState->GetSharedFunds(), 0);
	TestTrue(TEXT("Adding funds succeeds"), GameState->AddFunds(500));
	TestEqual(TEXT("Balance after adding"), GameState->GetSharedFunds(), 500);
	TestFalse(TEXT("Negative add is rejected"), GameState->AddFunds(-100));
	TestEqual(TEXT("Balance is unchanged by a negative add"), GameState->GetSharedFunds(), 500);
	TestFalse(TEXT("Negative spend is rejected"), GameState->TrySpendFunds(-50));
	TestEqual(TEXT("Balance is unchanged by a negative spend"), GameState->GetSharedFunds(), 500);
	TestFalse(TEXT("Overspending is rejected"), GameState->TrySpendFunds(600));
	TestEqual(TEXT("Balance is unchanged by a failed spend"), GameState->GetSharedFunds(), 500);
	TestTrue(TEXT("Affordable spend succeeds"), GameState->TrySpendFunds(200));
	TestEqual(TEXT("Balance after spending"), GameState->GetSharedFunds(), 300);
	TestTrue(TEXT("HasEnoughFunds uses the balance"), GameState->HasEnoughFunds(300));
	TestFalse(TEXT("HasEnoughFunds rejects more than the balance"), GameState->HasEnoughFunds(301));

	GameState->SetSharedFunds(MAX_int32);
	TestTrue(TEXT("Adding on top of the max is clamped, not overflowed"), GameState->AddFunds(10));
	TestEqual(TEXT("Balance stays at INT32_MAX"), GameState->GetSharedFunds(), MAX_int32);

	GameState->SetSharedFunds(-42);
	TestEqual(TEXT("Negative restore is clamped to zero"), GameState->GetSharedFunds(), 0);

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBackpackExactTradeTest, "RuinMates.Economy.BackpackExactTrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBackpackExactTradeTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesBackpackExactTradeTest"));
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
	Bandage.Quality = ERuinmatesItemQuality::Common;
	Bandage.MaxStack = 10;

	TestTrue(TEXT("Twelve bandages fit in an empty bag"), Backpack->CanAddExactCount(Bandage, 12));
	TestFalse(TEXT("Zero is never accepted"), Backpack->CanAddExactCount(Bandage, 0));
	TestTrue(TEXT("Exact add succeeds"), Backpack->TryAddExact(Bandage, 12));
	TestEqual(TEXT("Twelve units occupy two slots"), Backpack->GetUsedSlotCount(), 2);

	TestTrue(TEXT("Removing a partial amount works"), Backpack->RemoveItemAt(0, 4));
	if (Backpack->GetSlots().IsValidIndex(0))
	{
		TestEqual(TEXT("First slot keeps the remainder"), Backpack->GetSlots()[0].Count, 6);
	}
	TestTrue(TEXT("Removing more than the stack clears the slot"), Backpack->RemoveItemAt(0, 6));
	if (Backpack->GetSlots().IsValidIndex(0))
	{
		TestTrue(TEXT("Slot is cleared"), Backpack->GetSlots()[0].IsEmpty());
	}
	TestEqual(TEXT("One stack remains"), Backpack->GetUsedSlotCount(), 1);
	TestFalse(TEXT("Removing from an empty slot fails"), Backpack->RemoveItemAt(0, 1));
	TestFalse(TEXT("Removing a zero amount fails"), Backpack->RemoveItemAt(1, 0));

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
