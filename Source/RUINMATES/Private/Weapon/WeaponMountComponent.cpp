#include "Weapon/WeaponMountComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"

UWeaponMountComponent::UWeaponMountComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UWeaponMountComponent::SetPawnMesh(USkeletalMeshComponent* InPawnMesh)
{
	PawnMesh = InPawnMesh;
}

void UWeaponMountComponent::SetWeaponMesh(USkeletalMeshComponent* InWeaponMesh)
{
	WeaponMesh = InWeaponMesh;
	bCapturedAuthoredRelative = false;
}

bool UWeaponMountComponent::IsUsableAttachPoint(const USkeletalMeshComponent* Mesh, FName SocketOrBone)
{
	if (!IsValid(Mesh) || SocketOrBone.IsNone())
	{
		return false;
	}

	// Prefer real sockets. Orphan bones (e.g. a root-level leftover) are unusable.
	if (Mesh->GetSocketByName(SocketOrBone) != nullptr)
	{
		return true;
	}

	const int32 BoneIdx = Mesh->GetBoneIndex(SocketOrBone);
	if (BoneIdx == INDEX_NONE)
	{
		return false;
	}

	const FName Parent = Mesh->GetParentBone(SocketOrBone);
	if (Parent.IsNone() && BoneIdx != 0)
	{
		return false;
	}

	return true;
}

FName UWeaponMountComponent::ResolveAttachPoint(const USkeletalMeshComponent* Mesh, FName Requested)
{
	if (IsUsableAttachPoint(Mesh, Requested))
	{
		return Requested;
	}

	static const FName Fallbacks[] = {
		FName(TEXT("hand_r")),
		FName(TEXT("WeaponSocket")),
		FName(TEXT("RifleSocket")),
		FName(TEXT("HandGrip_R"))
	};
	for (const FName& Candidate : Fallbacks)
	{
		if (Candidate != Requested && IsUsableAttachPoint(Mesh, Candidate))
		{
			return Candidate;
		}
	}

	return Requested;
}

bool UWeaponMountComponent::IsRightHandGrip(const USkeletalMeshComponent* Mesh, FName SocketName)
{
	if (SocketName.IsNone())
	{
		return false;
	}

	const USkeletalMeshSocket* RequestedSocket = IsValid(Mesh) ? Mesh->GetSocketByName(SocketName) : nullptr;
	const FName RequestedBone = RequestedSocket ? RequestedSocket->BoneName : SocketName;
	return RequestedBone == FName(TEXT("hand_r"))
		|| RequestedBone == FName(TEXT("ik_hand_gun"))
		|| RequestedBone == FName(TEXT("ik_hand_r"))
		|| RequestedBone == FName(TEXT("weapon_r"))
		|| SocketName == FName(TEXT("ik_hand_gun"))
		|| SocketName == FName(TEXT("ik_hand_r"))
		|| SocketName == FName(TEXT("weapon_r"));
}

void UWeaponMountComponent::CaptureAuthoredMeshRelative()
{
	if (bCapturedAuthoredRelative || !IsValid(WeaponMesh))
	{
		return;
	}

	AuthoredWeaponRelative = WeaponMesh->GetRelativeTransform();
	bCapturedAuthoredRelative = true;
}

void UWeaponMountComponent::ApplyMeshAttachRelative()
{
	if (!IsValid(WeaponMesh))
	{
		return;
	}

	if (!bCapturedAuthoredRelative)
	{
		CaptureAuthoredMeshRelative();
	}

	// Calibrated 3P mount is a complete socket-local transform, not a correction
	// stacked onto the preview component. Reapply it; never accumulate.
	if (!Mesh3PAttachSocket.IsNone() && WeaponMesh->GetAttachSocketName() == Mesh3PAttachSocket)
	{
		WeaponMesh->SetRelativeTransform(Mesh3PAttachTransform);
		return;
	}

	WeaponMesh->SetRelativeTransform(AuthoredWeaponRelative);
}

void UWeaponMountComponent::AttachToOwningPawn()
{
	if (!IsValid(PawnMesh) || !IsValid(WeaponMesh))
	{
		return;
	}

	CaptureAuthoredMeshRelative();

	FName Attach3P = DefaultGripSocket;
	if (!Mesh3PAttachSocket.IsNone() && IsUsableAttachPoint(PawnMesh, Mesh3PAttachSocket))
	{
		Attach3P = Mesh3PAttachSocket;
	}
	else
	{
		Attach3P = ResolveAttachPoint(PawnMesh, DefaultGripSocket);
	}

	if (!IsUsableAttachPoint(PawnMesh, Attach3P))
	{
		return;
	}

	WeaponMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	WeaponMesh->AttachToComponent(PawnMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Attach3P);
	ApplyMeshAttachRelative();
}

void UWeaponMountComponent::AttachToPawnSocket(USkeletalMeshComponent* InPawnMesh, FName SocketName)
{
	if (!IsValid(InPawnMesh) || !IsValid(WeaponMesh) || SocketName.IsNone())
	{
		return;
	}

	// Keep the configured pawn mesh in sync when a caller passes the body explicitly.
	PawnMesh = InPawnMesh;

	FName UseSocket = SocketName;
	if (!Mesh3PAttachSocket.IsNone())
	{
		const bool bRightHandGrip = (SocketName == DefaultGripSocket) || IsRightHandGrip(InPawnMesh, SocketName);
		// Remap right-hand grip notifies only. Reloads may transfer the weapon
		// to hand_l or a holster; those mounts must remain available.
		if (bRightHandGrip && IsUsableAttachPoint(InPawnMesh, Mesh3PAttachSocket))
		{
			UseSocket = Mesh3PAttachSocket;
		}
	}

	if (!IsUsableAttachPoint(InPawnMesh, UseSocket))
	{
		return;
	}

	WeaponMesh->AttachToComponent(InPawnMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, UseSocket);
	ApplyMeshAttachRelative();
}

void UWeaponMountComponent::DetachFromPawn()
{
	if (!bCapturedAuthoredRelative)
	{
		CaptureAuthoredMeshRelative();
	}

	if (!IsValid(WeaponMesh))
	{
		return;
	}

	WeaponMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	if (WeaponMesh->GetAttachParent())
	{
		WeaponMesh->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}
	WeaponMesh->SetRelativeTransform(AuthoredWeaponRelative);
}
