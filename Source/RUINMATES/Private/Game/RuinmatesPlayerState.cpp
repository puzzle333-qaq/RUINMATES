#include "Game/RuinmatesPlayerState.h"

#include "Game/RuinmatesGameState.h"
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
	// Legacy entry point kept so older blueprints and tests still compile. The wallet itself lives
	// on the game state; this only forwards to it so there is a single source of truth.
	if (!HasAuthority() || Amount == 0 || !GetWorld())
	{
		return;
	}
	if (ARuinmatesGameState* RuinState = GetWorld()->GetGameState<ARuinmatesGameState>())
	{
		if (Amount > 0)
		{
			RuinState->AddFunds(Amount);
		}
		else
		{
			RuinState->TrySpendFunds(-Amount);
		}
	}
}

int32 ARuinmatesPlayerState::GetTeamFunds() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const ARuinmatesGameState* RuinState = World->GetGameState<ARuinmatesGameState>())
		{
			return RuinState->GetSharedFunds();
		}
	}
	return 0;
}
