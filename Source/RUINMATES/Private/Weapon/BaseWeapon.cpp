#include "Weapon/BaseWeapon.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Stats/StatsComponent.h"
#include "Weapon/WeaponData.h"
#include "Character/RuinmatesCharacter.h"
#include "Weapon/WeaponMountComponent.h"
#include "Weapon/WeaponPoseComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

ABaseWeapon::ABaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Mount = CreateDefaultSubobject<UWeaponMountComponent>(TEXT("Mount"));
	AmmoInMag = CombatParams.Ammo.GetClampedStartingAmmoInMag();
	CarriedAmmo = FMath::Max(0, CombatParams.Ammo.StartingCarriedAmmo);
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();
	ApplyWeaponData();
	if (IsValid(Mount))
	{
		Mount->SetWeaponMesh(WeaponMesh);
		if (!WeaponData || WeaponData->MeshAttachSocket.IsNone())
		{
			// Keep the component default (hand_r) when data is missing.
		}
		else
		{
			Mount->Mesh3PAttachSocket = WeaponData->MeshAttachSocket;
			Mount->DefaultGripSocket = WeaponData->MeshAttachSocket;
		}
	}
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABaseWeapon, AmmoInMag);
	DOREPLIFETIME(ABaseWeapon, CarriedAmmo);
	DOREPLIFETIME(ABaseWeapon, WeaponStatus);
	DOREPLIFETIME(ABaseWeapon, FireNotifyId);
	DOREPLIFETIME(ABaseWeapon, EquipNotifyId);
}

void ABaseWeapon::SetWeaponData(UWeaponData* InData)
{
	WeaponData = InData;
	ApplyWeaponData();
}

void ABaseWeapon::ApplyWeaponData()
{
	if (!IsValid(WeaponData))
	{
		if (AmmoInMag <= 0)
		{
			AmmoInMag = CombatParams.Ammo.GetClampedStartingAmmoInMag();
		}
		if (CarriedAmmo <= 0)
		{
			CarriedAmmo = FMath::Max(0, CombatParams.Ammo.StartingCarriedAmmo);
		}
		return;
	}

	CombatParams = WeaponData->CombatParams;
	WeaponKind = RuinmatesWeaponTypes::KindFromWeaponTag(WeaponData->WeaponType);
	AmmoInMag = CombatParams.Ammo.GetClampedStartingAmmoInMag();
	CarriedAmmo = FMath::Max(0, CombatParams.Ammo.StartingCarriedAmmo);

	if (IsValid(WeaponMesh))
	{
		if (USkeletalMesh* DataMesh = WeaponData->GetWeaponMesh())
		{
			WeaponMesh->SetSkeletalMesh(DataMesh);
		}
	}
}

bool ABaseWeapon::CanFire() const
{
	if (IsReloading() || WeaponStatus == ERuinmatesWeaponStatus::Unequipped)
	{
		return false;
	}
	if (CombatParams.Ammo.UsesAmmo() && AmmoInMag <= 0)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return (World->GetTimeSeconds() - LastFireTimeSeconds) >= CombatParams.Fire.FireTime;
}

static void GetPawnView(const APawn* Pawn, FVector& OutLocation, FVector& OutDirection)
{
	FRotator ViewRot;
	Pawn->GetActorEyesViewPoint(OutLocation, ViewRot);
	OutDirection = ViewRot.Vector();
}

void ABaseWeapon::TryFire()
{
	FVector ViewLocation;
	FVector ViewDirection;
	if (const APawn* InstigatorPawn = GetInstigator())
	{
		if (const APlayerController* PC = Cast<APlayerController>(InstigatorPawn->GetController()))
		{
			FRotator ViewRot;
			PC->GetPlayerViewPoint(ViewLocation, ViewRot);
			ViewDirection = ViewRot.Vector();
		}
		else
		{
			GetPawnView(InstigatorPawn, ViewLocation, ViewDirection);
		}
	}
	else
	{
		ViewLocation = GetActorLocation();
		ViewDirection = GetActorForwardVector();
	}

	if (HasAuthority())
	{
		FireOnServer(ViewLocation, ViewDirection);
	}
	else
	{
		Server_RequestFire(ViewLocation, ViewDirection);
	}
}

void ABaseWeapon::Server_RequestFire_Implementation(FVector_NetQuantize ViewLocation, FVector_NetQuantizeNormal ViewDirection)
{
	FireOnServer(ViewLocation, ViewDirection);
}

bool ABaseWeapon::IsViewOriginTrusted(const FVector& ViewLocation) const
{
	const APawn* InstigatorPawn = GetInstigator();
	if (!IsValid(InstigatorPawn))
	{
		return true;
	}

	return FVector::Dist(ViewLocation, InstigatorPawn->GetActorLocation()) <= MaxViewOriginDistance;
}

