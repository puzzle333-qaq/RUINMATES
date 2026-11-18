#pragma once

#include "CoreMinimal.h"
#include "Gameplay/RuinmatesGameplayTags.h"
#include "GameplayTagContainer.h"
#include "WeaponTypes.generated.h"

// Behavioral kinds for the weapon module. Tags may be more specific (pistol, lmg);
// sniper shares the rifle hitscan path, melee uses a sweep instead of a trace.
UENUM(BlueprintType)
enum class ERuinmatesWeaponKind : uint8
{
	Rifle,
	Sniper,
	Melee
};

namespace RuinmatesWeaponTypes
{
	inline bool UsesHitscan(ERuinmatesWeaponKind Kind)
	{
		return Kind != ERuinmatesWeaponKind::Melee;
	}

	inline bool UsesMeleeSweep(ERuinmatesWeaponKind Kind)
	{
		return Kind == ERuinmatesWeaponKind::Melee;
	}

	inline bool UsesLeftHandFabrik(ERuinmatesWeaponKind Kind)
	{
		return Kind != ERuinmatesWeaponKind::Melee;
	}

	inline ERuinmatesWeaponKind KindFromWeaponTag(const FGameplayTag& Tag)
	{
		using namespace RuinmatesTags;
		if (Tag.MatchesTagExact(Tag_WeaponType_Melee))
		{
			return ERuinmatesWeaponKind::Melee;
		}
		if (Tag.MatchesTagExact(Tag_WeaponType_Sniper))
		{
			return ERuinmatesWeaponKind::Sniper;
		}
		return ERuinmatesWeaponKind::Rifle;
	}
}
