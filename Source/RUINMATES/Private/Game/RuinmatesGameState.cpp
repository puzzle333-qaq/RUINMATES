#include "Game/RuinmatesGameState.h"

#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"
#include "Save/RuinmatesSaveSubsystem.h"

ARuinmatesGameState::ARuinmatesGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ARuinmatesGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARuinmatesGameState, SharedFunds);
}

void ARuinmatesGameState::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// Restore the campaign balance once, when the map starts. Clients receive it by replication.
	UGameInstance* GameInstance = GetGameInstance();
	URuinmatesSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<URuinmatesSaveSubsystem>() : nullptr;
	if (Save)
	{
		SharedFunds = FMath::Clamp(Save->GetSharedFunds(), 0, MAX_int32);
	}
	BroadcastChanged();
}

bool ARuinmatesGameState::HasEnoughFunds(int32 Amount) const
{
	return Amount >= 0 && SharedFunds >= Amount;
}

bool ARuinmatesGameState::TrySpendFunds(int32 Amount)
{
	if (!HasAuthority() || Amount < 0)
	{
		return false;
	}
	if (Amount == 0)
	{
		return true;
	}
	if (SharedFunds < Amount)
	{
		return false;
	}

	SharedFunds -= Amount;
	BroadcastChanged();
	return true;
}

bool ARuinmatesGameState::AddFunds(int32 Amount)
{
	if (!HasAuthority() || Amount < 0)
	{
		return false;
	}
	if (Amount == 0)
	{
		return true;
	}

	const int64 Sum = static_cast<int64>(SharedFunds) + static_cast<int64>(Amount);
	SharedFunds = static_cast<int32>(FMath::Min<int64>(Sum, MAX_int32));
	BroadcastChanged();
	return true;
}

bool ARuinmatesGameState::SetSharedFunds(int32 Amount)
{
	if (!HasAuthority())
	{
		return false;
	}
	SharedFunds = FMath::Clamp(Amount, 0, MAX_int32);
	BroadcastChanged();
	return true;
}

void ARuinmatesGameState::OnRep_SharedFunds()
{
	BroadcastChanged();
}

void ARuinmatesGameState::BroadcastChanged()
{
	// Keep the in-memory campaign copy aligned without touching disk on every transaction.
	if (HasAuthority())
	{
		UGameInstance* GameInstance = GetGameInstance();
		if (URuinmatesSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<URuinmatesSaveSubsystem>() : nullptr)
		{
			Save->SetSharedFunds(SharedFunds);
		}
	}

	OnSharedFundsChanged.Broadcast();
}