void ABaseWeapon::FireOnServer(const FVector& ViewLocation, const FVector& ViewDirection)
{
	if (!HasAuthority() || !CanFire() || ViewDirection.IsNearlyZero())
	{
		return;
	}
	if (!IsViewOriginTrusted(ViewLocation))
	{
		return;
	}

	if (CombatParams.Ammo.UsesAmmo())
	{
		--AmmoInMag;
	}

	const UWorld* World = GetWorld();
	LastFireTimeSeconds = World ? World->GetTimeSeconds() : LastFireTimeSeconds;

	if (RuinmatesWeaponTypes::UsesMeleeSweep(WeaponKind))
	{
		// Melee sweep is a later path; do not pretend a hitscan is a slash.
		return;
	}

	++FireNotifyId;
	PlayFireMontages();

	FVector ImpactPoint = GetMuzzleLocation() + ViewDirection.GetSafeNormal() * TraceLength;
	const int32 PelletCount = FMath::Max(1, CombatParams.Ammo.PelletsPerShot);
	for (int32 Pellet = 0; Pellet < PelletCount; ++Pellet)
	{
		FHitResult Hit;
		if (WeaponTrace(Hit, ViewLocation, ViewDirection))
		{
			LastServerHit = Hit;
			ApplyServerHit(Hit);
			if (Pellet == 0)
			{
				ImpactPoint = Hit.ImpactPoint;
			}
		}
		else
		{
			LastServerHit = Hit;
			if (Pellet == 0 && !Hit.TraceEnd.IsNearlyZero())
			{
				ImpactPoint = Hit.TraceEnd;
			}
		}
	}

	BroadcastFireEffect(ImpactPoint);
}

void ABaseWeapon::ApplyServerHit(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	if (!IsValid(HitActor))
	{
		return;
	}

	if (UStatsComponent* Stats = UStatsComponent::FindStatsComponent(HitActor))
	{
		Stats->ChangeHealthByAmount(-CombatParams.Damage.Damage, GetInstigator());
	}
}

FVector ABaseWeapon::GetMuzzleLocation() const
{
	if (IsValid(WeaponMesh))
	{
		const FName Candidates[] = {
			MuzzleSocketName,
			FName(TEXT("Muzzle")),
			FName(TEXT("MuzzleSocket"))
		};
		for (const FName SocketName : Candidates)
		{
			if (!SocketName.IsNone() && WeaponMesh->DoesSocketExist(SocketName))
			{
				return WeaponMesh->GetSocketLocation(SocketName);
			}
		}
		return WeaponMesh->GetComponentLocation();
	}

	return GetActorLocation();
}

void ABaseWeapon::BuildQuery(FCollisionQueryParams& OutQuery, FCollisionResponseParams& OutResponse) const
{
	OutQuery = FCollisionQueryParams(SCENE_QUERY_STAT(RuinmatesWeaponTrace), false, this);
	OutQuery.bReturnPhysicalMaterial = true;
	OutQuery.AddIgnoredActor(this);
	if (const APawn* InstigatorPawn = GetInstigator())
	{
		OutQuery.AddIgnoredActor(InstigatorPawn);
	}

	OutResponse = FCollisionResponseParams();
	OutResponse.CollisionResponse.SetAllChannels(ECR_Ignore);
	OutResponse.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);
	OutResponse.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
	OutResponse.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
	OutResponse.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);
	OutResponse.CollisionResponse.SetResponse(TraceChannel, ECR_Block);
}

bool ABaseWeapon::WeaponTrace(FHitResult& OutHit, const FVector& ViewLocation, const FVector& ViewDirection) const
{
	OutHit = FHitResult();
	UWorld* World = GetWorld();
	if (!World || ViewDirection.IsNearlyZero() || TraceLength <= 0.f)
	{
		return false;
	}

	const FVector Direction = ViewDirection.GetSafeNormal();

	const FVector CamStart = ViewLocation;
	const FVector CamEnd = CamStart + Direction * TraceLength;

	FCollisionQueryParams QueryParams;
	FCollisionResponseParams ResponseParams;
	BuildQuery(QueryParams, ResponseParams);

	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(FMath::Max(0.1f, TraceRadius));
	const bool bCamHit = World->SweepSingleByChannel(
		OutHit,
		CamStart,
		CamEnd,
		FQuat::Identity,
		TraceChannel,
		SweepShape,
		QueryParams,
		ResponseParams);

	if (!bCamHit)
	{
		OutHit.Location = CamEnd;
		OutHit.ImpactPoint = CamEnd;
		OutHit.Normal = -Direction;
		OutHit.ImpactNormal = -Direction;
		OutHit.Distance = TraceLength;
		OutHit.Time = 1.f;
	}

	// Camera can see a target that the muzzle cannot: re-trace from the muzzle to the camera hit.
	const FVector Muzzle = GetMuzzleLocation();
	const FVector TargetPoint = bCamHit ? OutHit.ImpactPoint : CamEnd;
	if (FVector::DistSquared(Muzzle, CamStart) > (5.f * 5.f))
	{
		FHitResult MuzzleHit;
		const bool bMuzzleHit = World->SweepSingleByChannel(
			MuzzleHit,
			Muzzle,
			TargetPoint,
			FQuat::Identity,
			TraceChannel,
			SweepShape,
			QueryParams,
			ResponseParams);
		if (bMuzzleHit)
		{
			OutHit = MuzzleHit;
			return true;
		}
	}

	return bCamHit;
}

