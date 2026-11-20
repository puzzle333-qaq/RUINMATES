#include "Hub/RuinmatesHubDefinition.h"

#include "Hub/RuinmatesHubTypes.h"

namespace
{
	/** Creates a pure item cost without looking up assets or changing inventories. */
	FHubMaterialCost MakeCost(FName ItemId, int32 Count)
	{
		FHubMaterialCost Cost;
		Cost.ItemId = ItemId;
		Cost.Count = Count;
		return Cost;
	}

	/** Creates the basic Core prerequisite shared by first-floor room unlocks. */
	FHubPrerequisite MakeCorePrerequisite()
	{
		FHubPrerequisite Prerequisite;
		Prerequisite.RoomId = RuinmatesHub::CoreId();
		Prerequisite.MinimumLevel = 1;
		Prerequisite.bRequiresUnlocked = true;
		return Prerequisite;
	}

	/** Builds one recipe; semantic validation remains the responsibility of the construction manager. */
	FHubActionDefinition MakeAction(FName ActionId, FName RoomId, ERuinmatesHubActionType Type,
		int32 RequiredLevel, int32 TargetLevel, const TArray<FHubMaterialCost>& Costs)
	{
		FHubActionDefinition Action;
		Action.ActionId = ActionId;
		Action.RoomId = RoomId;
		Action.Type = Type;
		Action.RequiredLevel = RequiredLevel;
		Action.TargetLevel = TargetLevel;
		Action.Costs = Costs;
		return Action;
	}
}

URuinmatesHubDefinition::URuinmatesHubDefinition()
{
	FillTestDefaults(this);
}

void URuinmatesHubDefinition::FillTestDefaults(URuinmatesHubDefinition* Definition)
{
	if (!Definition)
	{
		return;
	}

	Definition->Actions.Reset();
	Definition->FacilityRequiresPower = {{RuinmatesHub::CoreId(),false},{RuinmatesHub::ExitId(),false},
		{RuinmatesHub::MedicalId(),true},{RuinmatesHub::WorkshopId(),true},
		{RuinmatesHub::ElevatorId(),true},{RuinmatesHub::BlackMarketId(),true}};
	Definition->ResourcePolicy = ERuinmatesHubResourcePolicy::WarehouseOnly;
	Definition->WarehouseCapacity = 96;
	Definition->GeneratorItemId = TEXT("Hub_Generator");
	Definition->FuelItemId = TEXT("Hub_Fuel");
	Definition->FuelCapacity = 100.f;
	Definition->UnitsPerFuel = 20.f;
	Definition->BurnPerSecond = 0.02f;
	Definition->SimulationInterval = 1.f;
	Definition->DestinationMap.Reset();
	Definition->bTestEconomy = true;
	Definition->WorkshopServiceId = NAME_None;

	// Clearing, unlocking and upgrading are distinct intents, even when their target level matches.
	for (FName RoomId : { RuinmatesHub::CoreId(), RuinmatesHub::MedicalId(), RuinmatesHub::WorkshopId(),
		RuinmatesHub::ElevatorId(), RuinmatesHub::BlackMarketId(), RuinmatesHub::ExitId() })
	{
		Definition->Actions.Add(MakeAction(FName(*(RoomId.ToString() + TEXT(".Clear"))), RoomId,
			ERuinmatesHubActionType::Clear, 0, 0, { MakeCost(TEXT("Hub_Scrap"), 3) }));
	}

	for (FName RoomId : { RuinmatesHub::MedicalId(), RuinmatesHub::WorkshopId(),
		RuinmatesHub::ElevatorId(), RuinmatesHub::BlackMarketId() })
	{
		FHubActionDefinition Unlock = MakeAction(FName(*(RoomId.ToString() + TEXT(".Unlock"))), RoomId,
			ERuinmatesHubActionType::Unlock, 0, 1,
			{ MakeCost(TEXT("Hub_Parts"), 2), MakeCost(TEXT("Circuit"), 1) });
		Unlock.Prerequisites.Add(MakeCorePrerequisite());
		Unlock.bRequiresPower = true;
		Definition->Actions.Add(MoveTemp(Unlock));
	}

	// The initial route out requires neither electricity nor Core upgrades. The manager's RestoreExit
	// operation restores/clears the entrance together, so a separate paid Clear is not a prerequisite.
	Definition->Actions.Add(MakeAction(TEXT("Hub.Exit.Restore"), RuinmatesHub::ExitId(),
		ERuinmatesHubActionType::RestoreExit, 0, 1, { MakeCost(TEXT("Hub_Scrap"), 2) }));

	// Core Lv.1 is intentionally power-independent, avoiding a generator/unlock prerequisite cycle.
	Definition->Actions.Add(MakeAction(TEXT("Hub.Core.Upgrade1"), RuinmatesHub::CoreId(),
		ERuinmatesHubActionType::Upgrade, 0, 1,
		{ MakeCost(TEXT("Hub_Scrap"), 5), MakeCost(TEXT("Circuit"), 2) }));
	Definition->Actions.Add(MakeAction(TEXT("Hub.Core.Upgrade2"), RuinmatesHub::CoreId(),
		ERuinmatesHubActionType::Upgrade, 1, 2,
		{ MakeCost(TEXT("Hub_Scrap"), 10), MakeCost(TEXT("Circuit"), 3) }));

	Definition->StarterKit = {
		MakeCost(TEXT("Hub_Scrap"), 12), MakeCost(TEXT("Hub_Parts"), 8),
		MakeCost(TEXT("Circuit"), 6), MakeCost(TEXT("Hub_Generator"), 1),
		MakeCost(TEXT("Hub_Fuel"), 3)
	};
}
