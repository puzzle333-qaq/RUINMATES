#include "Misc/AutomationTest.h"
#include "Character/RuinmatesCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Stats/StatsComponent.h"
#include "Weapon/BaseWeapon.h"
#include "Weapon/WeaponData.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	AActor* SpawnBlockingBox(UWorld* World, const FVector& Location, const FVector& Extent, const FActorSpawnParameters& SpawnParams)
	{
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Location), SpawnParams);
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Box"));
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Box->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Box->SetCanEverAffectNavigation(false);
		Actor->AddInstanceComponent(Box);
		Box->RegisterComponent();
		Actor->SetRootComponent(Box);
		Box->SetWorldLocation(Location);
		Box->UpdateBounds();
		Box->UpdateComponentToWorld();
		return Actor;
	}

	void CleanupWorld(AActor* A, AActor* B, AActor* C, UWorld* World)
	{
		if (IsValid(A)) { A->Destroy(); }
		if (IsValid(B)) { B->Destroy(); }
		if (IsValid(C)) { C->Destroy(); }
		if (World)
		{
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBaseWeaponFireTest, "RuinMates.Batch2.BaseWeaponFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBaseWeaponFireTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesBaseWeaponFireTest"));
	if (!TestNotNull(TEXT("Created a transient world"), World))
	{
		return false;
	}
	World->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ABaseWeapon* Weapon = World->SpawnActor<ABaseWeapon>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!TestTrue(TEXT("Weapon spawned"), IsValid(Weapon)))
	{
		CleanupWorld(nullptr, nullptr, nullptr, World);
		return false;
	}

	UWeaponData* Data = NewObject<UWeaponData>(Weapon);
	Data->CombatParams.Spread.HipFireSpread = 0.f;
	Data->CombatParams.Fire.FireTime = 0.25f;
	Data->CombatParams.Ammo.MagCapacity = 2;
	Data->CombatParams.Ammo.StartingAmmoInMag = 2;
	Data->CombatParams.Ammo.StartingCarriedAmmo = 0;
	Data->CombatParams.Damage.Damage = 15.f;
	Weapon->SetWeaponData(Data);

	TestEqual(TEXT("Ammo initializes from data"), Weapon->GetAmmoInMag(), 2);
	TestTrue(TEXT("Can fire with ammo and no mesh"), Weapon->CanFire());
	TestEqual(TEXT("Muzzle falls back to the actor location when the mesh has no socket"), Weapon->GetMuzzleLocation(), Weapon->GetActorLocation());

	Weapon->TryFire();
	TestEqual(TEXT("Server fire consumes one round"), Weapon->GetAmmoInMag(), 1);
	TestFalse(TEXT("FireTime blocks a second immediate shot"), Weapon->CanFire());

	Weapon->CombatParams.Fire.FireTime = 0.f;
	TestTrue(TEXT("Clearing FireTime allows the next shot"), Weapon->CanFire());
	Weapon->TryFire();
	TestEqual(TEXT("Second server shot empties the mag"), Weapon->GetAmmoInMag(), 0);
	TestFalse(TEXT("Empty mag cannot fire"), Weapon->CanFire());

	const int32 EmptyMag = Weapon->GetAmmoInMag();
	Weapon->SetRole(ROLE_SimulatedProxy);
	Weapon->CombatParams.Ammo.MagCapacity = 2;
	Weapon->TryFire();
	TestEqual(TEXT("Simulated proxy does not consume ammo locally"), Weapon->GetAmmoInMag(), EmptyMag);

	Weapon->Destroy();
	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesBaseWeaponTraceTest, "RuinMates.Batch2.BaseWeaponTrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesBaseWeaponTraceTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesBaseWeaponTraceTest"));
	if (!TestNotNull(TEXT("Created a transient world"), World))
	{
		return false;
	}
	World->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ABaseWeapon* Weapon = World->SpawnActor<ABaseWeapon>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	ARuinmatesCharacter* Shooter = World->SpawnActor<ARuinmatesCharacter>(FVector(-50.f, 0.f, 0.f), FRotator::ZeroRotator, SpawnParams);
	AActor* LowWall = SpawnBlockingBox(World, FVector(150.f, 0.f, 0.f), FVector(25.f, 200.f, 80.f), SpawnParams);
	AActor* HighTarget = SpawnBlockingBox(World, FVector(400.f, 0.f, 200.f), FVector(20.f, 40.f, 40.f), SpawnParams);
	if (!TestTrue(TEXT("Trace fixtures spawned"), IsValid(Weapon) && IsValid(Shooter) && IsValid(LowWall) && IsValid(HighTarget)))
	{
		CleanupWorld(Weapon, Shooter, LowWall, World);
		if (IsValid(HighTarget)) { HighTarget->Destroy(); }
		return false;
	}

	Weapon->SetInstigator(Shooter);
	Weapon->CombatParams.Spread.HipFireSpread = 0.f;
	Weapon->TraceRadius = 1.f;

	// Camera looks over the low wall and can see the high target. The muzzle is at the origin
	// and is blocked by the low wall. The server must not use a "camera can see it" hit.
	FHitResult Hit;
	const FVector CameraLoc(0.f, 0.f, 200.f);
	const FVector CameraDir = (FVector(400.f, 0.f, 200.f) - CameraLoc).GetSafeNormal();
	const bool bHit = Weapon->WeaponTrace(Hit, CameraLoc, CameraDir);
	TestTrue(TEXT("Occlusion trace hits something"), bHit);
	TestEqual(TEXT("Muzzle occlusion hits the low wall, not the camera-visible target"), Hit.GetActor(), LowWall);

	UWeaponData* Data = NewObject<UWeaponData>(Weapon);
	Data->CombatParams.Spread.HipFireSpread = 0.f;
	Data->CombatParams.Fire.FireTime = 0.f;
	Data->CombatParams.Ammo.MagCapacity = 5;
	Data->CombatParams.Ammo.StartingAmmoInMag = 5;
	Data->CombatParams.Damage.Damage = 10.f;
	Weapon->SetWeaponData(Data);

	UStatsComponent* TargetStats = NewObject<UStatsComponent>(HighTarget);
	TargetStats->RegisterComponent();
	TestEqual(TEXT("Target starts at full health"), TargetStats->Health, 100.f);

	const int32 AmmoReady = Weapon->GetAmmoInMag();
	Weapon->TryFire();
	TestEqual(TEXT("Trusted fire consumes ammo"), Weapon->GetAmmoInMag(), AmmoReady - 1);
	TestNotEqual(TEXT("Server last hit is not the camera-only high target from a default muzzle fire"), Weapon->LastServerHit.GetActor(), HighTarget);
	TestEqual(TEXT("No client HitResult path damaged the camera-only target"), TargetStats->Health, 100.f);

	HighTarget->Destroy();
	CleanupWorld(Weapon, Shooter, LowWall, World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
