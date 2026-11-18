#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "RuinmatesGameState.generated.h"

DECLARE_MULTICAST_DELEGATE(FRuinmatesSharedFundsChanged);

// Server-owned team wallet. Every player reads the same value; only the server mutates it.
// The save subsystem keeps the in-memory campaign copy in sync so map travel keeps the balance.
UCLASS()
class RUINMATES_API ARuinmatesGameState : public AGameState
{
	GENERATED_BODY()

public:
	ARuinmatesGameState();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FRuinmatesSharedFundsChanged OnSharedFundsChanged;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Economy")
	int32 GetSharedFunds() const { return SharedFunds; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Economy")
	bool HasEnoughFunds(int32 Amount) const;

	/** Server only. Never goes below zero. Returns false when the balance is insufficient. */
	bool TrySpendFunds(int32 Amount);

	/** Server only. Rejects negative amounts; clamps at INT32_MAX. */
	bool AddFunds(int32 Amount);

	/** Server only. Used by save restore; clamps into [0, INT32_MAX]. */
	bool SetSharedFunds(int32 Amount);

protected:
	UFUNCTION()
	void OnRep_SharedFunds();

	void BroadcastChanged();

	UPROPERTY(ReplicatedUsing = OnRep_SharedFunds)
	int32 SharedFunds = 0;
};
