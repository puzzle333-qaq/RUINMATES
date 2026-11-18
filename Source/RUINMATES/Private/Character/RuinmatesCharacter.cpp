#include "Character/RuinmatesCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
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
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/OverlapResult.h"
#include "Interface/Interactable.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesLootContainer.h"
#include "Inventory/RuinmatesMerchant.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Game/RuinmatesGameState.h"
#include "UI/RuinmatesTradeScreen.h"
#include "UI/RuinmatesCrosshairWidget.h"
#include "UI/RuinmatesTerminalScreen.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"


ARuinmatesCharacter::ARuinmatesCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
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
	Backpack = CreateDefaultSubobject<URuinmatesBackpackComponent>(TEXT("Backpack"));
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
	if (IsValid(CameraBoom))
	{
		HipArmLength = CameraBoom->TargetArmLength;
		HipSocketOffset = CameraBoom->SocketOffset;
		HipTargetOffset = CameraBoom->TargetOffset;
	}
	if (IsValid(WeaponPose))
	{
		WeaponPose->SetBodyMesh(GetMesh());
	}
	BindEquippedWeapon(nullptr);
	EnsureCrosshair();
}

void ARuinmatesCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAimCamera(DeltaSeconds);
	UpdateCrosshairVisibility();
	UpdateSearchDistance();
}

void ARuinmatesCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	EnsureCrosshair();
}

void ARuinmatesCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BindAimInput(PlayerInputComponent);
	BindInventoryInput(PlayerInputComponent);
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
	DOREPLIFETIME(ARuinmatesCharacter, ActiveSearch);
	DOREPLIFETIME(ARuinmatesCharacter, ActiveMerchant);
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
	ApplyMovementSpeed();
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
	ApplyMovementSpeed();
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

	if (bWantsToSprint && (bIsCrouched || IsWeaponReloading() || bWantsToAim))
	{
		bWantsToSprint = false;
	}

	const float AimScale = bWantsToAim ? AimSpeedMultiplier : 1.f;
	const float Speed = bWantsToSprint ? SprintSpeed : RunSpeed * AimScale;
	Movement->MaxWalkSpeed = Speed;
	Movement->MaxWalkSpeedCrouched = RunSpeed * CrouchSpeedMultiplier * AimScale;

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
		// FireTime defaults to 0.1s while the pistol fire montage is 0.667s. Restarting the montage on
		// every shot keeps it at its first frames, so the shot pose never reaches full slot weight and
		// the locomotion pose keeps the upper body (guns look dead while moving and shooting).
		// Let a running fire montage finish instead of restarting it.
		if (UAnimMontage* FireMontage = Data->ThirdPersonMontages.ResolveFireMontage())
		{
			UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
			if (!Anim || !Anim->Montage_IsPlaying(FireMontage))
			{
				PlayBodyMontage(FireMontage);
			}
		}
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

void ARuinmatesCharacter::UpdateAimCamera(float DeltaSeconds)
{
	if (!IsLocallyControlled() || !IsValid(CameraBoom))
	{
		return;
	}

	const float TargetAlpha = bWantsToAim ? 1.f : 0.f;
	AimCameraAlpha = FMath::FInterpTo(AimCameraAlpha, TargetAlpha, DeltaSeconds, AimCameraInterpSpeed);
	CameraBoom->TargetArmLength = FMath::Lerp(HipArmLength, AimArmLength, AimCameraAlpha);
	CameraBoom->SocketOffset = FMath::Lerp(HipSocketOffset, AimSocketOffset, AimCameraAlpha);
	CameraBoom->TargetOffset = FMath::Lerp(HipTargetOffset, AimTargetOffset, AimCameraAlpha);
}

void ARuinmatesCharacter::EnsureCrosshair()
{
	if (!IsLocallyControlled() || CrosshairWidget)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	CrosshairWidget = CreateWidget<URuinmatesCrosshairWidget>(PlayerController, URuinmatesCrosshairWidget::StaticClass());
	if (!CrosshairWidget)
	{
		return;
	}

	CrosshairWidget->AddToViewport(20);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CrosshairWidget->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f));
	}
	PlayerController->SetShowMouseCursor(false);
	PlayerController->SetInputMode(FInputModeGameOnly());
	UpdateCrosshairVisibility();
}

