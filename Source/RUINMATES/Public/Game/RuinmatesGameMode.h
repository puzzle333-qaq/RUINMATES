#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "RuinmatesGameMode.generated.h"

// Listen-server cooperative mode: 1-4 players. Does not start a dedicated server.
UCLASS()
class RUINMATES_API ARuinmatesGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ARuinmatesGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Session")
	int32 GetMaxPartySize() const { return MaxPartySize; }

	/** Multiplier applied when a player damages a teammate. Config/blueprint overridable. */
	UPROPERTY(EditDefaultsOnly, Config, BlueprintReadOnly, Category = "RuinMates|Combat", meta = (ClampMin = "0", ClampMax = "1"))
	float FriendlyFireMultiplier = 0.25f;

public:
	/** Clamped friendly-fire multiplier (0 disables teammate damage). */
	UFUNCTION(BlueprintPure, Category = "RuinMates|Combat")
	float GetFriendlyFireMultiplier() const { return FMath::Clamp(FriendlyFireMultiplier, 0.f, 1.f); }

	/** True when both actors are different player characters, i.e. the same co-op team. */
	UFUNCTION(BlueprintPure, Category = "RuinMates|Combat")
	static bool AreTeammates(const AActor* First, const AActor* Second);

	/**
	 * Server-side damage scaling: teammates take BaseDamage * multiplier, everyone else takes the
	 * full amount. Pure so it can be unit tested.
	 */
	UFUNCTION(BlueprintPure, Category = "RuinMates|Combat")
	float ScaleDamageForTarget(float BaseDamage, const AActor* DamageInstigator, const AActor* DamageVictim) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Session", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxPartySize = 4;
};