bool ABaseWeapon::CanReload() const
{
	if (IsReloading() || WeaponStatus == ERuinmatesWeaponStatus::Unequipped)
	{
		return false;
	}
	if (!CombatParams.Ammo.UsesAmmo())
	{
		return false;
	}
	return AmmoInMag < CombatParams.Ammo.MagCapacity && CarriedAmmo > 0;
}

void ABaseWeapon::TryReload()
{
	if (HasAuthority())
	{
		StartReloadOnServer();
	}
	else
	{
		Server_RequestReload();
	}
}

void ABaseWeapon::Server_RequestReload_Implementation()
{
	StartReloadOnServer();
}

void ABaseWeapon::StartReloadOnServer()
{
	if (!HasAuthority() || !CanReload())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}

	WeaponStatus = ERuinmatesWeaponStatus::Reloading;
	PlayReloadMontages();

	const float Duration = GetReloadDuration();
	if (Duration <= 0.f)
	{
		FinishReloadOnServer();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReloadTimer, this, &ABaseWeapon::FinishReloadOnServer, Duration, false);
	}
}

void ABaseWeapon::FinishReloadOnServer()
{
	if (!HasAuthority() || WeaponStatus != ERuinmatesWeaponStatus::Reloading)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}

	const int32 Space = FMath::Max(0, CombatParams.Ammo.MagCapacity - AmmoInMag);
	const int32 Taken = FMath::Min(Space, CarriedAmmo);
	AmmoInMag += Taken;
	CarriedAmmo -= Taken;
	WeaponStatus = ERuinmatesWeaponStatus::Idle;
}

EReloadType ABaseWeapon::GetReloadType() const
{
	return AmmoInMag <= 0 ? EReloadType::Empty : EReloadType::Tactical;
}

float ABaseWeapon::GetReloadDuration() const
{
	if (IsValid(WeaponData))
	{
		if (UAnimMontage* Montage = WeaponData->ThirdPersonMontages.ResolveReloadMontage(GetReloadType()))
		{
			const float Length = Montage->GetPlayLength();
			if (Length > 0.f)
			{
				return Length;
			}
		}
	}
	return ReloadFallbackDuration;
}

void ABaseWeapon::NotifyEquipped()
{
	if (WeaponStatus == ERuinmatesWeaponStatus::Unequipped)
	{
		WeaponStatus = ERuinmatesWeaponStatus::Idle;
	}
	++EquipNotifyId;
	PlayEquipMontages(EquipNotifyId <= 1);
}

void ABaseWeapon::NotifyUnequipped()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}
	WeaponStatus = ERuinmatesWeaponStatus::Unequipped;
	if (IsValid(Mount))
	{
		Mount->DetachFromPawn();
	}
	SetActorHiddenInGame(true);
}

void ABaseWeapon::OnRep_WeaponStatus()
{
	if (WeaponStatus == ERuinmatesWeaponStatus::Reloading)
	{
		PlayReloadMontages();
		if (ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(GetInstigator()))
		{
			Character->PlayEquippedMontages(false, false, true);
		}
	}
	else if (ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(GetInstigator()))
	{
		if (UWeaponPoseComponent* Pose = Character->GetWeaponPose())
		{
			Pose->SetReloading(false);
		}
	}
}

void ABaseWeapon::OnRep_FireNotifyId()
{
	PlayFireMontages();
	if (ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(GetInstigator()))
	{
		Character->PlayEquippedMontages(false, true, false);
	}
}

void ABaseWeapon::OnRep_EquipNotifyId()
{
	PlayEquipMontages(EquipNotifyId <= 1);
}

void ABaseWeapon::PlayWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage || !IsValid(WeaponMesh))
	{
		return;
	}
	if (UAnimInstance* Anim = WeaponMesh->GetAnimInstance())
	{
		Anim->Montage_Play(Montage);
	}
}

void ABaseWeapon::PlayFireMontages()
{
	if (!IsValid(WeaponData))
	{
		return;
	}
	PlayWeaponMontage(WeaponData->WeaponMontages.ResolveFireMontage());
}

