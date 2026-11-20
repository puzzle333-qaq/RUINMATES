#include "Save/RuinmatesSaveSubsystem.h"

#include "Character/RuinmatesCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/RuinmatesPlayerState.h"
#include "Game/RuinmatesGameState.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	/** Strict saved-grid validation; the disk never controls current static item definitions. */
	bool NormalizeSavedGrid(const URuinmatesItemDatabaseSubsystem* Database, int32 Capacity,
		TArray<FRuinmatesItemStack>& Slots)
	{
		if (Capacity < 1 || Capacity > 512 || Slots.Num() > Capacity) return false;
		for (FRuinmatesItemStack& Stack : Slots)
		{
			if (Stack.ItemId.IsNone() && Stack.Count == 0)
			{
				Stack = FRuinmatesItemStack();
				continue;
			}
			if (Stack.ItemId.IsNone() || Stack.Count <= 0 || !Database || !Database->IsLoaded()) return false;
			const FRuinmatesItemRow* Row = Database->FindItemDefinition(Stack.ItemId);
			if (!Row || Row->MaxStack < 1 || Stack.Count > Row->MaxStack) return false;
			Stack = URuinmatesItemDatabaseSubsystem::CreateStackFromRow(*Row, Stack.ItemId, Stack.Count);
			Stack.bRevealed = true;
		}
		Slots.SetNum(Capacity);
		return true;
	}

	/** Scope validation without giving persistence another mutable construction manager. */
	int32 SavedRoomMaxLevel(FName RoomId)
	{
		if (RoomId == RuinmatesHub::CoreId()) return 2;
		return RoomId == RuinmatesHub::MedicalId() || RoomId == RuinmatesHub::WorkshopId()
			|| RoomId == RuinmatesHub::ElevatorId() || RoomId == RuinmatesHub::BlackMarketId()
			|| RoomId == RuinmatesHub::ExitId() ? 1 : -1;
	}
}

void URuinmatesSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Inventory validation must be available before selecting a Hub generation at startup.
	Collection.InitializeDependency<URuinmatesItemDatabaseSubsystem>();
	Campaign = NewObject<URuinmatesSaveGame>(this, TEXT("Campaign"));
#if WITH_EDITOR
	// Reproducible process-restart acceptance uses a GUID-only development profile. Shipping
	// builds never expose this override; a user's normal campaign slot is never selected by it.
	FString TestProfile;
	if (FParse::Value(FCommandLine::Get(),TEXT("HubTestProfile="),TestProfile)
		&& TestProfile.StartsWith(TEXT("HubPlaytest_")) && TestProfile.Len()==44)
	{
		FGuid ProfileGuid;
		if (FGuid::ParseExact(TestProfile.Mid(12),EGuidFormats::Digits,ProfileGuid)) SaveSlotName=TestProfile;
	}
#endif
	LoadCampaign();
}

bool URuinmatesSaveSubsystem::CanMutateSave() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		// Tests / early startup: allow in-memory mutation, slot IO still uses CanMutate at write time if a world exists.
		return true;
	}

	const ENetMode Mode = World->GetNetMode();
	return Mode != NM_Client;
}

int32 URuinmatesSaveSubsystem::GetSharedFunds() const
{
	return IsValid(Campaign) ? Campaign->SharedFunds : 0;
}

bool URuinmatesSaveSubsystem::SetSharedFunds(int32 NewFunds)
{
	if (!CanMutateSave() || !IsValid(Campaign))
	{
		return false;
	}
	Campaign->SharedFunds = FMath::Max(0, NewFunds);
	return true;
}

bool URuinmatesSaveSubsystem::SetMemberProgress(const FString& StableId, int32 Level, const TArray<FName>& TalentIds, const FString& EquippedLoadoutId)
{
	if (!CanMutateSave() || !IsValid(Campaign) || StableId.IsEmpty())
	{
		return false;
	}

	FRuinmatesMemberSave& Member = Campaign->FindOrAddMember(StableId);
	Member.Level = FMath::Max(1, Level);
	Member.TalentIds = TalentIds;
	Member.EquippedLoadoutId = EquippedLoadoutId;
	return true;
}

