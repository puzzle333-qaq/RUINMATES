#include "Misc/AutomationTest.h"
#include "Gameplay/RuinmatesGameplayTags.h"
#include "Stats/StatsComponent.h"
#include "Weapon/WeaponData.h"
#include "Weapon/WeaponParams.h"
#include "World/TestInteractActor.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "GameFramework/DefaultPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesWeaponTypeTagsTest, "RuinMates.Batch1.WeaponTypeTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesWeaponTypeTagsTest::RunTest(const FString& Parameters)
{
	using namespace RuinmatesTags;
	TestTrue(TEXT("Rifle tag is valid"), Tag_WeaponType_Rifle.GetTag().IsValid());
	TestTrue(TEXT("Pistol tag is valid"), Tag_WeaponType_Pistol.GetTag().IsValid());
	TestTrue(TEXT("Melee tag is valid"), Tag_WeaponType_Melee.GetTag().IsValid());
	TestTrue(TEXT("Shotgun tag is valid"), Tag_WeaponType_Shotgun.GetTag().IsValid());
	TestTrue(TEXT("Sniper tag is valid"), Tag_WeaponType_Sniper.GetTag().IsValid());
	TestTrue(TEXT("LMG tag is valid"), Tag_WeaponType_LMG.GetTag().IsValid());
	TestEqual(TEXT("Rifle tag path"), Tag_WeaponType_Rifle.GetTag().ToString(), FString(TEXT("Weapon.type.rifle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesWeaponParamsTest, "RuinMates.Batch1.WeaponParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesWeaponParamsTest::RunTest(const FString& Parameters)
{
	FWeaponCombatParams Params;
	Params.Fire.FireTime = 0.2f;
	Params.Fire.BurstShotInterval = 0.f;
	TestEqual(TEXT("Burst interval falls back to FireTime"), Params.Fire.GetBurstShotInterval(), 0.2f);

	Params.Fire.BurstShotInterval = 0.05f;
	TestEqual(TEXT("Burst interval uses explicit value"), Params.Fire.GetBurstShotInterval(), 0.05f);

	Params.Ammo.MagCapacity = 20;
	Params.Ammo.StartingAmmoInMag = 99;
	TestEqual(TEXT("Starting ammo is clamped to mag capacity"), Params.Ammo.GetClampedStartingAmmoInMag(), 20);

	FWeaponRecoilParams Recoil;
	TestEqual(TEXT("Empty recoil pattern is safe"), Recoil.GetStep(3).Pitch, 0.f);
	Recoil.RecoilPattern = { FRecoilStep(1.f, 0.5f), FRecoilStep(0.8f, -0.2f) };
	TestEqual(TEXT("Recoil step 0"), Recoil.GetStep(0).Pitch, 1.f);
	TestEqual(TEXT("Recoil past end repeats last"), Recoil.GetStep(9).Pitch, 0.8f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesWeaponDataNullSafeTest, "RuinMates.Batch1.WeaponDataNullSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesWeaponDataNullSafeTest::RunTest(const FString& Parameters)
{
	const FMontageData Montages;
	TestNull(TEXT("Fire montage is null-safe"), Montages.ResolveFireMontage());
	TestNull(TEXT("Equip montage is null-safe"), Montages.ResolveEquipMontage(false));
	TestNull(TEXT("First-equip montage is null-safe"), Montages.ResolveEquipMontage(true));
	TestNull(TEXT("Reload montage is null-safe"), Montages.ResolveReloadMontage(EReloadType::Empty));

	const FPlayerAnim Anims;
	TestNull(TEXT("Idle anim is null-safe"), Anims.GetIdleAnim());
	TestNull(TEXT("Idle falls back safely"), Anims.GetIdleAnim());

	UWeaponData* Data = NewObject<UWeaponData>();
	TestTrue(TEXT("WeaponData object created"), IsValid(Data));
	TestNull(TEXT("Weapon mesh may be null"), Data->GetWeaponMesh());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesStatsComponentTest, "RuinMates.Batch1.StatsComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesStatsComponentTest::RunTest(const FString& Parameters)
{
	UStatsComponent* Stats = NewObject<UStatsComponent>();
	TestTrue(TEXT("Stats component created"), IsValid(Stats));
	TestEqual(TEXT("Default health"), Stats->Health, 100.f);
	TestEqual(TEXT("Normalized health"), Stats->GetHealthNormalized(), 1.f);

	TestTrue(TEXT("Damage leaves the owner alive"), Stats->ChangeHealthByAmount(-40.f, nullptr));
	TestEqual(TEXT("Health after damage"), Stats->Health, 60.f);
	TestEqual(TEXT("Death not started yet"), Stats->DeathState, EDeathState::NotDead);

	TestFalse(TEXT("Lethal damage reports not alive"), Stats->ChangeHealthByAmount(-60.f, nullptr));
	TestEqual(TEXT("Health is zero"), Stats->Health, 0.f);
	TestEqual(TEXT("Death started"), Stats->DeathState, EDeathState::DeadStart);

	const EDeathState DeathAfterFirst = Stats->DeathState;
	Stats->StartDeath();
	TestEqual(TEXT("StartDeath is idempotent"), Stats->DeathState, DeathAfterFirst);

	TestTrue(TEXT("Stamina remains after small drain"), Stats->ChangeStaminaByAmount(-10.f));
	TestEqual(TEXT("Stamina after drain"), Stats->Stamina, 90.f);
	TestFalse(TEXT("Full drain reports depleted"), Stats->ChangeStaminaByAmount(-200.f));
	TestEqual(TEXT("Stamina clamped to 0"), Stats->Stamina, 0.f);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesStatsAuthorityTest, "RuinMates.Batch1.StatsAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesStatsAuthorityTest::RunTest(const FString& Parameters)
{
	AActor* ClientActor = NewObject<AActor>();
	TestTrue(TEXT("Client actor created"), IsValid(ClientActor));
	ClientActor->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Simulated proxy has no authority"), ClientActor->HasAuthority());

	UStatsComponent* ClientStats = NewObject<UStatsComponent>(ClientActor);
	TestTrue(TEXT("Client stats created"), IsValid(ClientStats));
	TestEqual(TEXT("Default health on client component"), ClientStats->Health, 100.f);

	TestTrue(TEXT("Rejected client damage still reports alive"), ClientStats->ChangeHealthByAmount(-40.f, nullptr));
	TestEqual(TEXT("Client cannot change health"), ClientStats->Health, 100.f);
	TestEqual(TEXT("Client cannot start death"), ClientStats->DeathState, EDeathState::NotDead);

	ClientStats->StartDeath();
	TestEqual(TEXT("Direct StartDeath is ignored without authority"), ClientStats->DeathState, EDeathState::NotDead);

	TestTrue(TEXT("Rejected client stamina drain still reports remaining"), ClientStats->ChangeStaminaByAmount(-20.f));
	TestEqual(TEXT("Client cannot change stamina"), ClientStats->Stamina, 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesInteractAuthorityTest, "RuinMates.Batch1.InteractAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesInteractAuthorityTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && (Context.WorldType == EWorldType::Editor || Context.WorldType == EWorldType::Game))
			{
				World = Context.World();
				break;
			}
		}
	}

	bool bCreatedTransientWorld = false;
	if (!World)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesInteractAuthTest"));
		if (World)
		{
			World->AddToRoot();
			bCreatedTransientWorld = true;
		}
	}

	if (!TestNotNull(TEXT("Found or created a world for interact actors"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ATestInteractActor* Box = World->SpawnActor<ATestInteractActor>(SpawnParams);
	APawn* Pawn = World->SpawnActor<ADefaultPawn>(SpawnParams);
	const bool bActorsOk = TestTrue(TEXT("Test box spawned"), IsValid(Box))
		&& TestTrue(TEXT("Test pawn spawned"), IsValid(Pawn));
	if (!bActorsOk)
	{
		if (IsValid(Pawn)) { Pawn->Destroy(); }
		if (IsValid(Box)) { Box->Destroy(); }
		if (bCreatedTransientWorld && World)
		{
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}
		return false;
	}

	auto Cleanup = [&]()
	{
		if (IsValid(Pawn)) { Pawn->Destroy(); }
		if (IsValid(Box)) { Box->Destroy(); }
		if (bCreatedTransientWorld && World)
		{
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}
	};

	TestTrue(TEXT("Box implements Interactable"), Box->GetClass()->ImplementsInterface(UInteractable::StaticClass()));
	TestTrue(TEXT("Implementation accepts a valid pawn"), Box->CanInteract_Implementation(Pawn));

	Box->SetRole(ROLE_Authority);
	TestTrue(TEXT("Authority box has authority"), Box->HasAuthority());

	// Editor worlds have not initialized actors, so AActor::ProcessEvent otherwise
	// swallows Execute_* without calling C++ implementations. This guard is test-only
	// and does not change HasAuthority / server permission checks.
	{
		FEditorScriptExecutionGuard ScriptGuard;

		const int32 CallsBeforeNull = Box->GetCanInteractImplCalls();
		TestFalse(TEXT("Null pawn cannot interact"), IInteractable::Execute_CanInteract(Box, nullptr));
		TestTrue(TEXT("Execute_CanInteract invoked implementation for a null pawn"), Box->GetCanInteractImplCalls() > CallsBeforeNull);

		const int32 CallsBeforeValid = Box->GetCanInteractImplCalls();
		TestTrue(TEXT("Can interact with a valid pawn"), IInteractable::Execute_CanInteract(Box, Pawn));
		TestTrue(TEXT("Execute_CanInteract invoked implementation for a valid pawn"), Box->GetCanInteractImplCalls() > CallsBeforeValid);
		TestFalse(TEXT("Prompt is not empty"), IInteractable::Execute_GetInteractPrompt(Box, Pawn).IsEmpty());

		IInteractable::Execute_Interact(Box, Pawn);
		TestEqual(TEXT("Server interact increments count"), Box->GetInteractCount(), 1);

		Box->SetRole(ROLE_SimulatedProxy);
		TestFalse(TEXT("Simulated proxy has no authority"), Box->HasAuthority());
		IInteractable::Execute_Interact(Box, Pawn);
		TestEqual(TEXT("Client interact is ignored"), Box->GetInteractCount(), 1);
	}

	Cleanup();
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
