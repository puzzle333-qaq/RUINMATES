#include "Misc/AutomationTest.h"
#include "Weapon/WeaponPoseComponent.h"
#include "Gameplay/RuinmatesGameplayTags.h"
#include "Weapon/WeaponTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesWeaponTypesTest, "RuinMates.Batch2.WeaponTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesWeaponTypesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Rifle uses hitscan"), RuinmatesWeaponTypes::UsesHitscan(ERuinmatesWeaponKind::Rifle));
	TestTrue(TEXT("Sniper uses hitscan"), RuinmatesWeaponTypes::UsesHitscan(ERuinmatesWeaponKind::Sniper));
	TestFalse(TEXT("Melee does not use hitscan"), RuinmatesWeaponTypes::UsesHitscan(ERuinmatesWeaponKind::Melee));
	TestTrue(TEXT("Melee uses sweep"), RuinmatesWeaponTypes::UsesMeleeSweep(ERuinmatesWeaponKind::Melee));
	TestFalse(TEXT("Melee does not use left-hand FABRIK"), RuinmatesWeaponTypes::UsesLeftHandFabrik(ERuinmatesWeaponKind::Melee));
	TestEqual(TEXT("Melee tag maps to melee kind"), RuinmatesWeaponTypes::KindFromWeaponTag(RuinmatesTags::Tag_WeaponType_Melee), ERuinmatesWeaponKind::Melee);
	TestEqual(TEXT("Sniper tag maps to sniper kind"), RuinmatesWeaponTypes::KindFromWeaponTag(RuinmatesTags::Tag_WeaponType_Sniper), ERuinmatesWeaponKind::Sniper);
	TestEqual(TEXT("Rifle tag maps to rifle kind"), RuinmatesWeaponTypes::KindFromWeaponTag(RuinmatesTags::Tag_WeaponType_Rifle), ERuinmatesWeaponKind::Rifle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesWeaponPoseNullSafeTest, "RuinMates.Batch2.WeaponPoseNullSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesWeaponPoseNullSafeTest::RunTest(const FString& Parameters)
{
	UWeaponPoseComponent* Pose = NewObject<UWeaponPoseComponent>();
	if (!TestTrue(TEXT("Pose component created"), IsValid(Pose)))
	{
		return false;
	}

	FTransform World = FTransform(FRotator(10.f, 20.f, 30.f), FVector(100.f, 0.f, 0.f));
	TestFalse(TEXT("Missing weapon mesh is not a FABRIK target"), Pose->GetFabrikWorldTransform(World));
	TestTrue(TEXT("Failed resolve clears the output transform"), World.Equals(FTransform::Identity));

	Pose->UpdatePose(1.f);
	TestEqual(TEXT("Null mesh keeps FABRIK alpha at zero"), Pose->GetFabrikAlpha(), 0.f);
	TestTrue(TEXT("Null mesh keeps hand-space transform identity"), Pose->GetFabrikSocketTransform().Equals(FTransform::Identity));

	Pose->SetWeaponKind(ERuinmatesWeaponKind::Melee);
	Pose->SetWeaponMesh(nullptr);
	Pose->UpdatePose(1.f);
	TestEqual(TEXT("Melee disables FABRIK even after a large blend step"), Pose->GetFabrikAlpha(), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
