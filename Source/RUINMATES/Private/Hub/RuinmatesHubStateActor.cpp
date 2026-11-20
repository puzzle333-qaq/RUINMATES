#include "Hub/RuinmatesHubStateActor.h"

#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Save/RuinmatesSaveSubsystem.h"
#include "Hub/RuinmatesHubDefinition.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Templates/UnrealTemplate.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ARuinmatesHubStateActor::ARuinmatesHubStateActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("HubStateRoot")));
	Warehouse = CreateDefaultSubobject<URuinmatesBackpackComponent>(TEXT("SharedWarehouse"));
	GeneratorAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("GeneratorAudio"));
	GeneratorAudio->SetupAttachment(RootComponent); GeneratorAudio->bAutoActivate=false;
	CoreAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("CoreAudio"));
	CoreAudio->SetupAttachment(RootComponent); CoreAudio->bAutoActivate=false;
}

void ARuinmatesHubStateActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && IsValid(Definition) && IsValid(Warehouse))
	{
		Warehouse->SetCapacityOverride(Definition->WarehouseCapacity);
	}
	if (HasAuthority() && !Snapshot.bInitialized)
	{
		UGameInstance* GI = GetGameInstance();
		auto* Save = GI ? GI->GetSubsystem<URuinmatesSaveSubsystem>() : nullptr;
		if (Save && Save->IsProfileLoadBlocked())
		{ UE_LOG(LogTemp,Warning,TEXT("Hub initialization blocked: saved campaign is invalid or unsupported")); return; }
		FRuinmatesHubSnapshot Restored;
		TArray<FRuinmatesItemStack> Stored;
		if (Save && Save->GetSavedHubState(Restored, Stored) && Warehouse && IsValid(Definition))
		{
			Warehouse->SetCapacityOverride(Stored.Num());
			if (Warehouse->ReplaceSlotsForTransaction(Stored, Warehouse->GetRevision()))
			{
				Snapshot = Restored;
				Snapshot.Generator.Fuel = FMath::Clamp(Snapshot.Generator.Fuel, 0.f, Definition->FuelCapacity);
				if (!Snapshot.Generator.IsPowered()) Snapshot.Generator.bStarted = false;
				RecomputeOperatingState();
				Snapshot.bInitialized = true;
				ForceNetUpdate();
				OnRoomStateChanged.Broadcast(NAME_None);
			}
		}
		else InitializeDefaultState();
	}
	if (HasAuthority() && IsValid(Definition) && GetWorld())
	{
		LastSimulationTime = GetWorld()->GetTimeSeconds();
		GetWorldTimerManager().SetTimer(SimulationTimer, this, &ThisClass::SimulatePower,
			FMath::Max(0.1f, Definition->SimulationInterval), true);
	}
}

void ARuinmatesHubStateActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SimulationTimer);
	if (HasAuthority() && Snapshot.bInitialized && EndPlayReason != EEndPlayReason::Destroyed) SaveProgress();
	Super::EndPlay(EndPlayReason);
}

void ARuinmatesHubStateActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARuinmatesHubStateActor, Snapshot);
}

int32 ARuinmatesHubStateActor::GetRoomMaxLevel(FName RoomId)
{
	if (RoomId == RuinmatesHub::CoreId())
	{
		return 2;
	}
	return RoomId == RuinmatesHub::MedicalId() || RoomId == RuinmatesHub::WorkshopId()
		|| RoomId == RuinmatesHub::ElevatorId() || RoomId == RuinmatesHub::BlackMarketId()
		|| RoomId == RuinmatesHub::ExitId() ? 1 : -1;
}

bool ARuinmatesHubStateActor::InitializeDefaultState()
{
	if (!HasAuthority() || Snapshot.bInitialized)
	{
		return false;
	}
	Snapshot.Rooms.Reset();
	for (FName Id : { RuinmatesHub::CoreId(), RuinmatesHub::MedicalId(), RuinmatesHub::WorkshopId(),
		RuinmatesHub::ElevatorId(), RuinmatesHub::BlackMarketId(), RuinmatesHub::ExitId() })
	{
		FRuinmatesHubRoomState& Room = Snapshot.Rooms.AddDefaulted_GetRef();
		Room.RoomId = Id;
		if (Id == RuinmatesHub::CoreId())
		{
			Room.Permanent.bUnlocked = true;
			Room.Permanent.bStructureBlocked = false;
		}
	}
	Snapshot.bInitialized = true;
	ForceNetUpdate();
	OnRoomStateChanged.Broadcast(NAME_None);
	return true;
}

