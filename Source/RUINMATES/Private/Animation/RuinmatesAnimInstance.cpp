#include "Animation/RuinmatesAnimInstance.h"

#include "Character/RuinmatesCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapon/BaseWeapon.h"
#include "Weapon/WeaponPoseComponent.h"
#include "KismetAnimationLibrary.h"
#include "Animation/BlendSpace1D.h"

URuinmatesAnimInstance::URuinmatesAnimInstance() {}

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

	// Locomotion is global now: every weapon uses the character's shared blend space.
	Choice.BlendSpace = FallbackBlendSpace;

	// No per-weapon crouch clip exists any more, so crouch is handled procedurally.
	Choice.bApplyCrouchOffset = bCrouching && !bInAir;

	if (bInAir)
	{
		Choice.Sequence = VerticalSpeed > 0.f ? FallbackJump : FallbackFall;
	}
	else if (WeaponAnims.Idle)
	{
		Choice.Sequence = WeaponAnims.Idle.Get();
	}
	else
	{
		Choice.Sequence = FallbackIdle;
	}

	// One aim offset per weapon; it keeps updating while aiming and while hip firing.
	Choice.AimOffset = WeaponAnims.AimOffset.Get();

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
		Speed = MovementDirection = BlendX = BlendY = AimOffsetAlpha = UpperBodyAlpha = LeftHandAlpha = 0.f;
		bIsMoving = bIsCrouched = bIsAiming = bIsReloading = bIsFalling = bIsSprinting = false;
		LocomotionState = 0;
		bWasInAir = false;
		LandingTime = 0.f;
		CrouchAlpha = 0.f;
		CrouchPelvisOffset = FVector::ZeroVector;
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector Velocity = Movement ? Movement->Velocity : FVector::ZeroVector;
	const bool bCrouching = Character->bIsCrouched;
	const bool bInAir = Movement && Movement->IsFalling();
	const bool bAimingNow = Character->IsAiming();
	Speed = Velocity.Size2D();
	MovementDirection = UKismetAnimationLibrary::CalculateDirection(Velocity, Character->GetActorRotation());
	bIsMoving = Speed > 3.f;
	bIsCrouched = bCrouching;
	bIsAiming = bAimingNow;
	bIsReloading = Character->IsWeaponReloading();
	bIsFalling = bInAir;
	bIsSprinting = Character->IsSprinting();
	if (bWasInAir && !bInAir) LandingTime = 0.18f;
	LandingTime = FMath::Max(0.f, LandingTime - DeltaSeconds);
	bWasInAir = bInAir;
	LocomotionState = bInAir ? (Velocity.Z > 0.f ? 5 : 6) : (LandingTime > 0.f ? 7 : (bCrouching ? 4 : (bIsSprinting ? 3 : (Speed > 150.f ? 2 : (bIsMoving ? 1 : 0)))));
	if (const UWeaponPoseComponent* Pose = Character->GetWeaponPose())
	{
		LeftHandTransform = Pose->GetFabrikSocketTransform();
		LeftHandAlpha = Pose->GetFabrikAlpha();
	}

	const FRotator LookDelta = (Character->GetBaseAimRotation() - Character->GetActorRotation()).GetNormalized();
	LookPitch = FMath::Clamp(LookDelta.Pitch, -90.f, 90.f);
	LookYaw = FMath::Clamp(LookDelta.Yaw, -90.f, 90.f);
	AimPitch = LookPitch;
	AimYaw = LookYaw;

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
		bAimingNow,
		Velocity.Z,
		UnarmedBlendSpace,
		UnarmedIdle,
		UnarmedJump,
		UnarmedFall);

	WeaponBlendSpace = Choice.BlendSpace ? Choice.BlendSpace : UnarmedBlendSpace.Get();
	WeaponSequence = Choice.Sequence ? Choice.Sequence : UnarmedIdle.Get();
	WeaponAimOffset = Choice.AimOffset;
	// Existing offsets are normalized (-1..1), while designer-authored assets may use degrees.
	if (WeaponAimOffset)
	{
		const FBlendParameter& YawAxis = WeaponAimOffset->GetBlendParameter(0);
		const FBlendParameter& PitchAxis = WeaponAimOffset->GetBlendParameter(1);
		AimYaw = FMath::GetMappedRangeValueClamped(FVector2D(-90.f, 90.f), FVector2D(YawAxis.Min, YawAxis.Max), LookYaw);
		AimPitch = FMath::GetMappedRangeValueClamped(FVector2D(-90.f, 90.f), FVector2D(PitchAxis.Min, PitchAxis.Max), LookPitch);
	}
	AimOffsetAlpha = IsValid(WeaponAimOffset) ? 1.f : 0.f;
	UpperBodyAlpha = bHasWeapon ? 1.f : 0.f;
	AirSequence = LocomotionState == 7 ? UnarmedLand : (Velocity.Z > 0.f ? UnarmedJump : UnarmedFall);
	const bool bDirectionOnly = WeaponBlendSpace && WeaponBlendSpace->IsA<UBlendSpace1D>()
		&& WeaponBlendSpace->GetBlendParameter(0).Min < 0.f;
	BlendX = WeaponBlendSpace && WeaponBlendSpace->IsA<UBlendSpace1D>() && !bDirectionOnly ? Speed : MovementDirection;
	BlendY = Speed;

	const float TargetAlpha = Choice.bApplyCrouchOffset ? 1.f : 0.f;
	const float Blend = FMath::Clamp(CrouchBlendSpeed * DeltaSeconds, 0.f, 1.f);
	CrouchAlpha = FMath::Lerp(CrouchAlpha, TargetAlpha, Blend);
	CrouchPelvisOffset = FVector(0.f, 0.f, -CrouchPelvisDrop * CrouchAlpha);
}


