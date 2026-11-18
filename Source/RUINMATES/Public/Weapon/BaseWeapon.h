#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapon/WeaponParams.h"
#include "Weapon/WeaponData.h"
#include "Weapon/WeaponTypes.h"
#include "BaseWeapon.generated.h"

class USkeletalMeshComponent;
class UWeaponData;
class UWeaponMountComponent;
class UAnimMontage;
UENUM(BlueprintType)
enum class ERuinmatesWeaponStatus : uint8
{
	Idle,
	Firing,
	Reloading,
	Unequipped
};


// Hitscan / melee fire with server-authoritative tracing. The server never applies a client HitResult.
UCLASS()
class RUINMATES_API ABaseWeapon : public AActor
{
	GENERATED_BODY()

public:
	ABaseWeapon();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void SetWeaponData(UWeaponData* InData);

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	UWeaponData* GetWeaponData() const { return WeaponData; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	UWeaponMountComponent* GetMount() const { return Mount; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	ERuinmatesWeaponKind GetWeaponKind() const { return WeaponKind; }

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void SetWeaponKind(ERuinmatesWeaponKind InKind) { WeaponKind = InKind; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	int32 GetAmmoInMag() const { return AmmoInMag; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	bool CanFire() const;

	// Requests a shot. Clients send only view origin/direction; the server re-traces itself.
	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void TryFire();

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	int32 GetCarriedAmmo() const { return CarriedAmmo; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	ERuinmatesWeaponStatus GetWeaponStatus() const { return WeaponStatus; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	bool IsReloading() const { return WeaponStatus == ERuinmatesWeaponStatus::Reloading; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	bool CanReload() const;

	// Server-authoritative reload. Clients only send the request.
	UFUNCTION(BlueprintCallable, Category = "RuinMates|Weapon")
	void TryReload();

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	int32 GetFireNotifyId() const { return FireNotifyId; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Weapon")
	int32 GetEquipNotifyId() const { return EquipNotifyId; }

	void NotifyEquipped();
	void NotifyUnequipped();

	// Used when no reload montage is assigned. 0 finishes the reload immediately.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Weapon|Reload", meta = (ClampMin = "0"))
	float ReloadFallbackDuration = 1.5f;


	// Server-side trace. Camera ray is occluded by geometry between the muzzle and the camera hit.
	bool WeaponTrace(FHitResult& OutHit, const FVector& ViewLocation, const FVector& ViewDirection) const;

	FVector GetMuzzleLocation() const;

	FWeaponCombatParams CombatParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Weapon|Trace", meta = (ClampMin = "0"))
	float TraceLength = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Weapon|Trace", meta = (ClampMin = "0"))
	float TraceRadius = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Weapon|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Weapon|Trace")
	FName MuzzleSocketName = TEXT("Muzzle");

	// Reject a client view origin farther than this from the instigator (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuinMates|Weapon|Trace", meta = (ClampMin = "0"))
	float MaxViewOriginDistance = 1000.f;

	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Weapon|Trace")
	FHitResult LastServerHit;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuinMates|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuinMates|Weapon")
	TObjectPtr<UWeaponMountComponent> Mount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Weapon")
	TObjectPtr<UWeaponData> WeaponData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Weapon")
	ERuinmatesWeaponKind WeaponKind = ERuinmatesWeaponKind::Rifle;

	UPROPERTY(Replicated)
	int32 AmmoInMag = 0;

	float LastFireTimeSeconds = -1.0e6f;

	UFUNCTION(Server, Reliable)
	void Server_RequestFire(FVector_NetQuantize ViewLocation, FVector_NetQuantizeNormal ViewDirection);

	UPROPERTY(Replicated)
	int32 CarriedAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponStatus)
	ERuinmatesWeaponStatus WeaponStatus = ERuinmatesWeaponStatus::Idle;

	UPROPERTY(ReplicatedUsing = OnRep_FireNotifyId)
	int32 FireNotifyId = 0;

	UPROPERTY(ReplicatedUsing = OnRep_EquipNotifyId)
	int32 EquipNotifyId = 0;

	FTimerHandle ReloadTimer;

	UFUNCTION()
	void OnRep_WeaponStatus();

	UFUNCTION()
	void OnRep_FireNotifyId();

	UFUNCTION()
	void OnRep_EquipNotifyId();

	UFUNCTION(Server, Reliable)
	void Server_RequestReload();

	void StartReloadOnServer();
	void FinishReloadOnServer();
	void PlayWeaponMontage(UAnimMontage* Montage);
	void PlayFireMontages();
	void BroadcastFireEffect(const FVector& ImpactPoint);
	void SpawnFireEffectLocal(const FVector& ImpactPoint);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireEffect(FVector_NetQuantize ImpactPoint);
	void PlayReloadMontages();
	void PlayEquipMontages(bool bFirstDraw);
	float GetReloadDuration() const;
	EReloadType GetReloadType() const;


	void ApplyWeaponData();
	void FireOnServer(const FVector& ViewLocation, const FVector& ViewDirection);
	bool IsViewOriginTrusted(const FVector& ViewLocation) const;
	void ApplyServerHit(const FHitResult& Hit);
	void BuildQuery(FCollisionQueryParams& OutQuery, FCollisionResponseParams& OutResponse) const;
};
