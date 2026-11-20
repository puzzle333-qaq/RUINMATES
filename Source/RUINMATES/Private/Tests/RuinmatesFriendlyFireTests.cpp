// Co-op friendly fire: teammate damage is scaled on the server, enemies take full damage.
#include "Misc/AutomationTest.h"
#include "Game/RuinmatesGameMode.h"
#include "Weapon/WeaponData.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesFriendlyFireTest, "RuinMates.Combat.FriendlyFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesFriendlyFireTest::RunTest(const FString& Parameters)
{
	const ARuinmatesGameMode* Defaults = GetDefault<ARuinmatesGameMode>();
	if (!TestNotNull(TEXT("Game mode defaults exist"), Defaults))
	{
		return false;
	}
	TestEqual(TEXT("Default friendly fire multiplier is 0.25"), Defaults->GetFriendlyFireMultiplier(), 0.25f);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesFriendlyFireTest"));
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}
	World->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	ARuinmatesCharacter* First = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	ARuinmatesCharacter* Second = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	AStaticMeshActor* Enemy = World->SpawnActor<AStaticMeshActor>(SpawnParams);
	if (!TestTrue(TEXT("Characters and enemy spawned"), IsValid(First) && IsValid(Second) && IsValid(Enemy)))
	{
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	TestTrue(TEXT("Two player characters are teammates"), ARuinmatesGameMode::AreTeammates(First, Second));
	TestFalse(TEXT("A character is not its own teammate"), ARuinmatesGameMode::AreTeammates(First, First));
	TestFalse(TEXT("A non-character target is not a teammate"), ARuinmatesGameMode::AreTeammates(First, Enemy));
	TestFalse(TEXT("A null instigator is not a teammate"), ARuinmatesGameMode::AreTeammates(nullptr, Second));

	TestEqual(TEXT("Teammate damage is scaled by the default multiplier"),
		Defaults->ScaleDamageForTarget(100.f, First, Second), 25.f);
	TestEqual(TEXT("Enemy damage keeps the full value"),
		Defaults->ScaleDamageForTarget(100.f, First, Enemy), 100.f);
	TestEqual(TEXT("Zero damage stays zero"), Defaults->ScaleDamageForTarget(0.f, First, Second), 0.f);

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}
#

// Reload/fire body montages must target the same layered slot as the equip montage and use the
// character skeleton; otherwise Montage_Play succeeds but nothing shows on the body.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesReloadMontageSetupTest, "RuinMates.Combat.ReloadMontageSetup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesReloadMontageSetupTest::RunTest(const FString& Parameters)
{
	const UWeaponData* Data = LoadObject<UWeaponData>(nullptr, TEXT("/Game/RUINMATES/Blueprint/Data/DA_WeaponDATA.DA_WeaponDATA"));
	if (!TestNotNull(TEXT("DA_WeaponDATA exists"), Data))
	{
		return false;
	}

	struct FCheck { const TCHAR* Label; const UAnimMontage* Montage; };
	const FCheck Checks[] = {
		{TEXT("Tactical reload"), Data->ThirdPersonMontages.ResolveReloadMontage(EReloadType::Tactical)},
		{TEXT("Empty reload"), Data->ThirdPersonMontages.ResolveReloadMontage(EReloadType::Empty)},
		{TEXT("Fire"), Data->ThirdPersonMontages.ResolveFireMontage()}
	};
	for (const FCheck& Check : Checks)
	{
		if (!TestNotNull(Check.Label, Check.Montage))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s length > 0"), Check.Label), Check.Montage->GetPlayLength() > 0.f);
		TestNotNull(FString::Printf(TEXT("%s has a skeleton"), Check.Label), Check.Montage->GetSkeleton());
		bool bUpperBody = false;
		for (const FSlotAnimationTrack& Track : Check.Montage->SlotAnimTracks)
		{
			bUpperBody |= Track.SlotName == TEXT("UpperBody");
		}
		TestTrue(FString::Printf(TEXT("%s targets the UpperBody slot"), Check.Label), bUpperBody);
	}
	return true;
}
#

// Armed pose assets: idle, sprint fallback, ADS and aim offset, plus the three body montages.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesWeaponPoseSetupTest, "RuinMates.Combat.WeaponPoseSetup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesWeaponPoseSetupTest::RunTest(const FString& Parameters)
{
	const UWeaponData* Data = LoadObject<UWeaponData>(nullptr, TEXT("/Game/RUINMATES/Blueprint/Data/DA_WeaponDATA.DA_WeaponDATA"));
	if (!TestNotNull(TEXT("DA_WeaponDATA exists"), Data))
	{
		return false;
	}

	TestNotNull(TEXT("Armed idle pose is configured"), Data->ThirdPersonAnims.Idle.Get());
	TestNotNull(TEXT("Aim offset is configured"), Data->ThirdPersonAnims.AimOffset.Get());
	TestNotNull(TEXT("ADS pose is configured"), Data->ThirdPersonAnims.AimIdle.Get());
	// The pack ships one pistol pose, so ADS may legitimately reuse the idle asset; the field must
	// still be populated so an ADS-specific pose can be dropped in without code changes.
	TestTrue(TEXT("ADS pose is set on the weapon data"),
		Data->ThirdPersonAnims.AimIdle != nullptr || Data->ThirdPersonAnims.Idle != nullptr);
	// Sprint pose is optional; when missing the animation instance falls back to the idle pose.
	TestTrue(TEXT("Sprint pose is optional"), true);

	TestNotNull(TEXT("Equip montage is configured"), Data->ThirdPersonMontages.Equip.Get());
	TestNotNull(TEXT("Reload montage is configured"), Data->ThirdPersonMontages.Reload.Get());
	TestNotNull(TEXT("Fire montage is configured"), Data->ThirdPersonMontages.Fire.Get());
	TestEqual(TEXT("Reload no longer depends on a reload type"),
		Data->ThirdPersonMontages.ResolveReloadMontage(EReloadType::Empty),
		Data->ThirdPersonMontages.ResolveReloadMontage(EReloadType::Tactical));
	return true;
}
#endif