bool URuinmatesSaveSubsystem::GetMemberProgress(const FString& StableId, int32& OutLevel, TArray<FName>& OutTalentIds, FString& OutEquippedLoadoutId) const
{
	OutLevel = 0;
	OutTalentIds.Reset();
	OutEquippedLoadoutId.Reset();
	if (!IsValid(Campaign))
	{
		return false;
	}
	if (const FRuinmatesMemberSave* Member = const_cast<URuinmatesSaveGame*>(Campaign.Get())->FindMember(StableId))
	{
		OutLevel = Member->Level;
		OutTalentIds = Member->TalentIds;
		OutEquippedLoadoutId = Member->EquippedLoadoutId;
		return true;
	}
	return false;
}

bool URuinmatesSaveSubsystem::SaveCampaign()
{
	if (!CanMutateSave() || !IsValid(Campaign) || SaveSlotName.IsEmpty())
	{
		return false;
	}
	return WriteCandidateGeneration(DuplicateObject<URuinmatesSaveGame>(Campaign, this));
}

bool URuinmatesSaveSubsystem::LoadCampaign()
{
	if (!CanMutateSave() || SaveSlotName.IsEmpty())
	{
		return false;
	}
	URuinmatesSaveGame* Best = nullptr;
	FString BestSlot;
	bool bFoundFiles=false;
	for (const FString& Slot : { SaveSlotName, SaveSlotName + TEXT("_Hub_A"), SaveSlotName + TEXT("_Hub_B") })
	{
		if (!UGameplayStatics::DoesSaveGameExist(Slot, 0)) continue;
		bFoundFiles=true;
		URuinmatesSaveGame* Loaded = Cast<URuinmatesSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (!ValidateAndNormalizeCampaign(Loaded)) continue;
		if (!Best || Loaded->SaveGeneration > Best->SaveGeneration)
		{
			Best = Loaded;
			BestSlot = Slot;
		}
	}
	if (!Best)
	{
		if (bFoundFiles) BlockedProfileName=SaveSlotName;
		return false;
	}
	BlockedProfileName.Reset();
	Campaign = Best;
	ActiveProfileName = SaveSlotName;
	ActiveGenerationSlot = BestSlot == SaveSlotName ? FString() : BestSlot;
	IdentityToStableId.Reset();
	return true;
}

bool URuinmatesSaveSubsystem::ValidateAndNormalizeCampaign(URuinmatesSaveGame* Candidate) const
{
	if (!IsValid(Candidate) || Candidate->SaveVersion < 0
		|| Candidate->SaveVersion > URuinmatesSaveGame::CurrentSaveVersion
		|| Candidate->SaveGeneration < 0 || Candidate->SharedFunds < 0 || Candidate->Members.Num() > 128
		|| Candidate->LocalHostProfileId.IsEmpty() || Candidate->LocalHostProfileId.Len() > 512) return false;
	const URuinmatesItemDatabaseSubsystem* Database = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URuinmatesItemDatabaseSubsystem>() : nullptr;
	TSet<FString> MemberIds;
	for (FRuinmatesMemberSave& Member : Candidate->Members)
	{
		if (Member.StableId.IsEmpty() || Member.StableId.Len() > 512 || Member.Level < 1
			|| MemberIds.Contains(Member.StableId) || uint8(Member.BackpackTier) > uint8(ERuinmatesBackpackTier::Heavy)
			|| Member.BackpackCapacityOverride < 0 || Member.BackpackCapacityOverride > 512) return false;
		MemberIds.Add(Member.StableId);
		if (Member.bHasBackpack)
		{
			const int32 Capacity = Member.BackpackCapacityOverride > 0 ? Member.BackpackCapacityOverride
				: RuinmatesItems::GetBackpackCapacity(Member.BackpackTier);
			if (!NormalizeSavedGrid(Database, Capacity, Member.BackpackStacks)) return false;
		}
	}
	if (Candidate->bHasHubState)
	{
		if (Candidate->SaveVersion == 0 || Candidate->HubSnapshot.Rooms.Num() != 6 || Candidate->HubSnapshot.Revision < 0
			|| !FMath::IsFinite(Candidate->HubSnapshot.Generator.Fuel) || Candidate->HubSnapshot.Generator.Fuel < 0.f
			|| Candidate->HubSnapshot.Generator.Fuel > 1.e9f || Candidate->HubSnapshot.OpenFloors.Num() < 1
			|| Candidate->HubSnapshot.OpenFloors.Num() > 3) return false;
		const FRuinmatesHubGeneratorState& Generator = Candidate->HubSnapshot.Generator;
		if ((!Generator.bInstalled && (Generator.bStarted || Generator.Fuel > 0.f))
			|| (Generator.bStarted && Generator.Fuel <= 0.f)) return false;
		TSet<FName> RoomIds;
		for (FRuinmatesHubRoomState& Room : Candidate->HubSnapshot.Rooms)
		{
			const int32 MaxLevel = SavedRoomMaxLevel(Room.RoomId);
			if (MaxLevel < 0 || RoomIds.Contains(Room.RoomId) || Room.Permanent.Level < 0 || Room.Permanent.Level > MaxLevel
				|| (Room.Permanent.bUnlocked && Room.Permanent.bStructureBlocked)
				|| (Room.Permanent.Level > 0 && !Room.Permanent.bUnlocked)) return false;
			RoomIds.Add(Room.RoomId);
			Room.Runtime = FRuinmatesHubRuntimeState();
		}
		TSet<int32> Floors;
		for (int32 Floor : Candidate->HubSnapshot.OpenFloors)
		{
			if (Floor < 1 || Floor > 3 || Floors.Contains(Floor)) return false;
			Floors.Add(Floor);
		}
		if (!Floors.Contains(1) || !NormalizeSavedGrid(Database, Candidate->WarehouseCapacity, Candidate->WarehouseStacks)) return false;
		Candidate->HubSnapshot.bInitialized = false;
	}
	Candidate->SaveVersion = URuinmatesSaveGame::CurrentSaveVersion;
	return true;
}

