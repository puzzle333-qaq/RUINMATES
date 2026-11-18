#include "Game/RuinmatesPlayerState.h"

#include "Net/UnrealNetwork.h"

ARuinmatesPlayerState::ARuinmatesPlayerState()
{
	CampaignLevel = 1;
}

void ARuinmatesPlayerState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		EnsureStableMemberId();
	}
}

void ARuinmatesPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARuinmatesPlayerState, StableMemberId);
	DOREPLIFETIME(ARuinmatesPlayerState, CampaignLevel);
	DOREPLIFETIME(ARuinmatesPlayerState, EquippedLoadoutId);
	DOREPLIFETIME(ARuinmatesPlayerState, TalentIds);
}

void ARuinmatesPlayerState::EnsureStableMemberId()
{
	if (!StableMemberId.IsEmpty())
	{
		return;
	}

	const FString Unique = GetUniqueId().IsValid() ? GetUniqueId()->ToString() : FString();
	StableMemberId = Unique.IsEmpty() ? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens) : Unique;
}

void ARuinmatesPlayerState::SetStableMemberId(const FString& InId)
{
	if (HasAuthority())
	{
		StableMemberId = InId;
	}
}

void ARuinmatesPlayerState::SetCampaignLevel(int32 InLevel)
{
	if (HasAuthority())
	{
		CampaignLevel = FMath::Max(1, InLevel);
	}
}

void ARuinmatesPlayerState::SetEquippedLoadoutId(const FString& InId)
{
	if (HasAuthority())
	{
		EquippedLoadoutId = InId;
	}
}

void ARuinmatesPlayerState::SetTalentIds(const TArray<FName>& InTalents)
{
	if (HasAuthority())
	{
		TalentIds = InTalents;
	}
}