void ARuinmatesCharacter::UpdateCrosshairVisibility()
{
	if (!CrosshairWidget)
	{
		return;
	}
	CrosshairWidget->SetVisibility(bWantsToAim ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void ARuinmatesCharacter::BindAimInput(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	UInputAction* AimAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Aim.IA_Aim"));
	if (!EnhancedInput || !AimAction)
	{
		return;
	}

	EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &ARuinmatesCharacter::StartAim);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &ARuinmatesCharacter::StopAim);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Canceled, this, &ARuinmatesCharacter::StopAim);

	if (UInputAction* FireAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Fire.IA_Fire")))
	{
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Triggered, this, &ARuinmatesCharacter::TryFireWeapon);
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!InputSubsystem || InputSubsystem->QueryKeysMappedToAction(AimAction).Num() > 0)
	{
		return;
	}

	UInputMappingContext* AimContext = NewObject<UInputMappingContext>(this);
	AimContext->MapKey(AimAction, EKeys::RightMouseButton);
	InputSubsystem->AddMappingContext(AimContext, 1);
}

void ARuinmatesCharacter::BindInventoryInput(UInputComponent* PlayerInputComponent)
{
	if (!PlayerInputComponent)
	{
		return;
	}
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ARuinmatesCharacter::ToggleBackpack);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &ARuinmatesCharacter::TryInteract);
	PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ARuinmatesCharacter::CloseTerminal);
}

void ARuinmatesCharacter::ToggleBackpack()
{
	// Trading and looting are mutually exclusive: Tab first closes the trade screen.
	if (IsTrading())
	{
		EndTrade();
		return;
	}
	if (IsValid(InventoryScreen) && InventoryScreen->IsInViewport())
	{
		CloseTerminal();
		return;
	}
	OpenInventoryScreen();
}

void ARuinmatesCharacter::CloseTerminal()
{
	if (IsValid(ActiveSearch))
	{
		if (HasAuthority())
		{
			EndSearch();
			return;
		}
		Server_StopSearch();
		ActiveSearch = nullptr;
	}
	CloseInventoryScreen();
}

void ARuinmatesCharacter::TryInteract()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		ViewLocation = GetActorLocation();
		ViewRotation = GetActorRotation();
	}

	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RuinmatesInteract), false, this);
	AActor* Target = nullptr;
	if (World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * 450.f, ECC_Visibility, Query))
	{
		if (Hit.GetActor() && Hit.GetActor()->GetClass()->ImplementsInterface(UInteractable::StaticClass())
			&& FVector::Dist(Hit.GetActor()->GetActorLocation(), GetActorLocation()) <= 350.f)
		{
			Target = Hit.GetActor();
		}
	}

	if (!Target)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjectQuery;
		ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
		World->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectQuery, FCollisionShape::MakeSphere(220.f), Query);
		float Best = MAX_FLT;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Candidate = Overlap.GetActor();
			if (!IsValid(Candidate) || !Candidate->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
			{
				continue;
			}
			if (!IInteractable::Execute_CanInteract(Candidate, this))
			{
				continue;
			}
			const float Distance = FVector::Dist(Candidate->GetActorLocation(), GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Target = Candidate;
			}
		}
	}

	if (!Target)
	{
		return;
	}
	if (HasAuthority())
	{
		Server_InteractActor_Implementation(Target);
	}
	else
	{
		Server_InteractActor(Target);
	}
}

void ARuinmatesCharacter::Server_InteractActor_Implementation(AActor* Target)
{
	if (!IsValid(Target) || FVector::Dist(Target->GetActorLocation(), GetActorLocation()) > 400.f)
	{
		return;
	}
	if (Target->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		IInteractable::Execute_Interact(Target, this);
	}
}

