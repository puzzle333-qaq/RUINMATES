#include "Animation/RuinmatesAnimInstance.h"

#include "Character/RuinmatesCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapon/BaseWeapon.h"

URuinmatesAnimInstance::URuinmatesAnimInstance()
{
	static ConstructorHelpers::FObjectFinder<UBlendSpace> BlendSpaceFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.BS_Idle_Walk_Run"));
	if (BlendSpaceFinder.Succeeded())
	{
		UnarmedBlendSpace = BlendSpaceFinder.Object;
		WeaponBlendSpace = UnarmedBlendSpace;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	if (IdleFinder.Succeeded())
	{
		UnarmedIdle = IdleFinder.Object;
		WeaponSequence = UnarmedIdle;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump.MM_Jump"));
	if (JumpFinder.Succeeded())
	{
		UnarmedJump = JumpFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> FallFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Fall_Loop.MM_Fall_Loop"));
	if (FallFinder.Succeeded())
	{
		UnarmedFall = FallFinder.Object;
	}
}

FRuinmatesAnimChoice URuinmatesAnimInstance::ChooseAnims(
	const FPlayerAnim& WeaponAnims,
	bool bCrouching,
	bool bInAir,
	bool bAiming,
	float VerticalSpeed,
	UBlendSpace* FallbackBlendSpace,
	UAnimSequence* FallbackIdle,
	UAnimSequence* FallbackJump,
	UAnimSequence* FallbackFall)
{
	FRuinmatesAnimChoice Choice;

	if (bCrouching && WeaponAnims.Strafe_Crouching)
	{
		Choice.BlendSpace = WeaponAnims.Strafe_Crouching;
	}
	else if (WeaponAnims.Strafe_Standing)
	{
		Choice.BlendSpace = WeaponAnims.Strafe_Standing;
	}
	else if (WeaponAnims.Strafe_Walking)
	{
		Choice.BlendSpace = WeaponAnims.Strafe_Walking;
	}
	else
	{
		Choice.BlendSpace = FallbackBlendSpace;
	}

	const bool bHasCrouchClip = bCrouching && (WeaponAnims.CrouchIdleAnim || WeaponAnims.Strafe_Crouching);
	Choice.bApplyCrouchOffset = bCrouching && !bInAir && !bHasCrouchClip;

	if (bInAir)
	{
		Choice.Sequence = VerticalSpeed > 0.f ? FallbackJump : FallbackFall;
	}
	else if (bCrouching && WeaponAnims.CrouchIdleAnim)
	{
		Choice.Sequence = WeaponAnims.CrouchIdleAnim;
	}
	else if (!bCrouching && bAiming && WeaponAnims.AimIdleAnim)
	{
		Choice.Sequence = WeaponAnims.AimIdleAnim;
	}
	else if (WeaponAnims.IdleAnim)
	{
		Choice.Sequence = WeaponAnims.IdleAnim;
	}
	else
	{
		Choice.Sequence = FallbackIdle;
	}

	if (bAiming && WeaponAnims.AimOffset_Aim)
	{
		Choice.AimOffset = WeaponAnims.AimOffset_Aim;
	}
	else
	{
		Choice.AimOffset = WeaponAnims.AimOffset_Hip;
	}

	return Choice;
}

void URuinmatesAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(TryGetPawnOwner());
	if (!Character)
	{
		WeaponBlendSpace = UnarmedBlendSpace;
		WeaponSequence = UnarmedIdle;
		WeaponAimOffset = nullptr;
		bHasWeapon = false;
		CrouchAlpha = 0.f;
		CrouchPelvisOffset = FVector::ZeroVector;
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector Velocity = Movement ? Movement->Velocity : FVector::ZeroVector;
	const bool bCrouching = Character->bIsCrouched;
	const bool bInAir = Movement && Movement->IsFalling();
	const bool bAiming = Character->IsAiming();

	const FRotator LookDelta = (Character->GetControlRotation() - Character->GetActorRotation()).GetNormalized();
	LookPitch = FMath::Clamp(LookDelta.Pitch, -90.f, 90.f);
	LookYaw = FMath::Clamp(LookDelta.Yaw, -180.f, 180.f);

	FPlayerAnim WeaponAnims;
	bHasWeapon = false;
	if (const ABaseWeapon* Weapon = Character->GetEquippedWeapon())
	{
		if (const UWeaponData* Data = Weapon->GetWeaponData())
		{
			WeaponAnims = Data->ThirdPersonAnims;
			bHasWeapon = true;
		}
	}

	const FRuinmatesAnimChoice Choice = ChooseAnims(
		WeaponAnims,
		bCrouching,
		bInAir,
		bAiming,
		Velocity.Z,
		UnarmedBlendSpace,
		UnarmedIdle,
		UnarmedJump,
		UnarmedFall);

	WeaponBlendSpace = Choice.BlendSpace ? Choice.BlendSpace : UnarmedBlendSpace.Get();
	WeaponSequence = Choice.Sequence ? Choice.Sequence : UnarmedIdle.Get();
	WeaponAimOffset = Choice.AimOffset;

	const float TargetAlpha = Choice.bApplyCrouchOffset ? 1.f : 0.f;
	const float Blend = FMath::Clamp(CrouchBlendSpeed * DeltaSeconds, 0.f, 1.f);
	CrouchAlpha = FMath::Lerp(CrouchAlpha, TargetAlpha, Blend);
	CrouchPelvisOffset = FVector(0.f, 0.f, -CrouchPelvisDrop * CrouchAlpha);
}
