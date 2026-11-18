#include "Save/RuinmatesSaveSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/RuinmatesPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void URuinmatesSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Campaign = NewObject<URuinmatesSaveGame>(this, TEXT("Campaign"));
	LoadCampaign();
}

bool URuinmatesSaveSubsystem::CanMutateSave() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		// Tests / early startup: allow in-memory mutation, slot IO still uses CanMutate at write time if a world exists.
		return true;
	}

	const ENetMode Mode = World->GetNetMode();
	return Mode != NM_Client;
}

int32 URuinmatesSaveSubsystem::GetSharedFunds() const
{
	return IsValid(Campaign) ? Campaign->SharedFunds : 0;
}

bool URuinmatesSaveSubsystem::SetSharedFunds(int32 NewFunds)
{
	if (!CanMutateSave() || !IsValid(Campaign))
	{
		return false;
	}
	Campaign->SharedFunds = FMath::Max(0, NewFunds);
	return true;
}

bool URuinmatesSaveSubsystem::SetMemberProgress(const FString& StableId, int32 Level, const TArray<FName>& TalentIds, const FString& EquippedLoadoutId)
{
	if (!CanMutateSave() || !IsValid(Campaign) || StableId.IsEmpty())
	{
		return false;
	}

	FRuinmatesMemberSave& Member = Campaign->FindOrAddMember(StableId);
	Member.Level = FMath::Max(1, Level);
	Member.TalentIds = TalentIds;
	Member.EquippedLoadoutId = EquippedLoadoutId;
	return true;
}

bool URuinmatesSaveSubsystem::GetMemberProgress(const FString& StableId, int32& OutLevel, TArray<FName>& OutTalentIds, FString& OutEquippedLoadoutId) const
{
	OutLevel = 0;
	OutTalentIds.Reset();
	OutEquippedLoadoutId.Reset();
	if (!IsValid(Campaign))
	{
		return false;
	}
	if (const FRuinmatesMemberSave* Member = const_cast<URuinmatesSaveGame*>(Campaign.Get())->FindMember(StableId))
	{
		OutLevel = Member->Level;
		OutTalentIds = Member->TalentIds;
		OutEquippedLoadoutId = Member->EquippedLoadoutId;
		return true;
	}
	return false;
}

bool URuinmatesSaveSubsystem::SaveCampaign()
{
	if (!CanMutateSave() || !IsValid(Campaign) || SaveSlotName.IsEmpty())
	{
		return false;
	}
	return UGameplayStatics::SaveGameToSlot(Campaign, SaveSlotName, 0);
}

bool URuinmatesSaveSubsystem::LoadCampaign()
{
	if (!CanMutateSave() || SaveSlotName.IsEmpty())
	{
		return false;
	}
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		return false;
	}
	if (URuinmatesSaveGame* Loaded = Cast<URuinmatesSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0)))
	{
		Campaign = Loaded;
		return true;
	}
	return false;
}

void URuinmatesSaveSubsystem::ResetInMemoryCampaign()
{
	if (!CanMutateSave())
	{
		return;
	}
	Campaign = NewObject<URuinmatesSaveGame>(this, TEXT("Campaign"));
}

FString URuinmatesSaveSubsystem::MakeIdentityKey(const ARuinmatesPlayerState* PlayerState)
{
	if (!IsValid(PlayerState))
	{
		return FString();
	}
	if (PlayerState->GetUniqueId().IsValid())
	{
		return PlayerState->GetUniqueId()->ToString();
	}
	return FString::Printf(TEXT("PlayerId:%d"), PlayerState->GetPlayerId());
}

bool URuinmatesSaveSubsystem::CapturePartyFromWorld(UWorld* World)
{
	if (!CanMutateSave() || !IsValid(World))
	{
		return false;
	}

	bool bCapturedAny = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		ARuinmatesPlayerState* PS = PC ? Cast<ARuinmatesPlayerState>(PC->PlayerState) : nullptr;
		if (!IsValid(PS))
		{
			continue;
		}
		PS->EnsureStableMemberId();
		const FString Key = MakeIdentityKey(PS);
		if (!Key.IsEmpty())
		{
			IdentityToStableId.Add(Key, PS->GetStableMemberId());
		}
		SetMemberProgress(PS->GetStableMemberId(), PS->GetCampaignLevel(), PS->GetTalentIds(), PS->GetEquippedLoadoutId());
		bCapturedAny = true;
	}
	return bCapturedAny;
}

void URuinmatesSaveSubsystem::RestoreOrAssignMember(ARuinmatesPlayerState* PlayerState)
{
	if (!CanMutateSave() || !IsValid(PlayerState))
	{
		return;
	}

	const FString Key = MakeIdentityKey(PlayerState);
	if (const FString* Cached = IdentityToStableId.Find(Key))
	{
		if (!Cached->IsEmpty())
		{
			PlayerState->SetStableMemberId(*Cached);
		}
	}

	PlayerState->EnsureStableMemberId();
	if (!Key.IsEmpty())
	{
		IdentityToStableId.Add(Key, PlayerState->GetStableMemberId());
	}

	int32 Level = 1;
	TArray<FName> Talents;
	FString Loadout;
	if (GetMemberProgress(PlayerState->GetStableMemberId(), Level, Talents, Loadout))
	{
		PlayerState->SetCampaignLevel(Level);
		PlayerState->SetTalentIds(Talents);
		PlayerState->SetEquippedLoadoutId(Loadout);
	}
}
