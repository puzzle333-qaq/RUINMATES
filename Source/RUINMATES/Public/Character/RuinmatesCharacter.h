#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RuinmatesCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStatsComponent;
class UWeaponPoseComponent;
class URuinmatesBackpackComponent;
class URuinmatesTerminalScreen;
class ARuinmatesLootContainer;
class UWeaponInventoryComponent;
class ABaseWeapon;
class UAnimMontage;
class UInputComponent;
class URuinmatesCrosshairWidget;
class ARuinmatesMerchant;
class URuinmatesTradeScreen;

// Third-person character: movement and stamina only. Not a port of AShooterCharacter.
// Weapon pose, mounting, and firing live in separate components.
UCLASS()
class RUINMATES_API ARuinmatesCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARuinmatesCharacter();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Movement")
	void StartSprint();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Movement")
	void StopSprint();

	UFUNCTION(BlueprintPure, Category = "RuinMates|Movement")
	bool IsSprinting() const { return bWantsToSprint; }

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Movement")
	void ToggleCrouch();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Movement")
	void StartAim();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Movement")
	void StopAim();

	UFUNCTION(BlueprintPure, Category = "RuinMates|Movement")
	bool IsAiming() const { return bWantsToAim; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Stats")
	UStatsComponent* GetStats() const { return Stats; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Camera")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	UWeaponPoseComponent* GetWeaponPose() const { return WeaponPose; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	UWeaponInventoryComponent* GetWeaponInventory() const { return WeaponInventory; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Backpack")
	URuinmatesBackpackComponent* GetBackpack() const { return Backpack; }

	void ToggleBackpack();
	void CloseTerminal();
	void TryInteract();
	void BeginSearch(ARuinmatesLootContainer* Container);
	void RequestPickup(int32 SlotIndex);
	void RequestTakeAllLoot();
	void RequestDiscard(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	ABaseWeapon* GetEquippedWeapon() const { return EquippedWeapon; }

	// Server-authoritative equip. Binds mount + pose and plays the third-person equip montage when present.
	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void EquipWeapon(ABaseWeapon* NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void UnequipWeapon();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void TryFireWeapon();

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void TryReloadWeapon();

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	bool IsWeaponReloading() const;

	// ---- Trading -------------------------------------------------------------
	// Server starts a trade with a merchant; the owning client gets the trade screen opened
	// through OnRep_ActiveMerchant. Only one interaction UI is open at a time.
	void BeginTrade(ARuinmatesMerchant* Merchant);
	void EndTrade();

	UFUNCTION(BlueprintPure, Category = "RuinMates|Trade")
	ARuinmatesMerchant* GetActiveMerchant() const { return ActiveMerchant; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Trade")
	bool IsTrading() const { return ActiveMerchant != nullptr; }

	// Client requests. Only the item id / slot and quantity cross the wire; the server prices them.
	void RequestBuyItem(FName ItemId, int32 Count);
	void RequestSellItem(int32 SlotIndex, int32 Count);

	// Body montages from the equipped weapon data. The server calls this directly.
	// Clients call it from weapon OnRep so the third-person fire/reload actually plays.
	void PlayEquippedMontages(bool bJustEquipped, bool bJustFired, bool bJustReloaded);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuinMates|Stats")
	TObjectPtr<UStatsComponent> Stats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuinMates|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuinMates|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuinMates|Weapon")
	TObjectPtr<UWeaponPoseComponent> WeaponPose;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuinMates|Weapon")
	TObjectPtr<UWeaponInventoryComponent> WeaponInventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuinMates|Backpack")
	TObjectPtr<URuinmatesBackpackComponent> Backpack;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveSearch)
	TObjectPtr<ARuinmatesLootContainer> ActiveSearch;

	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon)
	TObjectPtr<ABaseWeapon> EquippedWeapon;


	// Speeds taken from the pre-trim third-person locomotion (not CQB TacticalTrim zeros).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Movement")
	float RunSpeed = 220.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Movement")
	float SprintSpeed = 320.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Movement", meta = (ClampMin = "0"))
	float CrouchSpeedMultiplier = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Movement", meta = (ClampMin = "0.1", ClampMax = "1"))
	float AimSpeedMultiplier = 0.5f;

	// Over-the-shoulder pose. The orbit point sits at the shoulder, and the camera stays behind it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Camera", meta = (ClampMin = "40"))
	float AimArmLength = 170.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Camera")
	FVector AimSocketOffset = FVector(0.f, 55.f, 35.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Camera")
	FVector AimTargetOffset = FVector(15.f, 20.f, 55.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Camera", meta = (ClampMin = "0.1"))
	float AimCameraInterpSpeed = 8.f;

	UPROPERTY(ReplicatedUsing = OnRep_WantsToSprint)
	bool bWantsToSprint = false;

	UPROPERTY(ReplicatedUsing = OnRep_WantsToAim)
	bool bWantsToAim = false;

	void BindStatsDelegates();
	void ApplyMovementSpeed();
	void UpdateArmedFacing();
	void UpdateAimCamera(float DeltaSeconds);
	void EnsureCrosshair();
	void UpdateCrosshairVisibility();
	void BindAimInput(UInputComponent* PlayerInputComponent);
	void BindInventoryInput(UInputComponent* PlayerInputComponent);
	void OpenInventoryScreen();
	void CloseInventoryScreen();
	void EndSearch();
	void UpdateSearchDistance();

	UFUNCTION()
	void OnRep_ActiveSearch();

	UFUNCTION(Server, Reliable)
	void Server_InteractActor(AActor* Target);

	UFUNCTION(Server, Reliable)
	void Server_StopSearch();

	UFUNCTION(Server, Reliable)
	void Server_PickupLoot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void Server_TakeAllLoot();

	UFUNCTION(Server, Reliable)
	void Server_DiscardBackpackSlot(int32 SlotIndex);

	UFUNCTION()
	void HandleStaminaDepleted();

	UFUNCTION()
	void OnRep_WantsToSprint();

	UFUNCTION()
	void OnRep_WantsToAim();

	UFUNCTION(Server, Reliable)
	void Server_SetWantsToSprint(bool bNewWantsToSprint);

	UFUNCTION(Server, Reliable)
	void Server_SetWantsToAim(bool bNewWantsToAim);

	void SetWantsToSprintLocal(bool bNewWantsToSprint);
	void SetWantsToAimLocal(bool bNewWantsToAim);

	UFUNCTION()
	void OnRep_EquippedWeapon(ABaseWeapon* PreviousWeapon);

	// ---- Trading (protected plumbing) ---------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_ActiveMerchant)
	TObjectPtr<ARuinmatesMerchant> ActiveMerchant;

	UPROPERTY()
	TObjectPtr<URuinmatesTradeScreen> TradeScreen;

	UFUNCTION()
	void OnRep_ActiveMerchant();

	UFUNCTION(Server, Reliable)
	void Server_BuyItem(FName ItemId, int32 Count);

	UFUNCTION(Server, Reliable)
	void Server_SellItem(int32 SlotIndex, int32 Count);

	UFUNCTION(Server, Reliable)
	void Server_EndTrade();

	UFUNCTION(Client, Reliable)
	void Client_TradeResult(bool bSuccess, const FText& Message);

	void OpenTradeScreen();
	void CloseTradeScreen();

	// Server only: merchant still valid and in range.
	bool ValidateTradeState(ARuinmatesMerchant*& OutMerchant);

	UFUNCTION(Server, Reliable)
	void Server_EquipWeapon(ABaseWeapon* NewWeapon);

	UFUNCTION(Server, Reliable)
	void Server_UnequipWeapon();

	void BindEquippedWeapon(ABaseWeapon* PreviousWeapon);
	void PlayBodyMontage(UAnimMontage* Montage);

	float HipArmLength = 400.f;
	FVector HipSocketOffset = FVector::ZeroVector;
	FVector HipTargetOffset = FVector::ZeroVector;
	float AimCameraAlpha = 0.f;

	UPROPERTY()
	TObjectPtr<URuinmatesCrosshairWidget> CrosshairWidget;

	UPROPERTY()
	TObjectPtr<URuinmatesTerminalScreen> InventoryScreen;
};
