#pragma once

#include "CoreMinimal.h"
#include "WeaponParams.generated.h"

// One shot's kick in degrees. Positive pitch looks up. Positive yaw looks right.
USTRUCT(BlueprintType)
struct RUINMATES_API FRecoilStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Recoil", meta = (DisplayName = "Pitch (Up +, Down -)"))
	float Pitch = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Recoil", meta = (DisplayName = "Yaw (Right +, Left -)"))
	float Yaw = 0.f;

	FRecoilStep() = default;
	FRecoilStep(float InPitch, float InYaw) : Pitch(InPitch), Yaw(InYaw) {}
};

UENUM(BlueprintType)
enum class EFireType : uint8
{
	Single     UMETA(DisplayName = "Single"),
	Automatic  UMETA(DisplayName = "Automatic"),
	Burst      UMETA(DisplayName = "Burst")
};

USTRUCT(BlueprintType)
struct RUINMATES_API FWeaponFireParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Fire")
	EFireType FireType = EFireType::Single;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Fire")
	bool bCanSwitchFireMode = false;

	// Empty = the owner can decide defaults from weapon kind later.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Fire")
	TArray<EFireType> AllowedFireModes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Fire", meta = (ClampMin = "2"))
	int32 BurstCount = 3;

	// Seconds between burst shots. 0 = use FireTime.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Fire", meta = (ClampMin = "0"))
	float BurstShotInterval = 0.f;

	// Seconds between shots (fire rate).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Fire", meta = (ClampMin = "0", DisplayName = "Fire Time (s)"))
	float FireTime = 0.1f;

	float GetBurstShotInterval() const
	{
		return BurstShotInterval > 0.f ? BurstShotInterval : FireTime;
	}

	bool AllowsFireType(EFireType Type) const
	{
		if (AllowedFireModes.Num() == 0)
		{
			return Type == FireType;
		}
		return AllowedFireModes.Contains(Type);
	}
};

USTRUCT(BlueprintType)
struct RUINMATES_API FWeaponSpreadParams
{
	GENERATED_BODY()

	// Hip fire is aimed at the view direction. Kept at zero so old data assets do not add a random cone.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Spread", meta = (ClampMin = "0", ClampMax = "30"))
	float HipFireSpread = 0.f;
};

USTRUCT(BlueprintType)
struct RUINMATES_API FWeaponAmmoParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Ammo", meta = (ClampMin = "0"))
	int32 MagCapacity = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Ammo", meta = (ClampMin = "0"))
	int32 StartingAmmoInMag = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Ammo", meta = (ClampMin = "0"))
	int32 StartingCarriedAmmo = 90;

	// Pellets per trigger pull. 1 = a single bullet.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Ammo", meta = (ClampMin = "1", ClampMax = "32"))
	int32 PelletsPerShot = 1;

	bool UsesAmmo() const { return MagCapacity > 0; }

	int32 GetClampedStartingAmmoInMag() const
	{
		return FMath::Clamp(StartingAmmoInMag, 0, FMath::Max(0, MagCapacity));
	}
};

USTRUCT(BlueprintType)
struct RUINMATES_API FWeaponRecoilParams
{
	GENERATED_BODY()

	// Shot order. After the last entry, the last step is repeated.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Recoil")
	TArray<FRecoilStep> RecoilPattern;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Recoil", meta = (ClampMin = "0"))
	float RecoilPitchScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Recoil", meta = (ClampMin = "0"))
	float RecoilYawScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Recoil", meta = (ClampMin = "0"))
	float AimRecoilMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Recoil", meta = (ClampMin = "0"))
	float RecoilRecoverySpeed = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Recoil", meta = (ClampMin = "0"))
	float RecoilRecoveryDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Recoil", meta = (ClampMin = "0"))
	float RecoilResetTime = 0.4f;

	FRecoilStep GetStep(int32 ShotIndex) const
	{
		if (RecoilPattern.Num() == 0)
		{
			return FRecoilStep();
		}
		const int32 Index = FMath::Clamp(ShotIndex, 0, RecoilPattern.Num() - 1);
		return RecoilPattern[Index];
	}
};

USTRUCT(BlueprintType)
struct RUINMATES_API FWeaponDamageParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Damage")
	float Damage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Damage", meta = (ClampMin = "0"))
	float HeadshotMultiplier = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Damage", meta = (ClampMin = "0"))
	float LimbMultiplier = 0.8f;
};

// Pure combat parameter bundle. No reload / magazine presentation, no 1P visual recoil.
USTRUCT(BlueprintType)
struct RUINMATES_API FWeaponCombatParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Fire")
	FWeaponFireParams Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Spread")
	FWeaponSpreadParams Spread;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Ammo")
	FWeaponAmmoParams Ammo;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Recoil")
	FWeaponRecoilParams Recoil;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Damage")
	FWeaponDamageParams Damage;
};
