#include "Game/RuinmatesPlayerState.h"

#include "GameFramework/GameStateBase.h"
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
	DOREPLIFETIME(ARuinmatesPlayerState, TeamFunds);
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

void ARuinmatesPlayerState::AddTeamFunds(int32 Amount)
{
	if (!HasAuthority() || Amount == 0)
	{
		return;
	}

	AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		TeamFunds = FMath::Max(0, TeamFunds + Amount);
		return;
	}

	for (APlayerState* Other : GameState->PlayerArray)
	{
		if (ARuinmatesPlayerState* Member = Cast<ARuinmatesPlayerState>(Other))
		{
			Member->TeamFunds = FMath::Max(0, Member->TeamFunds + Amount);
		}
	}
}