bool ARuinmatesHubStateActor::GetRoomState(FName RoomId, FRuinmatesHubRoomState& OutState) const
{
	OutState = FRuinmatesHubRoomState();
	if (!Snapshot.bInitialized)
	{
		return false;
	}
	const FRuinmatesHubRoomState* Room = Snapshot.Rooms.FindByPredicate(
		[RoomId](const FRuinmatesHubRoomState& Candidate) { return Candidate.RoomId == RoomId; });
	if (!Room)
	{
		return false;
	}
	OutState = *Room;
	return true;
}

bool ARuinmatesHubStateActor::CommitRoomState(const FRuinmatesHubRoomState& NewState)
{
	const int32 MaxLevel = GetRoomMaxLevel(NewState.RoomId);
	if (!HasAuthority() || bTransactionInProgress || !Snapshot.bInitialized || MaxLevel < 0
		|| NewState.Permanent.Level < 0 || NewState.Permanent.Level > MaxLevel
		|| (NewState.Runtime.bOperational
			&& (!NewState.Permanent.bUnlocked || NewState.Permanent.bStructureBlocked)))
	{
		return false;
	}
	FRuinmatesHubRoomState* Room = Snapshot.Rooms.FindByPredicate(
		[&NewState](const FRuinmatesHubRoomState& Candidate) { return Candidate.RoomId == NewState.RoomId; });
	if (!Room)
	{
		return false;
	}
	if (*Room == NewState)
	{
		return true;
	}
	*Room = NewState;
	ForceNetUpdate();
	OnRoomStateChanged.Broadcast(NewState.RoomId);
	return true;
}

void ARuinmatesHubStateActor::OnRep_Snapshot()
{
	RefreshOperatingAudio();
	OnRoomStateChanged.Broadcast(NAME_None);
}

URuinmatesBackpackComponent* ARuinmatesHubStateActor::GetInitiatorBackpack(APawn* Initiator) const
{
	const ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(Initiator);
	return IsValid(Character) && Character->GetWorld() == GetWorld() ? Character->GetBackpack() : nullptr;
}

bool ARuinmatesHubStateActor::IsRequestAllowed(APawn* Initiator, int32 ExpectedRevision, FText& OutReason) const
{
	if (!HasAuthority() || !Snapshot.bInitialized || !IsValid(Definition) || !IsValid(Initiator)
		|| Initiator->GetWorld() != GetWorld() || !Initiator->HasAuthority() || bTransactionInProgress)
	{
		OutReason = FText::FromString(TEXT("基地状态未就绪"));
		return false;
	}
	if (ExpectedRevision != Snapshot.Revision)
	{
		OutReason = FText::FromString(TEXT("基地状态已变化，请重试"));
		return false;
	}
	return true;
}

