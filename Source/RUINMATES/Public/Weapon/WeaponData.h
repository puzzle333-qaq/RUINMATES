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

// Third-person animation data. Final layout: five assets per weapon, everything else is global.
USTRUCT(BlueprintType)
struct RUINMATES_API FPlayerAnim
{
	GENERATED_BODY()

	// Shared持枪基础姿态 across weapons of the same type.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UAnimSequence> Idle = nullptr;


	// Sprint pose: armed upper body while sprinting. Optional; falls back to Idle.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UAnimSequence> Sprint = nullptr;

	// ADS pose: armed upper body while aiming. Optional; falls back to Idle.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UAnimSequence> AimIdle = nullptr;
	// Aim direction correction; shared across weapons when the pose matches.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UBlendSpace> AimOffset = nullptr;

	UAnimSequence* GetIdleAnim() const { return Idle.Get(); }
};

USTRUCT(BlueprintType)
struct RUINMATES_API FMontageData
{
	GENERATED_BODY()

	// One equip montage for first draw and every later equip.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equip")
	TObjectPtr<UAnimMontage> Equip = nullptr;

	// One reload montage for both empty and tactical reloads.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reload")
	TObjectPtr<UAnimMontage> Reload = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	TObjectPtr<UAnimMontage> Fire = nullptr;

	UAnimMontage* ResolveEquipMontage(bool bFirstDraw = false) const { return Equip.Get(); }
	UAnimMontage* ResolveReloadMontage(EReloadType /*Type*/ = EReloadType::Tactical) const { return Reload.Get(); }
	UAnimMontage* ResolveFireMontage() const { return Fire.Get(); }
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