void ARuinmatesCharacter::BeginSearch(ARuinmatesLootContainer* Container)
{
	if (!HasAuthority())
	{
		return;
	}
	ActiveSearch = Container;
	if (IsLocallyControlled())
	{
		OpenInventoryScreen();
	}
}

void ARuinmatesCharacter::EndSearch()
{
	if (IsValid(ActiveSearch))
	{
		ActiveSearch->StopSearch(this);
	}
	ActiveSearch = nullptr;
	if (IsLocallyControlled())
	{
		CloseInventoryScreen();
	}
}

void ARuinmatesCharacter::Server_StopSearch_Implementation()
{
	EndSearch();
}

void ARuinmatesCharacter::OnRep_ActiveSearch()
{
	if (IsValid(ActiveSearch))
	{
		OpenInventoryScreen();
	}
	else
	{
		CloseInventoryScreen();
	}
}

void ARuinmatesCharacter::UpdateSearchDistance()
{
	if (!IsValid(ActiveSearch))
	{
		return;
	}
	if (FVector::Dist(ActiveSearch->GetActorLocation(), GetActorLocation()) <= ActiveSearch->GetCloseDistance())
	{
		return;
	}
	if (HasAuthority())
	{
		EndSearch();
	}
	else
	{
		Server_StopSearch();
		ActiveSearch = nullptr;
		CloseInventoryScreen();
	}
}

void ARuinmatesCharacter::OpenInventoryScreen()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!IsLocallyControlled() || !PlayerController)
	{
		return;
	}
	if (!InventoryScreen)
	{
		InventoryScreen = CreateWidget<URuinmatesTerminalScreen>(PlayerController, URuinmatesTerminalScreen::StaticClass());
	}
	if (!InventoryScreen)
	{
		return;
	}
	if (!InventoryScreen->IsInViewport())
	{
		InventoryScreen->AddToViewport(30);
	}
	InventoryScreen->OpenFor(this, ActiveSearch);
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(InventoryScreen->TakeWidget());
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);
}

void ARuinmatesCharacter::CloseInventoryScreen()
{
	if (InventoryScreen && InventoryScreen->IsInViewport())
	{
		InventoryScreen->RemoveFromParent();
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (IsLocallyControlled())
		{
			PlayerController->SetShowMouseCursor(false);
			PlayerController->SetInputMode(FInputModeGameOnly());
		}
	}
}

void ARuinmatesCharacter::RequestPickup(int32 SlotIndex)
{
	if (HasAuthority())
	{
		Server_PickupLoot_Implementation(SlotIndex);
	}
	else
	{
		Server_PickupLoot(SlotIndex);
	}
}

void ARuinmatesCharacter::RequestTakeAllLoot()
{
	if (HasAuthority())
	{
		Server_TakeAllLoot_Implementation();
	}
	else
	{
		Server_TakeAllLoot();
	}
}

void ARuinmatesCharacter::RequestDiscard(int32 SlotIndex)
{
	if (HasAuthority())
	{
		Server_DiscardBackpackSlot_Implementation(SlotIndex);
	}
	else
	{
		Server_DiscardBackpackSlot(SlotIndex);
	}
}

void ARuinmatesCharacter::Server_PickupLoot_Implementation(int32 SlotIndex)
{
	if (!IsValid(ActiveSearch))
	{
		return;
	}
	ActiveSearch->GiveTo(this, SlotIndex);
}

void ARuinmatesCharacter::Server_TakeAllLoot_Implementation()
{
	if (!IsValid(ActiveSearch))
	{
		return;
	}
	ActiveSearch->GiveAllRevealed(this);
}

void ARuinmatesCharacter::Server_DiscardBackpackSlot_Implementation(int32 SlotIndex)
{
	if (Backpack)
	{
		Backpack->DiscardSlot(SlotIndex);
	}
}



// ---------------------------------------------------------------------------
// Trading
// ---------------------------------------------------------------------------

