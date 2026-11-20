#include "Misc/AutomationTest.h"

#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Same item identity in separate stacks; no production table or save slot is modified. */
	FRuinmatesItemStack MakeHubTestStack(FName Id, int32 Count, int32 MaxStack = 10)
	{
		FRuinmatesItemStack Stack;
		Stack.ItemId = Id;
		Stack.Count = Count;
		Stack.MaxStack = MaxStack;
		Stack.DisplayName = FText::FromName(Id);
		return Stack;
	}

	/** Compares held/static fields to catch accidental changes on a rejected recipe. */
	bool EqualHubTestSlots(const TArray<FRuinmatesItemStack>& Left, const TArray<FRuinmatesItemStack>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			const FRuinmatesItemStack& A = Left[Index];
			const FRuinmatesItemStack& B = Right[Index];
			if (A.ItemId != B.ItemId || A.Count != B.Count || A.MaxStack != B.MaxStack
				|| A.Quality != B.Quality || A.UnitValue != B.UnitValue || !A.DisplayName.EqualTo(B.DisplayName)
				|| !A.Description.EqualTo(B.Description) || A.bUpgradeMaterial != B.bUpgradeMaterial
				|| A.UseEffect != B.UseEffect || A.UseAmount != B.UseAmount || A.bRevealed != B.bRevealed)
			{
				return false;
			}
		}
		return true;
	}

	/** Temporary GameInstance owns a transient database override. Never invokes SaveCampaign. */
	struct FHubInventoryFixture
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		UDataTable* Table = nullptr;
		URuinmatesItemDatabaseSubsystem* Database = nullptr;

		FHubInventoryFixture()
		{
			if (!GEngine)
			{
				return;
			}
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			Database = GameInstance->GetSubsystem<URuinmatesItemDatabaseSubsystem>();
			if (World)
			{
				World->InitializeActorsForPlay(FURL());
			}
			Table = NewObject<UDataTable>(GameInstance, NAME_None, RF_Transient);
			Table->RowStruct = FRuinmatesItemRow::StaticStruct();
			FRuinmatesItemRow Metal;
			Metal.DisplayName = FText::FromString(TEXT("Test metal"));
			Metal.Description = FText::FromString(TEXT("Transient construction test row"));
			Metal.MaxStack = 10;
			Metal.Quality = ERuinmatesItemQuality::Fine;
			Metal.SellPrice = 17;
			Metal.bUpgradeMaterial = true;
			Table->AddRow(TEXT("HubTestMetal"), Metal);
			FRuinmatesItemRow Circuit = Metal;
			Circuit.DisplayName = FText::FromString(TEXT("Test circuit"));
			Circuit.MaxStack = 4;
			Table->AddRow(TEXT("HubTestCircuit"), Circuit);
			if (Database)
			{
				Database->SetTableOverride(Table);
			}
		}

		~FHubInventoryFixture()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}
			if (World && GEngine)
			{
				GEngine->DestroyWorldContext(World);
			}
			if (World)
			{
				World->RemoveFromRoot();
			}
			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		URuinmatesBackpackComponent* SpawnGrid()
		{
			if (!World)
			{
				return nullptr;
			}
			FActorSpawnParameters Parameters;
			Parameters.ObjectFlags |= RF_Transient;
			AActor* Owner = World->SpawnActor<AActor>(Parameters);
			if (!Owner)
			{
				return nullptr;
			}
			URuinmatesBackpackComponent* Grid = NewObject<URuinmatesBackpackComponent>(Owner);
			Owner->AddInstanceComponent(Grid);
			Grid->RegisterComponent();
			return Grid;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubRecipeConsumeTest, "RuinMates.Hub.Inventory.AtomicRecipeConsumption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubRecipeConsumeTest::RunTest(const FString& Parameters)
{
	TArray<FRuinmatesItemStack> Slots = { MakeHubTestStack(TEXT("Metal"), 3),
		MakeHubTestStack(TEXT("Circuit"), 2), MakeHubTestStack(TEXT("Metal"), 4), FRuinmatesItemStack() };
	const TArray<FRuinmatesItemStack> Original = Slots;
	TMap<FName, int32> Costs;
	Costs.Add(TEXT("Metal"), 6);
	Costs.Add(TEXT("Circuit"), 3);
	TestFalse(TEXT("A later missing material rejects the entire recipe"),
		URuinmatesBackpackComponent::ConsumeItemsFromSlots(Slots, Costs));
	TestTrue(TEXT("Insufficient recipe changes no field of any stack"), EqualHubTestSlots(Slots, Original));
	Costs.Reset();
	// Recipe arrays must aggregate duplicate IDs before this unique-key TMap primitive.
	Costs.FindOrAdd(TEXT("Metal")) += 2;
	Costs.FindOrAdd(TEXT("Metal")) += 4;
	Costs.FindOrAdd(TEXT("Circuit")) = 2;
	TestEqual(TEXT("Duplicate recipe entries were aggregated"), Costs[TEXT("Metal")], 6);
	TestTrue(TEXT("Exact totals across separate stacks succeed"),
		URuinmatesBackpackComponent::ConsumeItemsFromSlots(Slots, Costs));
	TestTrue(TEXT("First consumed metal stack is canonical empty"), Slots[0].ItemId.IsNone() && Slots[0].Count == 0);
	TestTrue(TEXT("Circuit stack is consumed exactly"), Slots[1].IsEmpty());
	TestEqual(TEXT("Second metal stack keeps the exact remainder"), Slots[2].Count, 1);
	TestEqual(TEXT("Slot layout is preserved"), Slots.Num(), 4);

	const TArray<FRuinmatesItemStack> BeforeFree = Slots;
	Costs.Reset();
	TestTrue(TEXT("An empty recipe is a free action"), URuinmatesBackpackComponent::ConsumeItemsFromSlots(Slots, Costs));
	TestTrue(TEXT("Free action does not normalize or touch slots"), EqualHubTestSlots(Slots, BeforeFree));
	TArray<FRuinmatesItemStack> Malformed = { MakeHubTestStack(TEXT("Metal"), 11, 10) };
	Costs.Add(TEXT("Metal"), 1);
	TestFalse(TEXT("Overfilled input stacks cannot be used to fund construction"),
		URuinmatesBackpackComponent::ConsumeItemsFromSlots(Malformed, Costs));
	TestEqual(TEXT("Malformed stack rejection leaves its count untouched"), Malformed[0].Count, 11);
	Costs.Reset();
	Costs.Add(TEXT("Metal"), 0);
	TestFalse(TEXT("Zero-count requirements are invalid"), URuinmatesBackpackComponent::ConsumeItemsFromSlots(Slots, Costs));
	Costs[TEXT("Metal")] = -1;
	TestFalse(TEXT("Negative requirements cannot generate resources"), URuinmatesBackpackComponent::ConsumeItemsFromSlots(Slots, Costs));
	Costs.Reset();
	Costs.Add(NAME_None, 1);
	TestFalse(TEXT("Missing cost ID is invalid"), URuinmatesBackpackComponent::ConsumeItemsFromSlots(Slots, Costs));
	TestTrue(TEXT("Invalid requirements leave all slots untouched"), EqualHubTestSlots(Slots, BeforeFree));

	TArray<FRuinmatesItemStack> Large = { MakeHubTestStack(TEXT("Metal"), MAX_int32, MAX_int32),
		MakeHubTestStack(TEXT("Metal"), MAX_int32, MAX_int32) };
	Costs.Reset();
	Costs.Add(TEXT("Metal"), MAX_int32);
	TestTrue(TEXT("Large aggregate uses wide arithmetic without overflow"), URuinmatesBackpackComponent::ConsumeItemsFromSlots(Large, Costs));
	TestTrue(TEXT("Exactly one INT32_MAX stack was consumed"), Large[0].IsEmpty() && Large[1].Count == MAX_int32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubInventoryCapacityTest, "RuinMates.Hub.Inventory.ConfigurableCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubInventoryCapacityTest::RunTest(const FString& Parameters)
{
	FHubInventoryFixture Fixture;
	URuinmatesBackpackComponent* Grid = Fixture.SpawnGrid();
	if (!TestTrue(TEXT("Transient inventory and item database exist"), Grid && Fixture.Database))
	{
		return false;
	}
	TestEqual(TEXT("Default override preserves the personal ten-slot tier"), Grid->GetCapacity(), 10);
	TestEqual(TEXT("Default capacity override is zero"), Grid->GetCapacityOverride(), 0);
	TestTrue(TEXT("The same component can configure a ninety-six-slot warehouse"), Grid->SetCapacityOverride(96));
	TestEqual(TEXT("Warehouse capacity is actual grid capacity"), Grid->GetSlots().Num(), 96);
	TArray<FRuinmatesItemStack> Candidate = Grid->GetSlots();
	Candidate[95] = MakeHubTestStack(TEXT("HubTestMetal"), 2);
	TestTrue(TEXT("Warehouse last slot can hold canonical items"), Grid->ReplaceSlotsForTransaction(Candidate, Grid->GetRevision()));
	const uint32 BeforeShrink = Grid->GetRevision();
	TestFalse(TEXT("Capacity shrink cannot truncate a valid occupied tail slot"), Grid->SetCapacityOverride(10));
	TestEqual(TEXT("Rejected shrink retains capacity"), Grid->GetCapacity(), 96);
	TestEqual(TEXT("Rejected shrink retains revision"), Grid->GetRevision(), BeforeShrink);
	TestEqual(TEXT("Rejected shrink retains the items"), Grid->CountItem(TEXT("HubTestMetal")), 2);
	TestFalse(TEXT("Zero is not an accepted runtime capacity"), Grid->SetCapacityOverride(0));
	TestFalse(TEXT("Unbounded runtime capacities are rejected"), Grid->SetCapacityOverride(513));
	TestTrue(TEXT("Legacy one-slot removal behavior is preserved"), Grid->RemoveItemAt(95, 999));
	TestEqual(TEXT("Legacy removal still consumes only the available amount"), Grid->CountItem(TEXT("HubTestMetal")), 0);
	TestTrue(TEXT("Empty tail slots allow a safe shrink"), Grid->SetCapacityOverride(10));
	TestEqual(TEXT("Safe shrink resizes storage"), Grid->GetSlots().Num(), 10);
	Grid->GetOwner()->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client role cannot change capacity"), Grid->SetCapacityOverride(96));
	Grid->GetOwner()->SetRole(ROLE_Authority);
	Grid->SetSlotsForTest({ MakeHubTestStack(TEXT("HubTestMetal"), MAX_int32, MAX_int32),
		MakeHubTestStack(TEXT("HubTestMetal"), MAX_int32, MAX_int32) });
	TestEqual(TEXT("Counting large totals saturates rather than overflows"), Grid->CountItem(TEXT("HubTestMetal")), MAX_int32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubInventoryTransactionTest, "RuinMates.Hub.Inventory.PreparedTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubInventoryTransactionTest::RunTest(const FString& Parameters)
{
	FHubInventoryFixture Fixture;
	URuinmatesBackpackComponent* Grid = Fixture.SpawnGrid();
	if (!TestTrue(TEXT("Transient inventory and database exist"), Grid && Fixture.Database))
	{
		return false;
	}
	int32 Notifications = 0;
	const FDelegateHandle Handle = Grid->OnChanged.AddLambda([&Notifications]() { ++Notifications; });
	const uint32 InitialRevision = Grid->GetRevision();
	TArray<FRuinmatesItemStack> Candidate = { MakeHubTestStack(TEXT("HubTestMetal"), 3) };
	Candidate[0].MaxStack = 999;
	Candidate[0].UnitValue = MAX_int32;
	Candidate[0].Quality = ERuinmatesItemQuality::Precious;
	TArray<FRuinmatesItemStack> Canonical;
	TestTrue(TEXT("Prepare validates without committing"), Grid->PrepareSlotsForTransaction(Candidate, InitialRevision, Canonical));
	TestEqual(TEXT("Prepare pads to the actual grid capacity"), Canonical.Num(), 10);
	TestEqual(TEXT("Canonical max stack comes from the database"), Canonical[0].MaxStack, 10);
	TestEqual(TEXT("Canonical value ignores incoming value"), Canonical[0].UnitValue, 17);
	TestEqual(TEXT("Canonical quality ignores incoming quality"), Canonical[0].Quality, ERuinmatesItemQuality::Fine);
	TestEqual(TEXT("Prepare never changes held items"), Grid->CountItem(TEXT("HubTestMetal")), 0);
	TestEqual(TEXT("Prepare never publishes"), Notifications, 0);
	TestTrue(TEXT("Prepare output may alias its input"), Grid->PrepareSlotsForTransaction(Canonical, InitialRevision, Canonical));
	TestTrue(TEXT("No-notify replacement stages canonical slots"), Grid->ReplaceSlotsForTransaction(Canonical, InitialRevision, false));
	TestEqual(TEXT("Staging keeps revision available for rollback"), Grid->GetRevision(), InitialRevision);
	TestEqual(TEXT("Staging emits no partial-transaction event"), Notifications, 0);
	TestEqual(TEXT("Staged inventory is present"), Grid->CountItem(TEXT("HubTestMetal")), 3);
	const TArray<FRuinmatesItemStack> Before = Grid->GetSlots();
	TMap<FName, int32> Costs;
	Costs.Add(TEXT("HubTestMetal"), 2);
	TestTrue(TEXT("Candidate materials can be deducted"), URuinmatesBackpackComponent::ConsumeItemsFromSlots(Canonical, Costs));
	TestTrue(TEXT("Second stage still uses the same revision"), Grid->ReplaceSlotsForTransaction(Canonical, InitialRevision, false));
	TestTrue(TEXT("A rejected outer action can restore the original slots at the same revision"), Grid->ReplaceSlotsForTransaction(Before, InitialRevision, false));
	TestEqual(TEXT("Rollback restores exact held count"), Grid->CountItem(TEXT("HubTestMetal")), 3);
	TestEqual(TEXT("Rollback emits no event"), Notifications, 0);
	Grid->PublishTransactionChange();
	TestEqual(TEXT("One publication advances revision once"), Grid->GetRevision(), InitialRevision + 1);
	TestEqual(TEXT("One publication emits one final event"), Notifications, 1);
	Grid->PublishTransactionChange();
	TestEqual(TEXT("Repeated publication does not advance again"), Grid->GetRevision(), InitialRevision + 1);
	TestEqual(TEXT("Repeated publication does not emit again"), Notifications, 1);
	TestFalse(TEXT("A stale transaction cannot overwrite published slots"), Grid->ReplaceSlotsForTransaction({}, InitialRevision));
	TestEqual(TEXT("Rejected stale transaction keeps held count"), Grid->CountItem(TEXT("HubTestMetal")), 3);

	const uint32 CurrentRevision = Grid->GetRevision();
	TArray<FRuinmatesItemStack> Sentinel = { MakeHubTestStack(TEXT("Sentinel"), 1) };
	const TArray<FRuinmatesItemStack> SentinelBefore = Sentinel;
	TArray<FRuinmatesItemStack> Invalid = { MakeHubTestStack(TEXT("UnknownHubItem"), 1) };
	Invalid[0].DisplayName = FText::FromString(TEXT("Test metal"));
	TestFalse(TEXT("Unknown IDs cannot pass by borrowing a valid display name"), Grid->PrepareSlotsForTransaction(Invalid, CurrentRevision, Sentinel));
	TestTrue(TEXT("Rejected prepare leaves caller output untouched"), EqualHubTestSlots(Sentinel, SentinelBefore));
	Invalid = { MakeHubTestStack(TEXT("HubTestMetal"), 11) };
	TestFalse(TEXT("Database stack limit is strict"), Grid->ReplaceSlotsForTransaction(Invalid, CurrentRevision));
	Invalid[0].Count = -1;
	TestFalse(TEXT("Negative counts are rejected"), Grid->ReplaceSlotsForTransaction(Invalid, CurrentRevision));
	Invalid[0].Count = 0;
	TestFalse(TEXT("Named zero-count slots are malformed"), Grid->ReplaceSlotsForTransaction(Invalid, CurrentRevision));
	Invalid = { MakeHubTestStack(NAME_None, 1) };
	TestFalse(TEXT("Count without ID is malformed"), Grid->ReplaceSlotsForTransaction(Invalid, CurrentRevision));
	Invalid.SetNum(11);
	TestFalse(TEXT("Oversized slot arrays are rejected without truncation"), Grid->ReplaceSlotsForTransaction(Invalid, CurrentRevision));
	Fixture.Database->SetTableOverride(nullptr);
	TestFalse(TEXT("Nonempty transactions fail closed without an item database"), Grid->ReplaceSlotsForTransaction(Before, CurrentRevision));
	Grid->GetOwner()->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Clients cannot prepare or replace even empty slot snapshots"), Grid->ReplaceSlotsForTransaction({}, CurrentRevision));
	Grid->PublishTransactionChange();
	TestEqual(TEXT("Rejected operations leave published revision unchanged"), Grid->GetRevision(), CurrentRevision);
	Grid->GetOwner()->SetRole(ROLE_Authority);
	Grid->OnChanged.Remove(Handle);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
