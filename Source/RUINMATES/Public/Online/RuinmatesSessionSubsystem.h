#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RuinmatesSessionSubsystem.generated.h"

UENUM(BlueprintType)
enum class ERuinmatesTravelStage : uint8
{
	Base,
	DangerZone
};

// Listen-server party and map flow: Base -> Danger Zone -> Base.
// Platform rooms (EOS / Steam) are an optional boundary: this default path uses the engine Null
// online layer or a local listen URL, and never claims a real platform lobby is available.
UCLASS()
class RUINMATES_API URuinmatesSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Session")
	int32 GetMaxPartySize() const { return MaxPartySize; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Session")
	bool IsInParty() const { return bInParty; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Session")
	bool IsPartyHost() const { return bIsHost; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Session")
	ERuinmatesTravelStage GetTravelStage() const { return TravelStage; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Session")
	FString GetStatusText() const { return StatusText; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Session")
	FName GetActiveOnlineSubsystemName() const { return ActiveOnlineSubsystemName; }

	// True only when a non-Null online subsystem is actually present at runtime.
	UFUNCTION(BlueprintPure, Category = "RuinMates|Session")
	bool IsUsingPlatformOnline() const;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Session")
	bool CanOpenPlatformInvite() const { return IsUsingPlatformOnline() && bInParty; }

	// Host a 1-4 player listen party. Does not travel by itself.
	// This is not a platform room unless IsUsingPlatformOnline() is true (currently expected to be false).
	UFUNCTION(BlueprintCallable, Category = "RuinMates|Session")
	bool HostListenParty();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Session")
	void LeaveParty();

	// Returns false on the Null online layer. There is no fake invite UI.
	UFUNCTION(BlueprintCallable, Category = "RuinMates|Session")
	bool OpenPlatformInvite();

	// Host-only map travel. Maps must be set; missing names fail instead of opening a dummy room.
	UFUNCTION(BlueprintCallable, Category = "RuinMates|Session")
	bool TravelToDangerZone();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Session")
	bool TravelToBase();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RuinMates|Session")
	int32 MaxPartySize = 4;

	// Set these to the packaged base / danger maps. Empty names refuse travel.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RuinMates|Session")
	FName BaseMapName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RuinMates|Session")
	FName DangerMapName;

protected:
	bool bInParty = false;
	bool bIsHost = false;
	ERuinmatesTravelStage TravelStage = ERuinmatesTravelStage::Base;
	FString StatusText;
	FName ActiveOnlineSubsystemName = NAME_None;

	void RefreshOnlineSubsystemName();
	bool HostServerTravel(FName MapName, ERuinmatesTravelStage NewStage);
};
