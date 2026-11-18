#pragma once

#include "Commandlets/Commandlet.h"
#include "RuinmatesAnimSetupCommandlet.generated.h"

// Binds ABP_Unarmed to the weapon anim properties and points the pistol fire montage at DefaultSlot.
UCLASS()
class URuinmatesAnimSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URuinmatesAnimSetupCommandlet();

	virtual int32 Main(const FString& Params) override;
};