void ABaseWeapon::BroadcastFireEffect(const FVector& ImpactPoint)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		Multicast_PlayFireEffect(ImpactPoint);
		return;
	}

	SpawnFireEffectLocal(ImpactPoint);
	if (HasAuthority() && GetNetMode() == NM_ListenServer)
	{
		Multicast_PlayFireEffect(ImpactPoint);
	}
}

void ABaseWeapon::Multicast_PlayFireEffect_Implementation(FVector_NetQuantize ImpactPoint)
{
	if (!HasAuthority())
	{
		SpawnFireEffectLocal(ImpactPoint);
	}
}

static UNiagaraSystem* LoadProjectFireSystem(const TCHAR* Path)
{
	return LoadObject<UNiagaraSystem>(nullptr, Path);
}

void ABaseWeapon::SpawnFireEffectLocal(const FVector& ImpactPoint)
{
	if (GetNetMode() == NM_DedicatedServer || !IsValid(WeaponMesh))
	{
		return;
	}

	UNiagaraSystem* MuzzleSystem = IsValid(WeaponData) ? WeaponData->FireEffect.Get() : nullptr;
	if (!MuzzleSystem)
	{
		MuzzleSystem = LoadProjectFireSystem(TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire.NS_WeaponFire"));
	}
	UNiagaraSystem* TracerSystem = IsValid(WeaponData) ? WeaponData->TracerEffect.Get() : nullptr;
	if (!TracerSystem)
	{
		TracerSystem = LoadProjectFireSystem(TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_Tracer.NS_WeaponFire_Tracer"));
	}

	const FVector Muzzle = GetMuzzleLocation();
	const FVector Direction = (ImpactPoint - Muzzle).GetSafeNormal();
	const FRotator MuzzleRotation = Direction.IsNearlyZero() ? WeaponMesh->GetComponentRotation() : Direction.Rotation();

	FName Socket = NAME_None;
	const FName Candidates[] = { MuzzleSocketName, FName(TEXT("Muzzle")), FName(TEXT("MuzzleSocket")) };
	for (const FName Candidate : Candidates)
	{
		if (!Candidate.IsNone() && WeaponMesh->DoesSocketExist(Candidate))
		{
			Socket = Candidate;
			break;
		}
	}

	auto SpawnOne = [&](UNiagaraSystem* System, bool bTracer)
	{
		if (!System)
		{
			return;
		}
		UNiagaraComponent* Effect = nullptr;
		if (Socket.IsNone())
		{
			Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this, System, Muzzle, MuzzleRotation, FVector::OneVector, true, false, ENCPoolMethod::None, false);
		}
		else
		{
			Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(
				System,
				WeaponMesh,
				Socket,
				FVector::ZeroVector,
				MuzzleRotation,
				EAttachLocation::SnapToTarget,
				true,
				false,
				ENCPoolMethod::None,
				false);
		}
		if (!Effect)
		{
			return;
		}
		Effect->SetVariableBool(TEXT("User.Trigger"), true);
		if (bTracer)
		{
			Effect->SetVariableVec3(TEXT("User.MuzzlePosition"), Muzzle);
			Effect->SetVariablePosition(TEXT("User.MuzzlePosition"), Muzzle);
			TArray<FVector> Impacts;
			Impacts.Add(ImpactPoint);
			UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(Effect, TEXT("User.ImpactPositions"), Impacts);
		}
		else
		{
			Effect->SetVariableVec3(TEXT("User.Direction"), Direction);
		}
		Effect->Activate(true);
	};

	SpawnOne(MuzzleSystem, false);
	SpawnOne(TracerSystem, true);
}

void ABaseWeapon::PlayReloadMontages()
{
	if (!IsValid(WeaponData))
	{
		return;
	}
	PlayWeaponMontage(WeaponData->WeaponMontages.ResolveReloadMontage(GetReloadType()));
}

void ABaseWeapon::PlayEquipMontages(bool bFirstDraw)
{
	if (!IsValid(WeaponData))
	{
		return;
	}
	PlayWeaponMontage(WeaponData->WeaponMontages.ResolveEquipMontage(bFirstDraw));
}

bool ABaseWeapon::UsesAmmo() const
{
	return IsValid(WeaponData) && WeaponData->CombatParams.Ammo.UsesAmmo();
}

int32 ABaseWeapon::AddCarriedAmmo(int32 Amount)
{
	if (Amount <= 0 || !HasAuthority() || !UsesAmmo())
	{
		return 0;
	}
	const int64 NewTotal = FMath::Min<int64>(static_cast<int64>(CarriedAmmo) + static_cast<int64>(Amount), MAX_int32);
	const int32 Added = static_cast<int32>(NewTotal) - CarriedAmmo;
	CarriedAmmo = static_cast<int32>(NewTotal);
	return Added;
}