FRuinmatesHubActionQuote ARuinmatesHubStateActor::GetActionQuote(APawn* Initiator, FName ActionId) const
{
	FRuinmatesHubActionQuote Quote;
	if (!IsValid(Definition) || !Snapshot.bInitialized)
	{
		Quote.Reason = FText::FromString(TEXT("基地配置未就绪"));
		return Quote;
	}
	const FHubActionDefinition* Action = Definition->Actions.FindByPredicate(
		[ActionId](const FHubActionDefinition& Entry) { return Entry.ActionId == ActionId; });
	FRuinmatesHubRoomState Room;
	if (!Action || !GetRoomState(Action->RoomId, Room))
	{
		Quote.Reason = FText::FromString(TEXT("无效建设项目"));
		return Quote;
	}
	const int32 ScopeMax = GetRoomMaxLevel(Action->RoomId);
	if (Action->TargetLevel < 0 || Action->TargetLevel > ScopeMax || Action->RequiredLevel < 0
		|| Action->RequiredLevel > ScopeMax
		|| Definition->Actions.FilterByPredicate([ActionId](const FHubActionDefinition& A) { return A.ActionId == ActionId; }).Num() != 1
		|| (Action->Type == ERuinmatesHubActionType::Upgrade && Action->TargetLevel != Action->RequiredLevel + 1)
		|| (Action->Type == ERuinmatesHubActionType::RestoreExit && Action->RoomId != RuinmatesHub::ExitId())
		|| uint8(Action->Type) > uint8(ERuinmatesHubActionType::RestoreExit))
	{
		Quote.Reason = FText::FromString(TEXT("建设配置超出当前等级范围"));
		return Quote;
	}
	for (const FHubMaterialCost& Cost : Action->Costs)
	{
		const int64 Sum = int64(Quote.Required.FindRef(Cost.ItemId)) + Cost.Count;
		if (Cost.ItemId.IsNone() || Cost.Count <= 0 || Sum > MAX_int32)
		{
			Quote.Reason = FText::FromString(TEXT("建设配方无效"));
			return Quote;
		}
		Quote.Required.Add(Cost.ItemId, int32(Sum));
	}
	const URuinmatesBackpackComponent* Backpack = GetInitiatorBackpack(Initiator);
	bool bMaterials = true;
	for (const TPair<FName, int32>& Cost : Quote.Required)
	{
		const int64 Have = (Warehouse ? Warehouse->CountItem(Cost.Key) : 0)
			+ int64(Definition->ResourcePolicy == ERuinmatesHubResourcePolicy::WarehouseThenBackpack && Backpack
				? Backpack->CountItem(Cost.Key) : 0);
		Quote.Available.Add(Cost.Key, int32(FMath::Min<int64>(Have, MAX_int32)));
		bMaterials &= Have >= Cost.Value;
	}
	if (Room.Permanent.Level != Action->RequiredLevel)
	{
		Quote.Reason = FText::FromString(TEXT("等级条件不满足或项目已完成"));
		return Quote;
	}
	if ((Action->Type == ERuinmatesHubActionType::Clear && Room.Permanent.bCleared)
		|| (Action->Type == ERuinmatesHubActionType::Unlock && (Room.Permanent.bUnlocked || Room.Permanent.bStructureBlocked))
		|| (Action->Type == ERuinmatesHubActionType::Upgrade && (!Room.Permanent.bUnlocked || !Room.Permanent.bCleared || Room.Permanent.bStructureBlocked))
		|| (Action->Type == ERuinmatesHubActionType::RestoreExit && Room.Permanent.bUnlocked))
	{
		Quote.Reason = FText::FromString(Room.Permanent.bStructureBlocked ? TEXT("入口被阻挡，需要先清理") : TEXT("建设已完成或尚未解锁"));
		return Quote;
	}
	for (const FHubPrerequisite& Requirement : Action->Prerequisites)
	{
		FRuinmatesHubRoomState Prerequisite;
		if (Requirement.MinimumLevel < 0 || !GetRoomState(Requirement.RoomId, Prerequisite) || Prerequisite.Permanent.Level < Requirement.MinimumLevel
			|| (Requirement.bRequiresUnlocked && !Prerequisite.Permanent.bUnlocked))
		{
			Quote.Reason = FText::FromString(TEXT("前置设施条件不满足"));
			return Quote;
		}
	}
	if (Action->bRequiresPower && !Snapshot.Generator.IsPowered())
	{
		Quote.Reason = FText::FromString(TEXT("需要恢复供电"));
		return Quote;
	}
	Quote.bCanExecute = bMaterials;
	Quote.Reason = FText::FromString(bMaterials ? TEXT("可以建设") : TEXT("建设材料不足"));
	return Quote;
}

