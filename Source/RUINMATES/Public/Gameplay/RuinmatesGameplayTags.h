#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace RuinmatesTags
{
	// Weapon type tags only. Attachment / loadout slot tags are intentionally not migrated.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_None);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Rifle);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Pistol);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Shotgun);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Sniper);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_LMG);
}
