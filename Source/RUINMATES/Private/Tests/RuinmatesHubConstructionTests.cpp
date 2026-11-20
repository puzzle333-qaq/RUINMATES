// Actual manager/inventory integration, including durable candidate commits and failure rollback.
// Separate graphical/network and new-process runs cover map integration and process restart.
#include "Misc/AutomationTest.h"

#include "Character/RuinmatesCharacter.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Hub/RuinmatesHubDefinition.h"
#include "Hub/RuinmatesHubStateActor.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Save/RuinmatesSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Complete slot comparison ensures rejected actions cannot quietly consume a different item. */
	bool ConstructionSlotsEqual(const TArray<FRuinmatesItemStack>& A, const TArray<FRuinmatesItemStack>& B)
	{
		if (A.Num() != B.Num()) return false;
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].ItemId != B[Index].ItemId || A[Index].Count != B[Index].Count
				|| A[Index].Quality != B[Index].Quality || A[Index].MaxStack != B[Index].MaxStack
				|| A[Index].UnitValue != B[Index].UnitValue || A[Index].UseEffect != B[Index].UseEffect
				|| A[Index].UseAmount != B[Index].UseAmount || A[Index].bUpgradeMaterial != B[Index].bUpgradeMaterial
				|| A[Index].bRevealed != B[Index].bRevealed || !A[Index].DisplayName.EqualTo(B[Index].DisplayName)
				|| !A[Index].Description.EqualTo(B[Index].Description)) return false;
		}
		return true;
	}

	/** Isolated GI/transient item definitions; writes only its disposable GUID campaign profile. */
	struct FHubConstructionFixture
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		UDataTable* Table = nullptr;
		URuinmatesItemDatabaseSubsystem* Database = nullptr;
		ARuinmatesHubStateActor* Hub = nullptr;
		ARuinmatesCharacter* Character = nullptr;
		FString Profile;

		FHubConstructionFixture()
		{
			if (!GEngine) return;
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			Profile = TEXT("HubConstructionTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			if (auto* Save = GameInstance->GetSubsystem<URuinmatesSaveSubsystem>())
			{ Save->SaveSlotName = Profile; Save->ResetInMemoryCampaign(); }
			World = GameInstance->GetWorld();
			Database = GameInstance->GetSubsystem<URuinmatesItemDatabaseSubsystem>();
			if (!World || !Database) return;
			World->InitializeActorsForPlay(FURL());
			Table = NewObject<UDataTable>(GameInstance, NAME_None, RF_Transient);
			Table->RowStruct = FRuinmatesItemRow::StaticStruct();
			AddRow(TEXT("Hub_Scrap"), 50);
			AddRow(TEXT("Hub_Parts"), 50);
			AddRow(TEXT("Circuit"), 5);
			AddRow(TEXT("Hub_Generator"), 1);
			AddRow(TEXT("Hub_Fuel"), 10);
			Database->SetTableOverride(Table);
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Hub = World->SpawnActor<ARuinmatesHubStateActor>(Spawn);
			Character = World->SpawnActor<ARuinmatesCharacter>(Spawn);
			if (Hub)
			{
				Hub->Definition = NewObject<URuinmatesHubDefinition>(Hub, NAME_None, RF_Transient);
				Hub->Warehouse->SetCapacityOverride(96);
				Hub->InitializeDefaultState();
			}
		}

		~FHubConstructionFixture()
		{
			if (World) World->DestroyWorld(false);
			if (GameInstance) GameInstance->Shutdown();
			if (World && GEngine) GEngine->DestroyWorldContext(World);
			if (World) World->RemoveFromRoot();
			if (GameInstance) GameInstance->RemoveFromRoot();
			for (const FString& Suffix : {FString(TEXT("_Hub_A")), FString(TEXT("_Hub_B"))})
				UGameplayStatics::DeleteGameInSlot(Profile + Suffix, 0);
		}

		/** Preserves production IDs but only creates rows in this temporary fixture table. */
		void AddRow(FName Id, int32 MaxStack)
		{
			FRuinmatesItemRow Row;
			Row.DisplayName = FText::FromName(Id);
			Row.MaxStack = MaxStack;
			Row.bUpgradeMaterial = true;
			Table->AddRow(Id, Row);
		}

		bool IsReady() const
		{
			return Hub && Hub->Definition && Hub->Warehouse && Hub->IsHubInitialized()
				&& Character && Character->HasAuthority() && Character->GetBackpack() && Database;
		}

		/** Splits fixture quantities using the actual canonical row stack limits. */
		bool SetInventory(URuinmatesBackpackComponent* Inventory, const TMap<FName, int32>& Counts)
		{
			if (!Inventory || !Database) return false;
			TArray<FRuinmatesItemStack> Slots;
			for (const TPair<FName, int32>& Entry : Counts)
			{
				const FRuinmatesItemRow* Row = Database->FindItemDefinition(Entry.Key);
				if (!Row || Entry.Value < 0) return false;
				for (int32 Remaining = Entry.Value; Remaining > 0;)
				{
					const int32 Count = FMath::Min(Remaining, Row->MaxStack);
					Slots.Add(URuinmatesItemDatabaseSubsystem::CreateStackFromRow(*Row, Entry.Key, Count));
					Remaining -= Count;
				}
			}
			return Inventory->ReplaceSlotsForTransaction(Slots, Inventory->GetRevision());
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubStarterAndConstructionTest,
	"RuinMates.Hub.Construction.StarterExitAndAtomicUpgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubStarterAndConstructionTest::RunTest(const FString& Parameters)
{
	FHubConstructionFixture Fixture;
	if (!TestTrue(TEXT("Real manager, authority character and transient database are ready"), Fixture.IsReady())) return false;
	ARuinmatesHubStateActor* Hub = Fixture.Hub;
	URuinmatesBackpackComponent* Warehouse = Hub->Warehouse;
	FText Reason;
	const int32 BeforeClaim = Hub->GetSnapshot().Revision;
	TestTrue(TEXT("Starter grant succeeds through the manager"), Hub->TryClaimStarterKit(Fixture.Character, Reason));
	TestEqual(TEXT("Starter grant commits one construction revision"), Hub->GetSnapshot().Revision, BeforeClaim + 1);
	TestTrue(TEXT("Starter grant marks its one-time flag"), Hub->GetSnapshot().bStarterKitClaimed);
	TestEqual(TEXT("Starter scrap is actual warehouse stock"), Warehouse->CountItem(TEXT("Hub_Scrap")), 12);
	TestEqual(TEXT("Starter circuit six units split at five per stack"), Warehouse->CountItem(TEXT("Circuit")), 6);
	TestEqual(TEXT("Starter includes one generator item"), Warehouse->CountItem(TEXT("Hub_Generator")), 1);
	const TArray<FRuinmatesItemStack> Claimed = Warehouse->GetSlots();
	const uint32 ClaimWarehouseRevision = Warehouse->GetRevision();
	TestFalse(TEXT("Repeat starter claim is rejected"), Hub->TryClaimStarterKit(Fixture.Character, Reason));
	TestTrue(TEXT("Repeat starter claim changes no inventory"), ConstructionSlotsEqual(Claimed, Warehouse->GetSlots()));
	TestEqual(TEXT("Repeat starter claim changes no warehouse revision"), Warehouse->GetRevision(), ClaimWarehouseRevision);
	TestEqual(TEXT("Repeat starter claim changes no construction revision"), Hub->GetSnapshot().Revision, BeforeClaim + 1);

	TestFalse(TEXT("Initial generator supplies no electricity"), Hub->GetSnapshot().Generator.IsPowered());
	TestTrue(TEXT("Exit restoration quote has no power prerequisite"), Hub->GetActionQuote(Fixture.Character, TEXT("Hub.Exit.Restore")).bCanExecute);
	const int32 BeforeExit = Hub->GetSnapshot().Revision;
	TestTrue(TEXT("Exit can be restored before installing power"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Exit.Restore"), BeforeExit, Reason));
	FRuinmatesHubRoomState Exit;
	Hub->GetRoomState(RuinmatesHub::ExitId(), Exit);
	TestTrue(TEXT("Exit restoration permanently clears and unlocks the entrance"), Exit.Permanent.bCleared
		&& Exit.Permanent.bUnlocked && !Exit.Permanent.bStructureBlocked && Exit.Permanent.Level == 1);
	TestTrue(TEXT("Restored exit is operational without electricity"), Exit.Runtime.bOperational);
	TestEqual(TEXT("Exit consumes its exact two scrap"), Warehouse->CountItem(TEXT("Hub_Scrap")), 10);
	TestEqual(TEXT("Exit commits one construction revision"), Hub->GetSnapshot().Revision, BeforeExit + 1);
	const TArray<FRuinmatesItemStack> BeforeCoreClear = Warehouse->GetSlots();
	TestFalse(TEXT("Core upgrade cannot bypass its separate clearing action"), Hub->TryExecuteAction(Fixture.Character,
		TEXT("Hub.Core.Upgrade1"), Hub->GetSnapshot().Revision, Reason));
	TestTrue(TEXT("Rejected uncleared upgrade spends no materials"), ConstructionSlotsEqual(BeforeCoreClear, Warehouse->GetSlots()));
	TestTrue(TEXT("Core clearing succeeds as a separate paid action"), Hub->TryExecuteAction(Fixture.Character,
		TEXT("Hub.Core.Clear"), Hub->GetSnapshot().Revision, Reason));
	TestEqual(TEXT("Separate Core clearing costs three more scrap"), Warehouse->CountItem(TEXT("Hub_Scrap")), 7);

	const TArray<FRuinmatesItemStack> StockBeforeMissing = Warehouse->GetSlots();
	TArray<FRuinmatesItemStack> MissingCircuit = StockBeforeMissing;
	for (FRuinmatesItemStack& Stack : MissingCircuit)
	{
		if (Stack.ItemId == FName(TEXT("Circuit"))) Stack = FRuinmatesItemStack();
	}
	TestTrue(TEXT("Fixture removes only circuit to create a compound-cost shortage"), Warehouse->ReplaceSlotsForTransaction(MissingCircuit, Warehouse->GetRevision()));
	const TArray<FRuinmatesItemStack> BeforeRejected = Warehouse->GetSlots();
	const int32 BeforeRejectedRevision = Hub->GetSnapshot().Revision;
	const uint32 BeforeRejectedWarehouseRevision = Warehouse->GetRevision();
	TestFalse(TEXT("Core upgrade quote reports the missing material"), Hub->GetActionQuote(Fixture.Character, TEXT("Hub.Core.Upgrade1")).bCanExecute);
	TestFalse(TEXT("One missing compound ingredient rejects Core upgrade"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Core.Upgrade1"), BeforeRejectedRevision, Reason));
	TestTrue(TEXT("Compound-cost rejection consumes none of the available scrap"), ConstructionSlotsEqual(BeforeRejected, Warehouse->GetSlots()));
	TestEqual(TEXT("Compound-cost rejection keeps inventory revision"), Warehouse->GetRevision(), BeforeRejectedWarehouseRevision);
	TestEqual(TEXT("Compound-cost rejection keeps construction revision"), Hub->GetSnapshot().Revision, BeforeRejectedRevision);
	FRuinmatesHubRoomState Core;
	Hub->GetRoomState(RuinmatesHub::CoreId(), Core);
	TestEqual(TEXT("Rejected upgrade does not grant a level"), Core.Permanent.Level, 0);

	TestTrue(TEXT("Fixture restores the exact pre-shortage warehouse"), Warehouse->ReplaceSlotsForTransaction(StockBeforeMissing, Warehouse->GetRevision()));
	const int32 BeforeUpgrade = Hub->GetSnapshot().Revision;
	const uint32 BeforeUpgradeWarehouse = Warehouse->GetRevision();
	int32 WarehouseEvents = 0;
	bool bFinalStateObserved = false;
	const FDelegateHandle Handle = Warehouse->OnChanged.AddLambda([&]()
	{
		++WarehouseEvents;
		FRuinmatesHubRoomState CurrentCore;
		Hub->GetRoomState(RuinmatesHub::CoreId(), CurrentCore);
		bFinalStateObserved = CurrentCore.Permanent.Level == 1 && Warehouse->CountItem(TEXT("Hub_Scrap")) == 2
			&& Warehouse->CountItem(TEXT("Circuit")) == 4;
	});
	TestTrue(TEXT("Affordable Core upgrade succeeds"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Core.Upgrade1"), BeforeUpgrade, Reason));
	Warehouse->OnChanged.Remove(Handle);
	Hub->GetRoomState(RuinmatesHub::CoreId(), Core);
	TestEqual(TEXT("Successful upgrade changes Core level"), Core.Permanent.Level, 1);
	TestEqual(TEXT("Successful action commits one construction revision"), Hub->GetSnapshot().Revision, BeforeUpgrade + 1);
	TestEqual(TEXT("Successful action commits one warehouse revision"), Warehouse->GetRevision(), BeforeUpgradeWarehouse + 1);
	TestEqual(TEXT("Successful action emits one warehouse event"), WarehouseEvents, 1);
	TestTrue(TEXT("Inventory notification observes the complete final construction state"), bFinalStateObserved);
	TestEqual(TEXT("Exit, clearing and upgrade leave exactly two starter scrap"), Warehouse->CountItem(TEXT("Hub_Scrap")), 2);

	const FRuinmatesItemStack OneScrap = Fixture.Database->CreateItemStack(TEXT("Hub_Scrap"), 1);
	TestEqual(TEXT("Fixture adds one unit to make a fresh clear affordable"), Warehouse->TryAdd(OneScrap), 1);
	TestTrue(TEXT("The stale test action is otherwise currently affordable"), Hub->GetActionQuote(Fixture.Character, TEXT("Hub.Medical.Clear")).bCanExecute);
	const TArray<FRuinmatesItemStack> BeforeStale = Warehouse->GetSlots();
	TestFalse(TEXT("Stale construction intent cannot execute another action"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Medical.Clear"), BeforeUpgrade, Reason));
	TestTrue(TEXT("Stale intent consumes zero materials"), ConstructionSlotsEqual(BeforeStale, Warehouse->GetSlots()));
	TestEqual(TEXT("Stale intent does not advance construction revision"), Hub->GetSnapshot().Revision, BeforeUpgrade + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubMedicalPowerProgressionTest,
	"RuinMates.Hub.Construction.MedicalPowerProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubMedicalPowerProgressionTest::RunTest(const FString& Parameters)
{
	FHubConstructionFixture Fixture;
	if (!TestTrue(TEXT("Manager integration fixture is ready"), Fixture.IsReady())) return false;
	ARuinmatesHubStateActor* Hub = Fixture.Hub;
	URuinmatesBackpackComponent* Warehouse = Hub->Warehouse;
	FText Reason;
	TestTrue(TEXT("Starter materials are granted"), Hub->TryClaimStarterKit(Fixture.Character, Reason));
	TestTrue(TEXT("Core has its own paid clearing prerequisite"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Core.Clear"), Hub->GetSnapshot().Revision, Reason));
	TestTrue(TEXT("Core prerequisite is repaired without electricity"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Core.Upgrade1"), Hub->GetSnapshot().Revision, Reason));
	const TArray<FRuinmatesItemStack> BeforeBlocked = Warehouse->GetSlots();
	const int32 BeforeBlockedRevision = Hub->GetSnapshot().Revision;
	TestFalse(TEXT("A structurally blocked medical room cannot unlock"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Medical.Unlock"), BeforeBlockedRevision, Reason));
	TestTrue(TEXT("Blocked unlock costs nothing"), ConstructionSlotsEqual(BeforeBlocked, Warehouse->GetSlots()));
	TestTrue(TEXT("Separate medical clearing action succeeds"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Medical.Clear"), Hub->GetSnapshot().Revision, Reason));
	FRuinmatesHubRoomState Medical;
	Hub->GetRoomState(RuinmatesHub::MedicalId(), Medical);
	TestTrue(TEXT("Clearing removes permanent blocking but does not unlock"), Medical.Permanent.bCleared
		&& !Medical.Permanent.bStructureBlocked && !Medical.Permanent.bUnlocked && Medical.Permanent.Level == 0);
	const TArray<FRuinmatesItemStack> BeforePowerMissing = Warehouse->GetSlots();
	TestFalse(TEXT("Cleared room still cannot unlock without power"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Medical.Unlock"), Hub->GetSnapshot().Revision, Reason));
	TestTrue(TEXT("Missing power consumes no unlock materials"), ConstructionSlotsEqual(BeforePowerMissing, Warehouse->GetSlots()));

	TestTrue(TEXT("Installation consumes the configured generator item"), Hub->TryInstallGenerator(Fixture.Character, Hub->GetSnapshot().Revision, Reason));
	TestEqual(TEXT("Installed generator item is removed"), Warehouse->CountItem(TEXT("Hub_Generator")), 0);
	TestTrue(TEXT("Generator installation is permanent"), Hub->GetSnapshot().Generator.bInstalled);
	TestFalse(TEXT("Installed hardware alone supplies no power"), Hub->GetSnapshot().Generator.IsPowered());
	const int32 BeforeDuplicateInstall = Hub->GetSnapshot().Revision;
	TestFalse(TEXT("Repeated installation is rejected"), Hub->TryInstallGenerator(Fixture.Character, BeforeDuplicateInstall, Reason));
	TestEqual(TEXT("Repeated installation does not advance revision"), Hub->GetSnapshot().Revision, BeforeDuplicateInstall);
	TestFalse(TEXT("Empty generator cannot start"), Hub->TrySetGeneratorStarted(Fixture.Character, true, Hub->GetSnapshot().Revision, Reason));
	TestTrue(TEXT("One whole fuel item can be added"), Hub->TryAddFuel(Fixture.Character, Hub->GetSnapshot().Revision, Reason));
	TestEqual(TEXT("One fuel item was consumed exactly"), Warehouse->CountItem(TEXT("Hub_Fuel")), 2);
	TestEqual(TEXT("Fuel conversion uses definition units"), Hub->GetSnapshot().Generator.Fuel, Hub->Definition->UnitsPerFuel);
	TestFalse(TEXT("Fuel alone does not turn on the generator"), Hub->GetSnapshot().Generator.IsPowered());
	TestTrue(TEXT("Fueled installed generator starts"), Hub->TrySetGeneratorStarted(Fixture.Character, true, Hub->GetSnapshot().Revision, Reason));
	TestTrue(TEXT("Started installed fueled generator supplies the base"), Hub->GetSnapshot().Generator.IsPowered());
	TestTrue(TEXT("Cleared powered room can unlock"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Medical.Unlock"), Hub->GetSnapshot().Revision, Reason));
	Hub->GetRoomState(RuinmatesHub::MedicalId(), Medical);
	TestTrue(TEXT("Medical unlock is permanent and service is operating"), Medical.Permanent.bUnlocked
		&& Medical.Permanent.Level == 1 && Medical.Runtime.bPowered && Medical.Runtime.bOperational);
	TestEqual(TEXT("Medical unlock charges the exact parts"), Warehouse->CountItem(TEXT("Hub_Parts")), 6);
	TestEqual(TEXT("Medical unlock charges the exact circuit"), Warehouse->CountItem(TEXT("Circuit")), 3);
	const FRuinmatesHubPermanentState RepairedMedical = Medical.Permanent;
	FRuinmatesHubRoomState Core;
	Hub->GetRoomState(RuinmatesHub::CoreId(), Core);
	const FRuinmatesHubPermanentState RepairedCore = Core.Permanent;
	const TArray<FRuinmatesItemStack> BeforeShutdown = Warehouse->GetSlots();
	const float BeforeShutdownFuel = Hub->GetSnapshot().Generator.Fuel;
	TestTrue(TEXT("Generator can stop through its real authority service"), Hub->TrySetGeneratorStarted(Fixture.Character, false, Hub->GetSnapshot().Revision, Reason));
	Hub->GetRoomState(RuinmatesHub::MedicalId(), Medical);
	Hub->GetRoomState(RuinmatesHub::CoreId(), Core);
	TestFalse(TEXT("Shutdown removes actual base power"), Hub->GetSnapshot().Generator.IsPowered());
	TestFalse(TEXT("Unpowered medical service is disabled"), Medical.Runtime.bOperational);
	TestTrue(TEXT("Shutdown preserves every permanent medical fact"), Medical.Permanent == RepairedMedical);
	TestTrue(TEXT("Shutdown preserves permanent Core repair"), Core.Permanent == RepairedCore);
	TestTrue(TEXT("Shutdown consumes no construction items"), ConstructionSlotsEqual(BeforeShutdown, Warehouse->GetSlots()));
	TestEqual(TEXT("Explicit shutdown does not burn fuel"), Hub->GetSnapshot().Generator.Fuel, BeforeShutdownFuel);
	TestTrue(TEXT("Only completed first floor is open"), Hub->GetSnapshot().OpenFloors.Num() == 1 && Hub->GetSnapshot().OpenFloors[0] == 1);
	TestTrue(TEXT("Missing exploration destination remains explicitly unconfigured"), Hub->Definition->DestinationMap.IsNull());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubResourceSourceTest,
	"RuinMates.Hub.Construction.WarehouseThenOwnBackpack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubResourceSourceTest::RunTest(const FString& Parameters)
{
	FHubConstructionFixture Fixture;
	if (!TestTrue(TEXT("Resource policy fixture is ready"), Fixture.IsReady())) return false;
	ARuinmatesHubStateActor* Hub = Fixture.Hub;
	URuinmatesBackpackComponent* Warehouse = Hub->Warehouse;
	URuinmatesBackpackComponent* Backpack = Fixture.Character->GetBackpack();
	FRuinmatesHubRoomState PreparedCore;
	Hub->GetRoomState(RuinmatesHub::CoreId(), PreparedCore);
	PreparedCore.Permanent.bCleared = true;
	PreparedCore.Permanent.bStructureBlocked = false;
	TestTrue(TEXT("Fixture explicitly prepares Core clearing independently of source-policy test"), Hub->CommitRoomState(PreparedCore));
	TestTrue(TEXT("Fixture fills the real shared warehouse"), Fixture.SetInventory(Warehouse, {{TEXT("Hub_Scrap"),2},{TEXT("Circuit"),1}}));
	TestTrue(TEXT("Fixture fills the initiating character inventory"), Fixture.SetInventory(Backpack, {{TEXT("Hub_Scrap"),4},{TEXT("Circuit"),1},{TEXT("Hub_Parts"),2}}));
	const TArray<FRuinmatesItemStack> BeforeWarehouse = Warehouse->GetSlots(), BeforeBackpack = Backpack->GetSlots();
	FText Reason;
	const int32 Before = Hub->GetSnapshot().Revision;
	TestFalse(TEXT("Warehouse-only policy does not silently spend personal stock"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Core.Upgrade1"), Before, Reason));
	TestTrue(TEXT("Policy rejection leaves both sources unchanged"), ConstructionSlotsEqual(BeforeWarehouse, Warehouse->GetSlots())
		&& ConstructionSlotsEqual(BeforeBackpack, Backpack->GetSlots()));
	Hub->Definition->ResourcePolicy = ERuinmatesHubResourcePolicy::WarehouseThenBackpack;
	const FRuinmatesHubActionQuote Quote = Hub->GetActionQuote(Fixture.Character, TEXT("Hub.Core.Upgrade1"));
	TestTrue(TEXT("Combined eligible sources make the quote affordable"), Quote.bCanExecute);
	TestEqual(TEXT("Quote includes exactly the eligible scrap"), Quote.Available.FindRef(TEXT("Hub_Scrap")), 6);
	TestEqual(TEXT("Quote includes exactly the eligible circuits"), Quote.Available.FindRef(TEXT("Circuit")), 2);
	const uint32 WarehouseRevision = Warehouse->GetRevision(), BackpackRevision = Backpack->GetRevision();
	TestTrue(TEXT("Manager atomically consumes the compound bill across both inventories"), Hub->TryExecuteAction(Fixture.Character, TEXT("Hub.Core.Upgrade1"), Before, Reason));
	TestEqual(TEXT("Warehouse scrap is consumed first"), Warehouse->CountItem(TEXT("Hub_Scrap")), 0);
	TestEqual(TEXT("Only the remaining three scrap are taken from backpack"), Backpack->CountItem(TEXT("Hub_Scrap")), 1);
	TestEqual(TEXT("Warehouse circuit consumed exactly"), Warehouse->CountItem(TEXT("Circuit")), 0);
	TestEqual(TEXT("Backpack circuit consumed exactly"), Backpack->CountItem(TEXT("Circuit")), 0);
	TestEqual(TEXT("Unrelated personal materials remain"), Backpack->CountItem(TEXT("Hub_Parts")), 2);
	TestEqual(TEXT("Combined action publishes warehouse once"), Warehouse->GetRevision(), WarehouseRevision + 1);
	TestEqual(TEXT("Combined action publishes backpack once"), Backpack->GetRevision(), BackpackRevision + 1);
	TestEqual(TEXT("Combined action advances construction once"), Hub->GetSnapshot().Revision, Before + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubInvalidConstructionConfigTest,
	"RuinMates.Hub.Construction.InvalidConfigurationRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubInvalidConstructionConfigTest::RunTest(const FString& Parameters)
{
	FHubConstructionFixture Fixture;
	if (!TestTrue(TEXT("Invalid configuration fixture is ready"), Fixture.IsReady())) return false;
	ARuinmatesHubStateActor* Hub = Fixture.Hub;
	URuinmatesBackpackComponent* Warehouse = Hub->Warehouse;
	FText Reason;
	TestTrue(TEXT("Starter kit provides otherwise affordable configured actions"), Hub->TryClaimStarterKit(Fixture.Character, Reason));
	TestTrue(TEXT("Core clearing prerequisite is satisfied through the real action"), Hub->TryExecuteAction(Fixture.Character,
		TEXT("Hub.Core.Clear"), Hub->GetSnapshot().Revision, Reason));
	FHubActionDefinition* Upgrade = Hub->Definition->Actions.FindByPredicate([](const FHubActionDefinition& Action)
	{
		return Action.ActionId == FName(TEXT("Hub.Core.Upgrade1"));
	});
	if (!TestNotNull(TEXT("Default Core upgrade exists"), Upgrade)) return false;
	const int32 ValidTarget = Upgrade->TargetLevel;
	Upgrade->TargetLevel = 3;
	const TArray<FRuinmatesItemStack> BeforeInvalidTarget = Warehouse->GetSlots();
	const int32 BeforeInvalidRevision = Hub->GetSnapshot().Revision;
	const uint32 BeforeInvalidInventoryRevision = Warehouse->GetRevision();
	TestFalse(TEXT("Quote rejects a configuration that targets Core level three"), Hub->GetActionQuote(Fixture.Character, Upgrade->ActionId).bCanExecute);
	TestFalse(TEXT("Authority refuses an out-of-scope target level"), Hub->TryExecuteAction(Fixture.Character,
		Upgrade->ActionId, BeforeInvalidRevision, Reason));
	TestTrue(TEXT("Invalid target configuration consumes no item"), ConstructionSlotsEqual(BeforeInvalidTarget, Warehouse->GetSlots()));
	TestEqual(TEXT("Invalid target configuration does not advance inventory revision"), Warehouse->GetRevision(), BeforeInvalidInventoryRevision);
	TestEqual(TEXT("Invalid target configuration does not advance construction revision"), Hub->GetSnapshot().Revision, BeforeInvalidRevision);
	FRuinmatesHubRoomState Core;
	Hub->GetRoomState(RuinmatesHub::CoreId(), Core);
	TestEqual(TEXT("Invalid target configuration cannot create level three"), Core.Permanent.Level, 0);
	Upgrade->TargetLevel = ValidTarget;
	TestTrue(TEXT("Restoring valid configuration restores the quote"), Hub->GetActionQuote(Fixture.Character, Upgrade->ActionId).bCanExecute);

	TestTrue(TEXT("Real generator installation prepares fuel validation"), Hub->TryInstallGenerator(Fixture.Character,
		Hub->GetSnapshot().Revision, Reason));
	const TArray<FRuinmatesItemStack> BeforeInvalidFuel = Warehouse->GetSlots();
	const int32 BeforeFuelRevision = Hub->GetSnapshot().Revision;
	const uint32 BeforeFuelInventoryRevision = Warehouse->GetRevision();
	const float ValidUnits = Hub->Definition->UnitsPerFuel;
	Hub->Definition->UnitsPerFuel = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN fuel conversion is rejected by authority"), Hub->TryAddFuel(Fixture.Character,
		BeforeFuelRevision, Reason));
	Hub->Definition->UnitsPerFuel = ValidUnits;
	const float ValidCapacity = Hub->Definition->FuelCapacity;
	Hub->Definition->FuelCapacity = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN maximum capacity is rejected by authority"), Hub->TryAddFuel(Fixture.Character,
		BeforeFuelRevision, Reason));
	Hub->Definition->FuelCapacity = ValidCapacity;
	TestTrue(TEXT("Invalid fuel configuration consumes no fuel item"), ConstructionSlotsEqual(BeforeInvalidFuel, Warehouse->GetSlots()));
	TestEqual(TEXT("Invalid fuel configuration advances no inventory revision"), Warehouse->GetRevision(), BeforeFuelInventoryRevision);
	TestEqual(TEXT("Invalid fuel configuration advances no construction revision"), Hub->GetSnapshot().Revision, BeforeFuelRevision);
	TestTrue(TEXT("Invalid fuel configuration leaves runtime fuel finite"), FMath::IsFinite(Hub->GetSnapshot().Generator.Fuel));
	TestEqual(TEXT("Invalid fuel configuration leaves runtime fuel unchanged"), Hub->GetSnapshot().Generator.Fuel, 0.f);
	TestTrue(TEXT("Valid fuel addition still works after rejected configuration"), Hub->TryAddFuel(Fixture.Character,
		BeforeFuelRevision, Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubManagerRollbackTest, "RuinMates.Hub.Save.ManagerRollbackAndReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubManagerRollbackTest::RunTest(const FString& Parameters)
{
	FHubConstructionFixture Fixture;
	if (!TestTrue(TEXT("Isolated manager fixture"), Fixture.IsReady())) return false;
	auto* Save = Fixture.GameInstance->GetSubsystem<URuinmatesSaveSubsystem>();
	FText Reason;
	if (!TestTrue(TEXT("One-time kit is durably committed"), Fixture.Hub->TryClaimStarterKit(Fixture.Character, Reason))) return false;
	const auto Before = Fixture.Hub->Warehouse->GetSlots();
	const int32 Revision = Fixture.Hub->GetSnapshot().Revision;
	const uint32 InventoryRevision = Fixture.Hub->Warehouse->GetRevision();
	Save->bFailNextHubSaveForTest = true;
	TestFalse(TEXT("Disk failure rejects actual paid restore"), Fixture.Hub->TryExecuteAction(Fixture.Character,
		TEXT("Hub.Exit.Restore"), Revision, Reason));
	TestTrue(TEXT("Failed save rolls back every material"), ConstructionSlotsEqual(Before, Fixture.Hub->Warehouse->GetSlots()));
	TestEqual(TEXT("Failed save leaves state revision unchanged"), Fixture.Hub->GetSnapshot().Revision, Revision);
	TestEqual(TEXT("Failed save emits no inventory revision"), Fixture.Hub->Warehouse->GetRevision(), InventoryRevision);
	FRuinmatesHubRoomState Exit;
	Fixture.Hub->GetRoomState(RuinmatesHub::ExitId(),Exit);
	TestFalse(TEXT("Failed save leaves exit locked"), Exit.Permanent.bUnlocked);
	TestTrue(TEXT("Retry succeeds once"), Fixture.Hub->TryExecuteAction(Fixture.Character,TEXT("Hub.Exit.Restore"),Revision,Reason));
	Save->ResetInMemoryCampaign();
	TestTrue(TEXT("Disk reload succeeds"), Save->LoadCampaign());
	FRuinmatesHubSnapshot Loaded;
	TArray<FRuinmatesItemStack> LoadedSlots;
	TestTrue(TEXT("Reload contains completed Hub"), Save->GetSavedHubState(Loaded,LoadedSlots));
	TestTrue(TEXT("Claim flag survives reload"), Loaded.bStarterKitClaimed);
	const auto* LoadedExit = Loaded.Rooms.FindByPredicate([](const auto& R) { return R.RoomId==RuinmatesHub::ExitId(); });
	TestTrue(TEXT("Exit restoration survives reload"), LoadedExit && LoadedExit->Permanent.bUnlocked);
	TestTrue(TEXT("Exact consumed warehouse persists"), ConstructionSlotsEqual(Fixture.Hub->Warehouse->GetSlots(),LoadedSlots));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
