#include "Character/RuinmatesCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "Stats/StatsComponent.h"
#include "Weapon/BaseWeapon.h"
#include "Weapon/WeaponData.h"
#include "Weapon/WeaponInventoryComponent.h"
#include "Weapon/WeaponMountComponent.h"
#include "Weapon/WeaponPoseComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"


ARuinmatesCharacter::ARuinmatesCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	GetCapsuleComponent()->SetCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 500.f, 0.f);
	Movement->JumpZVelocity = 500.f;
	Movement->AirControl = 0.35f;
	Movement->MaxWalkSpeed = RunSpeed;
	Movement->MaxWalkSpeedCrouched = RunSpeed * CrouchSpeedMultiplier;
	Movement->MinAnalogWalkSpeed = 20.f;
	Movement->BrakingDecelerationWalking = 2000.f;
	Movement->NavAgentProps.bCanCrouch = true;
	Movement->SetCrouchedHalfHeight(60.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	Stats = CreateDefaultSubobject<UStatsComponent>(TEXT("Stats"));

	WeaponPose = CreateDefaultSubobject<UWeaponPoseComponent>(TEXT("WeaponPose"));

	WeaponInventory = CreateDefaultSubobject<UWeaponInventoryComponent>(TEXT("WeaponInventory"));
}

void ARuinmatesCharacter::BindStatsDelegates()
{
	if (IsValid(Stats))
	{
		Stats->OnStaminaDepleted.AddUniqueDynamic(this, &ARuinmatesCharacter::HandleStaminaDepleted);
	}
}

void ARuinmatesCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	BindStatsDelegates();
}
void ARuinmatesCharacter::BeginPlay()
{
	Super::BeginPlay();

	BindStatsDelegates();

	ApplyMovementSpeed();
	if (IsValid(WeaponPose))
	{
		WeaponPose->SetBodyMesh(GetMesh());
	}
	BindEquippedWeapon(nullptr);
}

void ARuinmatesCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Same hook as 9aa225f AShooterCharacter::PossessedBy -> Combat->SpawnInventory().
	// CombatComponent::BeginPlay did not spawn weapons.
	if (HasAuthority() && IsValid(WeaponInventory))
	{
		WeaponInventory->SpawnInventory();
	}
}

void ARuinmatesCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARuinmatesCharacter, bWantsToSprint);
	DOREPLIFETIME(ARuinmatesCharacter, bWantsToAim);
	DOREPLIFETIME(ARuinmatesCharacter, EquippedWeapon);
}

bool ARuinmatesCharacter::CanJumpInternal_Implementation() const
{
	if (!Super::CanJumpInternal_Implementation())
	{
		return false;
	}
	if (!IsValid(Stats) || Stats->DeathState != EDeathState::NotDead)
	{
		return false;
	}
	return Stats->HasStaminaForJump();
}

void ARuinmatesCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	// Predicted clients also run OnJumped; deduct stamina only on the server.
	if (HasAuthority() && IsValid(Stats))
	{
		Stats->ChangeStaminaByAmount(-Stats->JumpStaminaCost);
	}
}

void ARuinmatesCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (bWantsToSprint)
	{
		SetWantsToSprintLocal(false);
	}
	else
	{
		ApplyMovementSpeed();
	}
}

void ARuinmatesCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	ApplyMovementSpeed();
}

void ARuinmatesCharacter::StartSprint()
{
	SetWantsToSprintLocal(true);
}

void ARuinmatesCharacter::StopSprint()
{
	SetWantsToSprintLocal(false);
}

void ARuinmatesCharacter::ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ARuinmatesCharacter::StartAim()
{
	if (!IsValid(EquippedWeapon))
	{
		SetWantsToAimLocal(false);
		return;
	}

	if (bWantsToSprint)
	{
		SetWantsToSprintLocal(false);
	}
	SetWantsToAimLocal(true);
}

void ARuinmatesCharacter::StopAim()
{
	SetWantsToAimLocal(false);
}

void ARuinmatesCharacter::SetWantsToAimLocal(bool bNewWantsToAim)
{
	if (bNewWantsToAim && !IsValid(EquippedWeapon))
	{
		bNewWantsToAim = false;
	}

	if (bWantsToAim == bNewWantsToAim)
	{
		return;
	}

	bWantsToAim = bNewWantsToAim;
	if (!HasAuthority())
	{
		Server_SetWantsToAim(bNewWantsToAim);
	}
}

