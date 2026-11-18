#include "Save/RuinmatesSaveGame.h"

FRuinmatesMemberSave* URuinmatesSaveGame::FindMember(const FString& StableId)
{
	if (StableId.IsEmpty())
	{
		return nullptr;
	}
	for (FRuinmatesMemberSave& Member : Members)
	{
		if (Member.StableId == StableId)
		{
			return &Member;
		}
	}
	return nullptr;
}

FRuinmatesMemberSave& URuinmatesSaveGame::FindOrAddMember(const FString& StableId)
{
	if (FRuinmatesMemberSave* Existing = FindMember(StableId))
	{
		return *Existing;
	}

	FRuinmatesMemberSave Added;
	Added.StableId = StableId;
	Added.Level = 1;
	const int32 Index = Members.Add(Added);
	return Members[Index];
}
