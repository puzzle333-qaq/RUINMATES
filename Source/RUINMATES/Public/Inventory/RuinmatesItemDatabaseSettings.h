#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RuinmatesItemDatabaseSettings.generated.h"

class UDataTable;

// Project Settings > Game > RuinMates Items.
// The single configurable pointer to the authoritative item database. Nothing else may hardcode
// a DataTable path: loot, backpack, merchant and trade all go through the item subsystem.
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "RuinMates Items"))
class RUINMATES_API URuinmatesItemDatabaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URuinmatesItemDatabaseSettings();

	UPROPERTY(config, EditAnywhere, Category = "Items", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> ItemTable;

	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }
};
