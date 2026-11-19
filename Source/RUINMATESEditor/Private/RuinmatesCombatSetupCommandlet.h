#pragma once
#include "Commandlets/Commandlet.h"
#include "RuinmatesCombatSetupCommandlet.generated.h"

/** Builds the independent, editable combat animation graph from existing mannequin assets. */
UCLASS()
class URuinmatesCombatSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	URuinmatesCombatSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
