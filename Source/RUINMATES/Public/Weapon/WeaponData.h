#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Engine/DataAsset.h"
#include "Engine/SkeletalMesh.h"
#include "NiagaraSystem.h"
#include "GameplayTagContainer.h"
#include "Weapon/WeaponParams.h"
class UTexture2D;

#include "WeaponData.generated.h"

// Which reload montage to resolve. Presentation / mag-swap logic lives elsewhere.
UENUM(BlueprintType)
enum class EReloadType : uint8
{
	Tactical UMETA(DisplayName = "Tactical (mag not empty)"),
	Empty    UMETA(DisplayName = "Empty"),
	First    UMETA(DisplayName = "First Reload")
};

// Third-person locomotion / pose mapping. 1P pose layers are not included.
USTRUCT(BlueprintType)
struct RUINMATES_API FPlayerAnim
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UAnimSequence> IdleAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UAnimSequence> AimIdleAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UAnimSequence> CrouchIdleAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UAnimSequence> SprintAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UBlendSpace> AimOffset_Hip = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UBlendSpace> AimOffset_Aim = nullptr;

	// Locomotion blend spaces use the mannequin axes: X = direction (-180..180), Y = speed (cm/s).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UBlendSpace> Strafe_Standing = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UBlendSpace> Strafe_Crouching = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UBlendSpace> Strafe_Walking = nullptr;

	UAnimSequence* GetIdleAnim() const { return IdleAnim.Get(); }
	UAnimSequence* GetAimIdleAnim() const { return AimIdleAnim ? AimIdleAnim.Get() : IdleAnim.Get(); }
};

USTRUCT(BlueprintType)
struct RUINMATES_API FMontageData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equip")
	TObjectPtr<UAnimMontage> EquipMontage = nullptr;

	// First draw this session. Falls back to EquipMontage when null.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equip")
	TObjectPtr<UAnimMontage> FirstEquipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	TObjectPtr<UAnimMontage> FireMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reload")
	TObjectPtr<UAnimMontage> EmptyReloadMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reload")
	TObjectPtr<UAnimMontage> TacticalReloadMontage = nullptr;

	// Optional random variants. Empty entries are ignored.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Variants")
	TArray<TObjectPtr<UAnimMontage>> FireMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Variants")
	TArray<TObjectPtr<UAnimMontage>> EquipMontages;

	UAnimMontage* ResolveFireMontage() const
	{
		TArray<UAnimMontage*> Candidates;
		for (const TObjectPtr<UAnimMontage>& Montage : FireMontages)
		{
			if (Montage)
			{
				Candidates.Add(Montage.Get());
			}
		}
		if (FireMontage)
		{
			Candidates.AddUnique(FireMontage.Get());
		}
		if (Candidates.Num() == 0)
		{
			return nullptr;
		}
		return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	}

	UAnimMontage* ResolveEquipMontage(bool bFirstDraw = false) const
	{
		if (bFirstDraw && FirstEquipMontage)
		{
			return FirstEquipMontage.Get();
		}

		TArray<UAnimMontage*> Candidates;
		for (const TObjectPtr<UAnimMontage>& Montage : EquipMontages)
		{
			if (Montage)
			{
				Candidates.Add(Montage.Get());
			}
		}
		if (EquipMontage)
		{
			Candidates.AddUnique(EquipMontage.Get());
		}
		if (Candidates.Num() == 0)
		{
			return FirstEquipMontage.Get();
		}
		return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	}

	UAnimMontage* ResolveReloadMontage(EReloadType Type) const
	{
		if (Type == EReloadType::Empty)
		{
			if (EmptyReloadMontage) return EmptyReloadMontage.Get();
			if (TacticalReloadMontage) return TacticalReloadMontage.Get();
			return nullptr;
		}
		if (Type == EReloadType::Tactical)
		{
			if (TacticalReloadMontage) return TacticalReloadMontage.Get();
			if (EmptyReloadMontage) return EmptyReloadMontage.Get();
			return nullptr;
		}
		if (EmptyReloadMontage) return EmptyReloadMontage.Get();
		if (TacticalReloadMontage) return TacticalReloadMontage.Get();
		return nullptr;
	}
};

// Per-weapon visual / animation data asset. Not a type-keyed category table.
// Mesh and montage refs may be null; resolve helpers return nullptr instead of crashing.
UCLASS(BlueprintType)
class RUINMATES_API UWeaponData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Weapon")
	FGameplayTag WeaponType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Weapon")
	TObjectPtr<USkeletalMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Weapon")
	FName MeshAttachSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Weapon")
	FName LeftHandSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Weapon")
	FWeaponCombatParams CombatParams;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Weapon|ThirdPerson")
	FPlayerAnim ThirdPersonAnims;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Weapon|ThirdPerson")
	FMontageData ThirdPersonMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Weapon|WeaponMesh")
	FMontageData WeaponMontages;

	// Muzzle flash. Empty uses NS_WeaponFire. Spawned on the muzzle socket when a shot is fired.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Weapon|Effects")
	TObjectPtr<UNiagaraSystem> FireEffect = nullptr;

	// Optional tracer from the muzzle to the impact point. Empty uses NS_WeaponFire_Tracer.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Weapon|Effects")
	TObjectPtr<UNiagaraSystem> TracerEffect = nullptr;

	// Optional field-HUD portrait for this weapon. Empty keeps the neutral weapon glyph; the icon is
	// presentation-only and never used for gameplay or replication.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Weapon|HUD")
	TSoftObjectPtr<UTexture2D> HUDIcon;

	USkeletalMesh* GetWeaponMesh() const { return WeaponMesh.Get(); }
};
