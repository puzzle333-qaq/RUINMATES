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

	/** Game-thread snapshot consumed by the clean combat graph; no owner property paths. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	float Speed = 0.f;
	/** Signed movement angle relative to the actor, in degrees. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	float MovementDirection = 0.f;
	/** BlendSpace input, adapting existing 1D speed assets and 2D MovementDirection/speed assets. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	float BlendX = 0.f;
	/** Speed axis for 2D locomotion. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	float BlendY = 0.f;
	/** Replicated base aim pitch relative to the body. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	float AimPitch = 0.f;
	/** Replicated base aim yaw relative to the body. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	float AimYaw = 0.f;
	/** Idle, walk, run, sprint, crouch, jump, fall, land; transition rules use these indices. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	int32 LocomotionState = 0;
	/** Whether movement is above the idle threshold. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	bool bIsMoving = false;
	/** Character capsule crouch state. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	bool bIsCrouched = false;
	/** Replicated aim intent. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	bool bIsAiming = false;
	/** Replicated weapon reload state. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	bool bIsReloading = false;
	/** Movement falling state. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	bool bIsFalling = false;
	/** Replicated sprint state. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	bool bIsSprinting = false;
	/** AimOffset weight is zero when no compatible asset is selected. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	float AimOffsetAlpha = 0.f;
	/** Upper-body overlay weight, disabled when unarmed. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	float UpperBodyAlpha = 0.f;
	/** Left-hand target in right-hand bone space, from WeaponPoseComponent. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	FTransform LeftHandTransform;
	/** Smoothed IK weight respecting reload and animation curves. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	float LeftHandAlpha = 0.f;
	/** Sequence used by jump, fall and landing states. */
	UPROPERTY(BlueprintReadOnly, Category = "RuinMates|Anim")
	TObjectPtr<UAnimSequence> AirSequence;
	/** Landing fallback assigned by the animation Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Anim|Fallback")
	TObjectPtr<UAnimSequence> UnarmedLand;

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
	bool bWasInAir = false;
	float LandingTime = 0.f;
	float CrouchAlpha = 0.f;
};