bool ARuinmatesHubStateActor::CommitWithResources(APawn* Initiator, const TMap<FName, int32>& Costs,
	const FRuinmatesHubSnapshot& Candidate, FText& OutReason)
{
	if (!IsRequestAllowed(Initiator, Snapshot.Revision, OutReason) || !Warehouse) return false;
	TGuardValue<bool> Guard(bTransactionInProgress, true);
	URuinmatesBackpackComponent* Backpack = Definition->ResourcePolicy == ERuinmatesHubResourcePolicy::WarehouseThenBackpack
		? GetInitiatorBackpack(Initiator) : nullptr;
	const TArray<FRuinmatesItemStack> OldWarehouse = Warehouse->GetSlots();
	const TArray<FRuinmatesItemStack> OldBackpack = Backpack ? Backpack->GetSlots() : TArray<FRuinmatesItemStack>();
	TArray<FRuinmatesItemStack> NewWarehouse = OldWarehouse, NewBackpack = OldBackpack;
	TMap<FName, int32> WarehouseCost, BackpackCost;
	for (const TPair<FName, int32>& Cost : Costs)
	{
		if (Cost.Key.IsNone() || Cost.Value <= 0) return false;
		const int32 FromWarehouse = FMath::Min(Warehouse->CountItem(Cost.Key), Cost.Value);
		if (FromWarehouse > 0) WarehouseCost.Add(Cost.Key, FromWarehouse);
		const int32 Remainder = Cost.Value - FromWarehouse;
		if (Remainder > 0)
		{
			if (!Backpack || Backpack->CountItem(Cost.Key) < Remainder)
			{
				OutReason = FText::FromString(TEXT("建设材料不足，未扣除任何材料"));
				return false;
			}
			BackpackCost.Add(Cost.Key, Remainder);
		}
	}
	if (!URuinmatesBackpackComponent::ConsumeItemsFromSlots(NewWarehouse, WarehouseCost)
		|| !URuinmatesBackpackComponent::ConsumeItemsFromSlots(NewBackpack, BackpackCost)) return false;
	const uint32 WarehouseRevision = Warehouse->GetRevision(), BackpackRevision = Backpack ? Backpack->GetRevision() : 0;
	TArray<FRuinmatesItemStack> ValidatedWarehouse, ValidatedBackpack;
	if (!Warehouse->PrepareSlotsForTransaction(NewWarehouse, WarehouseRevision, ValidatedWarehouse)
		|| (Backpack && !Backpack->PrepareSlotsForTransaction(NewBackpack, BackpackRevision, ValidatedBackpack))) return false;
	const FRuinmatesHubSnapshot OldSnapshot = Snapshot;
	if (!Warehouse->ReplaceSlotsForTransaction(ValidatedWarehouse, WarehouseRevision, false)) return false;
	if (Backpack && !Backpack->ReplaceSlotsForTransaction(ValidatedBackpack, BackpackRevision, false))
	{
		Warehouse->ReplaceSlotsForTransaction(OldWarehouse, WarehouseRevision, false);
		return false;
	}
	Snapshot = Candidate;
	Snapshot.Revision = OldSnapshot.Revision + 1;
	RecomputeOperatingState();
	if (!PersistCandidate())
	{
		Snapshot = OldSnapshot;
		Warehouse->ReplaceSlotsForTransaction(OldWarehouse, WarehouseRevision, false);
		if (Backpack) Backpack->ReplaceSlotsForTransaction(OldBackpack, BackpackRevision, false);
		OutReason = FText::FromString(TEXT("保存失败，建设与资源已回退"));
		return false;
	}
	Warehouse->PublishTransactionChange();
	if (Backpack) Backpack->PublishTransactionChange();
	ForceNetUpdate();
	OnRoomStateChanged.Broadcast(NAME_None);
	OutReason = FText::FromString(TEXT("建设完成"));
	return true;
}

bool ARuinmatesHubStateActor::TryExecuteAction(APawn* Initiator, FName ActionId, int32 ExpectedRevision, FText& OutReason)
{
	if (!IsRequestAllowed(Initiator, ExpectedRevision, OutReason)) return false;
	const FRuinmatesHubActionQuote Quote = GetActionQuote(Initiator, ActionId);
	OutReason = Quote.Reason;
	if (!Quote.bCanExecute) return false;
	const FHubActionDefinition* Action = Definition->Actions.FindByPredicate([ActionId](const FHubActionDefinition& A) { return A.ActionId == ActionId; });
	FRuinmatesHubSnapshot Candidate = Snapshot;
	FRuinmatesHubRoomState* Room = Candidate.Rooms.FindByPredicate([Action](const FRuinmatesHubRoomState& R) { return R.RoomId == Action->RoomId; });
	if (!Room) return false;
	if (Action->Type == ERuinmatesHubActionType::Clear) { Room->Permanent.bCleared = true; Room->Permanent.bStructureBlocked = false; }
	else if (Action->Type == ERuinmatesHubActionType::Upgrade)
	{
		Room->Permanent.Level = Action->TargetLevel;
	}
	else
	{
		Room->Permanent.Level = Action->TargetLevel;
		Room->Permanent.bUnlocked = true;
		Room->Permanent.bCleared = true;
		Room->Permanent.bStructureBlocked = false;
	}
	return CommitWithResources(Initiator, Quote.Required, Candidate, OutReason);
}

