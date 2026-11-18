#include "Weapon/WeaponPoseComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

UWeaponPoseComponent::UWeaponPoseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(false);
}

void UWeaponPoseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePose(DeltaTime);
}

void UWeaponPoseComponent::SetBodyMesh(USkeletalMeshComponent* InBodyMesh)
{
	BodyMesh = InBodyMesh;
}

void UWeaponPoseComponent::SetWeaponMesh(USkeletalMeshComponent* InWeaponMesh)
{
	WeaponMesh = InWeaponMesh;
}

bool UWeaponPoseComponent::ComponentHasSocket(const USceneComponent* Component, FName SocketName) const
{
	return IsValid(Component) && !SocketName.IsNone() && Component->DoesSocketExist(SocketName);
}

bool UWeaponPoseComponent::GetFabrikWorldTransform(FTransform& OutWorld) const
{
	OutWorld = FTransform::Identity;
	if (!IsValid(WeaponMesh))
	{
		return false;
	}

	WeaponMesh->RefreshBoneTransforms();
	WeaponMesh->UpdateComponentToWorld();

	// Authored left-hand sockets only. Do not fall back to muzzle/barrel approximations.
	const FName Candidates[] = {
		PreferredFabrikSocket,
		FName(TEXT("GripSocket")),
		FName(TEXT("FABRIK")),
		FName(TEXT("LeftHandSocket")),
		FName(TEXT("lefthand")),
		FName(TEXT("IK_Hand_L")),
		FName(TEXT("hand_l_socket")),
		FName(TEXT("ik_hand_l"))
	};

	for (const FName SocketName : Candidates)
	{
		if (ComponentHasSocket(WeaponMesh, SocketName))
		{
			OutWorld = WeaponMesh->GetSocketTransform(SocketName, RTS_World);
			return true;
		}
	}

	return false;
}

void UWeaponPoseComponent::WriteHandSpace(const FTransform& World, FTransform& Out) const
{
	if (!IsValid(BodyMesh) || BodyMesh->GetBoneIndex(TEXT("hand_r")) == INDEX_NONE)
	{
		Out = FTransform::Identity;
		return;
	}

	FVector OutLocation;
	FRotator OutRotation;
	BodyMesh->TransformToBoneSpace(
		TEXT("hand_r"),
		World.GetLocation(),
		World.Rotator(),
		OutLocation,
		OutRotation);
	Out.SetLocation(OutLocation);
	Out.SetRotation(OutRotation.Quaternion());
	Out.SetScale3D(FVector::OneVector);
}

void UWeaponPoseComponent::UpdatePose(float DeltaTime)
{
	if (!RuinmatesWeaponTypes::UsesLeftHandFabrik(WeaponKind) || !IsValid(WeaponMesh))
	{
		FabrikSocketTransform = FTransform::Identity;
		FabrikAlpha = 0.f;
		return;
	}

	FTransform SocketWorld;
	const bool bHaveFabrik = GetFabrikWorldTransform(SocketWorld);
	float WantAlpha = 0.f;
	if (bHaveFabrik)
	{
		WriteHandSpace(SocketWorld, FabrikSocketTransform);
		WantAlpha = 1.f;
	}
	else
	{
		FabrikSocketTransform = FTransform::Identity;
		WantAlpha = 0.f;
	}

	if (bDisableLeftHandIKDuringReload && bIsReloading)
	{
		WantAlpha = 0.f;
	}

	if (!DisableLeftHandIKCurveName.IsNone() && IsValid(BodyMesh))
	{
		if (UAnimInstance* Anim = BodyMesh->GetAnimInstance())
		{
			if (Anim->GetCurveValue(DisableLeftHandIKCurveName) > 0.01f)
			{
				WantAlpha = 0.f;
			}
		}
	}

	const float Blend = FMath::Clamp(LeftHandIKBlendSpeed * DeltaTime, 0.f, 1.f);
	FabrikAlpha = FMath::Lerp(FabrikAlpha, WantAlpha, Blend);
}