void ARuinmatesCharacter::BeginTrade(ARuinmatesMerchant* Merchant)
{
	if (!HasAuthority() || !IsValid(Merchant))
	{
		return;
	}
	// Only one interaction UI at a time: a trade closes the loot terminal.
	if (IsValid(ActiveSearch))
	{
		EndSearch();
	}
	ActiveMerchant = Merchant;
	if (IsLocallyControlled())
	{
		OpenTradeScreen();
	}
}

void ARuinmatesCharacter::EndTrade()
{
	if (HasAuthority())
	{
		ActiveMerchant = nullptr;
		if (IsLocallyControlled())
		{
			CloseTradeScreen();
		}
	}
	else
	{
		Server_EndTrade();
	}
}

void ARuinmatesCharacter::Server_EndTrade_Implementation()
{
	ActiveMerchant = nullptr;
	if (IsLocallyControlled())
	{
		CloseTradeScreen();
	}
}

void ARuinmatesCharacter::OnRep_ActiveMerchant()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	if (IsValid(ActiveMerchant))
	{
		OpenTradeScreen();
	}
	else
	{
		CloseTradeScreen();
	}
}

bool ARuinmatesCharacter::ValidateTradeState(ARuinmatesMerchant*& OutMerchant)
{
	OutMerchant = nullptr;
	ARuinmatesMerchant* Merchant = ActiveMerchant;
	if (!IsValid(Merchant) || !IsValid(Backpack))
	{
		return false;
	}
	// Players who walked away lose the trade instead of trading from across the map.
	const float Distance = FVector::Dist(Merchant->GetActorLocation(), GetActorLocation());
	if (Distance > Merchant->GetInteractionDistance() + 100.f)
	{
		ActiveMerchant = nullptr;
		return false;
	}
	OutMerchant = Merchant;
	return true;
}

void ARuinmatesCharacter::RequestBuyItem(FName ItemId, int32 Count)
{
	if (HasAuthority())
	{
		Server_BuyItem_Implementation(ItemId, Count);
	}
	else
	{
		Server_BuyItem(ItemId, Count);
	}
}

void ARuinmatesCharacter::RequestSellItem(int32 SlotIndex, int32 Count)
{
	if (HasAuthority())
	{
		Server_SellItem_Implementation(SlotIndex, Count);
	}
	else
	{
		Server_SellItem(SlotIndex, Count);
	}
}

void ARuinmatesCharacter::Server_BuyItem_Implementation(FName ItemId, int32 Count)
{
	ARuinmatesMerchant* Merchant = nullptr;
	if (!ValidateTradeState(Merchant))
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeNoMerchant", "商人不在范围内"));
		return;
	}
	if (Count <= 0 || Count > 99)
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeBadCount", "数量不合法"));
		return;
	}

	URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this);
	const FRuinmatesItemRow* Row = Database ? Database->FindItemDefinition(ItemId) : nullptr;
	if (!Row)
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeUnknownItem", "未知物品"));
		return;
	}
	if (!Row->bCanBuy || !Merchant->SellsItem(ItemId))
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeNotSold", "该商人不出售此物品"));
		return;
	}

	// Server-side pricing only. The client never sends a price.
	const int64 Total64 = static_cast<int64>(FMath::Max(0, Row->BuyPrice)) * static_cast<int64>(Count);
	if (Total64 <= 0 || Total64 > MAX_int32)
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeBadPrice", "价格无效"));
		return;
	}
	const int32 TotalPrice = static_cast<int32>(Total64);

	ARuinmatesGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARuinmatesGameState>() : nullptr;
	if (!GameState || !GameState->HasEnoughFunds(TotalPrice))
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeNoFunds", "团队资金不足"));
		return;
	}

	const FRuinmatesItemStack Stack = Database->CreateItemStack(ItemId, Count);
	if (!IsValid(Backpack) || !Backpack->CanAddExactCount(Stack, Count))
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeNoSpace", "背包空间不足"));
		return;
	}

	// All-or-nothing: charge, then deliver; refund if the delivery somehow fails.
	if (!GameState->TrySpendFunds(TotalPrice))
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeNoFunds", "团队资金不足"));
		return;
	}
	if (!Backpack->TryAddExact(Stack, Count))
	{
		GameState->AddFunds(TotalPrice);
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeNoSpace", "背包空间不足"));
		return;
	}

	Client_TradeResult(true, FText::Format(
		NSLOCTEXT("RuinMates", "TradeBought", "购买 {0} ×{1}，花费 {2}"),
		Row->DisplayName, FText::AsNumber(Count), FText::AsNumber(TotalPrice)));
}