void ARuinmatesHubStateActor::RecomputeOperatingState()
{
	for (FRuinmatesHubRoomState& Room : Snapshot.Rooms)
	{
		Room.Runtime.bPowered = Snapshot.Generator.IsPowered();
		Room.Runtime.bGeneratorInstalled = Snapshot.Generator.bInstalled;
		const bool* RequiresPower=Definition ? Definition->FacilityRequiresPower.Find(Room.RoomId) : nullptr;
		const bool bNeedsPower = RequiresPower ? *RequiresPower : true;
		Room.Runtime.bOperational = Room.Permanent.bUnlocked && !Room.Permanent.bStructureBlocked
			&& (!bNeedsPower || Room.Runtime.bPowered);
	}
	RefreshOperatingAudio();
}

void ARuinmatesHubStateActor::RefreshOperatingAudio()
{
	const auto* Core=Snapshot.Rooms.FindByPredicate([](const auto& R){ return R.RoomId==RuinmatesHub::CoreId(); });
	auto Apply=[](UAudioComponent* Audio,bool bActive)
	{
		if (!IsValid(Audio)) return;
		if (bActive && IsValid(Audio->Sound)) { if (!Audio->IsPlaying()) Audio->Play(); }
		else Audio->Stop();
	};
	Apply(GeneratorAudio,Snapshot.Generator.IsPowered());
	Apply(CoreAudio,Snapshot.Generator.IsPowered() && Core && Core->Permanent.Level>0);
}

bool ARuinmatesHubStateActor::TryInstallGenerator(APawn* Initiator, int32 ExpectedRevision, FText& OutReason)
{
	if (!IsRequestAllowed(Initiator, ExpectedRevision, OutReason)) return false;
	if (Snapshot.Generator.bInstalled) { OutReason = FText::FromString(TEXT("发电机已安装")); return false; }
	FRuinmatesHubSnapshot Candidate = Snapshot;
	Candidate.Generator.bInstalled = true;
	return CommitWithResources(Initiator, {{Definition->GeneratorItemId,1}}, Candidate, OutReason);
}

bool ARuinmatesHubStateActor::TryAddFuel(APawn* Initiator, int32 ExpectedRevision, FText& OutReason)
{
	if (!IsRequestAllowed(Initiator, ExpectedRevision, OutReason)) return false;
	if (!Snapshot.Generator.bInstalled || !FMath::IsFinite(Definition->UnitsPerFuel)
		|| !FMath::IsFinite(Definition->FuelCapacity) || Definition->FuelCapacity <= 0.f
		|| !FMath::IsFinite(Snapshot.Generator.Fuel) || Definition->UnitsPerFuel <= 0.f
		|| Snapshot.Generator.Fuel + Definition->UnitsPerFuel > Definition->FuelCapacity)
	{ OutReason = FText::FromString(TEXT("需要安装发电机或燃料容量不足")); return false; }
	FRuinmatesHubSnapshot Candidate = Snapshot;
	Candidate.Generator.Fuel += Definition->UnitsPerFuel;
	return CommitWithResources(Initiator, {{Definition->FuelItemId,1}}, Candidate, OutReason);
}

bool ARuinmatesHubStateActor::TrySetGeneratorStarted(APawn* Initiator, bool bStarted, int32 ExpectedRevision, FText& OutReason)
{
	if (!IsRequestAllowed(Initiator, ExpectedRevision, OutReason)) return false;
	if (!Snapshot.Generator.bInstalled || (bStarted && Snapshot.Generator.Fuel <= 0.f))
	{ OutReason = FText::FromString(TEXT("需要已安装且有燃料的发电机")); return false; }
	FRuinmatesHubSnapshot Candidate = Snapshot;
	Candidate.Generator.bStarted = bStarted;
	return CommitWithResources(Initiator, {}, Candidate, OutReason);
}

