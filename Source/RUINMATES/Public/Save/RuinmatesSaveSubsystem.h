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

	/**
	 * Authority-only Hub transaction persistence. Clones the existing campaign, captures player
	 * backpacks, writes/validates the inactive A/B generation, then replaces the cached snapshot.
	 * Failure leaves the previous cache and last good generation intact; never overwrites legacy.
	 */
	bool PersistHubState(const FRuinmatesHubSnapshot& Hub, const TArray<FRuinmatesItemStack>& Warehouse, UWorld* World);

	/** Returns durable saved facts only; StateActor recomputes runtime before setting initialized. */
	bool GetSavedHubState(FRuinmatesHubSnapshot& OutHub, TArray<FRuinmatesItemStack>& OutWarehouse) const;

	/** Last complete cached campaign snapshot. Callers must not mutate or use it as live Hub state. */
	const URuinmatesSaveGame* GetCampaignSnapshot() const { return Campaign; }

	/** Invalid/unsupported disk profiles cannot be silently replaced with a fresh default Hub. */
	bool IsProfileLoadBlocked() const { return !BlockedProfileName.IsEmpty() && BlockedProfileName==SaveSlotName; }

#if WITH_DEV_AUTOMATION_TESTS
	/** One-shot failure before any candidate mutation or disk write, for manager rollback tests. */
	bool bFailNextHubSaveForTest = false;
#endif

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	void ResetInMemoryCampaign();

	// Copy every player state's campaign fields into the in-memory save and the identity cache.
	UFUNCTION(BlueprintCallable, Category = "RuinMates|Save")
	bool CapturePartyFromWorld(UWorld* World);

	// Reuse a cached stable id when UniqueNetId is missing after travel, then apply saved progress.
	void RestoreOrAssignMember(ARuinmatesPlayerState* PlayerState);

	/** Restores the possessed member's canonical saved inventory after GameMode spawns a pawn. */
	bool RestoreBackpack(class ARuinmatesCharacter* Character);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Save")
	FString SaveSlotName = TEXT("RuinMatesCampaign");

protected:
	UPROPERTY()
	TObjectPtr<URuinmatesSaveGame> Campaign;

	// Session-only map from UniqueNetId or "PlayerId:N" to the stable member id.
	TMap<FString, FString> IdentityToStableId;

	/** The last validated generation's exact slot; the next save writes the other slot. */
	FString ActiveGenerationSlot;
	FString ActiveProfileName;
	FString BlockedProfileName;

	/** Normalizes a candidate in isolation; unsupported schemas or invalid data fail closed. */
	bool ValidateAndNormalizeCampaign(URuinmatesSaveGame* Candidate) const;

	/** Captures world party/backpack data into a clone, without replacing the cached campaign. */
	bool CaptureWorldIntoCandidate(UWorld* World, URuinmatesSaveGame* Candidate, bool& bOutCapturedAny) const;

	/** Writes, reloads and compares a whole campaign before publishing its new generation. */
	bool WriteCandidateGeneration(URuinmatesSaveGame* Candidate);

	/** Stable local host uses profile identity; remote members continue to use their server identity. */
	FString ResolveMemberId(ARuinmatesPlayerState* PlayerState, const URuinmatesSaveGame* Candidate) const;

	static FString MakeIdentityKey(const ARuinmatesPlayerState* PlayerState);
};