bool URuinmatesSaveSubsystem::WriteCandidateGeneration(URuinmatesSaveGame* Candidate)
{
	if (!CanMutateSave() || IsProfileLoadBlocked() || !IsValid(Campaign) || !Candidate || SaveSlotName.IsEmpty()
		|| Campaign->SaveGeneration == MAX_int64) return false;
	Candidate->SaveVersion = URuinmatesSaveGame::CurrentSaveVersion;
	Candidate->SaveGeneration = Campaign->SaveGeneration + 1;
	if (!ValidateAndNormalizeCampaign(Candidate))
	{ UE_LOG(LogTemp, Verbose, TEXT("Campaign candidate validation rejected Hub save")); return false; }
	const FString SlotA = SaveSlotName + TEXT("_Hub_A");
	const FString SlotB = SaveSlotName + TEXT("_Hub_B");
	const FString Target = ActiveProfileName == SaveSlotName && ActiveGenerationSlot == SlotA ? SlotB : SlotA;
	TArray<uint8> ExpectedBytes, ActualBytes;
	// Canonical item reconstruction changes FText identities. Verify disk bytes before normalization.
	if (!UGameplayStatics::SaveGameToMemory(Candidate, ExpectedBytes)
		|| !UGameplayStatics::SaveDataToSlot(ExpectedBytes, Target, 0)) return false;
	const bool bExactReadBack = UGameplayStatics::LoadDataFromSlot(ActualBytes, Target, 0) && ExpectedBytes == ActualBytes;
	URuinmatesSaveGame* Verified = bExactReadBack ? Cast<URuinmatesSaveGame>(UGameplayStatics::LoadGameFromMemory(ActualBytes)) : nullptr;
	if (!ValidateAndNormalizeCampaign(Verified) || Verified->SaveGeneration != Candidate->SaveGeneration
		|| !bExactReadBack)
	{
		// Only the inactive candidate slot is removed. The active good generation/legacy survives.
		UE_LOG(LogTemp, Warning, TEXT("Campaign read-back verification failed (candidate rooms=%d loaded rooms=%d warehouse=%d/%d bytes=%d/%d)"),
			Candidate->HubSnapshot.Rooms.Num(), Verified ? Verified->HubSnapshot.Rooms.Num() : -1,
			Candidate->WarehouseStacks.Num(), Verified ? Verified->WarehouseStacks.Num() : -1, ExpectedBytes.Num(), ActualBytes.Num());
		UGameplayStatics::DeleteGameInSlot(Target, 0);
		return false;
	}
	Campaign = Verified;
	ActiveProfileName = SaveSlotName;
	ActiveGenerationSlot = Target;
	return true;
}

bool URuinmatesSaveSubsystem::PersistHubState(const FRuinmatesHubSnapshot& Hub,
	const TArray<FRuinmatesItemStack>& Warehouse, UWorld* World)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bFailNextHubSaveForTest)
	{
		bFailNextHubSaveForTest = false;
		return false;
	}