void ARuinmatesHubStateActor::SimulatePower()
{
	if (!HasAuthority() || !GetWorld() || !Definition || bTransactionInProgress || bPreviewSession
		|| !FMath::IsFinite(Definition->BurnPerSecond) || Definition->BurnPerSecond < 0.f) return;
	const float Now = GetWorld()->GetTimeSeconds();
	const float Elapsed = FMath::Max(0.f, Now - LastSimulationTime);
	LastSimulationTime = Now;
	if (!Snapshot.Generator.IsPowered()) return;
	Snapshot.Generator.Fuel = FMath::Max(0.f, Snapshot.Generator.Fuel - Elapsed * FMath::Max(0.f, Definition->BurnPerSecond));
	if (Snapshot.Generator.Fuel <= 0.f) { Snapshot.Generator.bStarted = false; ++Snapshot.Revision; }
	RecomputeOperatingState();
	ForceNetUpdate();
	OnRoomStateChanged.Broadcast(NAME_None);
	if (!Snapshot.Generator.bStarted || Now - LastAutosaveTime >= 30.f)
	{
		if (SaveProgress()) LastAutosaveTime = Now;
		else UE_LOG(LogTemp, Warning, TEXT("Hub fuel autosave failed; last complete campaign generation retained"));
	}
}

bool ARuinmatesHubStateActor::IsFacilityOperational(FName RoomId) const
{
	FRuinmatesHubRoomState State;
	return GetRoomState(RoomId, State) && State.Runtime.bOperational;
}

namespace
{
	/** Exact candidate insertion; canonical data comes from the existing server item row. */
	bool AddHubItems(TArray<FRuinmatesItemStack>& Slots, const FRuinmatesItemStack& Template, int32 Count, int32 Capacity)
	{
		if (Count <= 0 || Template.ItemId.IsNone() || Template.MaxStack <= 0 || Capacity <= 0) return false;
		Slots.SetNum(Capacity);
		int32 Remaining = Count;
		for (FRuinmatesItemStack& Slot : Slots)
		{
			if (!Slot.IsEmpty() && Slot.ItemId == Template.ItemId && Slot.Quality == Template.Quality)
			{
				const int32 Moved = FMath::Min(Remaining, FMath::Max(0, Template.MaxStack - Slot.Count));
				Slot.Count += Moved;
				Remaining -= Moved;
			}
		}
		for (FRuinmatesItemStack& Slot : Slots)
		{
			if (Remaining <= 0) break;
			if (Slot.IsEmpty())
			{
				Slot = Template;
				Slot.Count = FMath::Min(Remaining, Template.MaxStack);
				Remaining -= Slot.Count;
			}
		}
		return Remaining == 0;
	}
}

bool ARuinmatesHubStateActor::TryTransferWarehouse(APawn* Initiator, FName ItemId, int32 Count, bool bDeposit,
	int32 ExpectedRevision, FText& OutReason)
{
	if (!IsRequestAllowed(Initiator, ExpectedRevision, OutReason) || Count <= 0 || Count > 10000 || !Warehouse) return false;
	URuinmatesBackpackComponent* Backpack = GetInitiatorBackpack(Initiator);
	const URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this);
	const FRuinmatesItemRow* Row = Database ? Database->FindItemDefinition(ItemId) : nullptr;
	if (!Backpack || !Row) return false;
	TGuardValue<bool> Guard(bTransactionInProgress, true);
	URuinmatesBackpackComponent* Source = bDeposit ? Backpack : Warehouse.Get();
	URuinmatesBackpackComponent* Destination = bDeposit ? Warehouse.Get() : Backpack;
	const TArray<FRuinmatesItemStack> OldSource = Source->GetSlots(), OldDestination = Destination->GetSlots();
	TArray<FRuinmatesItemStack> NewSource = OldSource, NewDestination = OldDestination;
	if (!URuinmatesBackpackComponent::ConsumeItemsFromSlots(NewSource, {{ItemId, Count}})
		|| !AddHubItems(NewDestination, URuinmatesItemDatabaseSubsystem::CreateStackFromRow(*Row, ItemId, 1), Count, Destination->GetCapacity()))
	{ OutReason = FText::FromString(TEXT("数量不足或目标空间不足")); return false; }
	const uint32 SourceRevision = Source->GetRevision(), DestinationRevision = Destination->GetRevision();
	TArray<FRuinmatesItemStack> ValidSource, ValidDestination;
	if (!Source->PrepareSlotsForTransaction(NewSource, SourceRevision, ValidSource)
		|| !Destination->PrepareSlotsForTransaction(NewDestination, DestinationRevision, ValidDestination)) return false;
	if (!Source->ReplaceSlotsForTransaction(ValidSource, SourceRevision, false)) return false;
	if (!Destination->ReplaceSlotsForTransaction(ValidDestination, DestinationRevision, false))
	{
		Source->ReplaceSlotsForTransaction(OldSource, SourceRevision, false);
		return false;
	}
	++Snapshot.Revision;
	if (!PersistCandidate())
	{
		--Snapshot.Revision;
		Source->ReplaceSlotsForTransaction(OldSource, SourceRevision, false);
		Destination->ReplaceSlotsForTransaction(OldDestination, DestinationRevision, false);
		OutReason = FText::FromString(TEXT("保存失败，物品转移已回退")); return false;
	}
	Source->PublishTransactionChange(); Destination->PublishTransactionChange();
	ForceNetUpdate(); OnRoomStateChanged.Broadcast(NAME_None);
	OutReason = FText::FromString(TEXT("仓库已更新")); return true;
}

