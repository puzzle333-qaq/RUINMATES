#include "Misc/AutomationTest.h"

#include "Character/RuinmatesCharacter.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/RuinmatesPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Save/RuinmatesSaveSubsystem.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Complete, valid durable Hub fixture; deliberately sets runtime to verify that it is not saved. */
	FRuinmatesHubSnapshot MakeSavedHubFixture()
	{
		FRuinmatesHubSnapshot Hub;
		Hub.bInitialized = true;
		Hub.Revision = 8;
		Hub.bStarterKitClaimed = true;
		Hub.Generator.bInstalled = true;
		Hub.Generator.bStarted = true;
		Hub.Generator.Fuel = 20.f;
		for (FName Id : { RuinmatesHub::CoreId(), RuinmatesHub::MedicalId(), RuinmatesHub::WorkshopId(),
			RuinmatesHub::ElevatorId(), RuinmatesHub::BlackMarketId(), RuinmatesHub::ExitId() })
		{
			FRuinmatesHubRoomState& Room = Hub.Rooms.AddDefaulted_GetRef();
			Room.RoomId = Id;
			Room.Runtime.bPowered = true;
			if (Id == RuinmatesHub::CoreId() || Id == RuinmatesHub::MedicalId())
			{
				Room.Permanent.Level = 1;
				Room.Permanent.bCleared = true;
				Room.Permanent.bUnlocked = true;
				Room.Permanent.bStructureBlocked = false;
				Room.Runtime.bOperational = true;
			}
		}
		return Hub;
	}

	/** All disk writes use a GUID profile. The normal campaign is only read by startup initialization. */
	struct FHubSaveFixture
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		URuinmatesSaveSubsystem* Save = nullptr;
		URuinmatesItemDatabaseSubsystem* Database = nullptr;
		FString Slot;
		TArray<FString> ExtraProfiles;

		FHubSaveFixture()
		{
			Slot = TEXT("RuinMatesHubSaveTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			if (!GEngine) return;
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			Save = GameInstance->GetSubsystem<URuinmatesSaveSubsystem>();
			Database = GameInstance->GetSubsystem<URuinmatesItemDatabaseSubsystem>();
			if (Save)
			{
				Save->SaveSlotName = Slot;
				Save->ResetInMemoryCampaign();
			}
			if (World) World->InitializeActorsForPlay(FURL());
			UDataTable* Table = NewObject<UDataTable>(GameInstance, NAME_None, RF_Transient);
			Table->RowStruct = FRuinmatesItemRow::StaticStruct();
			FRuinmatesItemRow Scrap;
			Scrap.DisplayName = FText::FromString(TEXT("Transient scrap"));
			Scrap.MaxStack = 50;
			Scrap.SellPrice = 9;
			Table->AddRow(TEXT("Hub_Scrap"), Scrap);
			FRuinmatesItemRow Circuit = Scrap;
			Circuit.DisplayName = FText::FromString(TEXT("Transient circuit"));
			Circuit.MaxStack = 5;
			Table->AddRow(TEXT("Circuit"), Circuit);
			if (Database) Database->SetTableOverride(Table);
		}

		~FHubSaveFixture()
		{
			if (World) World->DestroyWorld(false);
			if (GameInstance) GameInstance->Shutdown();
			if (World && GEngine) GEngine->DestroyWorldContext(World);
			if (World) World->RemoveFromRoot();
			if (GameInstance) GameInstance->RemoveFromRoot();
			ExtraProfiles.Add(Slot);
			for (const FString& Profile : ExtraProfiles)
			{
				for (const FString& Suffix : { FString(), FString(TEXT("_Hub_A")), FString(TEXT("_Hub_B")) })
				{
					UGameplayStatics::DeleteGameInSlot(Profile + Suffix, 0);
				}
			}
		}

		bool IsReady() const { return World && Save && Database; }

		TArray<FRuinmatesItemStack> Warehouse() const
		{
			TArray<FRuinmatesItemStack> Slots;
			Slots.SetNum(96);
			Slots[0] = Database->CreateItemStack(TEXT("Hub_Scrap"), 7);
			Slots[1] = Database->CreateItemStack(TEXT("Circuit"), 5);
			Slots[2] = Database->CreateItemStack(TEXT("Circuit"), 1);
			return Slots;
		}

		/** Possessed authority character has a genuine PlayerState identity and backpack to capture. */
		ARuinmatesCharacter* SpawnMember(ARuinmatesPlayerState*& OutPlayerState)
		{
			FActorSpawnParameters Parameters;
			Parameters.ObjectFlags |= RF_Transient;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			APlayerController* PC = World->SpawnActor<APlayerController>(Parameters);
			OutPlayerState = World->SpawnActor<ARuinmatesPlayerState>(Parameters);
			ARuinmatesCharacter* Character = World->SpawnActor<ARuinmatesCharacter>(Parameters);
			if (!PC || !OutPlayerState || !Character) return nullptr;
			auto* LocalPlayer=NewObject<ULocalPlayer>(GEngine);
			PC->SetPlayer(LocalPlayer);
			PC->SetPlayerState(OutPlayerState);
			OutPlayerState->SetOwner(PC);
			PC->Possess(Character);
			return Character;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubSaveGenerationsTest, "RuinMates.Hub.Save.LegacyAndGenerationRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubSaveGenerationsTest::RunTest(const FString& Parameters)
{
	FHubSaveFixture Fixture;
	if (!TestTrue(TEXT("Isolated save and database are ready"), Fixture.IsReady())) return false;
	URuinmatesSaveGame* Legacy = NewObject<URuinmatesSaveGame>();
	Legacy->SharedFunds = 741;
	Legacy->FindOrAddMember(TEXT("legacy-guest")).Level = 4;
	TestEqual(TEXT("Pre-versioned save field defaults to zero"), Legacy->SaveVersion, 0);
	TestTrue(TEXT("Legacy fixture writes only its GUID profile"), UGameplayStatics::SaveGameToSlot(Legacy, Fixture.Slot, 0));
	TestTrue(TEXT("Loader accepts the legacy campaign"), Fixture.Save->LoadCampaign());
	TestEqual(TEXT("Legacy money survives schema upgrade"), Fixture.Save->GetSharedFunds(), 741);
	FRuinmatesHubSnapshot Absent;
	TArray<FRuinmatesItemStack> AbsentWarehouse;
	TestFalse(TEXT("Legacy campaign does not pretend to contain a saved Hub"), Fixture.Save->GetSavedHubState(Absent, AbsentWarehouse));
	ARuinmatesPlayerState* PS = nullptr;
	ARuinmatesCharacter* Character = Fixture.SpawnMember(PS);
	if (!TestTrue(TEXT("Authority host and PlayerState exist"), Character && PS)) return false;
	PS->SetStableMemberId(TEXT("session-generated-guid"));
	Fixture.Save->RestoreOrAssignMember(PS);
	TestEqual(TEXT("Offline local host uses the stable profile identity"), PS->GetStableMemberId(), FString(TEXT("LocalHost")));
	PS->SetCampaignLevel(3);
	PS->SetTalentIds({ TEXT("Salvager") });
	URuinmatesBackpackComponent* Backpack = Character->GetBackpack();
	TestTrue(TEXT("Personal capacity override can be captured"), Backpack->SetCapacityOverride(15));
	TestTrue(TEXT("Real backpack contains canonical existing items"), Backpack->ReplaceSlotsForTransaction(
		{ Fixture.Database->CreateItemStack(TEXT("Hub_Scrap"), 3) }, Backpack->GetRevision()));
	const FRuinmatesHubSnapshot Hub = MakeSavedHubFixture();
	TestTrue(TEXT("Whole Hub, inventory and member candidate is durably persisted"), Fixture.Save->PersistHubState(Hub, Fixture.Warehouse(), Fixture.World));
	const URuinmatesSaveGame* Saved = Fixture.Save->GetCampaignSnapshot();
	TestEqual(TEXT("First generation is one"), Saved->SaveGeneration, int64(1));
	TestEqual(TEXT("Version is explicitly stamped on new generation"), Saved->SaveVersion, URuinmatesSaveGame::CurrentSaveVersion);
	TestEqual(TEXT("Existing funds are preserved by Hub save"), Saved->SharedFunds, 741);
	const FRuinmatesMemberSave* Member = Saved->Members.FindByPredicate([](const FRuinmatesMemberSave& Entry)
	{
		return Entry.StableId == TEXT("LocalHost");
	});
	if (!TestNotNull(TEXT("Host member is keyed by stable profile identity"), Member)) return false;
	TestTrue(TEXT("Backpack data is explicitly present"), Member->bHasBackpack);
	TestEqual(TEXT("Personal backpack tier is retained"), Member->BackpackTier, ERuinmatesBackpackTier::Issue);
	TestEqual(TEXT("Personal capacity override is retained"), Member->BackpackCapacityOverride, 15);
	TestEqual(TEXT("Personal slots retain all three units"), Member->BackpackStacks[0].Count, 3);
	Backpack->RestoreSavedInventory(ERuinmatesBackpackTier::Issue, 0, {});
	TestTrue(TEXT("Saved inventory restores onto a newly empty member grid"), Fixture.Save->RestoreBackpack(Character));
	TestEqual(TEXT("Restored personal grid capacity is correct"), Backpack->GetCapacity(),15);
	TestEqual(TEXT("Restored personal items are canonical"),Backpack->CountItem(TEXT("Hub_Scrap")),3);
	TestTrue(TEXT("Old guest progress is retained in complete candidate"), Saved->Members.ContainsByPredicate([](const FRuinmatesMemberSave& Entry)
	{
		return Entry.StableId == TEXT("legacy-guest") && Entry.Level == 4;
	}));
	FRuinmatesHubSnapshot LoadedHub;
	TArray<FRuinmatesItemStack> LoadedWarehouse;
	TestTrue(TEXT("Durable Hub state can be queried"), Fixture.Save->GetSavedHubState(LoadedHub, LoadedWarehouse));
	TestFalse(TEXT("Loaded state remains gated until StateActor recomputes"), LoadedHub.bInitialized);
	for (const FRuinmatesHubRoomState& Room : LoadedHub.Rooms)
	{
		TestFalse(TEXT("Temporary power is never persisted per room"), Room.Runtime.bPowered);
		TestFalse(TEXT("Temporary operational state is never persisted"), Room.Runtime.bOperational);
	}
	TestTrue(TEXT("Installed running hardware is retained independently"), LoadedHub.Generator.bInstalled && LoadedHub.Generator.bStarted);
	TestEqual(TEXT("Fuel is retained"), LoadedHub.Generator.Fuel, 20.f);
	TestTrue(TEXT("Opened first-floor data is retained"), LoadedHub.OpenFloors == TArray<int32>({1}));
	TestTrue(TEXT("Starter claim is retained"), LoadedHub.bStarterKitClaimed);
	TestEqual(TEXT("Warehouse slots retain real capacity"), LoadedWarehouse.Num(), 96);
	TestEqual(TEXT("Warehouse separate circuit stacks retain counts"), LoadedWarehouse[1].Count + LoadedWarehouse[2].Count, 6);
	Fixture.Save->SetSharedFunds(999);
	TestTrue(TEXT("Existing SaveCampaign uses the same generation channel"), Fixture.Save->SaveCampaign());
	TestEqual(TEXT("Second full save advances generation"), Fixture.Save->GetCampaignSnapshot()->SaveGeneration, int64(2));
	TestTrue(TEXT("First generation remains on disk"), UGameplayStatics::DoesSaveGameExist(Fixture.Slot + TEXT("_Hub_A"), 0));
	TestTrue(TEXT("Second generation is written separately"), UGameplayStatics::DoesSaveGameExist(Fixture.Slot + TEXT("_Hub_B"), 0));
	Fixture.Save->ResetInMemoryCampaign();
	TestTrue(TEXT("Reload selects the highest valid generation"), Fixture.Save->LoadCampaign());
	TestEqual(TEXT("Reload preserves latest complete funds"), Fixture.Save->GetSharedFunds(), 999);
	TestEqual(TEXT("Reload preserves latest generation"), Fixture.Save->GetCampaignSnapshot()->SaveGeneration, int64(2));
	URuinmatesSaveGame* LegacyAfter = Cast<URuinmatesSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0));
	TestTrue(TEXT("Legacy slot is never overwritten by new saves"), LegacyAfter && LegacyAfter->SaveVersion == 0 && LegacyAfter->SharedFunds == 741);
	PS->SetStableMemberId(TEXT("different-session-guid"));
	PS->SetCampaignLevel(1);
	Fixture.Save->RestoreOrAssignMember(PS);
	TestEqual(TEXT("Offline identity resolves after a new session GUID"), PS->GetStableMemberId(), FString(TEXT("LocalHost")));
	TestEqual(TEXT("Stable offline profile restores member progress"), PS->GetCampaignLevel(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubSaveFailureTest, "RuinMates.Hub.Save.FailedCandidateKeepsGoodSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubSaveFailureTest::RunTest(const FString& Parameters)
{
	FHubSaveFixture Fixture;
	if (!TestTrue(TEXT("Isolated Hub save fixture exists"), Fixture.IsReady())) return false;
	FRuinmatesHubSnapshot Hub = MakeSavedHubFixture();
	if (!TestTrue(TEXT("First valid generation persists"), Fixture.Save->PersistHubState(Hub, Fixture.Warehouse(), Fixture.World))) return false;
	const URuinmatesSaveGame* Before = Fixture.Save->GetCampaignSnapshot();
	Hub.Revision = 9;
	Fixture.Save->bFailNextHubSaveForTest = true;
	TestFalse(TEXT("Injected disk boundary failure rejects the candidate"), Fixture.Save->PersistHubState(Hub, Fixture.Warehouse(), Fixture.World));
	TestTrue(TEXT("Injected failure keeps the exact previous cache"), Fixture.Save->GetCampaignSnapshot() == Before);
	TestFalse(TEXT("Injected failure writes no inactive slot"), UGameplayStatics::DoesSaveGameExist(Fixture.Slot + TEXT("_Hub_B"), 0));
	TestFalse(TEXT("Failure injection resets after one attempt"), Fixture.Save->bFailNextHubSaveForTest);
	TArray<FRuinmatesItemStack> Unknown = Fixture.Warehouse();
	Unknown[0].ItemId = TEXT("UnregisteredSavedItem");
	TestFalse(TEXT("Unknown saved item rejects a full candidate"), Fixture.Save->PersistHubState(Hub, Unknown, Fixture.World));
	TestTrue(TEXT("Invalid item does not replace the previous campaign"), Fixture.Save->GetCampaignSnapshot() == Before);
	FRuinmatesHubSnapshot InvalidHub = Hub;
	InvalidHub.Rooms[0].Permanent.Level = 3;
	TestFalse(TEXT("Invalid permanent level rejects persistence"), Fixture.Save->PersistHubState(InvalidHub, Fixture.Warehouse(), Fixture.World));
	InvalidHub = Hub;
	InvalidHub.Generator.Fuel = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Non-finite fuel cannot enter a save"), Fixture.Save->PersistHubState(InvalidHub, Fixture.Warehouse(), Fixture.World));
	TestEqual(TEXT("Rejected candidates never increment the saved generation"), Fixture.Save->GetCampaignSnapshot()->SaveGeneration, int64(1));

	URuinmatesSaveGame* Future = DuplicateObject<URuinmatesSaveGame>(Before, GetTransientPackage());
	Future->SaveVersion = URuinmatesSaveGame::CurrentSaveVersion + 1;
	Future->SaveGeneration = 100;
	TestTrue(TEXT("Future schema fixture writes only the inactive GUID slot"), UGameplayStatics::SaveGameToSlot(Future, Fixture.Slot + TEXT("_Hub_B"), 0));
	TestTrue(TEXT("Loader falls back from unsupported new schema to a valid older generation"), Fixture.Save->LoadCampaign());
	TestEqual(TEXT("Unsupported future version does not replace good progress"), Fixture.Save->GetCampaignSnapshot()->SaveGeneration, int64(1));
	URuinmatesSaveGame* BadItem = DuplicateObject<URuinmatesSaveGame>(Fixture.Save->GetCampaignSnapshot(), GetTransientPackage());
	BadItem->SaveGeneration = 101;
	BadItem->WarehouseStacks[0].ItemId = TEXT("UnknownFromDisk");
	TestTrue(TEXT("Unknown item fixture writes only the inactive GUID slot"), UGameplayStatics::SaveGameToSlot(BadItem, Fixture.Slot + TEXT("_Hub_B"), 0));
	TestTrue(TEXT("Loader also falls back from unknown item data"), Fixture.Save->LoadCampaign());
	TestEqual(TEXT("Unknown item candidate cannot displace the valid generation"), Fixture.Save->GetCampaignSnapshot()->SaveGeneration, int64(1));
	const URuinmatesSaveGame* GoodCache = Fixture.Save->GetCampaignSnapshot();
	const FString BadOnlyProfile = Fixture.Slot + TEXT("_BadOnly");
	Fixture.ExtraProfiles.Add(BadOnlyProfile);
	UGameplayStatics::SaveGameToSlot(Future, BadOnlyProfile + TEXT("_Hub_A"), 0);
	Fixture.Save->SaveSlotName = BadOnlyProfile;
	TestFalse(TEXT("A profile with only an unsupported save fails to load"), Fixture.Save->LoadCampaign());
	TestTrue(TEXT("Failed loading leaves the old cache intact"), Fixture.Save->GetCampaignSnapshot() == GoodCache);
	TestTrue(TEXT("Unsupported-only profile blocks accidental replacement"), Fixture.Save->IsProfileLoadBlocked());
	TestFalse(TEXT("Saving cannot overwrite an unsupported-only profile"),Fixture.Save->SaveCampaign());
	Fixture.Save->SaveSlotName = Fixture.Slot;
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
