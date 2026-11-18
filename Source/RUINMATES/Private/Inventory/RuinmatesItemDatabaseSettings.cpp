#include "Inventory/RuinmatesItemDatabaseSettings.h"

URuinmatesItemDatabaseSettings::URuinmatesItemDatabaseSettings()
{
	ItemTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/RUINMATES/Data/Items/DT_Items.DT_Items")));
}