void ARuinmatesCharacter::Server_SetWantsToAim_Implementation(bool bNewWantsToAim)
{
	SetWantsToAimLocal(bNewWantsToAim);
}

void ARuinmatesCharacter::OnRep_WantsToAim()
{
}

void ARuinmatesCharacter::SetWantsToSprintLocal(bool bNewWantsToSprint)
{
	if (bNewWantsToSprint && (bIsCrouched || (IsValid(Stats) && Stats->Stamina <= 0.f)))
	{
		bNewWantsToSprint = false;
	}

	if (bWantsToSprint == bNewWantsToSprint)
	{
		ApplyMovementSpeed();
		return;
	}

	bWantsToSprint = bNewWantsToSprint;
	ApplyMovementSpeed();

	if (!HasAuthority())
	{
		Server_SetWantsToSprint(bNewWantsToSprint);
	}
}

void ARuinmatesCharacter::Server_SetWantsToSprint_Implementation(bool bNewWantsToSprint)
{
	SetWantsToSprintLocal(bNewWantsToSprint);
}

void ARuinmatesCharacter::OnRep_WantsToSprint()
{
	ApplyMovementSpeed();
}

void ARuinmatesCharacter::ApplyMovementSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!IsValid(Movement))
	{
		return;
	}

	if (bWantsToSprint && (bIsCrouched || IsWeaponReloading()))
	{
		bWantsToSprint = false;
	}

	const float Speed = bWantsToSprint ? SprintSpeed : RunSpeed;
	Movement->MaxWalkSpeed = Speed;
	Movement->MaxWalkSpeedCrouched = RunSpeed * CrouchSpeedMultiplier;

	if (HasAuthority() && IsValid(Stats))
	{
		Stats->SetSprinting(bWantsToSprint);
	}
}

void ARuinmatesCharacter::HandleStaminaDepleted()
{
	if (bWantsToSprint)
	{
		SetWantsToSprintLocal(false);
	}
}

bool ARuinmatesCharacter::IsWeaponReloading() const
{
	return IsValid(EquippedWeapon) && EquippedWeapon->IsReloading();
}

void ARuinmatesCharacter::EquipWeapon(ABaseWeapon* NewWeapon)
{
	if (HasAuthority())
	{
		Server_EquipWeapon_Implementation(NewWeapon);
	}
	else
	{
		Server_EquipWeapon(NewWeapon);
	}
}

void ARuinmatesCharacter::UnequipWeapon()
{
	if (HasAuthority())
	{
		Server_UnequipWeapon_Implementation();
	}
	else
	{
		Server_UnequipWeapon();
	}
}

void ARuinmatesCharacter::Server_EquipWeapon_Implementation(ABaseWeapon* NewWeapon)
{
	if (!IsValid(NewWeapon) || NewWeapon == EquippedWeapon)
	{
		return;
	}

	ABaseWeapon* Previous = EquippedWeapon;
	if (IsValid(Previous))
	{
		Previous->NotifyUnequipped();
		Previous->SetOwner(nullptr);
		Previous->SetInstigator(nullptr);
	}

	EquippedWeapon = NewWeapon;
	EquippedWeapon->SetOwner(this);
	EquippedWeapon->SetInstigator(this);
	EquippedWeapon->SetActorHiddenInGame(false);
	EquippedWeapon->NotifyEquipped();
	BindEquippedWeapon(Previous);
	PlayEquippedMontages(true, false, false);
}

void ARuinmatesCharacter::Server_UnequipWeapon_Implementation()
{
	if (!IsValid(EquippedWeapon))
	{
		return;
	}

	ABaseWeapon* Previous = EquippedWeapon;
	Previous->NotifyUnequipped();
	Previous->SetOwner(nullptr);
	Previous->SetInstigator(nullptr);
	EquippedWeapon = nullptr;
	BindEquippedWeapon(Previous);
}

void ARuinmatesCharacter::OnRep_EquippedWeapon(ABaseWeapon* PreviousWeapon)
{
	BindEquippedWeapon(PreviousWeapon);
	if (IsValid(EquippedWeapon))
	{
		PlayEquippedMontages(true, false, false);
	}
}

