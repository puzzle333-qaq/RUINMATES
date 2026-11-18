#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RuinmatesPlayerState.generated.h"

// Replicated per-player campaign identity. The stable id is what the host save uses as the member key.
UCLASS()
class RUINMATES_API ARuinmatesPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ARuinmatesPlayerState();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Save")
	FString GetStableMemberId() const { return StableMemberId; }

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	void SetStableMemberId(const FString& InId);

	UFUNCTION(BlueprintPure, Category = "RuinMates|Save")
	int32 GetCampaignLevel() const { return CampaignLevel; }

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	void SetCampaignLevel(int32 InLevel);

	UFUNCTION(BlueprintPure, Category = "RuinMates|Save")
	FString GetEquippedLoadoutId() const { return EquippedLoadoutId; }

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	void SetEquippedLoadoutId(const FString& InId);

	UFUNCTION(BlueprintPure, Category = "RuinMates|Save")
	const TArray<FName>& GetTalentIds() const { return TalentIds; }

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	void SetTalentIds(const TArray<FName>& InTalents);

	void EnsureStableMemberId();

protected:
	UPROPERTY(Replicated)
	FString StableMemberId;

	UPROPERTY(Replicated)
	int32 CampaignLevel = 1;

	UPROPERTY(Replicated)
	FString EquippedLoadoutId;

	UPROPERTY(Replicated)
	TArray<FName> TalentIds;
};
