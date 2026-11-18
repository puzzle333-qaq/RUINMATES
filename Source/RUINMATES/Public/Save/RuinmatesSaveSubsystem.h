#pragma once

#include "CoreMinimal.h"
#include "Save/RuinmatesSaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RuinmatesSaveSubsystem.generated.h"

class ARuinmatesPlayerState;

// Host-authoritative campaign save. Lives on the Game Instance so in-memory data
// survives map travel; SaveCampaign / LoadCampaign also write a slot so the data
// survives a process restart.
UCLASS()
class RUINMATES_API URuinmatesSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Save")
	bool CanMutateSave() const;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Save")
	int32 GetSharedFunds() const;

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	bool SetSharedFunds(int32 NewFunds);

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	bool SetMemberProgress(const FString& StableId, int32 Level, const TArray<FName>& TalentIds, const FString& EquippedLoadoutId);

	UFUNCTION(BlueprintPure, Category = "RuinMates|Save")
	bool GetMemberProgress(const FString& StableId, int32& OutLevel, TArray<FName>& OutTalentIds, FString& OutEquippedLoadoutId) const;

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	bool SaveCampaign();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	bool LoadCampaign();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	void ResetInMemoryCampaign();

	// Copy every player state's campaign fields into the in-memory save and the identity cache.
	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	bool CapturePartyFromWorld(UWorld* World);

	// Reuse a cached stable id when UniqueNetId is missing after travel, then apply saved progress.
	void RestoreOrAssignMember(ARuinmatesPlayerState* PlayerState);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Save")
	FString SaveSlotName = TEXT("RuinMatesCampaign");

protected:
	UPROPERTY()
	TObjectPtr<URuinmatesSaveGame> Campaign;

	// Session-only map from UniqueNetId or "PlayerId:N" to the stable member id.
	TMap<FString, FString> IdentityToStableId;

	static FString MakeIdentityKey(const ARuinmatesPlayerState* PlayerState);
};
