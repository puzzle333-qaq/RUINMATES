#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Weapon/WeaponData.h"
#include "RuinmatesAnimInstance.generated.h"

class UBlendSpace;

// What the AnimBP should sample this frame. Filled from the pawn and the equipped UWeaponData.
struct RUINMATES_API FRuinmatesAnimChoice
{
	UBlendSpace* BlendSpace = nullptr;
	UAnimSequence* Sequence = nullptr;
	UBlendSpace* AimOffset = nullptr;
	bool bApplyCrouchOffset = false;
};

// One graph for every weapon. NativeUpdateAnimation writes the properties below from the
// equipped weapon's ThirdPersonAnims, and the AnimBP binds its idle / blend space / crouch pins to them.
UCLASS()
class RUINMATES_API URuinmatesAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	URuinmatesAnimInstance();

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// Idle-state sequence, locomotion blend space, aim offset, and whether a procedural crouch is needed.
	static FRuinmatesAnimChoice ChooseAnims(
		const FPlayerAnim& WeaponAnims,
		bool bCrouching,
		bool bInAir,
		bool bAiming,
		float VerticalSpeed,
		UBlendSpace* FallbackBlendSpace,
		UAnimSequence* FallbackIdle,
		UAnimSequence* FallbackJump,
		UAnimSequence* FallbackFall);

	// Bound by the locomotion blend space player. Never left null when the unarmed fallback loaded.
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UBlendSpace> WeaponBlendSpace = nullptr;

	// Bound by the idle sequence player (standing idle, aim idle, or crouch idle).
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UAnimSequence> WeaponSequence = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UBlendSpace> WeaponAimOffset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	float LookYaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	float LookPitch = 0.f;

	// Additive component-space pelvis offset. Zero unless the weapon has no crouch clip.
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	FVector CrouchPelvisOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	bool bHasWeapon = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim|Fallback")
	TObjectPtr<UBlendSpace> UnarmedBlendSpace = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim|Fallback")
	TObjectPtr<UAnimSequence> UnarmedIdle = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim|Fallback")
	TObjectPtr<UAnimSequence> UnarmedJump = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim|Fallback")
	TObjectPtr<UAnimSequence> UnarmedFall = nullptr;

	// How far the pelvis drops when a weapon data asset has no crouch animation.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim|Crouch", meta = (ClampMin = "0"))
	float CrouchPelvisDrop = 45.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim|Crouch", meta = (ClampMin = "0"))
	float CrouchBlendSpeed = 8.f;

private:
	float CrouchAlpha = 0.f;
};