bool ARuinmatesHubStateActor::TryClaimStarterKit(APawn* Initiator, FText& OutReason)
{
	if (!IsRequestAllowed(Initiator, Snapshot.Revision, OutReason) || !Warehouse) return false;
	if (Snapshot.bStarterKitClaimed) { OutReason = FText::FromString(TEXT("首次物资已领取")); return false; }
	const URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this);
	if (!Database || !Database->IsLoaded()) return false;
	TGuardValue<bool> Guard(bTransactionInProgress, true);
	const TArray<FRuinmatesItemStack> OldSlots = Warehouse->GetSlots();
	TArray<FRuinmatesItemStack> NewSlots = OldSlots;
	for (const FHubMaterialCost& Entry : Definition->StarterKit)
	{
		const FRuinmatesItemRow* Row = Database->FindItemDefinition(Entry.ItemId);
		if (!Row || !AddHubItems(NewSlots, URuinmatesItemDatabaseSubsystem::CreateStackFromRow(*Row, Entry.ItemId, 1), Entry.Count, Warehouse->GetCapacity()))
		{ OutReason = FText::FromString(TEXT("首次物资配置无效或仓库已满")); return false; }
	}
	const uint32 OldRevision = Warehouse->GetRevision();
	if (!Warehouse->ReplaceSlotsForTransaction(NewSlots, OldRevision, false)) return false;
	Snapshot.bStarterKitClaimed = true;
	++Snapshot.Revision;
	if (!PersistCandidate())
	{
		Snapshot.bStarterKitClaimed = false;
		--Snapshot.Revision;
		Warehouse->ReplaceSlotsForTransaction(OldSlots, OldRevision, false);
		OutReason=FText::FromString(TEXT("保存失败，首次物资未领取，仓库已回退"));
		return false;
	}
	Warehouse->PublishTransactionChange(); ForceNetUpdate();
	OnRoomStateChanged.Broadcast(NAME_None);
	OutReason = FText::FromString(TEXT("首次建设物资已存入共享仓库")); return true;
}

bool ARuinmatesHubStateActor::PersistCandidate()
{
	return SaveProgress();
}

bool ARuinmatesHubStateActor::SaveProgress()
{
	UGameInstance* GI = GetGameInstance();
	auto* Save = GI ? GI->GetSubsystem<URuinmatesSaveSubsystem>() : nullptr;
	return !bPreviewSession && HasAuthority() && Snapshot.bInitialized && Warehouse && Save
		&& Save->PersistHubState(Snapshot, Warehouse->GetSlots(), GetWorld());
}

#if WITH_EDITOR
void ARuinmatesHubStateActor::PreviewAllRoomsForCapture(int32 CoreLevel, bool bPowered)
{
	if (!HasAuthority() || !GetWorld() || !GetWorld()->IsPlayInEditor())
	{
		return;
	}
	InitializeDefaultState();
	bPreviewSession = true;
	const TArray<FRuinmatesHubRoomState> Rooms = Snapshot.Rooms;
	for (FRuinmatesHubRoomState Room : Rooms)
	{
		const bool bCore = Room.RoomId == RuinmatesHub::CoreId();
		Room.Permanent.Level = FMath::Clamp(CoreLevel, 0, GetRoomMaxLevel(Room.RoomId));
		Room.Permanent.bCleared = CoreLevel > 0;
		Room.Permanent.bUnlocked = bCore || CoreLevel > 0;
		Room.Permanent.bStructureBlocked = !bCore && CoreLevel <= 0;
		Room.Runtime.bPowered = bPowered;
		Room.Runtime.bOperational = bPowered && Room.Permanent.bUnlocked && !Room.Permanent.bStructureBlocked;
		CommitRoomState(Room);
	}
}
#endif
