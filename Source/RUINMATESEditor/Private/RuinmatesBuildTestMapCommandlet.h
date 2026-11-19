#pragma once

#include "Commandlets/Commandlet.h"
#include "RuinmatesBuildTestMapCommandlet.generated.h"

// Generates the Phase 1B functional test map at
// /Game/RUINMATES/Maps/Lvl_TestPhase1B. Rebuilds the map from existing actors and blueprints only:
// two player starts, a movement floor, shooting targets, loot containers (default / multi-quality /
// adjacent), a merchant stand and a full-bag pickup row. It never touches the shipping maps.
UCLASS()
class URuinmatesBuildTestMapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URuinmatesBuildTestMapCommandlet();

	virtual int32 Main(const FString& Params) override;
};