void ARuinmatesCharacter::Server_SellItem_Implementation(int32 SlotIndex, int32 Count)
{
	ARuinmatesMerchant* Merchant = nullptr;
	if (!ValidateTradeState(Merchant))
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeNoMerchant", "商人不在范围内"));
		return;
	}
	if (!Merchant->AllowsSelling())
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeNoSell", "该商人不收货"));
		return;
	}
	if (!IsValid(Backpack) || !Backpack->GetSlots().IsValidIndex(SlotIndex) || Count <= 0)
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeBadSlot", "格子无效"));
		return;
	}

	// Read the stack on the server; item id and quality are never taken from the client.
	const FRuinmatesItemStack Stack = Backpack->GetSlots()[SlotIndex];
	if (Stack.IsEmpty())
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeBadSlot", "格子无效"));
		return;
	}

	URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this);
	const FRuinmatesItemRow* Row = Database ? Database->FindItemDefinition(Stack.ItemId) : nullptr;
	if (!Row)
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeUnknownItem", "未知物品"));
		return;
	}
	if (!Row->bCanSell)
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeSellBlocked", "该物品不能出售"));
		return;
	}

	const int32 Quantity = FMath::Clamp(Count, 1, Stack.Count);
	const int64 Total64 = static_cast<int64>(FMath::Max(0, Row->SellPrice)) * static_cast<int64>(Quantity);
	if (Total64 <= 0 || Total64 > MAX_int32)
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeBadPrice", "价格无效"));
		return;
	}

	ARuinmatesGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARuinmatesGameState>() : nullptr;
	if (!GameState)
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeNoWallet", "团队资金不可用"));
		return;
	}

	if (!Backpack->RemoveItemAt(SlotIndex, Quantity))
	{
		Client_TradeResult(false, NSLOCTEXT("RuinMates", "TradeBadSlot", "格子无效"));
		return;
	}
	GameState->AddFunds(static_cast<int32>(Total64));

	Client_TradeResult(true, FText::Format(
		NSLOCTEXT("RuinMates", "TradeSold", "出售 {0} ×{1}，获得 {2}"),
		Row->DisplayName, FText::AsNumber(Quantity), FText::AsNumber(static_cast<int32>(Total64))));
}

void ARuinmatesCharacter::Client_TradeResult_Implementation(bool bSuccess, const FText& Message)
{
	if (IsValid(TradeScreen))
	{
		TradeScreen->ShowResult(bSuccess, Message);
	}
}

void ARuinmatesCharacter::OpenTradeScreen()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!IsLocallyControlled() || !PlayerController || !IsValid(ActiveMerchant))
	{
		return;
	}
	if (!TradeScreen)
	{
		TradeScreen = CreateWidget<URuinmatesTradeScreen>(PlayerController, URuinmatesTradeScreen::StaticClass());
	}
	if (!TradeScreen)
	{
		return;
	}
	if (!TradeScreen->IsInViewport())
	{
		TradeScreen->AddToViewport(31);
	}
	TradeScreen->SetContext(this, ActiveMerchant);

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(TradeScreen->TakeWidget());
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);
}

void ARuinmatesCharacter::CloseTradeScreen()
{
	if (IsValid(TradeScreen) && TradeScreen->IsInViewport())
	{
		TradeScreen->RemoveFromParent();
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (IsLocallyControlled())
		{
			PlayerController->SetShowMouseCursor(false);
			PlayerController->SetInputMode(FInputModeGameOnly());
		}
	}
}