#endif
	if (!CanMutateSave() || !IsValid(Campaign) || !IsValid(World) || World->GetNetMode() == NM_Client
		|| World->GetGameInstance() != GetGameInstance()) return false;
	URuinmatesSaveGame* Candidate = DuplicateObject<URuinmatesSaveGame>(Campaign, this);
	Candidate->SaveVersion = URuinmatesSaveGame::CurrentSaveVersion;
	Candidate->bHasHubState = true;
	Candidate->HubSnapshot = Hub;
	Candidate->HubSnapshot.bInitialized = false;
	for (FRuinmatesHubRoomState& Room : Candidate->HubSnapshot.Rooms) Room.Runtime = FRuinmatesHubRuntimeState();
	Candidate->WarehouseStacks = Warehouse;
	Candidate->WarehouseCapacity = Warehouse.IsEmpty() ? Candidate->WarehouseCapacity : Warehouse.Num();
	bool bCapturedAny = false;
	return CaptureWorldIntoCandidate(World, Candidate, bCapturedAny) && WriteCandidateGeneration(Candidate);
}

bool URuinmatesSaveSubsystem::GetSavedHubState(FRuinmatesHubSnapshot& OutHub,
	TArray<FRuinmatesItemStack>& OutWarehouse) const
{
	if (!IsValid(Campaign) || !Campaign->bHasHubState) return false;
	OutHub = Campaign->HubSnapshot;
	OutHub.bInitialized = false;
	for (FRuinmatesHubRoomState& Room : OutHub.Rooms) Room.Runtime = FRuinmatesHubRuntimeState();
	OutWarehouse = Campaign->WarehouseStacks;
	return true;
}

void URuinmatesSaveSubsystem::ResetInMemoryCampaign()
{
	if (!CanMutateSave())
	{
		return;
	}
	Campaign = NewObject<URuinmatesSaveGame>(this, TEXT("Campaign"));
	IdentityToStableId.Reset();
	ActiveProfileName.Reset();
	BlockedProfileName.Reset();
	ActiveGenerationSlot.Reset();
}

FString URuinmatesSaveSubsystem::MakeIdentityKey(const ARuinmatesPlayerState* PlayerState)
{
	if (!IsValid(PlayerState))
	{
		return FString();
	}
	if (PlayerState->GetUniqueId().IsValid())
	{
		return PlayerState->GetUniqueId()->ToString();
	}
	return FString::Printf(TEXT("PlayerId:%d"), PlayerState->GetPlayerId());
}

bool URuinmatesSaveSubsystem::CapturePartyFromWorld(UWorld* World)
{
	if (!CanMutateSave() || !IsValid(World))
	{
		return false;
	}

	URuinmatesSaveGame* Candidate = DuplicateObject<URuinmatesSaveGame>(Campaign, this);
	bool bCapturedAny = false;
	if (!CaptureWorldIntoCandidate(World, Candidate, bCapturedAny) || !ValidateAndNormalizeCampaign(Candidate)) return false;
	Campaign = Candidate;
	return bCapturedAny;
}

FString URuinmatesSaveSubsystem::ResolveMemberId(ARuinmatesPlayerState* PlayerState,
	const URuinmatesSaveGame* Candidate) const
{
	if (!IsValid(PlayerState)) return FString();
	if (PlayerState->GetUniqueId().IsValid()) return PlayerState->GetUniqueId()->ToString();
	const APlayerController* OwnerPC = PlayerState->GetPlayerController();
	if (IsValid(OwnerPC) && OwnerPC->IsLocalController() && PlayerState->GetWorld())
	{
		// Only the first local controller is the offline host. Never merge remote guests or
		// additional local players into the same member when online IDs are unavailable.
		for (FConstPlayerControllerIterator It = PlayerState->GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (IsValid(PC) && PC->IsLocalController())
			{
				if (PC == OwnerPC) return Candidate ? Candidate->LocalHostProfileId : TEXT("LocalHost");
				break;
			}
		}
	}
	if (const FString* Cached = IdentityToStableId.Find(MakeIdentityKey(PlayerState))) return *Cached;
	PlayerState->EnsureStableMemberId();
	return PlayerState->GetStableMemberId();
}