void ARuinmatesCharacter::BindEquippedWeapon(ABaseWeapon* PreviousWeapon)
{
	if (IsValid(PreviousWeapon) && PreviousWeapon != EquippedWeapon)
	{
		if (UWeaponMountComponent* PrevMount = PreviousWeapon->GetMount())
		{
			PrevMount->DetachFromPawn();
		}
	}

	if (IsValid(WeaponPose))
	{
		WeaponPose->SetBodyMesh(GetMesh());
	}

	if (!IsValid(EquippedWeapon))
	{
		if (IsValid(WeaponPose))
		{
			WeaponPose->SetWeaponMesh(nullptr);
			WeaponPose->SetReloading(false);
			WeaponPose->SetPreferredFabrikSocket(NAME_None);
		}
		UpdateArmedFacing();
		return;
	}

	if (UWeaponMountComponent* Mount = EquippedWeapon->GetMount())
	{
		Mount->SetPawnMesh(GetMesh());
		Mount->SetWeaponMesh(EquippedWeapon->GetWeaponMesh());
		if (const UWeaponData* Data = EquippedWeapon->GetWeaponData())
		{
			if (!Data->MeshAttachSocket.IsNone())
			{
				Mount->Mesh3PAttachSocket = Data->MeshAttachSocket;
				Mount->DefaultGripSocket = Data->MeshAttachSocket;
			}
		}
		Mount->AttachToOwningPawn();
	}

	if (IsValid(WeaponPose))
	{
		WeaponPose->SetWeaponMesh(EquippedWeapon->GetWeaponMesh());
		WeaponPose->SetWeaponKind(EquippedWeapon->GetWeaponKind());
		WeaponPose->SetReloading(EquippedWeapon->IsReloading());
		const UWeaponData* Data = EquippedWeapon->GetWeaponData();
		WeaponPose->SetPreferredFabrikSocket(Data ? Data->LeftHandSocket : NAME_None);
	}

	UpdateArmedFacing();
}

void ARuinmatesCharacter::TryFireWeapon()
{
	if (!IsValid(EquippedWeapon) || IsWeaponReloading())
	{
		return;
	}

	const int32 NotifyBefore = EquippedWeapon->GetFireNotifyId();
	EquippedWeapon->TryFire();
	if (HasAuthority() && EquippedWeapon->GetFireNotifyId() != NotifyBefore)
	{
		PlayEquippedMontages(false, true, false);
	}
}

void ARuinmatesCharacter::TryReloadWeapon()
{
	if (!IsValid(EquippedWeapon))
	{
		return;
	}

	const bool bWasReloading = EquippedWeapon->IsReloading();
	EquippedWeapon->TryReload();
	if (HasAuthority() && EquippedWeapon->IsReloading() && !bWasReloading)
	{
		if (bWantsToSprint)
		{
			SetWantsToSprintLocal(false);
		}
		if (IsValid(WeaponPose))
		{
			WeaponPose->SetReloading(true);
		}
		PlayEquippedMontages(false, false, true);
	}
}

void ARuinmatesCharacter::PlayBodyMontage(UAnimMontage* Montage)
{
	if (!Montage || !GetMesh())
	{
		return;
	}
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		Anim->Montage_Play(Montage);
	}
}

void ARuinmatesCharacter::PlayEquippedMontages(bool bJustEquipped, bool bJustFired, bool bJustReloaded)
{
	if (!IsValid(EquippedWeapon))
	{
		return;
	}

	const UWeaponData* Data = EquippedWeapon->GetWeaponData();
	if (!Data)
	{
		return;
	}

	if (bJustEquipped)
	{
		PlayBodyMontage(Data->ThirdPersonMontages.ResolveEquipMontage(EquippedWeapon->GetEquipNotifyId() <= 1));
	}
	if (bJustFired)
	{
		PlayBodyMontage(Data->ThirdPersonMontages.ResolveFireMontage());
	}
	if (bJustReloaded)
	{
		const EReloadType ReloadType = EquippedWeapon->GetAmmoInMag() <= 0 ? EReloadType::Empty : EReloadType::Tactical;
		PlayBodyMontage(Data->ThirdPersonMontages.ResolveReloadMontage(ReloadType));
	}

	if (IsValid(WeaponPose))
	{
		WeaponPose->SetReloading(EquippedWeapon->IsReloading());
	}
}

void ARuinmatesCharacter::UpdateArmedFacing()
{
	const bool bArmed = IsValid(EquippedWeapon);
	bUseControllerRotationYaw = bArmed;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// Armed stance faces the camera so strafe blend spaces and aim offsets match the shot.
		Movement->bOrientRotationToMovement = !bArmed;
	}

	if (!bArmed && bWantsToAim)
	{
		SetWantsToAimLocal(false);
	}
}