bool URuinmatesSaveSubsystem::CaptureWorldIntoCandidate(UWorld* World, URuinmatesSaveGame* Candidate,
	bool& bOutCapturedAny) const
{
	bOutCapturedAny = false;
	if (!IsValid(World) || World->GetNetMode() == NM_Client || World->GetGameInstance() != GetGameInstance()
		|| !IsValid(Candidate)) return false;
	// The wallet lives on the game state; mirror it into the campaign before anything is written.
	if (const ARuinmatesGameState* RuinState = World->GetGameState<ARuinmatesGameState>())
	{
		Candidate->SharedFunds = RuinState->GetSharedFunds();
	}
	auto CaptureProgress = [this, Candidate, &bOutCapturedAny](ARuinmatesPlayerState* PS) -> FRuinmatesMemberSave*
	{
		if (!IsValid(PS) || !PS->HasAuthority()) return nullptr;
		const FString Id = ResolveMemberId(PS, Candidate);
		if (Id.IsEmpty()) return nullptr;
		FRuinmatesMemberSave& Member = Candidate->FindOrAddMember(Id);
		Member.Level = PS->GetCampaignLevel();
		Member.TalentIds = PS->GetTalentIds();
		Member.EquippedLoadoutId = PS->GetEquippedLoadoutId();
		bOutCapturedAny = true;
		return &Member;
	};
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		CaptureProgress(PC ? PC->GetPlayerState<ARuinmatesPlayerState>() : nullptr);
	}
	TSet<FString> CapturedBackpacks;
	for (TActorIterator<ARuinmatesCharacter> It(World); It; ++It)
	{
		ARuinmatesCharacter* Character = *It;
		if (!IsValid(Character) || Character->IsActorBeingDestroyed()) continue;
		ARuinmatesPlayerState* PS = Character->GetPlayerState<ARuinmatesPlayerState>();
		FRuinmatesMemberSave* Member = CaptureProgress(PS);
		URuinmatesBackpackComponent* Backpack = Character->GetBackpack();
		if (!Member || !IsValid(Backpack)) continue;
		if (CapturedBackpacks.Contains(Member->StableId)) return false;
		CapturedBackpacks.Add(Member->StableId);
		TArray<FRuinmatesItemStack> Canonical;
		if (!Backpack->PrepareSlotsForTransaction(Backpack->GetSlots(), Backpack->GetRevision(), Canonical)) return false;
		Member->bHasBackpack = true;
		Member->BackpackTier = Backpack->GetTier();
		Member->BackpackCapacityOverride = Backpack->GetCapacityOverride();
		Member->BackpackStacks = MoveTemp(Canonical);
	}
	return true;
}

void URuinmatesSaveSubsystem::RestoreOrAssignMember(ARuinmatesPlayerState* PlayerState)
{
	if (!CanMutateSave() || !IsValid(PlayerState))
	{
		return;
	}

	const FString Key = MakeIdentityKey(PlayerState);
	PlayerState->SetStableMemberId(ResolveMemberId(PlayerState, Campaign));
	if (!Key.IsEmpty())
	{
		IdentityToStableId.Add(Key, PlayerState->GetStableMemberId());
	}

	int32 Level = 1;
	TArray<FName> Talents;
	FString Loadout;
	if (GetMemberProgress(PlayerState->GetStableMemberId(), Level, Talents, Loadout))
	{
		PlayerState->SetCampaignLevel(Level);
		PlayerState->SetTalentIds(Talents);
		PlayerState->SetEquippedLoadoutId(Loadout);
	}
}

bool URuinmatesSaveSubsystem::RestoreBackpack(ARuinmatesCharacter* Character)
{
	if (!CanMutateSave() || !IsValid(Character) || !Character->HasAuthority() || !Campaign) return false;
	auto* PS = Character->GetPlayerState<ARuinmatesPlayerState>();
	if (!PS) return false;
	RestoreOrAssignMember(PS);
	const auto* Member = Campaign->FindMember(PS->GetStableMemberId());
	auto* Backpack = Character->GetBackpack();
	if (!Member || !Member->bHasBackpack || !Backpack) return false;
	// A newly spawned pawn is restored once by GameMode; never reset a live pawn from the cache.
	return Backpack->RestoreSavedInventory(Member->BackpackTier, Member->BackpackCapacityOverride, Member->BackpackStacks);
}
